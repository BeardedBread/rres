/**********************************************************************************************
*
*   rres-raylib v1.0 - rres loaders specific for raylib data structures
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2020-2021 Ramon Santamaria (@raysan5)
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
**********************************************************************************************/

#ifndef RRES_RAYLIB_H
#define RRES_RAYLIB_H

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
//...

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
//...

//----------------------------------------------------------------------------------
// Global variables
//----------------------------------------------------------------------------------
//...

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

RRESDEF void *rresLoadRaw(rresData rres, int *size);
RRESDEF char *rresLoadText(rresData rres);
RRESDEF Image rresLoadImage(rresData rres);
RRESDEF Wave rresLoadWave(rresData rres);
RRESDEF Font rresLoadFont(rresData rres);
RRESDEF Mesh rresLoadMesh(rresData rres);
RRESDEF Material rresLoadMaterial(rresData rres);
RRESDEF Model rresLoadModel(rresData rres);

#endif // RRES_RAYLIB_H

/***********************************************************************************
*
*   RRES IMPLEMENTATION
*
************************************************************************************/

#if defined(RRES_RAYLIB_IMPLEMENTATION)

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
//...

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
//...

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
//...

//----------------------------------------------------------------------------------
// Module specific Functions Declaration
//----------------------------------------------------------------------------------

// Load simple data chunks that are later required by multi-chunk resources

static void *rresLoadDataChunkRaw(rresDataChunk chunk);         // Load chunk: RRES_DATA_RAW
static char *rresLoadDataChunkText(rresDataChunk chunk);        // Load chunk: RRES_DATA_TEXT
static Image rresLoadDataChunkImage(rresDataChunk chunk);       // Load chunk: RRES_DATA_IMAGE
static Wave rresLoadDataChunkWave(rresDataChunk chunk);         // Load chunk: RRES_DATA_WAVE
static void *rresLoadDataChunkVertex(rresDataChunk chunk);      // Load chunk: RRES_DATA_VERTEX
static CharInfo *rresLoadDataChunkChars(rresDataChunk chunk);   // Load chunk: RRES_DATA_CHARS (multi)
//static Mesh *rresLoadDataChunkMesh(rresDataChunk chunk);        // Load chunk: RRES_DATA_MESH (multi)


//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------

void *rresLoadRaw(rresData rres, int *size)
{
    void *data = NULL;
    
    if ((rres.count >= 1) && (rres.chunks[0].type == RRES_DATA_RAW))
    {
        data = rresLoadDataChunkRaw(rres.chunks[0]);
    }

    return data;
}

// NOTE: Text must be '\0' terminated
char *rresLoadText(rresData rres)
{
    char *text = NULL;
    
    if ((rres.count >= 1) && (rres.chunks[0].type == RRES_DATA_TEXT))
    {
        text = rresLoadDataChunkText(rres.chunks[0]);
    }

    return text;
}

Image rresLoadImage(rresData rres)
{
    Image image = { 0 };
    
    if ((rres.count >= 1) && (rres.chunks[0].type == RRES_DATA_IMAGE))
    {
        image = rresLoadDataChunkImage(chunk);
    }
    
    return image;
}

Wave rresLoadWave(rresData rres)
{
    Wave wave = { 0 };
    
    if ((rres.count >= 1) && (rres.chunks[0].type == RRES_DATA_WAVE))
    {
        wave.sampleCount = rres.chunks[0].props[0];
        wave.sampleRate = rres.chunks[0].props[1];
        wave.sampleSize = rres.chunks[0].props[2];
        wave.channels = rres.chunks[0].props[3];
        
        unsigned int size = sampleCount*sampleSize/8;
        wave.data = RL_MALLOC(size);
        memcpy(wave.data, rres.chunks[0].data, size);   
    }
    
    return wave;
}

Font rresLoadFont(rresData rres)
{
    Font font = { 0 };

    // Font resource could consist of several chunks:
    //  - RRES_DATA_FONT: Basic font properties, no data
    //  - RRES_DATA_IMAGE: Image atlas for the font characters
    //  - RRES_DATA_CHARS: Font glyphs information (optional)
    if (rres.count >= 2)
    {
        if (rres.chunks[0].type == RRES_DATA_FONT)
        {
            // Load font basic properties from chunk[0]
            font.baseSize = rres.chunks[0].props[0];           // Base size (default chars height)
            font.charsCount = rres.chunks[0].props[1];         // Number of characters (glyphs)
            font.charsPadding = rres.chunks[0].props[2];       // Padding around the chars
            
            // Font rectangle properties come as individual properties
            font.recs = (Rectangle *)RRES_MALLOC(font.charsCount*sizeof(Rectangle));
            
            for (int i = 0; i < font.charsCount; i++)
            {
                font.recs[i].x = (float)rres.chunks[0].props[3 + i*4];
                font.recs[i].y = (float)rres.chunks[0].props[3 + i*4 + 1];
                font.recs[i].width = (float)rres.chunks[0].props[3 + i*4 + 2];
                font.recs[i].height = (float)rres.chunks[0].props[3 + i*4 + 3];
            }
            
            // NOTE: RRES_DATA_FONT chunk does not contain data (chunks[0].data == NULL) 
            
            // Load font image chunk
            if ((rres.count >= 2) && (rres.chunks[1].type == RRES_DATA_IMAGE))
            {
                Image image = rresLoadDataChunkImage(rres.chunks[1]);
                font.texture = LoadTextureFromImage(image);
                UnloadImage(image);
                
                // TODO: Consider the case there is no RRES_DATA_CHARS?
                //LoadFontFromImage(image, Color key, int firstChar);
            }
            
            // Load font chars info chunk
            if ((rres.count >= 4) && (rres.chunks[2].type == RRES_DATA_CHARS))
            {
                int charsCount = rres.chunks[2].props[0];
                
                for (int i = 0; i < charsCount; i++)
                {
                    // Check if next char resource is available
                    if (rres.count >= (5 + i*2))
                    {
                        font.chars[i].value = rres.chunks[3 + i].props[0];
                        font.chars[i].offsetX = rres.chunks[3 + i].props[1]; 
                        font.chars[i].offsetY = rres.chunks[3 + i].props[2]; 
                        font.chars[i].advanceX = rres.chunks[3 + i].props[3];
                    
                        font.chars[i].image = rresLoadDataChunkImage(rres.chunks[3 + i + 1]);
                    }
                }
            }
        }
    }

    return font;
}

Mesh rresLoadMesh(rresData rres)
{
    Mesh mesh = { 0 };

    mesh.vertexCount = rres.chunks[0].props[0];
    
    // TODO: Make sure all rres.chunks[i] vertexCount are the same?
    
    for (int i = 0; i < rres.count; i++)
    {
        if (rres.chunks[i].type == RRES_DATA_VERTEX)
        {
            switch (rres.chunks[i].props[1])
            {
                case RRES_VERT_POSITION: if (rres.chunks[i].props[2] == RRES_VERT_FORMAT_FLOAT) mesh.vertices = (float *)rres.chunks[i].data; break;
                case RRES_VERT_TEXCOORD1: if (rres.chunks[i].props[2] == RRES_VERT_FORMAT_FLOAT) mesh.texcoords = (float *)rres.chunks[i].data; break;
                case RRES_VERT_TEXCOORD2: if (rres.chunks[i].props[2] == RRES_VERT_FORMAT_FLOAT) mesh.texcoords2 = (float *)rres.chunks[i].data; break;
                case RRES_VERT_TEXCOORD3: break;    // Not supported by raylib mesh format
                case RRES_VERT_TEXCOORD4: break;    // Not supported by raylib mesh format
                case RRES_VERT_NORMAL: if (rres.chunks[i].props[2] == RRES_VERT_FORMAT_FLOAT) mesh.normals = (float *)rres.chunks[i].data; break;
                case RRES_VERT_TANGENT: if (rres.chunks[i].props[2] == RRES_VERT_FORMAT_FLOAT) mesh.tangents = (float *)rres.chunks[i].data; break;
                case RRES_VERT_COLOR: if (rres.chunks[i].props[2] == RRES_VERT_FORMAT_BYTE) mesh.colors = (unsigned char *)rres.chunks[i].data; break;
                case RRES_VERT_INDEX: if (rres.chunks[i].props[2] == RRES_VERT_FORMAT_SHORT) mesh.indices = (unsigned short *)rres.chunks[i].data; break;
                default: break;
            }
        }
    }
    
    return mesh;
}

//----------------------------------------------------------------------------------
// Module specific Functions Definition
//----------------------------------------------------------------------------------

// Load data chunk: RRES_DATA_RAW
static void *rresLoadDataChunkRaw(rresDataChunk chunk)
{
    void *rawData = NULL;
    
    if (chunk.type == RRES_DATA_RAW)
    {
        rawData = RL_MALLOC(chunks.props[0]);
        memcpy(rawData, chunk.data, chunk.props[0]);
    }
    
    return rawData;
}

// Load data chunk: RRES_DATA_TEXT
static char *rresLoadDataChunkText(rresDataChunk chunk)
{
    void *text = NULL;
    
    if (chunk.type == RRES_DATA_TEXT)
    {
        text = (char *)RL_MALLOC(chunks.props[0]);
        memcpy(text, chunk.data, chunk.props[0]);
    }
    
    return text;
}

// Load data chunk: RRES_DATA_IMAGE
static Image rresLoadDataChunkImage(rresDataChunk chunk)
{
    Image image = { 0 };
    
    if (chunk.type == RRES_DATA_IMAGE)
    {
        image.width = chunk.props[0];
        image.height = chunk.props[1];
        image.mipmaps = chunk.props[2];
        
        // WARNING: rresPixelFormat enum matches raylib PixelFormat enum values
        image.format = chunk.props[3];

        // NOTE: Image data size can computed from image properties
        unsigned int size = GetPixelDataSize(image.width, image.height, image.format);
        image.data = RL_MALLOC(size);
        memcpy(image.data, chunk.data, size);
        
        // TODO: Consider mipmaps data!
    }
    
    return image;
}

#endif // RRES_RAYLIB_IMPLEMENTATION
