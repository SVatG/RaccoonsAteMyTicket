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

C3D_Tex texFg;

void effectSlideInit() {
    loadTexture(&texFg, NULL, "romfs:/tex_fg6.bin");
}

void effectSlideRender(C3D_RenderTarget* targetLeft, C3D_RenderTarget* targetRight, float row, float iod) {
    // Frame starts (TODO pull out?)
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

     // Left eye
    C3D_FrameDrawOn(targetLeft);
    C3D_RenderTargetClear(targetLeft, C3D_CLEAR_ALL, 0x404040FF, 0);

    // Do fading
    fullscreenQuad(texFg, 0.0, 1.0);    
    fade();

    // Right eye?
    if(iod > 0.0) {
        C3D_FrameDrawOn(targetRight);
        C3D_RenderTargetClear(targetRight, C3D_CLEAR_ALL, 0x00ff00FF, 0);

        // Perform fading
        fullscreenQuad(texFg, 0.0, 1.0);
        fade();
    }

    // Swap
    C3D_FrameEnd(0);

}

void effectSlideExit() {
    C3D_TexDelete(&texFg);
}
