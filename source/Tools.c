#include "Tools.h"
#include "vshader_flat_shbin.h"
#include "vshader_skybox_shbin.h"
#include "Perlin.h"

extern C3D_Tex fade_tex;
extern float fadeVal;

// Sync
struct sync_device *rocket;

int RandomInteger() {
    return rand();
}

int nnnoise(int x, int y, int oct) {
    if(oct == 0) {
        return(0);
    }
    else {
        srand(x * 100003 + y);
        int randVal = rand() % (1 << 16);
        return(randVal / (2 << oct) + nnnoise(x / 2, y / 2, oct - 1));
    }
}

float badFresnel(float input, float expo) {
    return(fmax(0.0, 0.9 - powf(fmax(input, 0.0), expo)));
}

float lutPosPower(float input, float expo) {
    return(powf(fmax(input, 0.0), expo));
}

float lutAbsLinear(float input, float offset) {
    return(fmax(0.0, offset + abs(input)));
}

float lutAbsInverseLinear(float input, float offset) {
    return(fmax(0.0, offset - abs(input)));
}

float lutOne(float input, float ignored) {
    return(1.0);
}

float lutZero(float input, float ignored) {
    return(0.0);
}

float lutSquaredDist(float input, float offset, float scale) {
    return((input*input) * scale + offset);
}

float lutDist(float input, float offset, float scale) {
    return(sqrt(input*input) * scale + offset);
}

static TickCounter perfCounters[4];
static int perfCounterInitialized = 0;
void startPerfCounter(int idx) {
//     osTickCounterStart(&perfCounters[idx]);
}

void stopPerfCounter(int idx) {
//     osTickCounterUpdate(&perfCounters[idx]);
}

float readPerfCounter(int idx) {
//     return osTickCounterRead(&perfCounters[idx]);
}

void resetShadeEnv() {
    C3D_LightEnvBind(0);
    
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvOpRgb(env, 0, 0, 0);
    C3D_TexEnvOpAlpha(env, 0, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    
    C3D_TexEnv* env2 = C3D_GetTexEnv(1);
    C3D_TexEnvSrc(env2, C3D_Both, GPU_PREVIOUS, 0, 0);
    C3D_TexEnvOpRgb(env2, 0, 0, 0);
    C3D_TexEnvOpAlpha(env2, 0, 0, 0);
    C3D_TexEnvFunc(env2, C3D_Both, GPU_REPLACE);
    
    C3D_TexEnv* env3 = C3D_GetTexEnv(2);
    C3D_TexEnvSrc(env3, C3D_Both, GPU_PREVIOUS, 0, 0);
    C3D_TexEnvOpRgb(env3, 0, 0, 0);
    C3D_TexEnvOpAlpha(env3, 0, 0, 0);
    C3D_TexEnvFunc(env3, C3D_Both, GPU_REPLACE);
    
    C3D_TexEnv* env4 = C3D_GetTexEnv(3);
    C3D_TexEnvSrc(env4, C3D_Both, GPU_PREVIOUS, 0, 0);
    C3D_TexEnvOpRgb(env4, 0, 0, 0);
    C3D_TexEnvOpAlpha(env4, 0, 0, 0);
    C3D_TexEnvFunc(env4, C3D_Both, GPU_REPLACE);
}

// Overlay shader
static DVLB_s* vshader_flat_dvlb;
static shaderProgram_s shaderProgramFlat;
static int shaderProgramFlatCompiled;
static C3D_Mtx projection;

// Skybox shader
static DVLB_s* vshaderSkyboxDvlb;
static shaderProgram_s shaderProgramSkybox;
static int shaderProgramSkyboxCompiled;
static int uLocProjectionSkybox;
static int uLocModelviewSkybox;

void ensureFlatShader() {
    if(shaderProgramFlatCompiled == 0) {
        vshader_flat_dvlb = DVLB_ParseFile((u32*)vshader_flat_shbin, vshader_flat_shbin_size);
        shaderProgramInit(&shaderProgramFlat);
        shaderProgramSetVsh(&shaderProgramFlat, &vshader_flat_dvlb->DVLE[0]);
        shaderProgramFlatCompiled = 1;
    }
    C3D_BindProgram(&shaderProgramFlat);
    
    int uLocProjectionFlat = shaderInstanceGetUniformLocation(shaderProgramFlat.vertexShader, "projection");
    Mtx_OrthoTilt(&projection, 0.0, 400.0, 240.0, 0.0, 0.0, 1.0, true);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjectionFlat, &projection);
    
    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2);
}

void fullscreenQuad(C3D_Tex texture, float shift, float zoom) { 
    ensureFlatShader();
    
    C3D_TexSetFilter(&texture, GPU_LINEAR, GPU_NEAREST);
    C3D_TexBind(0, &texture);
    
    resetShadeEnv();
    
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    u8 fadeValInt = fadeVal * 255.0;
    C3D_TexEnvInit(env);
    // hack
    if(zoom == 1.0) {
        C3D_TexEnvColor(env, 0x00FFFFFF + (fadeValInt << 24));
    }
    else {
        C3D_TexEnvColor(env, 0xFFFFFFFF);
    }
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_CONSTANT, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);

    shift = shift / 400.0;
    float textureLeft = (0.0 + shift) * zoom;
    float textureRight = ((400.0 / 512.0) + shift) * zoom;
    float textureTop = ((1.0 - (float)SCREEN_HEIGHT / (float)512.0)) * zoom;
    float textureBottom = 1.0 * zoom;
   
    // Turn off depth test as well as write
    if(zoom == 1.0) {  // hack
        C3D_DepthTest(false, GPU_GEQUAL, GPU_WRITE_COLOR);
    }
    else {
        C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_COLOR);
    }
   
    // Draw a textured quad directly
    C3D_ImmDrawBegin(GPU_TRIANGLES);
        
        C3D_ImmSendAttrib(0.0, 0.0, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureLeft, textureBottom, 0.0f, 0.0f);

        C3D_ImmSendAttrib(SCREEN_WIDTH, SCREEN_HEIGHT, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureRight, textureTop, 0.0f, 0.0f);

        C3D_ImmSendAttrib(SCREEN_WIDTH, 0.0, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureRight, textureBottom, 0.0f, 0.0f);

        C3D_ImmSendAttrib(0.0, 0.0, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureLeft, textureBottom, 0.0f, 0.0f);

        C3D_ImmSendAttrib(0.0, SCREEN_HEIGHT, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureLeft, textureTop, 0.0f, 0.0f);

        C3D_ImmSendAttrib(SCREEN_WIDTH, SCREEN_HEIGHT, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureRight, textureTop, 0.0f, 0.0f);

    C3D_ImmDrawEnd();
}

void fullscreenQuadHR(C3D_Tex texture, float iod, float iodmult) {
    ensureFlatShader();
    
    C3D_TexSetFilter(&texture, GPU_LINEAR, GPU_NEAREST);
    C3D_TexBind(0, &texture);
    
    resetShadeEnv();
    
//     float preShift = iodmult > 0.0 ? 0.05 : 0.0;
//     float textureLeft = -iod * iodmult + preShift;
//     float textureRight = (float)SCREEN_WIDTH / (float)SCREEN_TEXTURE_WIDTH - iod * iodmult - preShift;
//     float textureTop = 1.0 - (float)SCREEN_HEIGHT / (float)SCREEN_TEXTURE_HEIGHT + preShift * ((float)SCREEN_HEIGHT / (float)SCREEN_WIDTH);
//     float textureBottom = 1.0 - preShift * ((float)SCREEN_HEIGHT / (float)SCREEN_WIDTH);
//     printf("lrtb %f %f %f %f\n", textureLeft, textureRight, textureTop, textureBottom);
    float textureLeft = 0.0;
    float textureRight = 1.0;
    float textureTop = (256.0 - 153.0) / 256.0;
    float textureBottom = 1.0;
//     printf("lrtb %f %f %f %f\n", textureLeft, textureRight, textureTop, textureBottom);
    
    // Turn off depth test as well as write
    C3D_DepthTest(false, GPU_GEQUAL, GPU_WRITE_COLOR);
    
    // Draw a textured quad directly
    C3D_ImmDrawBegin(GPU_TRIANGLES);
        
        C3D_ImmSendAttrib(0.0, 0.0, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureLeft, textureBottom, 0.0f, 0.0f);

        C3D_ImmSendAttrib(SCREEN_WIDTH, SCREEN_HEIGHT, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureRight, textureTop, 0.0f, 0.0f);

        C3D_ImmSendAttrib(SCREEN_WIDTH, 0.0, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureRight, textureBottom, 0.0f, 0.0f);

        C3D_ImmSendAttrib(0.0, 0.0, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureLeft, textureBottom, 0.0f, 0.0f);

        C3D_ImmSendAttrib(0.0, SCREEN_HEIGHT, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureLeft, textureTop, 0.0f, 0.0f);

        C3D_ImmSendAttrib(SCREEN_WIDTH, SCREEN_HEIGHT, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureRight, textureTop, 0.0f, 0.0f);

    C3D_ImmDrawEnd();
}

void fullscreenQuadHRNS(C3D_Tex texture, float iod, float iodmult) {
    ensureFlatShader();
    
    C3D_TexSetFilter(&texture, GPU_LINEAR, GPU_NEAREST);
    C3D_TexBind(0, &texture);
    
    resetShadeEnv();
    
//     float preShift = iodmult > 0.0 ? 0.05 : 0.0;
//     float textureLeft = -iod * iodmult + preShift;
//     float textureRight = (float)SCREEN_WIDTH / (float)SCREEN_TEXTURE_WIDTH - iod * iodmult - preShift;
//     float textureTop = 1.0 - (float)SCREEN_HEIGHT / (float)SCREEN_TEXTURE_HEIGHT + preShift * ((float)SCREEN_HEIGHT / (float)SCREEN_WIDTH);
//     float textureBottom = 1.0 - preShift * ((float)SCREEN_HEIGHT / (float)SCREEN_WIDTH);
//     printf("lrtb %f %f %f %f\n", textureLeft, textureRight, textureTop, textureBottom);
    float textureLeft = -0.18;
    float textureRight = 1.0 + 0.18;
    float textureTop = (256.0 - 153.0) / 256.0 - 0.18;
    float textureBottom = 1.0 + 0.18;
//     printf("lrtb %f %f %f %f\n", textureLeft, textureRight, textureTop, textureBottom);
    
    // Turn off depth test as well as write
    C3D_DepthTest(false, GPU_GEQUAL, GPU_WRITE_COLOR);
    
    // Draw a textured quad directly
    C3D_ImmDrawBegin(GPU_TRIANGLES);
        
        C3D_ImmSendAttrib(0.0, 0.0, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureLeft, textureBottom, 0.0f, 0.0f);

        C3D_ImmSendAttrib(SCREEN_WIDTH, SCREEN_HEIGHT, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureRight, textureTop, 0.0f, 0.0f);

        C3D_ImmSendAttrib(SCREEN_WIDTH, 0.0, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureRight, textureBottom, 0.0f, 0.0f);

        C3D_ImmSendAttrib(0.0, 0.0, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureLeft, textureBottom, 0.0f, 0.0f);

        C3D_ImmSendAttrib(0.0, SCREEN_HEIGHT, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureLeft, textureTop, 0.0f, 0.0f);

        C3D_ImmSendAttrib(SCREEN_WIDTH, SCREEN_HEIGHT, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureRight, textureTop, 0.0f, 0.0f);

    C3D_ImmDrawEnd();
}

void fullscreenQuadFlat(C3D_Tex texture) {
    ensureFlatShader();

    C3D_TexSetFilter(&texture, GPU_LINEAR, GPU_NEAREST);
    C3D_TexBind(0, &texture);
    
    resetShadeEnv();
    
    float textureLeft = 0.0;
    float textureRight = 1.0;
    float textureTop = 0.0;
    float textureBottom = 1.0;
    
    // Turn off depth test as well as write
    C3D_DepthTest(false, GPU_GEQUAL, GPU_WRITE_COLOR);
    
    // Draw a textured quad directly
    C3D_ImmDrawBegin(GPU_TRIANGLES);
        
        C3D_ImmSendAttrib(0.0, 0.0, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureRight, textureBottom, 0.0f, 0.0f); 

        C3D_ImmSendAttrib(SCREEN_WIDTH, SCREEN_HEIGHT, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureLeft, textureTop, 0.0f, 0.0f);

        C3D_ImmSendAttrib(SCREEN_WIDTH, 0.0, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureRight, textureTop, 0.0f, 0.0f);

        C3D_ImmSendAttrib(0.0, 0.0, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureRight, textureBottom, 0.0f, 0.0f);

        C3D_ImmSendAttrib(0.0, SCREEN_HEIGHT, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureLeft, textureBottom, 0.0f, 0.0f);

        C3D_ImmSendAttrib(SCREEN_WIDTH, SCREEN_HEIGHT, 0.5f, 0.0f);
        C3D_ImmSendAttrib(textureLeft, textureTop, 0.0f, 0.0f);

    C3D_ImmDrawEnd();
}

void fullscreenQuadGlitch(C3D_Tex texture, int parts, float time, float amount) {
    ensureFlatShader();

    C3D_TexSetFilter(&texture, GPU_LINEAR, GPU_NEAREST);
    C3D_TexBind(0, &texture);
    
    resetShadeEnv();
    
    // Turn off depth test as well as write
    C3D_DepthTest(false, GPU_GEQUAL, GPU_WRITE_COLOR);
    
    for(int i = 0; i < parts; i++) {
        float quadTop = ((float)i / (float)parts) * SCREEN_HEIGHT;
        float quadBottom = ((float)(i+1) / (float)parts) * SCREEN_HEIGHT;
        float textureLeft =1.0 -  ((float)(i+1) / (float)parts);
        float textureRight = 1.0 - ((float)(i) / (float)parts);
        
        
        float distAmt = pow(sin(noise_at(i * 0.05 - time, 0.1, 0.1)) * amount, 3.0);
        float textureTop = 0.0 + distAmt;
        float textureBottom = 1.0 + distAmt;
        
        float distAmt2 = pow(sin(noise_at((i + 1) * 0.05 - time, 0.1, 0.1)) * amount, 3.0);
        float textureTopLeft = 0.0 + distAmt2;
        float textureBottomLeft = 1.0 + distAmt2;
        
        // Draw a textured quad directly
        C3D_ImmDrawBegin(GPU_TRIANGLES);
            C3D_ImmSendAttrib(0.0, quadTop, 0.5f, 0.0f);
            C3D_ImmSendAttrib(textureRight, textureBottom, 0.0f, 0.0f); 

            C3D_ImmSendAttrib(SCREEN_WIDTH, quadBottom, 0.5f, 0.0f);
            C3D_ImmSendAttrib(textureLeft, textureTopLeft, 0.0f, 0.0f);

            C3D_ImmSendAttrib(SCREEN_WIDTH, quadTop, 0.5f, 0.0f);
            C3D_ImmSendAttrib(textureRight, textureTop, 0.0f, 0.0f);

            C3D_ImmSendAttrib(0.0, quadTop, 0.5f, 0.0f);
            C3D_ImmSendAttrib(textureRight, textureBottom, 0.0f, 0.0f);

            C3D_ImmSendAttrib(0.0, quadBottom, 0.5f, 0.0f);
            C3D_ImmSendAttrib(textureLeft, textureBottomLeft, 0.0f, 0.0f);

            C3D_ImmSendAttrib(SCREEN_WIDTH, quadBottom, 0.5f, 0.0f);
            C3D_ImmSendAttrib(textureLeft, textureTopLeft, 0.0f, 0.0f);
        C3D_ImmDrawEnd();
    }
}

int32_t mulf32(int32_t a, int32_t b) {
        long long result = (long long)a * (long long)b;
        return (int32_t)(result >> 12);
}

int32_t divf32(int32_t a, int32_t b) {
        long long result = (long long)(a << 12) / (long long)b;
        return (int32_t)(result);
}

int32_t loadObject(int32_t numFaces, const index_triangle_t* faces, const init_vertex_t* vertices, const init_vertex_t* normals, const vec2_t* texcoords, vertex* vbo) {
    for(int f = 0; f < numFaces; f++) {
        for(int v = 0; v < 3; v++) {
            // Set up vertex
            uint32_t vertexIndex = faces[f].v[v];
            vbo[f * 3 + v].position[0] = vertices[vertexIndex].x;
            vbo[f * 3 + v].position[1] = vertices[vertexIndex].y;
            vbo[f * 3 + v].position[2] = vertices[vertexIndex].z;
            
            // Set normal to face normal
            vbo[f * 3 + v].normal[0] = normals[faces[f].v[3]].x;
            vbo[f * 3 + v].normal[1] = normals[faces[f].v[3]].y;
            vbo[f * 3 + v].normal[2] = normals[faces[f].v[3]].z;
            
            vbo[f * 3 + v].texcoord[0] = texcoords[vertexIndex].x;
            vbo[f * 3 + v].texcoord[1] = texcoords[vertexIndex].y; 
        }
    }
    return numFaces * 3;
}

int32_t loadObjectRigged(int32_t numFaces, const index_triangle_t* faces, const init_vertex_t* vertices, const init_vertex_t* normals, const vec2_t* texcoords, vertex* vbo) {
    for(int f = 0; f < numFaces; f++) {
        for(int v = 0; v < 3; v++) {
            // Set up vertex
            uint32_t vertexIndex = faces[f].v[v];
            vbo[f * 3 + v].position[0] = vertices[vertexIndex].x;
            vbo[f * 3 + v].position[1] = vertices[vertexIndex].y;
            vbo[f * 3 + v].position[2] = vertices[vertexIndex].z;
            
            // Set normal to face normal
            vbo[f * 3 + v].normal[0] = normals[faces[f].v[3]].x;
            vbo[f * 3 + v].normal[1] = normals[faces[f].v[3]].y;
            vbo[f * 3 + v].normal[2] = normals[faces[f].v[3]].z;
            
            vbo[f * 3 + v].texcoord[0] = texcoords[vertexIndex].x;
            vbo[f * 3 + v].texcoord[1] = texcoords[vertexIndex].y; 
        }
    }
    return numFaces * 3;
}


int32_t loadObject2to1(int32_t numFaces, const index_trianglepv_t* faces, const init_vertex_t* vertices, const init_vertex_t* normals, const vec2_t* texcoords, vertex* vbo) {
    for(int f = 0; f < numFaces; f++) {
        for(int v = 0; v < 3; v++) {
            // Set up vertex
            uint32_t vertexIndex = faces[f].v[v];
            vbo[f * 3 + v].position[0] = vertices[vertexIndex].x;
            vbo[f * 3 + v].position[1] = vertices[vertexIndex].y;
            vbo[f * 3 + v].position[2] = vertices[vertexIndex].z;
            
            // Set normals to vertex normals
            vbo[f * 3 + v].normal[0] = normals[faces[f].v[v+3]].x;
            vbo[f * 3 + v].normal[1] = normals[faces[f].v[v+3]].y;
            vbo[f * 3 + v].normal[2] = normals[faces[f].v[v+3]].z;

            // Set texcoords
            vbo[f * 3 + v].texcoord[0] = texcoords[faces[f].v[v+6]].x;
            vbo[f * 3 + v].texcoord[1] = texcoords[faces[f].v[v+6]].y;
        }
    }
    return numFaces * 3;
}

int32_t loadObject2(int32_t numFaces, const index_trianglepv_t* faces, const init_vertex_t* vertices, const init_vertex_t* normals, const vec2_t* texcoords, vertex2* vbo) {
    for(int f = 0; f < numFaces; f++) {
        for(int v = 0; v < 3; v++) {
            // Set up vertex
            uint32_t vertexIndex = faces[f].v[v];
            vbo[f * 3 + v].position[0] = vertices[vertexIndex].x;
            vbo[f * 3 + v].position[1] = vertices[vertexIndex].y;
            vbo[f * 3 + v].position[2] = vertices[vertexIndex].z;
            
            // Set normals to vertex normals
            vbo[f * 3 + v].normal[0] = normals[faces[f].v[v+3]].x;
            vbo[f * 3 + v].normal[1] = normals[faces[f].v[v+3]].y;
            vbo[f * 3 + v].normal[2] = normals[faces[f].v[v+3]].z;
            
            // Set tangents to vertex tangents
            vbo[f * 3 + v].tangent[0] = normals[faces[f].v[v+3]].x;
            vbo[f * 3 + v].tangent[1] = normals[faces[f].v[v+3]].y;
            vbo[f * 3 + v].tangent[2] = normals[faces[f].v[v+3]].z;            
            
            // Set texcoords
            vbo[f * 3 + v].texcoord[0] = texcoords[faces[f].v[v+6]].x;
            vbo[f * 3 + v].texcoord[1] = texcoords[faces[f].v[v+6]].y;
        }
    }
    return numFaces * 3;
}

void fade() {
    if(fadeVal > 0) {
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_CONSTANT_ALPHA, GPU_ONE_MINUS_CONSTANT_ALPHA);
        fullscreenQuad(fade_tex, 0.0, 1.0);
    }
}

u8* readFileMem(const char* fileName, u32* fileSize, bool linear) {
    FILE *fileptr;
    u8 *buffer;
    long filelen;

    fileptr = fopen(fileName, "rb");
    fseek(fileptr, 0, SEEK_END);
    *fileSize = ftell(fileptr);
    rewind(fileptr);
    
    if(linear) {
        buffer = (u8*)linearAlloc((1 + (*fileSize))*sizeof(u8));
    }
    else {
        buffer = (u8*)malloc((1 + (*fileSize))*sizeof(u8));
    }
    fread(buffer, *fileSize, 1, fileptr);
    fclose(fileptr);
    return(buffer);
}

// Print a message and wait for A B (good for debugging without access
// to an actual ICD)
#define SHITTY_BREAKPOINTS
void waitForA(const char* msg) {
#ifdef SHITTY_BREAKPOINTS
    printf(msg);
    printf("\n");
    while(1) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_A) {
            break;
        }
    }
    
    while(1) {
        hidScanInput();
         u32 kDown = hidKeysDown();
         if (kDown & KEY_B) {
            break;
        }
    }
#endif
} 

// Texture loading helper using t3x format files and loading from romfs
void loadTexture(C3D_Tex* tex, C3D_TexCube* cube, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("Texture file not found: %s\n", path);
        return;
    }
    
    Tex3DS_Texture t3x = Tex3DS_TextureImportStdio(f, tex, cube, true); 
    fclose(f); 
    if (!t3x) {
        printf("Texture load failure on %s\n", path);
        return;
    }
    
    // Set basic options 
    C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(tex, GPU_REPEAT, GPU_REPEAT);

    // Delete the t3x object since we don't need it
    Tex3DS_TextureFree(t3x);
}

// Set bone mat uniforms from sync value
// Lerps the matrices for nicer between frame interpolation
// Assumes loop from last to first frame
void setBonesFromSync(fbxBasedObject* model, int* boneLocs, float row) {
    int frameCount = model->frameCount;
    int boneCount = model->boneCount;

    // Figure out where in the animation we are
    float animPosFloat = sync_get_val(model->frameSync, row);
    int animPos = (int)animPosFloat;
    float animPosRemainder = animPosFloat - (float)animPos;
    animPos = animPos % frameCount;
    int animPosNext = (animPos + 1) % frameCount;

    // Set bones
    C3D_Mtx boneMat;   
    for(int i = 0; i < boneCount; i++) {
        Mtx_Identity(&boneMat);
        for(int j = 0; j < 4 * 3; j++) {
            int amimIdxA = FBX_FRAME_IDX(animPos, i, j, boneCount);
            int amimIdxB = FBX_FRAME_IDX(animPosNext, i, j, boneCount);
            boneMat.m[j] = model->animFrames[amimIdxA] * (1.0 - animPosRemainder) + model->animFrames[amimIdxB] * animPosRemainder;
        }
        C3D_FVUnifMtx3x4(GPU_VERTEX_SHADER, boneLocs[i], &boneMat);
    }
}

// Get a matrix from sync value and bone nb
// good for parenting other crap onto a bone, like the camera
void getBoneMat(fbxBasedObject* model, float row, C3D_Mtx* boneMat, int boneNb) {
    int frameCount = model->frameCount;
    int boneCount = model->boneCount;

    // Figure out where in the animation we are
    float animPosFloat = sync_get_val(model->frameSync, row);
    int animPos = (int)animPosFloat;
    float animPosRemainder = animPosFloat - (float)animPos;
    animPos = animPos % frameCount;
    int animPosNext = (animPos + 1) % frameCount;

    // Set bones
    Mtx_Identity(boneMat);
    for(int j = 0; j < 4 * 3; j++) {
        int amimIdxA = FBX_FRAME_IDX(animPos, boneNb, j, boneCount);
        int amimIdxB = FBX_FRAME_IDX(animPosNext, boneNb, j, boneCount);
        boneMat->m[j] = model->animFrames[amimIdxA] * (1.0 - animPosRemainder) + model->animFrames[amimIdxB] * animPosRemainder;
    }
}

// Load an FBX file
// Will load the named object from the file, put the vertices in the vbo and the frames in frames, allocating both appropriately.
fbxBasedObject loadFBXObject(const char* filename, C3D_Tex* texture, const char* syncPrefix) {
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Error: failed to open file for reading.\n");
        exit(1);
    }

    // Read the vertCount and frameCount from the file
    int32_t vertCount, boneCount, frameCount;
    fread(&vertCount, sizeof(int32_t), 1, fp);
    fread(&boneCount, sizeof(int32_t), 1, fp);
    fread(&frameCount, sizeof(int32_t), 1, fp);
    printf("Loaded %d verts, %d bones, %d frames\n", vertCount, boneCount, frameCount);
    
    // Allocate memory for the object
    fbxBasedObject object;
    object.vertCount = vertCount;
    object.boneCount = boneCount;
    object.frameCount = frameCount;
    object.vbo = (vertex_rigged*)linearAlloc(sizeof(vertex_rigged) * vertCount);
    object.animFrames = (float*)malloc(sizeof(float) * frameCount * object.boneCount * 12);

    // Read the vbo from the file
    fread(object.vbo, sizeof(vertex_rigged), vertCount, fp);

    // Read the animFrames from the file
    fread(object.animFrames, sizeof(float), frameCount * object.boneCount * 12, fp);
    fclose(fp);

    // Load sync track
    char trackName[255];
    sprintf(trackName, "%s.frame", syncPrefix);
    object.frameSync = sync_get_track(rocket, trackName);

    // Store texture
    object.tex = texture;
    
    return object;
}

void freeFBXObject(fbxBasedObject* object) {
    linearFree(object->vbo);
    free(object->animFrames);
}

inline void skyboxQuadImmediate(vec3_t a, vec3_t b, vec3_t c, vec3_t d) {
    C3D_ImmSendAttrib(a.x, a.y, a.z, 0.0f);
    C3D_ImmSendAttrib(b.x, b.y, b.z, 0.0f);
    C3D_ImmSendAttrib(c.x, c.y, c.z, 0.0f);
    C3D_ImmSendAttrib(a.x, a.y, a.z, 0.0f);
    C3D_ImmSendAttrib(c.x, c.y, c.z, 0.0f);
    C3D_ImmSendAttrib(d.x, d.y, d.z, 0.0f);
}

void skyboxCubeImmediate(C3D_Tex* texture, float r, vec3_t cp, C3D_Mtx* modelview, C3D_Mtx* projection) {
    // Ensure skybox shader is compiled and bound
    if(shaderProgramSkyboxCompiled == 0) {
        vshaderSkyboxDvlb = DVLB_ParseFile((u32*)vshader_skybox_shbin, vshader_skybox_shbin_size);
        shaderProgramInit(&shaderProgramSkybox);
        shaderProgramSetVsh(&shaderProgramSkybox, &vshaderSkyboxDvlb->DVLE[0]);
        C3D_BindProgram(&shaderProgramSkybox);
        shaderProgramSkyboxCompiled = 1;
    }
    C3D_BindProgram(&shaderProgramSkybox);
    
    // Clear shade env and set up straight passthrough
    resetShadeEnv();
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    C3D_TexBind(0, texture);
    C3D_TexSetFilter(texture, GPU_LINEAR, GPU_NEAREST);
    C3D_TexSetWrap(texture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    // Set up shader uniforms
    uLocModelviewSkybox = shaderInstanceGetUniformLocation(shaderProgramSkybox.vertexShader, "modelview");
    uLocProjectionSkybox = shaderInstanceGetUniformLocation(shaderProgramSkybox.vertexShader, "projection");
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocModelviewSkybox, modelview);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjectionSkybox, projection);

    // Set up attributes for single attribute 3 floats
    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);

    // Cube corners
    vec3_t a = vec3add(cp, vec3(-r, -r, -r));
    vec3_t b = vec3add(cp, vec3( r, -r, -r));
    vec3_t c = vec3add(cp, vec3( r, -r,  r));
    vec3_t d = vec3add(cp, vec3(-r, -r,  r));
    vec3_t e = vec3add(cp, vec3(-r,  r, -r));
    vec3_t f = vec3add(cp, vec3( r,  r, -r));
    vec3_t g = vec3add(cp, vec3( r,  r,  r));
    vec3_t h = vec3add(cp, vec3(-r,  r,  r));
    
    // Now build the cube, immediate mode style
    C3D_CullFace(GPU_CULL_NONE);
    C3D_ImmDrawBegin(GPU_TRIANGLES);    
        skyboxQuadImmediate(a, b, c, d);
        skyboxQuadImmediate(d, c, g, h);
        skyboxQuadImmediate(h, g, f, e);
        skyboxQuadImmediate(e, f, b, a);
        skyboxQuadImmediate(b, f, g, c);
        skyboxQuadImmediate(e, a, d, h);
    C3D_ImmDrawEnd();
}