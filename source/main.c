/**
 * Nordlicht 2022 - Hotel Nordlicht
 * 
 * SVatG 2022 ~ halcy / Saga Musix / dotUser
 **/

#include "Tools.h"
#include "Rocket/sync.h"
#include "soundtrack.h"

#define CLEAR_COLOR 0x555555FF

C3D_Tex fade_tex;
static Pixel* fadePixels;
static Bitmap fadeBitmap;
float fadeVal;

#define min(a, b) (((a)<(b))?(a):(b))
#define max(a, b) (((a)>(b))?(a):(b))

// Rocket settings
#define ROCKET_HOST CONF_ROCKET_IP
#define SOC_ALIGN 0x1000
#define SOC_BUFFERSIZE 0x100000

static uint32_t *SOC_buffer = NULL;

int connect_rocket() {
#ifndef SYNC_PLAYER
    while(sync_tcp_connect(rocket, ROCKET_HOST, SYNC_DEFAULT_PORT)) {
        printf("Didn't work, again...\n");
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START) {
            return(1);
        }
        svcSleepThread(1000*1000*1000);
    }
#endif
    return(0);
}

#include <vshader_normalmapping_shbin.h>
#include <vshader_skybox_shbin.h>
#include <vshader_shbin.h>
#include <vshader_bones_shbin.h>

DVLB_s* vshader_dvlb;
DVLB_s* vshader_normalmapping_dvlb;
DVLB_s* vshader_bones_dvlb;
DVLB_s* vshader_skybox_dvlb;
shaderProgram_s shaderProgram;
shaderProgram_s shaderProgramNormalMapping;
shaderProgram_s shaderProgramBones;
shaderProgram_s shaderProgramSkybox;

// Effect sync structs
typedef void (*init_fun_t)();
typedef void (*render_fun_t)(C3D_RenderTarget* targetLeft, C3D_RenderTarget* targetRight, float iod, float time);
typedef void (*exit_fun_t)();

typedef struct effect {
    init_fun_t init;
    render_fun_t render;
    exit_fun_t exit;
} effect;

// Externs for effects
extern void effectIntroInit();
extern void effectIntroRender(C3D_RenderTarget* targetLeft, C3D_RenderTarget* targetRight, float row, float iod);
extern void effectIntroExit();

extern void effectBillboardsInit();
extern void effectBillboardsRender(C3D_RenderTarget* targetLeft, C3D_RenderTarget* targetRight, float row, float iod);
extern void effectBillboardsExit();

extern void effectSignScrollerInit();
extern void effectSignScrollerRender(C3D_RenderTarget* targetLeft, C3D_RenderTarget* targetRight, float row, float iod);
extern void effectSignScrollerExit();

extern void effectLichthausInit();
extern void effectLichthausRender(C3D_RenderTarget* targetLeft, C3D_RenderTarget* targetRight, float row, float iod);
extern void effectLichthausExit();

extern void effectInfinizoomInit();
extern void effectInfinizoomRender(C3D_RenderTarget* targetLeft, C3D_RenderTarget* targetRight, float row, float iod);
extern void effectInfinizoomExit();

int main() {
    bool DUMPFRAMES = false;
    bool DUMPFRAMES_3D = false;
    float DUMPFRAMES_3D_SEP = 0.4;

    // Set up effect list. Sequencing is done in Rocket
    #define EFFECT_MAX 5
    effect effect_list[EFFECT_MAX];

    effect_list[0].init = effectIntroInit;
    effect_list[0].render = effectIntroRender;
    effect_list[0].exit = effectIntroExit;

    effect_list[1].init = effectBillboardsInit;
    effect_list[1].render = effectBillboardsRender;
    effect_list[1].exit = effectBillboardsExit;

    effect_list[2].init = effectSignScrollerInit;
    effect_list[2].render = effectSignScrollerRender;
    effect_list[2].exit = effectSignScrollerExit;

    effect_list[3].init = effectLichthausInit;
    effect_list[3].render = effectLichthausRender;
    effect_list[3].exit = effectLichthausExit;

    effect_list[4].init = effectInfinizoomInit;
    effect_list[4].render = effectInfinizoomRender;
    effect_list[4].exit = effectInfinizoomExit;

    // Initialize graphics
    gfxInit(GSP_RGBA8_OES, GSP_BGR8_OES, false);
    gfxSet3D(true);
    consoleInit(GFX_BOTTOM, NULL);
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

    // Initialize the render targets
    C3D_RenderTarget* targetLeft = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTarget* targetRight = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(targetLeft, GFX_TOP, GFX_LEFT,  DISPLAY_TRANSFER_FLAGS);
    C3D_RenderTargetSetOutput(targetRight, GFX_TOP, GFX_RIGHT, DISPLAY_TRANSFER_FLAGS);

    romfsInit();

    int r = audio_init_preload();
    if (r != 0) return r;

    printf("Demobeginn imminent.\n");

    // Rocket startup
#ifndef SYNC_PLAYER
    printf("Now socketing...\n");
    SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    socInit(SOC_buffer, SOC_BUFFERSIZE);

    rocket = sync_create_device("sdmc:/sync");
#else
    rocket = sync_create_device("romfs:/sync");
#endif
    if (connect_rocket()) {
        return(0);
    }

    // Load shaders
    vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
    shaderProgramInit(&shaderProgram);
    shaderProgramSetVsh(&shaderProgram, &vshader_dvlb->DVLE[0]);

    vshader_skybox_dvlb = DVLB_ParseFile((u32*)vshader_skybox_shbin, vshader_skybox_shbin_size);
    shaderProgramInit(&shaderProgramSkybox);
    shaderProgramSetVsh(&shaderProgramSkybox, &vshader_skybox_dvlb->DVLE[0]);

    vshader_bones_dvlb = DVLB_ParseFile((u32*)vshader_bones_shbin, vshader_bones_shbin_size);
    shaderProgramInit(&shaderProgramBones);
    shaderProgramSetVsh(&shaderProgramBones, &vshader_bones_dvlb->DVLE[0]);

    vshader_normalmapping_dvlb = DVLB_ParseFile((u32*)vshader_normalmapping_shbin, vshader_normalmapping_shbin_size);
    shaderProgramInit(&shaderProgramNormalMapping);
    shaderProgramSetVsh(&shaderProgramNormalMapping, &vshader_normalmapping_dvlb->DVLE[0]);

    audio_init_mixer();

    // Init bitmap used for fading
    fadePixels = (Pixel*)linearAlloc(64 * 64 * sizeof(Pixel));
    InitialiseBitmap(&fadeBitmap, 64, 64, BytesPerRowForWidth(64), fadePixels);
    C3D_TexInit(&fade_tex, 64, 64, GPU_RGBA8);

    // Get first row value
    double row = 0.0;

#ifndef SYNC_PLAYER
    if(sync_update(rocket, (int)floor(row), &rocket_callbakcks, (void *)0)) {
        printf("Lost connection, retrying.\n");
        if(connect_rocket()) {
            return(0);
        }
    }
#endif

    const struct sync_track* sync_fade = sync_get_track(rocket, "global.fade");
    const struct sync_track* sync_effect = sync_get_track(rocket, "global.effect");;    
    const struct sync_track* sync_img = sync_get_track(rocket, "global.image");

    // Start up first effect
    int current_effect = (int)sync_get_val(sync_effect, row + 0.01);
    effect_list[current_effect].init();

    audio_init_play();
    row = audio_get_row();

    int fc = 0;
    while (aptMainLoop()) {
        if (!DUMPFRAMES) {
            row = audio_get_row();
        }
        else {
            printf("Frame dump %d\n", fc);
            row = ((double)fc * (32000.0 / 30.0)) / (double)SAMPLES_PER_ROW;
        }

#ifndef SYNC_PLAYER
        if (sync_update(rocket, (int)floor(row), &rocket_callbakcks, (void *)0)) {
            printf("Lost connection, retrying.\n");
            if (connect_rocket()) {
                return(0);
            }
        }
#endif

        // Effect switcher
        int new_effect = (int)sync_get_val(sync_effect, row);
        if(new_effect < 0) {
            new_effect = 0;
        }
        if(new_effect >= EFFECT_MAX) {
            new_effect = EFFECT_MAX - 1;
        }
        if(new_effect != -1 && new_effect != current_effect) {
            printf("effect switch %d -> %d\n", current_effect, new_effect);
            effect_list[current_effect].exit();
            current_effect = new_effect;
            effect_list[current_effect].init();
        }


        // Fading update
        fadeVal = sync_get_val(sync_fade, row);
        FillBitmap(&fadeBitmap, RGBAf(0.0, 0.0, 0.0, fadeVal));
        GSPGPU_FlushDataCache(fadePixels, 64 * 64 * sizeof(Pixel));
        GX_DisplayTransfer((u32*)fadePixels, GX_BUFFER_DIM(64, 64), (u32*)fade_tex.data, GX_BUFFER_DIM(64, 64), TEXTURE_TRANSFER_FLAGS);
        gspWaitForPPF();

        // Respond to user input
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START) {
            break; // break in order to return to hbmenu
        }
        float slider = osGet3DSliderState();
        if(DUMPFRAMES_3D) {
            slider = DUMPFRAMES_3D_SEP;
        }

        float iod = slider / 3.0;

        // Draw
        effect_list[current_effect].render(targetLeft, targetRight, row, iod);

        // Frame dumper code
        if(DUMPFRAMES) {
            gspWaitForP3D();
            gspWaitForPPF();

            u8* fbl = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);

            char fname[255];
            sprintf(fname, "3ds/dump%d/fb_left_%08d.raw", (fc/1000)+1, fc);
            FILE* file = fopen(fname,"w");
            fwrite(fbl, sizeof(int32_t), SCREEN_HEIGHT * SCREEN_WIDTH, file);
            fflush(file);
            fclose(file);

            if(DUMPFRAMES_3D) {
                u8* fbr = gfxGetFramebuffer(GFX_TOP, GFX_RIGHT, NULL, NULL);
                sprintf(fname, "3ds/dump%d/fb_right_%08d.raw", (fc/1000)+1, fc);
                file = fopen(fname,"w");
                fwrite(fbr, sizeof(int32_t), SCREEN_HEIGHT * SCREEN_WIDTH, file);
                fflush(file);
                fclose(file);
            }
        }

        fc++;
        audio_update();
    }

    printf("Demo over, go home.\n");

    linearFree(fadePixels);

    audio_deinit();

    // Deinitialize graphics
    socExit();
    C3D_Fini();
    gfxExit();

    return 0;
}
