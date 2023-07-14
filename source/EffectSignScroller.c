/**
* train platform info sign with the scrolltext
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#include "Tools.h"
#include "GraphicsLibrary/MonoFont.h"
 
// Global data: Matrices
static int uLocProjection;
static int uLocModelview; 
static C3D_Mtx projection;
 
// Global data: Lighting
static C3D_LightEnv lightEnv;
static C3D_Light light;
static C3D_LightLut lutPhong;
static C3D_LightLut lutShittyFresnel;

// Boooones
static int uLocBone[21];

static C3D_Tex texBase;
static C3D_Tex texSky;
static C3D_TexCube texSkyCube;
static fbxBasedObject modelSignpost;
static fbxBasedObject modelSign;

static const struct sync_track* sync_cpyr[3];
static const struct sync_track* sync_cpos[3];
static const struct sync_track* sync_zoom;

extern Font London40RegularMono; 

static Bitmap textmap;
static Pixel* textpx;
static C3D_Tex texScroll;

static void fontRender(int x, int y, const char* str) {
    Pixel col = 0xffff00ffu; // yellow, hopefully
    ClearBitmap(&textmap);
    DrawSimpleString(&textmap, &London40RegularMono, x, y, col, str);
    /*DrawSimpleString(&textmap, &London40RegularMono.rle.font, x, y, col, str);*/
    /*DrawCircle(&textmap, 128, 128, 30, col);*/
}
static void fontFlushToGPU(void) {
    //C3D_TexUpload(&texScroll, textmap.pixels);
    GSPGPU_FlushDataCache(textmap.pixels, 256 * 256 * sizeof(Pixel));
    GX_DisplayTransfer((u32*)textmap.pixels, GX_BUFFER_DIM(256, 256),
            (u32*)texScroll.data, GX_BUFFER_DIM(256, 256), TEXTURE_TRANSFER_FLAGS);
    gspWaitForPPF();
    /*C3D_SyncDisplayTransfer((u32*)textmap.pixels, GX_BUFFER_DIM(256, 256),
            (u32*)texScroll.data, GX_BUFFER_DIM(256, 256), TEXTURE_TRANSFER_FLAGS);*/
    /*C3D_SyncTextureCopy((u32*)textmap.pixels, GX_BUFFER_DIM(256, 256),
            (u32*)texScroll.data, GX_BUFFER_DIM(256, 256),
            256*256*sizeof(Pixel), TEXTURE_TRANSFER_FLAGS);*/
}

void effectSignScrollerInit() {
    // Prep general info: Shader (precompiled in main for important ceremonial reasons)
    C3D_BindProgram(&shaderProgramBones);

    // Prep general info: Uniform locs 
    uLocProjection = shaderInstanceGetUniformLocation(shaderProgramBones.vertexShader, "projection");
    uLocModelview = shaderInstanceGetUniformLocation(shaderProgramBones.vertexShader, "modelView");

    // Prep general info: Bones 
    char boneName[255];
    for(int i = 0; i < 21; i++) {
        sprintf(boneName, "bone%02d", i);
        uLocBone[i] = shaderInstanceGetUniformLocation(shaderProgramBones.vertexShader, boneName);
    }

    // init font stuff
    size_t w = 256, h = 256;
    size_t bpr = BytesPerRowForWidth(w);
    textpx = (Pixel*)linearAlloc(bpr * h);
    InitializeBitmap(&textmap, w, h, bpr, textpx);
    C3D_TexInit(&texScroll, w, h, GPU_RGBA8);

    // render font?!
    fontRender(4, 4, "hello world");
    fontFlushToGPU();

    // Load a model
    loadTexture(&texBase, NULL, "romfs:/tex_3ds_test.bin");
    loadTexture(&texSky, &texSkyCube, "romfs:/sky_cube.bin");
    modelSignpost = loadFBXObject("romfs:/obj_signpost_sign_post.vbo", &texBase, "signscroll.signpost.frame");
    modelSign = loadFBXObject("romfs:/obj_signpost_sign_signs.vbo", &texScroll, "signscroll.sign.frame");

    for (int i = 0; i < 3; ++i) {
        const char* names[3] = { "pitch", "yaw", "roll" };
        sprintf(boneName, "signscroll.c%s", names[i]);
        sync_cpyr[i] = sync_get_track(rocket, boneName);
        sprintf(boneName, "signscroll.cpos.%c", "xyz"[i]);
        sync_cpos[i] = sync_get_track(rocket, boneName);
    }
    sync_zoom = sync_get_track(rocket, "signscroll.czoom");
}

// TODO: Split out shade setup
static void drawModel(fbxBasedObject* model, float row) {
    // Update bone mats
    setBonesFromSync(model, uLocBone, row);

    // Set up attribute info
    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0 = position (float3)
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1 = bone indices (float2)
    AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 2); // v2 = bone weights (float2)
    AttrInfo_AddLoader(attrInfo, 3, GPU_FLOAT, 3); // v3 = normal (float3)
    AttrInfo_AddLoader(attrInfo, 4, GPU_FLOAT, 2); // v4 = texcoords (float2)

    // Begin frame and bind shader
    C3D_BindProgram(&shaderProgramBones);

    // Add VBO to draw buffer
    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, (void*)model->vbo, sizeof(vertex_rigged), 5, 0x43210);

    // Bind texture
    C3D_TexBind(0, model->tex);    
    C3D_TexSetFilter(model->tex, GPU_LINEAR, GPU_LINEAR);

    // Set up lighting
    C3D_LightEnvInit(&lightEnv);
    C3D_LightEnvBind(&lightEnv);
    
    LightLut_Phong(&lutPhong, 100.0);
    C3D_LightEnvLut(&lightEnv, GPU_LUT_D0, GPU_LUTINPUT_LN, false, &lutPhong);
    
    // Add funny edge lighting that makes 3D pop
    float lightStrengthFresnel = 0.5;
    LightLut_FromFunc(&lutShittyFresnel, badFresnel, lightStrengthFresnel, false);
    C3D_LightEnvLut(&lightEnv, GPU_LUT_FR, GPU_LUTINPUT_NV, false, &lutShittyFresnel);
    C3D_LightEnvFresnel(&lightEnv, GPU_PRI_SEC_ALPHA_FRESNEL);
    
    // Basic shading with diffuse + specular
    C3D_FVec lightVec = FVec4_New(0.0, 0.0, 0.0, 1.0);
    C3D_LightInit(&light, &lightEnv);
    C3D_LightColor(&light, 50.0, 50.0, 50.0);
    C3D_LightPosition(&light, &lightVec);

    C3D_Material lightMaterial = {
        { 0.2, 0.2, 0.2 }, //ambient
        { 1.0,  1.0,  1.0 }, //diffuse
        { 1.0f, 1.0f, 1.0f }, //specular0
        { 0.0f, 0.0f, 0.0f }, //specular1
        { 0.0f, 0.0f, 0.0f }, //emission
    };
    C3D_LightEnvMaterial(&lightEnv, &lightMaterial);

    // Set up texture combiners
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, GPU_FRAGMENT_PRIMARY_COLOR, 0);
    C3D_TexEnvOpRgb(env, 0, 0, 0);
    C3D_TexEnvFunc(env, C3D_RGB, GPU_MODULATE);
    
    env = C3D_GetTexEnv(1); 
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_RGB, GPU_FRAGMENT_SECONDARY_COLOR, GPU_PREVIOUS, 0);
    C3D_TexEnvOpRgb(env, GPU_TEVOP_RGB_SRC_ALPHA , 0, 0);
    C3D_TexEnvFunc(env, C3D_RGB, GPU_ADD);

    env = C3D_GetTexEnv(2);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Alpha, GPU_CONSTANT, GPU_PREVIOUS, 0);
    C3D_TexEnvFunc(env, C3D_Alpha, GPU_REPLACE);  

    // GPU state for normal drawing with transparency 
    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_BACK_CCW);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
 
    // Actual drawcall
    C3D_DrawArrays(GPU_TRIANGLES, 0, model->vertCount);
}

void effectSignScrollerRender(C3D_RenderTarget* targetLeft, C3D_RenderTarget* targetRight, float row, float iod) {

    fontRender(100, 100, "hello world");
    fontFlushToGPU();

    // Frame starts (TODO pull out?)   
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
 
    // Send modelview 
    C3D_Mtx baseview;
    Mtx_Identity(&baseview);
    /*Mtx_RotateZ(&baseview, M_PI, true);
    Mtx_RotateX(&baseview, -M_PI / 2, true);
    Mtx_RotateY(&baseview, M_PI, true);*/
    Mtx_Translate(&baseview, 0.0, -10.0, -200.0, true);
  
    C3D_Mtx camMat;
    //getBoneMat(&camProxy, row, &camMat, 3);
    //Mtx_Inverse(&camMat);
    float cpitch = sync_get_val(sync_cpyr[0], row),
          cyaw   = sync_get_val(sync_cpyr[1], row),
          croll  = sync_get_val(sync_cpyr[2], row);
    C3D_FVec cpos = FVec3_New(sync_get_val(sync_cpos[0], row),
                              sync_get_val(sync_cpos[1], row),
                              sync_get_val(sync_cpos[2], row));
    float czoom = sync_get_val(sync_zoom, row);

    C3D_FVec p1 = FVec3_New(0,0,1);
    C3D_FVec p2 = FVec3_New(0,0,0);
    C3D_FVec up = FVec3_New(0,1,0);
    Mtx_LookAt(&camMat, p1, p2, up, false);

    Mtx_RotateX(&camMat, cpitch, false);
    Mtx_RotateY(&camMat, cyaw  , false);
    Mtx_RotateZ(&camMat, croll , false);
    Mtx_Translate(&camMat, cpos.x, cpos.y, cpos.z, false);
    Mtx_Scale(&camMat, czoom, czoom, czoom);

    C3D_Mtx modelview;
    Mtx_Multiply(&modelview, &baseview, &camMat);

    C3D_Mtx skyview;
    Mtx_Multiply(&skyview, &baseview, &camMat);
    Mtx_RotateZ(&skyview, row * 0.05, true);

     // Left eye 
    C3D_FrameDrawOn(targetLeft);  
    C3D_RenderTargetClear(targetLeft, C3D_CLEAR_ALL, 0x404040FF, 0);
    
    // Uniform setup
    Mtx_PerspStereoTilt(&projection, 20.0f*M_PI/180.0f, 400.0f/240.0f, 0.01f, 6000.0f, -iod,  7.0f, false);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projection);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocModelview,  &modelview);

    // Dispatch drawcalls
    drawModel(&modelSignpost, row);
    drawModel(&modelSign, row);
    skyboxCubeImmediate(&texSky, 1000.0f, vec3(0.0f, 0.0f, 0.0f), &skyview, &projection); 

    // Do fading
    //fade();

    // Right eye?
    if(iod > 0.0) {
        C3D_FrameDrawOn(targetRight); 
        C3D_RenderTargetClear(targetRight, C3D_CLEAR_ALL, 0x00ff00FF, 0); 

        // Uniform setup
        Mtx_PerspStereoTilt(&projection, 20.0f*M_PI/180.0f, 400.0f/240.0f, 0.01f, 6000.0f, iod, 7.0f, false);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projection);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocModelview,  &modelview);

        // Dispatch drawcalls
        drawModel(&modelSignpost, row);
        //drawModel(&modelSign, row);
        skyboxCubeImmediate(&texSky, 1000.0f, vec3(0.0f, 0.0f, 0.0f), &skyview, &projection); 

        // Perform fading
        //fade();
    } 

    // Swap
    C3D_FrameEnd(0);

}

void effectSignScrollerExit() {
    linearFree(textpx);
    freeFBXObject(&modelSignpost);
    freeFBXObject(&modelSign);
    C3D_TexDelete(&texBase);
    C3D_TexDelete(&texSky);
}
