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
static C3D_Tex texFg;
static fbxBasedObject modelSignpost;
static fbxBasedObject modelSign;
static fbxBasedObject modelFloor;
static fbxBasedObject camProxy;

static const struct sync_track* syncText;
static const struct sync_track* syncSky;

extern Font London40Regular;

static Bitmap textmap;
static Pixel* textpx;
static C3D_Tex texScroll;

#define TEXTSIZE 512

#define SCREENij(i,j) 16, (16+48*(j)+124*(i))
#define SCREEN00 SCREENij(0,0)
#define SCREEN10 SCREENij(1,0)
#define SCREEN20 SCREENij(2,0)
#define SCREEN30 SCREENij(3,0)
#define SCREEN01 SCREENij(0,1)
#define SCREEN11 SCREENij(1,1)
#define SCREEN21 SCREENij(2,1)
#define SCREEN31 SCREENij(3,1)

// screen i: (16, 20+124*i); (16, 20+48+124*i)
static void fontRender(int x, int y, const char* str) {
    Pixel col = 0xffff00ffu; // yellow, hopefully
    CompositeSimpleString(&textmap, &London40Regular, x, y, col, SourceOverCompositionMode, str);
}
static void fontFlushToGPU(void) {
    GSPGPU_FlushDataCache(textmap.pixels, TEXTSIZE * TEXTSIZE * sizeof(Pixel));
    GX_DisplayTransfer((u32*)textmap.pixels, GX_BUFFER_DIM(TEXTSIZE, TEXTSIZE),
            (u32*)texScroll.data, GX_BUFFER_DIM(TEXTSIZE, TEXTSIZE), TEXTURE_TRANSFER_FLAGS);
    gspWaitForPPF();
}
static void signSetStrings(const char* a, const char* b) {
    ClearBitmap(&textmap);
    fontRender(SCREEN00, "Track 10  8:10");
    fontRender(SCREEN01, "Nordlicht 2023");
    fontRender(SCREEN10, "train stops at:");
    fontRender(SCREEN11, a);
    fontRender(SCREEN20, "Track 10  8:10");
    fontRender(SCREEN21, "Nordlicht 2023");
    fontRender(SCREEN30, "train stops at:");
    fontRender(SCREEN31, b);
    fontFlushToGPU();
}

static const char* COMPOS[] = {
    "oldschool gfx,",
    "newschool gfx,",
    "ascii/ansi,   ",
    "executable,   ",
    "streaming and ",
    "tracked music,",
    "photo, 512b,  ",
    "gravedigger,  ",
    "old&newschool ",
    "demo, wild,   ",
    "fun compo,    ",
    "nordlicht bbq,",
    "breakfast,    ",
    "cider,        ",
    "party,        ",
    "bigscreen,    ",
    "september,    ",
    "2023,         ",
    "lichthaus,    ",
    "bremen,       ",
    "trans rights! ",
};

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
    size_t w = TEXTSIZE, h = TEXTSIZE;
    size_t bpr = BytesPerRowForWidth(w);
    textpx = (Pixel*)linearAlloc(bpr * h);
    InitializeBitmap(&textmap, w, h, bpr, textpx);
    C3D_TexInit(&texScroll, w, h, GPU_RGBA8);

    // render font?!
    ClearBitmap(&textmap);
    /*fontRender(0, 0, "hello world");
    fontFlushToGPU();*/

    // Load a model
    loadTexture(&texBase, NULL, "romfs:/tex_signpost.bin");
    loadTexture(&texSky, &texSkyCube, "romfs:/sky_cube.bin");
    loadTexture(&texFg, NULL, "romfs:/tex_fg2.bin");
    modelSignpost = loadFBXObject("romfs:/obj_signpost_sign_post.vbo", &texBase, "signscroll.frame");
    modelSign = loadFBXObject("romfs:/obj_signpost_sign_signs.vbo", &texScroll, "signscroll.frame");
    modelFloor = loadFBXObject("romfs:/obj_signpost_floor.vbo", &texBase, "signscroll.frame");
    camProxy = loadFBXObject("romfs:/obj_signpost_cam_proxy.vbo", NULL, "signscroll.frame");
    syncText = sync_get_track(rocket, "signscroll.speed");
    syncSky = sync_get_track(rocket, "signscroll.sky");
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
    int index = (int)sync_get_val(syncText, row);
    index %= sizeof(COMPOS)/sizeof(*COMPOS);
    const char *compo = COMPOS[index];
    signSetStrings(compo, compo);

    // Frame starts (TODO pull out?)
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    // Send modelview
    C3D_Mtx baseview;
    Mtx_Identity(&baseview);
    Mtx_RotateZ(&baseview, M_PI, true);
    Mtx_RotateX(&baseview, -M_PI / 2, true);
    Mtx_RotateY(&baseview, M_PI, true);
    
    C3D_Mtx camMat;
    getBoneMat(&camProxy, row, &camMat, 0);
    Mtx_Inverse(&camMat);

    C3D_Mtx modelview;
    Mtx_Multiply(&modelview, &baseview, &camMat);

    C3D_Mtx skyview;
    float skyRot = sync_get_val(syncSky, row);
    Mtx_Multiply(&skyview, &baseview, &camMat);
    Mtx_RotateZ(&skyview, skyRot, true);

     // Left eye
    C3D_FrameDrawOn(targetLeft);
    C3D_RenderTargetClear(targetLeft, C3D_CLEAR_ALL, 0x404040FF, 0);

    // Uniform setup
    Mtx_PerspStereoTilt(&projection, 25.0f*M_PI/180.0f, 400.0f/240.0f, 0.01f, 6010.0f, -iod,  7.0f, false);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projection);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocModelview,  &modelview);

    // Dispatch drawcalls
    drawModel(&modelSignpost, row);
    drawModel(&modelSign, row);
    drawModel(&modelFloor, row);
    skyboxCubeImmediate(&texSky, 3000.0f, vec3(0.0f, 0.0f, 0.0f), &skyview, &projection);

    // Do fading
    fullscreenQuad(texFg, 0.0, 1.0);    
    fade();

    // Right eye?
    if(iod > 0.0) {
        C3D_FrameDrawOn(targetRight);
        C3D_RenderTargetClear(targetRight, C3D_CLEAR_ALL, 0x00ff00FF, 0);

        // Uniform setup
        Mtx_PerspStereoTilt(&projection, 25.0f*M_PI/180.0f, 400.0f/240.0f, 0.01f, 6010.0f, iod, 7.0f, false);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projection);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocModelview,  &modelview);

        // Dispatch drawcalls
        drawModel(&modelSignpost, row);
        drawModel(&modelSign, row);
        drawModel(&modelFloor, row);
        skyboxCubeImmediate(&texSky, 3000.0f, vec3(0.0f, 0.0f, 0.0f), &skyview, &projection);

        // Perform fading
        fullscreenQuad(texFg, 0.0, 1.0);
        fade();
    }

    // Swap
    C3D_FrameEnd(0);

}

void effectSignScrollerExit() {
    linearFree(textpx);
    freeFBXObject(&modelSignpost);
    freeFBXObject(&modelSign);
    freeFBXObject(&modelFloor);
    freeFBXObject(&camProxy);
    C3D_TexDelete(&texBase);
    C3D_TexDelete(&texSky);
    C3D_TexDelete(&texFg);
    C3D_TexDelete(&texScroll);
}
