/**********************************************************************************************
*
*   rREM v0.5 - A simple and easy to use raylib resource embedder
*
*   DESCRIPTION:
*       Tool to embbed resources (images, text, sounds...) into a file; possible uses:
*         - Embedding into a resource file (.rres)
*         - Directly embedding in executable program (.exe)
*
*   DEPENDENCIES:
*       raylib 2.0              - Windowing/input management and drawing.
*       raygui 2.0              - IMGUI controls (based on raylib).
*       miniz                   - zlib-style compression/decompression library (DEFLATE algorithm)
*
*   COMPILATION (Windows - MinGW):
*       gcc -o rrem.exe rrem.c external/miniz.c -s rrem.rc.data -Iexternal / 
*           -lraylib -lopengl32 -lgdi32 -std=c99 -Wall -mwindows
*
// TODO: Review rRES file structure!!!
*
*   rRES file Structure:
*
*   rRES Base Header        (8 bytes)
*       File ID - 'rRES'    (4 byte)
*       RRES Version        (1 byte)    // rRES Format Version (4bit MSB) + Subversion (4bit LSB)
*       <Reserved>          (1 byte)    // Useful in a future... Flags?
*       # Resources         (2 byte)    // Number of resources contained
*
*   rRES Res Info Header    (12 bytes)  // For every resource, one header
*       Res ID              (2 byte)    // Resource unique identifier
*       Res Type            (1 byte)    // Resource type
*                                           // 0 - Image
*                                           // 1 - Text
*                                           // 2 - Sound
*                                           // 3 - Model
*                                           // 4 - Raw
*                                           // 5 - Other?
*       Res Data Comp       (1 byte)    // Data Compression
*                                       // Compresion: 
*                                           // 0 - No compression
*                                           // 1 - RLE compression (custom)
*                                           // 2 - DEFLATE (LZ77+Huffman) compression (miniz lib)
*                                           // 3 - Others? (LZX, LZMA(7ZIP), BZ2, XZ,...)
*       Res Data Size       (4 byte)    // Data size in .rres file (compressed or not, only DATA)
*       Source Data Size    (4 byte)    // Source data size (uncompressed, only DATA)
*
*   rRES Resource Data                  // Depending on resource type, some 'parameters' preceed data
*       Image Data Params   (6 bytes)
*           Width           (2 byte)    // Image width
*           Height          (2 byte)    // Image height
*           Color format    (1 byte)    // Image data color format
*                                           // 0 - Monocrome (1 bit per pixel)
*                                           // 1 - Grayscale (8 bit)
*                                           // 2 - R5G6B5 (16 bit)
*                                           // 3 - R5G5B5A1 (16 bit)
*                                           // 4 - R4G4B4A4 (16 bit)
*                                           // 5 - R8G8B8 (24 bit)
*                                           // 6 - R8G8B8A8 (32 bit) - default
*           Mipmaps count   (1 byte)    // Mipmap images included - default 1
*           DATA                        // Image data
*       Sound Data Params   (6 bytes)   
*           Sample Rate     (2 byte)    // Sample rate (frequency)
*           BitsPerSample   (2 byte)    // Bits per sample
*           Channels        (1 byte)    // Channels (1 - mono, 2 - stereo)
*           <reserved>      (1 byte)    
*           DATA                        // Sound data
*       Model Data Params   (6 bytes?)
*           Num vertex      (4 byte)    // Number of vertex
*           Data Type       (1 byte)    // Vertex arrays provided --> Use like FLAGS: 01011101
*                                           // 0001 - Positions
*                                           // 0011 - position+textcoord
*                                           // position-color-UVcoords-normals-otherUV
*           Indexed Data    // Indexing data will be GREAT!...but it must be calculated!
*                           // Once all data is unindexed, just test positions-UVcoords-normals and create new index!
*                           // Or just store unindexed data... easier!
*           DATA                        // Model vertex data (32 bit per vertex - 1 float)
*       Text Data Params
*           DATA                        // Text data (processed like raw data)
*       Raw Data Params <no-params>
*           DATA                        // Raw data
*
*   USAGE: rrem [-v] [-h] [-o] [-cx] [-n <output_name>] <input_file> [<another_input_file>]
*
*          -v                       Print version information and exit immediately
*          -h                       Print usage help and exit immediately
*          -i                       Activates rayGUI custom user interface (science, bitches)
*          -n <output_file_name>    Name for the output file (default: data.rres) (NOT IMPLEMENTED)
*          -a <rres_file_name>      Append data to an existing .rres file (NOT IMPLEMENTED)
*          -o                       Generate an embeddable object (.o file)
*          -cx                      Specify compression algorithm for data (see usage below)
*
*
*   EXAMPLES:
*       rrem image01.png image02.jpg mysound.wav  # Create 'data.rres' and 'data.h' including those 3 files,\n"
*                                                   uses DEFLATE compression for pixel/wave data.\n"
*       rrem -n images image01.png image02.bmp    # Create 'images.rres' and 'images.h' including those 2 files,\n"
*                                                   uses DEFLATE compression for pixel data.\n"
*       rrem -o image01.png sound.wav text.txt    # Create 'data.o' and 'data.h' including those 3 files,\n"
*                                                   uses DEFLATE compression for pixel/wave/text data.\n"
*
*
*   DEVELOPERS:
*       Ramon Santamaria (@raysan5):   Developer, supervisor, updater and maintainer.
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2014-2017 Ramon Santamaria (@raysan5)
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
***********************************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>             // String management functions: strlen(), strrchr(), strcmp()

#include "raylib.h"             // Required for data loading and management

#define RAYGUI_IMPLEMENTATION
#include "external/raygui.h"    // Required for: IMGUI implementation

//#define RRES_IMPLEMENTATION
//#include "rres.h"               // Required for: rRES management --> Not yet available...

//#include "external/tinyfiledialogs.h"   // Required for: Open/Save file dialogs
#include "external/miniz.h"             // Required for: DEFLATE compression/decompression

//----------------------------------------------------------------------------------
// Some basic Defines
//----------------------------------------------------------------------------------
#define MAX_PATH_LENGTH     256         // Maximum length for resources file paths

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Loaded resource file required info
// TODO: Use rres predefined structs, some info is not exposed...
typedef struct {
    unsigned short resId;
    unsigned char fileName[256];
    unsigned int fileType;
    unsigned int compType;
    unsigned int crypType;
    unsigned int paramsCount;
    unsigned int dataSize;
    int *params;
    void *data;
    //RRES resource;
} ResourceInfo;

//----------------------------------------------------------------------------------
// Global variables
//----------------------------------------------------------------------------------
// Version information
const char *versionInfo = 
    "------------------------------------------------------------------------------\n"
    "rREM v0.1-dev by Ramon Santamaria (@raysan5)\n"
    "\n"
    "Copyright 2014 Ramon Santamaria. All rights reserved.\n"
    "\nVisit www.raylib.com for latest versions.\n"
    "------------------------------------------------------------------------------\n";

// Command line help (usage)
const char *usageHelp = 
    "------------------------------------------------------------------------------\n"
    "rREM Usage: rrem [options] <files>\n"
    "rrem creates a .rres resource with embedded files and a .h header to access embedded data.\n"
    "\n"
    "options:\n"
    "  -v, --version            Print version information and exit immediately\n"
    "  -h, --help               Print this help and exit immediately\n"
    "  -i                       Activates rayGUI custom user interface (science, bitches)\n"
    "  -o                       Generate an embeddable object (.o file)\n"
    "  -cx                      Specify compression algorithm for data\n"
    "       -c0                 No compression\n"
    "       -c1                 RLE compression (custom)\n"
    "       -c2                 DEFLATE compression (default)\n"
    "       -c3                 LZMA compression (NOT IMPLEMENTED YET)\n"
    "       -c4                 BZ2 compression (NOT IMPLEMENTED YET)\n"
    "\n"
    "examples:\n"
    "  # Create 'resources.rres' and 'resources.h' including 3 files:\n\n"
    "           rrem image01.png image02.jpg mysound.wav\n\n"
    "  # Create 'resources.o' and 'resources.h' including 3 files,\n"
    "  # uses NO compression for pixel/wave/text data.\n"
    "           rrem -c0 -o image01.png sound.wav text.txt\n\n"
    "\nCopyright 2014 Ramon Santamaria. All rights reserved.\n"
    "\nVisit www.raylib.com for latest versions.\n"
    "------------------------------------------------------------------------------\n";

//----------------------------------------------------------------------------------
// Module specific Functions Declaration
//----------------------------------------------------------------------------------

// TODO: Compression/Decompression functions and Encryption/Decription functions should be provided by external lib
//static unsigned char *CompressData(const unsigned char *data, unsigned long uncompSize, unsigned long *outCompSize);
//static unsigned char *DecompressData(const unsigned char *data, unsigned long compSize, int uncompSize);

static char *RemoveExtension(const char *fileName, char dot, char sep);

static int GetRRESFileType(const char *ext);


//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    // NOTE: We DON'T need the current working directory! We should use the rrem.exe folder
    // It's easier, just get the folder from argv[0]

    char currentPath[MAX_PATH_LENGTH];
    strcpy(currentPath, GetWorkingDirectory());

    ResourceInfo resources[256];       // RRES_MAX_RESOURCES
    unsigned int resCount = 0;

    // If arguments are greater than 1, we understand command line usage model
    // TODO: Maybe this behaviour is not the expected one...
    if (argc > 1)
    {
        // TODO: Parse arguments and fill resources[] array
        
        // TODO: Process all resources with required configuration
        
        // TODO: Generate rRES file embedding all resources (check if code object file is required!)
        
        return 0;
    }

    int dropsCount = 0;
    char **droppedFiles = { 0 };

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 450, "rREM - raylib resource embedded");
    
    int resSelected = -1;

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------
    
    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (IsFileDropped())
        {
            droppedFiles = GetDroppedFiles(&dropsCount);
            
            // Add dropped files to resources list
            for (int i = 0; i < dropsCount; i++)
            {
                // TODO: Probably we need to use struct RRES and struct RRESChunk
                
                resources[resCount + i].resId = 0;          // TODO: GenerateRRESId()
                strcpy(resources[resCount + i].fileName, droppedFiles[i]);
                resources[resCount + i].fileType = GetRRESFileType(GetExtension(droppedFiles[i]));
                resources[resCount + i].compType = 0;       // Default compression: NONE
                resources[resCount + i].crypType = 0;       // Default encryption: NONE
                
                // TODO: Read file data (depends on fileType)
                
                // TODO: Support multiple chunks resources!
                
                resources[resCount + i].paramsCount = 4;    // TODO: Depends on fileType
                resources[resCount + i].dataSize = 0;       // TODO: We get it when reading file...
                
                resources[resCount + i].params = (int *)malloc(sizeof(int)*resources[resCount + i].paramsCount);
                
                // TODO: Fill data parameters (depends on fileType)
                
                resources[resCount + i].data = 0;           // TODO: Point to resource chunk data
            }
            
            resCount += dropsCount;
            dropsCount = 0;

            ClearDroppedFiles();
        }
        
        // TODO: Implement files scrolling with mouse wheel
        
        // TODO: Implement files focus on mouse-over and file select/unselect on click

        // TODO: Implement file configuration (compression/encryption) --> use raygui
        
        // TODO: On SaveRRESFile(), process all resources data (compress/encrypt) --> second thread with progress bar?
        
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            if (resCount == 0) DrawText("Drop your files to this window!", 100, 40, 20, DARKGRAY);
            else
            {
                DrawText("Dropped files:", 100, 40, 20, DARKGRAY);
                
                for (int i = 0; i < resCount; i++)
                {
                    if (i%2 == 0) DrawRectangle(0, 85 + 40*i, GetScreenWidth(), 40, Fade(LIGHTGRAY, 0.5f));
                    else DrawRectangle(0, 85 + 40*i, GetScreenWidth(), 40, Fade(LIGHTGRAY, 0.3f));
                    
                    DrawText(resources[i].fileName, 120, 100 + 40*i, 10, GRAY);
                }
                
                DrawText("Drop new files...", 100, 110 + 40*resCount, 20, DARKGRAY);
            }

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    ClearDroppedFiles();    // Clear internal buffers
    
    CloseWindow();          // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

// TODO: Command line arguments could be per file... look for a better way to handle this...
static void ParseArguments(int argc, char *argv[])
{
    int compressionType = 0;
    
    // Before starting, process all arguments for options...
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            if ((strcmp(argv[i], "-v")==0) || (strcmp(argv[i], "--version")==0))
            {
                // Print version information and exit immediately
                printf("%s", versionInfo);
                exit(0);
            }
            else if ((strcmp(argv[i], "-h")==0) || (strcmp(argv[i], "--help")==0))
            {
                // Print help and exit immediately
                printf("%s", usageHelp);
                exit(0);
            }
            else if (strcmp(argv[i], "-o")==0)
            {
                // Generate an embeddable object (.o file)
                //genObjRes = true;
                //firstFileArgPos++;
                
                printf("Generating an embeddable object!\n");
            }
            else if (argv[i][1] == 'c')
            {
                // Specified compression algorithm for data (default: DEFLATE)
                switch (argv[i][2])
                {
                    case '0':
                    {
                        compressionType = 0;
                        printf("Data compression algorithm selected: NO COMPRESSION\n");
                    } break;
                    case '1':
                    {
                        compressionType = 1;
                        printf("Data compression algorithm selected: RLE (CUSTOM)\n");
                    } break;
                    case '2':
                    {
                        compressionType = 2;
                        printf("Data compression algorithm selected: DEFLATE\n");
                    } break;
                    case '3':
                    {
                        compressionType = 2;
                        printf("Data compression algorithm selected: LZMA (NOT IMPLEMENTED YET)\n");
                    } break;
                    case '4':
                    {
                        compressionType = 2;
                        printf("Data compression algorithm selected: BZ2 (NOT IMPLEMENTED YET)\n");
                    } break;
                    default: break;
                }
                
                //firstFileArgPos++;
            }
        }
    }
}


/*
// Data compression function
// NOTE: Allocated data MUST be freed!
static unsigned char *CompressData(const unsigned char *data, unsigned long uncompSize, unsigned long *outCompSize)
{
    int compStatus;
    unsigned long tempCompSize = compressBound(uncompSize);
    unsigned char *pComp;

    // Allocate buffer to hold compressed data
    pComp = (mz_uint8 *)malloc((size_t)tempCompSize);
    
    printf("Compression space reserved: %i\n", tempCompSize);
    
    // Check correct memory allocation
    if (!pComp)
    {
        printf("Out of memory!\n");
        return NULL;
    }

    // Compress data
    compStatus = compress(pComp, &tempCompSize, (const unsigned char *)data, uncompSize);
    
    // Check compression success
    if (compStatus != Z_OK)
    {
        printf("Compression failed!\n");
        free(pComp);
        return NULL;
    }

    printf("Compressed from %u bytes to %u bytes\n", (mz_uint32)uncompSize, (mz_uint32)tempCompSize);
    
    if (tempCompSize > uncompSize) printf("WARNING: Compressed data is larger than uncompressed data!!!\n");
    
    *outCompSize = tempCompSize;
    
    return pComp;
}

// Data decompression function
// NOTE: Allocated data MUST be freed!
static unsigned char *DecompressData(const unsigned char *data, unsigned long compSize, int uncompSize)
{
    int decompStatus;
    unsigned long tempUncompSize;
    unsigned char *pUncomp;
    
    // Allocate buffer to hold decompressed data
    pUncomp = (mz_uint8 *)malloc((size_t)uncompSize);
    
    // Check correct memory allocation
    if (!pUncomp)
    {
        printf("Out of memory!\n");
        return NULL;
    }
    
    // Decompress data
    decompStatus = uncompress(pUncomp, &tempUncompSize, data, compSize);
    
    if (decompStatus != Z_OK)
    {
        printf("Decompression failed!\n");
        free(pUncomp);
        return NULL;
    }
    
    if (uncompSize != (int)tempUncompSize)
    {
        printf("WARNING! Expected uncompressed size do not match! Data may be corrupted!\n");
        printf(" -- Expected uncompressed size: %i\n", uncompSize);
        printf(" -- Returned uncompressed size: %i\n", tempUncompSize);
    }

    printf("Decompressed from %u bytes to %u bytes\n", (mz_uint32)compSize, (mz_uint32)tempUncompSize);
    
    return pUncomp;
}

static unsigned char *CompressDataRLE(const unsigned char *data, unsigned int uncompSize, unsigned int *outCompSize)
{
    unsigned char *compData = (unsigned char *)malloc(uncompSize * 2 * sizeof(unsigned char));    // NOTE: We allocate some initial space to store compresed data, 
    // hopefully, it will be < uncompSize but in the worst possible case it could be 2 * uncompSize, so we allocate that space just in case...
    
    printf("Compresed data array allocated! Size: %i\n", uncompSize * 2);
    
    unsigned char count = 1;
    unsigned char currentValue, nextValue;
    
    int j = 0;
    
    currentValue = data[0];
    
    printf("First initial value: %i\n", currentValue);
    //getchar();
    
    for (int i = 1; i < uncompSize; i++)
    {
        nextValue = data[i];
        
        if (currentValue == nextValue)
        {
            if (count == 255)
            {
                compData[j] = count;
                compData[j + 1] = currentValue;
                
                j += 2;
                count = 1;
            }
            else count++;
        }
        else
        {
            compData[j] = count;
            compData[j + 1] = currentValue;
            
            //printf("Data stored Value-Count: %i - %i\n", currentValue, count);
            
            j += 2;
            count = 1;

            currentValue = nextValue;
        }
    }
    
    compData[j] = count;
    compData[j + 1] = currentValue;
    j += 2;
    
    printf("Data stored Value-Count: %i - %i\n", currentValue, count);
        
    compData[j] = 0;    // Just to indicate the end of data
                        // NOTE: Array lenght will be j
    
    printf("Data compressed!\n");

    // Resize memory block with realloc
    compData = (unsigned char *)realloc(compData, j * sizeof(unsigned char));
        
    if (compData == NULL)
    {
        printf("Error reallocating memory!");
        
        free(compData);    // Free the initial memory block
    }
  
    // REMEMBER: compData must be freed!

    //unsigned char *outData = (unsigned char *)malloc((j+1) * sizeof(unsigned char));        
    
    //for (int i = 0; i < (j+1); i++) outData[i] = compData[i];
    
    *outCompSize = (j + 1);  // New array of compressed data lenght

    return compData;    // REMEMBER! This memory should be freed!
}
*/

static int GetRRESFileType(const char *ext)
{
    int type = -1;
/*
    if ((strcmp(ext,"png")==0) || 
        (strcmp(ext,"jpg")==0) ||
        (strcmp(ext,"bmp")==0) ||
        (strcmp(ext,"tga")==0) ||
        (strcmp(ext,"gif")==0) ||
        (strcmp(ext,"pic")==0) ||
        (strcmp(ext,"psd")==0)) type = RRES_TYPE_IMAGE;         // Image
    else if ((strcmp(ext,"txt")==0) ||
             (strcmp(ext,"csv")==0)  ||
             (strcmp(ext,"info")==0)  ||
             (strcmp(ext,"md")==0)) type = RRES_TYPE_TEXT;      // Text
    else if ((strcmp(ext,"wav")==0) ||
             (strcmp(ext,"ogg")==0)) type = RRES_TYPE_WAVE;     // Audio
    else if (strcmp(ext,"obj")==0) type = RRES_TYPE_MESH;       // Mesh
    else type = RRES_TYPE_RAW;     // Unknown
*/
    return type;
}

// Removes the "extension" from a file name
//   fileName is the string to process.
//   dot is the extension separator.
//   sep is the path separator (0 means to ignore).
// NOTE: Returns an allocated string identical to the original but with the extension removed. It must be freed when you're finished with it.
static char *RemoveExtension(const char *fileName, char dot, char sep) 
{
    // NOTE: It seems that this function produces some memory leak...

    char *retstr, *lastdot, *lastsep;

    // Error checks and allocate string
    if (fileName == NULL) return NULL;
    
    if ((retstr = malloc(strlen(fileName) + 1)) == NULL) return NULL;

    // Make a copy and find the relevant characters
    strcpy(retstr, fileName);
    lastdot = strrchr(retstr, dot);
    lastsep = (sep == 0) ? NULL : strrchr(retstr, sep);

    // If it has an extension separator...
    if (lastdot != NULL) 
    {
        // ...and it's before the extenstion separator...
        if (lastsep != NULL) 
        {
            if (lastsep < lastdot) 
            {
                *lastdot = '\0';    // ...then remove it
            }
        } 
        else 
        {
            // Has extension separator with no path separator
            *lastdot = '\0';
        }
    }

    return retstr;    // Return the modified string
}

// Convert int value into 4-bytes array (char buffer)
static void IntToChar(int myint)
{
    unsigned char mychars[4];

    mychars[3] = (myint & (0xFF));
    mychars[2] = ((myint >> 8) & 0xFF);
    mychars[1] = ((myint >> 16) & 0xFF);
    mychars[0] = ((myint >> 24) & 0xFF);
  
/*   
    unsigned int final = 0;
    final |= ( mychars[0] << 24 );
    final |= ( mychars[1] << 16 );
    final |= ( mychars[2] <<  8 );
    final |= ( mychars[3]       );
    
    
    int num = (chars[0] << 24) + (chars[1] << 16) + (chars[2] << 8) + chars[3];
    
    int num = 0;
    for (int i = 0; i != 4; ++i) {
        num += chars[i] << (24 - i * 8);    // |= could have also been used
    }
    
inline unsigned short swap_16bit(unsigned short us)
{
    return (unsigned short)(((us & 0xFF00) >> 8) |
                            ((us & 0x00FF) << 8));
}

inline unsigned long swap_32bit(unsigned long ul)
{
    return (unsigned long)(((ul & 0xFF000000) >> 24) |
                           ((ul & 0x00FF0000) >>  8) |
                           ((ul & 0x0000FF00) <<  8) |
                           ((ul & 0x000000FF) << 24));
}
*/
}

// Converts file binary data into byte array code (.h) 
static void SaveFileAsCode(const char *fileName, const char *data, int dataSize)
{
	FILE *codeFile;

    codeFile = fopen(fileName, "wt");
    
    fprintf(codeFile, "const int dataSize = %i;", dataSize);
	fprintf(codeFile, "const unsigned char %s[%i] = {\n    ", fileName, dataSize);
	
	int blCounter = 0;		// Break line counter
	
	for (int i = 0; i < dataSize; i ++)
	{
		blCounter++;
        
        fprintf(codeFile, "0x%.2x, ", data[i]);
		
        if (blCounter >= 24)
		{
			fprintf(codeFile, "\n    ");
			blCounter = 0;
		}
	}
    
    fprintf(codeFile, " };\n");

	fclose(codeFile);
}

// Check if text string is codified as UTF-8
static bool IsUtf8(const char *string)
{
    if (!string) return 0;

    const unsigned char *bytes = (const unsigned char *)string;
    
    while (*bytes)
    {
        if( (// ASCII
             // use bytes[0] <= 0x7F to allow ASCII control characters
                bytes[0] == 0x09 ||
                bytes[0] == 0x0A ||
                bytes[0] == 0x0D ||
                (0x20 <= bytes[0] && bytes[0] <= 0x7E)
            )
        ) {
            bytes += 1;
            continue;
        }

        if( (// non-overlong 2-byte
                (0xC2 <= bytes[0] && bytes[0] <= 0xDF) &&
                (0x80 <= bytes[1] && bytes[1] <= 0xBF)
            )
        ) {
            bytes += 2;
            continue;
        }

        if( (// excluding overlongs
                bytes[0] == 0xE0 &&
                (0xA0 <= bytes[1] && bytes[1] <= 0xBF) &&
                (0x80 <= bytes[2] && bytes[2] <= 0xBF)
            ) ||
            (// straight 3-byte
                ((0xE1 <= bytes[0] && bytes[0] <= 0xEC) ||
                    bytes[0] == 0xEE ||
                    bytes[0] == 0xEF) &&
                (0x80 <= bytes[1] && bytes[1] <= 0xBF) &&
                (0x80 <= bytes[2] && bytes[2] <= 0xBF)
            ) ||
            (// excluding surrogates
                bytes[0] == 0xED &&
                (0x80 <= bytes[1] && bytes[1] <= 0x9F) &&
                (0x80 <= bytes[2] && bytes[2] <= 0xBF)
            )
        ) {
            bytes += 3;
            continue;
        }

        if( (// planes 1-3
                bytes[0] == 0xF0 &&
                (0x90 <= bytes[1] && bytes[1] <= 0xBF) &&
                (0x80 <= bytes[2] && bytes[2] <= 0xBF) &&
                (0x80 <= bytes[3] && bytes[3] <= 0xBF)
            ) ||
            (// planes 4-15
                (0xF1 <= bytes[0] && bytes[0] <= 0xF3) &&
                (0x80 <= bytes[1] && bytes[1] <= 0xBF) &&
                (0x80 <= bytes[2] && bytes[2] <= 0xBF) &&
                (0x80 <= bytes[3] && bytes[3] <= 0xBF)
            ) ||
            (// plane 16
                bytes[0] == 0xF4 &&
                (0x80 <= bytes[1] && bytes[1] <= 0x8F) &&
                (0x80 <= bytes[2] && bytes[2] <= 0xBF) &&
                (0x80 <= bytes[3] && bytes[3] <= 0xBF)
            )
        ) {
            bytes += 4;
            continue;
        }

        return 0;
    }

    return 1;
}

/*
// Generate C header data for resource usage
// NOTE: Defines resource name and identifier        
static void GenRRESHeaderFile(const char *rresHeaderName, RRES *resources, int resCount)
{
    FILE *headerFile = fopen(rresHeaderName, "wt");
    
    fprintf(headerFile, "#define NUM_RESOURCES %i\n\n", resCount);

    char *name = NULL;
    char *typeName = "";
    char *baseFileName = NULL;
    /*
    for (int i = firstFileArgPos; i < argc; i++)
    {
        baseFileName = GetFileName(argv[i]);
    
        int type = GetRRESFileType(GetExtension(argv[i]));
        
        switch (type)
        {
            case 0: typeName = "IMAGE"; break;
            case 1: typeName = "SOUND"; break;
            case 2: typeName = "MODEL"; break;
            case 3: typeName = "TEXT"; break;
            case 4: typeName = "RAW"; break;
            default: typeName = "UNKNOWN"; break;
        }
    
        name = RemoveExtension(baseFileName, '.', '/');      // String should be de-allocated 
        
        fprintf(headerFile, "#define RES_%s 0x%08x\t\t// Embedded as %s\n", name, resId, typeName);
        
        resId++;
    }
    */
    // free(name);
    // free(typeName);
    // free(baseFileName);
    
    // fclose(headerFile);
// }

// Generate C object compiled file to embed in executable
static void GenRRESObjectFile(const char *rresFileName)
{
    // If .o file generation is required, then we must generate a data.c file and compile it 
    // OPTION: Include tcc compiler lib in program to do not depend on external programs (that can not exist)

    FILE *rresFile = fopen(rresFileName, "rb");
    FILE *codeFile = fopen("data.c", "wt");
    
    // Get rresFile file size
    fseek(rresFile, 0, SEEK_END);
    int fileSize = ftell(rresFile);
    
    if (fileSize > (32*1024*1024)) printf("WARNING: The file you pretend to embed in the exe is larger than 32Mb!!!\n");
    
    printf("rRES file size: %i\n", fileSize);
    
    fprintf(codeFile, "// This file has been automatically generated by rREM - raylib Resource Embedder\n\n");
    
    fprintf(codeFile, "const unsigned char data[%i] = {\n    ", fileSize);
    
    unsigned char *data = (unsigned char *)malloc(fileSize);
    
    fseek(rresFile, 0, SEEK_SET);
    fread(data, fileSize, 1, rresFile);
    
    int blCounter = 0;		// break line counter
     
    for (int i = 0; i < fileSize-1; i ++)
    {
        blCounter++;
        
        fprintf(codeFile, "0x%.2x, ", data[i]);
        
        if (blCounter >= 24)
        {
            fprintf(codeFile, "\n    ");
            blCounter = 0;
        }
    }
    
    fprintf(codeFile, "0x%.2x };\n", data[fileSize-1]);

    fclose(codeFile);
    fclose(rresFile);
    
    free(data);

    //system("gcc -c data.c");  // Compile resource file into object file
    //remove("data.c");         // Remove .c file
}