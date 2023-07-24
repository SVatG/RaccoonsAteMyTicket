/**
* the zoomer thing
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

static C3D_Tex texBase, texCube;
static C3D_Tex texSky;
static C3D_TexCube texSkyCube;
static C3D_Tex texFg;
static fbxBasedObject modelSignpost;
static fbxBasedObject modelSign;
static fbxBasedObject modelCube;

static const struct sync_track* syncSky;
static const struct sync_track* sync_text;
static const struct sync_track* sync_camx, *sync_camy, *sync_camz;
static const struct sync_track* sync_camr, *sync_camth, *sync_camph;
static const struct sync_track* sync_crot;//, *sync_zfactor;
static const struct sync_track* sync_zoom;//, *sync_zoompow;
//static const struct sync_track* sync_cubex, *sync_cubey, *sync_cubez, *sync_cubeind;

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

inline static void quadImmediate(vec3_t a, vec3_t b, vec3_t c, vec3_t d, vec3_t N) {
    C3D_ImmSendAttrib(a.x, a.y, a.z, 0.0f); // pos
    C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f); // texcoord
    C3D_ImmSendAttrib(N.x, N.y, N.z, 0.0f); // normal

    C3D_ImmSendAttrib(b.x, b.y, b.z, 0.0f);
    C3D_ImmSendAttrib(1.0f, 0.0f, 0.0f, 0.0f); // texcoord
    C3D_ImmSendAttrib(N.x, N.y, N.z, 0.0f); // normal

    C3D_ImmSendAttrib(c.x, c.y, c.z, 0.0f);
    C3D_ImmSendAttrib(1.0f, 1.0f, 0.0f, 0.0f); // texcoord
    C3D_ImmSendAttrib(N.x, N.y, N.z, 0.0f); // normal

    C3D_ImmSendAttrib(a.x, a.y, a.z, 0.0f);
    C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f); // texcoord
    C3D_ImmSendAttrib(N.x, N.y, N.z, 0.0f); // normal

    C3D_ImmSendAttrib(c.x, c.y, c.z, 0.0f);
    C3D_ImmSendAttrib(1.0f, 1.0f, 0.0f, 0.0f); // texcoord
    C3D_ImmSendAttrib(N.x, N.y, N.z, 0.0f); // normal

    C3D_ImmSendAttrib(d.x, d.y, d.z, 0.0f);
    C3D_ImmSendAttrib(0.0f, 1.0f, 0.0f, 0.0f); // texcoord
    C3D_ImmSendAttrib(N.x, N.y, N.z, 0.0f); // normal
}

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
static void signSetStrings(void) {
    ClearBitmap(&textmap);
    fontRender(SCREEN00, "September 8-10");
    fontRender(SCREEN01, "Nordlicht 2023");
    fontRender(SCREEN10, "Lichthaus");
    fontRender(SCREEN11, "Bremen Germany");
    fontRender(SCREEN20, "September 8-10");
    fontRender(SCREEN21, "Nordlicht 2023");
    fontRender(SCREEN30, "Lichthaus");
    fontRender(SCREEN31, "Bremen Germany");
    fontFlushToGPU();
}

void effectInfinizoomInit() {
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
    loadTexture(&texCube, NULL, "romfs:/tex_3ds_test.bin");
    loadTexture(&texBase, NULL, "romfs:/tex_signpost.bin");
    loadTexture(&texSky, &texSkyCube, "romfs:/sky_cube.bin");
    loadTexture(&texFg, NULL, "romfs:/tex_fg2.bin");
    modelSignpost = loadFBXObject("romfs:/obj_signpost_sign_post.vbo", &texBase, "zoom.frame");
    modelSign = loadFBXObject("romfs:/obj_signpost_sign_signs.vbo", &texScroll, "zoom.frame");
    modelCube = loadFBXObject("romfs:/obj_cube_cube.vbo", &texCube, "zoom.frame");

    sync_text = sync_get_track(rocket, "zoom.text");
    syncSky = sync_get_track(rocket, "zoom.sky");

    sync_camx = sync_get_track(rocket, "zoom.camx" );
    sync_camy = sync_get_track(rocket, "zoom.camy" );
    sync_camz = sync_get_track(rocket, "zoom.camz" );
    sync_camr = sync_get_track(rocket, "zoom.camr" );
    sync_camth= sync_get_track(rocket, "zoom.camth");
    sync_camph= sync_get_track(rocket, "zoom.camph");
    sync_crot = sync_get_track(rocket, "zoom.camrot");
    sync_zoom = sync_get_track(rocket, "zoom.zoom" );
    //sync_zoompow=sync_get_track(rocket, "zoom.zpow");
    //sync_zfactor=sync_get_track(rocket, "zoom.zfactor");

    /*sync_cubex = sync_get_track(rocket, "zoom.cubex");
    sync_cubey = sync_get_track(rocket, "zoom.cubey");
    sync_cubez = sync_get_track(rocket, "zoom.cubez");
    sync_cubeind = sync_get_track(rocket, "zoom.cubeind");*/

    signSetStrings();
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

static struct { int ind; vec3_t pos; } cubes[] = {
    //{ .ind = 100, .pos = {0,0,0} },

    { .ind = 40, .pos = { 1.4f, 1   , -4 } },
    { .ind = 36, .pos = {-0.7f, 0.6f, -2 } },
    { .ind = 30, .pos = { 0.4f, 1.2f, -2 } },
    { .ind = 22, .pos = {-1   , 2.5f, -1 } },
    { .ind = 17, .pos = { 0.5f, 1   , -3 } },
    { .ind = 13, .pos = {-0.5f, 0.7f, -4 } },
    { .ind = 12, .pos = {-1.1f, 1.2f, -3 } },
    { .ind =  7, .pos = { 1   , 0.6f, -3 } },
    { .ind =  3, .pos = {-0.15f,0.6f,  0 } },
};

static void draw_zoom_stuff(float row, C3D_Mtx* baseview, C3D_Mtx* camMat, C3D_Mtx* proj) {
    float zlvlo = sync_get_val(sync_zoom, row), zpow = 2;//sync_get_val(sync_zoompow, row);
    float zlvl=fmodf(zlvlo,1);
    float zfactor= 30;//sync_get_val(sync_zfactor, row);

    float xoff = 0;

    C3D_Mtx m3, m2, m1, modelview;

    /*cubes[0].ind = (int)sync_get_val(sync_cubeind, row);
    cubes[0].pos.x = sync_get_val(sync_cubex, row);
    cubes[0].pos.y = sync_get_val(sync_cubey, row);
    cubes[0].pos.z = sync_get_val(sync_cubez, row);*/

    for (int i = -1; i <= 5; ++i) {
        if (!(i >= 0 && i < 30)) continue;
        int j = i;

        float rzoom = powf(zpow, -(zlvl+j));

        const float CCC = 0.98; float zzz=CCC;
        // I know, there's a closed-form version, but fuck it
        for (int k = 0; k < i; ++k) zzz*=CCC;
        zzz*=zfactor;

        Mtx_Identity(&m3);
        //Mtx_Scale(&m3, rzoom,rzoom,rzoom);
        Mtx_Translate(&m3, xoff * rzoom, -3*rzoom, -zzz * rzoom, false);

        Mtx_Multiply(&m2, &m3, baseview);
        Mtx_Multiply(&modelview, &m2, camMat);
        Mtx_Scale(&modelview, rzoom,rzoom,rzoom);

        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocModelview,  &modelview);

        drawModel(&modelSignpost, row);
        drawModel(&modelSign, row);

        for (size_t k = 0; k < sizeof(cubes)/sizeof(cubes[0]); ++k) {
            __auto_type cu = cubes[k];
            rzoom = powf(zpow, -(zlvlo-cu.ind));
            if (rzoom < 0.01 || rzoom >= 20) continue;

            Mtx_Identity(&m3);
            //Mtx_Scale(&m3, rzoom,rzoom,rzoom);
            Mtx_Translate(&m3, -cu.pos.x*rzoom, cu.pos.y*rzoom, cu.pos.z*rzoom, false);
            //Mtx_Translate(&m3, xoff * rzoom, 0, -zzz * rzoom, false);

            Mtx_Multiply(&m2, &m3, baseview);
            Mtx_Multiply(&modelview, &m2, camMat);
            Mtx_Scale(&modelview, rzoom*0.25,rzoom*0.25,rzoom*0.25);

            C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocModelview,  &modelview);
            drawModel(&modelCube, row);
        }
    }
}

void effectInfinizoomRender(C3D_RenderTarget* targetLeft, C3D_RenderTarget* targetRight, float row, float iod) {
    // Frame starts (TODO pull out?)
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    // Send modelview
    C3D_Mtx baseview, modelview;
    Mtx_Identity(&baseview);
    Mtx_RotateZ(&baseview, M_PI, true);
    Mtx_RotateX(&baseview, -M_PI / 2, true);
    Mtx_RotateY(&baseview, M_PI, true);

    C3D_Mtx camMat;
    {
        float r = sync_get_val(sync_camr , row),
              ph= sync_get_val(sync_camph, row),
              th= sync_get_val(sync_camth, row),
              rot=sync_get_val(sync_crot, row);

        float cinv = 1/cosf(ph);
        if (cosf(ph)*cosf(ph) < 0.001) cinv = 1;

        C3D_FVec clat = FVec3_New(sync_get_val(sync_camx, row),
                                  sync_get_val(sync_camy, row),
                                  sync_get_val(sync_camz, row)),
                 cpos = FVec3_New(clat.x - r*sinf(th)*cosf(ph),
                                  clat.y - r*cosf(th),
                                  clat.z - r*sinf(th)*sinf(ph)),
                 cup = FVec3_New(0, sync_get_val(sync_camy, row) + r * cinv, 0);

        Mtx_LookAt(&camMat, cpos, clat, cup, false);
        Mtx_RotateZ(&camMat, rot, false);
    }
    Mtx_Multiply(&modelview, &baseview, &camMat);

    C3D_Mtx skyview;
    float skyRot = sync_get_val(syncSky, row);
    Mtx_Multiply(&skyview, &baseview, &camMat);
    Mtx_RotateZ(&skyview, skyRot, true);

     // Left eye
    C3D_FrameDrawOn(targetLeft);
    C3D_RenderTargetClear(targetLeft, C3D_CLEAR_ALL, 0x404040FF, 0);

    // Uniform setup
    Mtx_PerspStereoTilt(&projection, 25.0f*M_PI/180.0f, 400.0f/240.0f, 0.01f, 12000.0f, -iod,  7.0f, false);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projection);
    //C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocModelview,  &modelview);

    // Dispatch drawcalls
    draw_zoom_stuff(row, &baseview, &camMat, &projection);
    skyboxCubeImmediate(&texSky, 4000.0f, vec3(0.0f, 0.0f, 0.0f), &skyview, &projection);

    // Do fading
    //fullscreenQuad(texFg, 0.0, 1.0);
    fade();

    // Right eye?
    if(iod > 0.0) {
        C3D_FrameDrawOn(targetRight);
        C3D_RenderTargetClear(targetRight, C3D_CLEAR_ALL, 0x00ff00FF, 0);

        // Uniform setup
        Mtx_PerspStereoTilt(&projection, 25.0f*M_PI/180.0f, 400.0f/240.0f, 0.01f, 12000.0f, iod, 7.0f, false);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projection);
        //C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocModelview,  &modelview);

        // Dispatch drawcalls
        draw_zoom_stuff(row, &baseview, &camMat, &projection);
        skyboxCubeImmediate(&texSky, 4000.0f, vec3(0.0f, 0.0f, 0.0f), &skyview, &projection);

        // Perform fading
        //fullscreenQuad(texFg, 0.0, 1.0);
        fade();
    }

    // Swap
    C3D_FrameEnd(0);

}

void effectInfinizoomExit() {
    linearFree(textpx);
    freeFBXObject(&modelSignpost);
    freeFBXObject(&modelSign);
    C3D_TexDelete(&texCube);
    C3D_TexDelete(&texBase);
    C3D_TexDelete(&texSky);
    C3D_TexDelete(&texFg);
}
