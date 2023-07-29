/**
* "invites u 2 nordlicht 2023"
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#include "Tools.h"
 
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

static C3D_Tex texIntro;
static C3D_Tex texSky;
static C3D_TexCube texSkyCube;
static C3D_Tex texFg;
static fbxBasedObject modelTextA;
static fbxBasedObject modelTextB;
static fbxBasedObject modelTextC;
static fbxBasedObject modelCube;
static fbxBasedObject modelTrain;
static fbxBasedObject modelEnv;
static fbxBasedObject modelTrees;
static fbxBasedObject camProxy;

// rot + zoom + skybox
static const struct sync_track* syncZoom;
static const struct sync_track* syncRot;
static const struct sync_track* syncSky;

void effectIntroInit() {
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

    // Load a model
    loadTexture(&texIntro, NULL, "romfs:/tex_intro.bin");
    loadTexture(&texSky, &texSkyCube, "romfs:/sky_cube.bin");
    loadTexture(&texFg, NULL, "romfs:/tex_fg1.bin");
    modelTextA = loadFBXObject("romfs:/obj_introtext_svatg.vbo", &texIntro, "intro.text");
    modelTextB = loadFBXObject("romfs:/obj_introtext_inviteyouto.vbo", &texIntro, "intro.text");
    modelTextC = loadFBXObject("romfs:/obj_introtext_nordlicht2023.vbo", &texIntro, "intro.text");
    camProxy = loadFBXObject("romfs:/obj_introtext_cam_proxy.vbo", &texIntro, "intro.cam");
    modelCube = loadFBXObject("romfs:/obj_introtext_env.vbo", &texIntro, "intro.cube");
    modelTrain = loadFBXObject("romfs:/obj_introtext_train.vbo", &texIntro, "intro.train");
    modelEnv = loadFBXObject("romfs:/obj_introtext_floor_real.vbo", &texIntro, "intro.env");
    modelTrees = loadFBXObject("romfs:/obj_introtext_trees.vbo", &texIntro, "intro.env");

    // More sync
    syncZoom = sync_get_track(rocket, "intro.zoom");
    syncRot = sync_get_track(rocket, "intro.rot");
    syncSky = sync_get_track(rocket, "intro.sky");
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

void effectIntroRender(C3D_RenderTarget* targetLeft, C3D_RenderTarget* targetRight, float row, float iod) {
    // Frame starts (TODO pull out?)   
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    
    // Get some sync vals
    float syncSkyVal = sync_get_val(syncSky, row);
    float syncZoomVal = sync_get_val(syncZoom, row) + 1.0;
    float syncRotVal = sync_get_val(syncRot, row);

    // Send modelview 
    C3D_Mtx baseview;
    Mtx_Identity(&baseview);
    Mtx_RotateZ(&baseview, M_PI, true);
    Mtx_RotateX(&baseview, -M_PI / 2, true);
    Mtx_RotateY(&baseview, M_PI, true);
    
    C3D_Mtx camMat;
    getBoneMat(&camProxy, row, &camMat, 3);
    Mtx_Inverse(&camMat);

    C3D_Mtx modelview;
    Mtx_Multiply(&modelview, &baseview, &camMat);
    Mtx_RotateY(&modelview, M_PI * syncRotVal, true);
    Mtx_Scale(&modelview, syncZoomVal, syncZoomVal, syncZoomVal);

    C3D_Mtx skyview;
    Mtx_Multiply(&skyview, &baseview, &camMat);
    Mtx_RotateZ(&skyview, syncSkyVal * M_PI, true);

    //Mtx_Scale(&modelview, 0.01, 0.01, 0.01);

     // Left eye 
    C3D_FrameDrawOn(targetLeft);  
    C3D_RenderTargetClear(targetLeft, C3D_CLEAR_ALL, 0xff0000FF, 0);
    
    // Uniform setup
    Mtx_PerspStereoTilt(&projection, 20.0f*M_PI/180.0f, 400.0f/240.0f, 0.01f, 6000.0f, -iod,  7.0f, false);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projection);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocModelview,  &modelview);

    // Dispatch drawcalls
    drawModel(&modelTextA, row);
    drawModel(&modelTextB, row);
    drawModel(&modelTextC, row);
    drawModel(&modelCube, row);
    drawModel(&modelTrain, row);
    drawModel(&modelEnv, row);
    drawModel(&modelTrees, row);
    skyboxCubeImmediate(&texSky, 1000.0f, vec3(0.0f, 0.0f, 0.0f), &skyview, &projection); 

    // Do fading and fg
    fullscreenQuad(texFg, 0.0, 1.0);
    fade();

    // Right eye?
    if(iod > 0.0) {
        C3D_FrameDrawOn(targetRight); 
        C3D_RenderTargetClear(targetRight, C3D_CLEAR_ALL, 0x00ff00FF, 0); 
        
        // Uniform setup
        Mtx_PerspStereoTilt(&projection, 20.0f*M_PI/180.0f, 400.0f/240.0f, 0.01f, 6000.0f, iod, 7.0f, false);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projection);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocModelview,  &modelview);

        // Dispatch drawcalls
        drawModel(&modelTextA, row);
        drawModel(&modelTextB, row);
        drawModel(&modelTextC, row);
        drawModel(&modelCube, row);
        drawModel(&modelTrain, row);
        drawModel(&modelEnv, row);
        drawModel(&modelTrees, row);        
        skyboxCubeImmediate(&texSky, 1000.0f, vec3(0.0f, 0.0f, 0.0f), &skyview, &projection); 

        // Do fading and fg
        fullscreenQuad(texFg, 0.0, 1.0);
        fade();
    } 

    // Swap
    C3D_FrameEnd(0);

}

void effectIntroExit() {
    freeFBXObject(&modelTextA); 
    freeFBXObject(&modelTextB);
    freeFBXObject(&modelTextC);
    freeFBXObject(&modelCube);
    freeFBXObject(&modelTrain);
    freeFBXObject(&camProxy);
    C3D_TexDelete(&texIntro);
    C3D_TexDelete(&texSky);
}
