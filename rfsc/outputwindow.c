// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#include "compileconfig.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graphics.h"
#include "outputwindow.h"
#include "settings.h"


double rfswindowdpiscaler = 1.0f;
int _lastseenrfswindoww = -1;
int _lastseenrfswindowh = -1;
int rfswindoww = 0;
int rfswindowh = 0;
int forcedoutputw = -1;
int forcedoutputh = -1;
static int rfsforceno3d = 0;
static int openinfullscreen = 0;
static SDL_Window *rfswindow = NULL;
static SDL_Renderer *rfsrenderer = NULL;
static rfssurf *current_winsrf = NULL;
static int currently3dacc = 0;
static char currentrendername[256] = "";


int outputwindow_Is3DAcc() {
    return currently3dacc;
}


const char *outputwindow_RendererName() {
    if (!rfsrenderer)
        return NULL;
    return currentrendername;
}

void outputwindow_ForceNo3DAcc() {
    rfsforceno3d = 1;
}

SDL_Renderer *outputwindow_GetRenderer() {
    outputwindow_EnsureOpenWindow(0);
    return rfsrenderer;
}

void outputwindow_SetFullscreen() {
    openinfullscreen = 1;
    if (rfswindow) {
        SDL_SetWindowFullscreen(rfswindow, SDL_WINDOW_FULLSCREEN);
    }
}

void outputwindow_EnsureOpenWindow(int forceno3d) {
    int fullscreen = openinfullscreen;
    if (rfsforceno3d) forceno3d = 1;
    if (!rfswindow) {
        #ifdef DEBUG_RENDERER
        fprintf(stderr,
            "rfsc/outputwindow.c: debug: trying window, "
            "forceno3d: %d\n", forceno3d);
        #endif
        SDL_DisplayMode DM = {0};
        SDL_GetDesktopDisplayMode(0, &DM);
        int displayw = DM.w;
        int displayh = DM.h;
        rfswindow = (!forceno3d ? SDL_CreateWindow(
            "Retro FPS Studio",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            (fullscreen ? displayw : default_windowed_width),
            (fullscreen ? displayh : default_windowed_height),
            SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE |
            SDL_WINDOW_OPENGL |
            (fullscreen ? SDL_WINDOW_FULLSCREEN : 0)
        ) : NULL);
        if (!rfswindow) {
            #ifdef DEBUG_RENDERER
            fprintf(stderr,
                "rfsc/outputwindow.c: debug: trying software window, "
                "forceno3d: %d\n", forceno3d);
            #endif
            rfswindow = SDL_CreateWindow(
                "Retro FPS Studio",
                SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                (fullscreen ? displayw : default_windowed_width),
                (fullscreen ? displayh : default_windowed_height),
                SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE |
                (fullscreen ? SDL_WINDOW_FULLSCREEN : 0)
            );
            forceno3d = 1;
        }
        if (!rfswindow) {
            #ifdef DEBUG_RENDERER
            fprintf(stderr,
                "rfsc/outputwindow.c: debug: window failed.\n");
            #endif
            return;
        } else {
            #ifdef DEBUG_RENDERER
            fprintf(stderr,
                "rfsc/outputwindow.c: debug: window open.\n");
            #endif
        }
    }
    if (rfswindow && !rfsrenderer) {
        if (!forceno3d) currently3dacc = 1;
        rfsrenderer = (!forceno3d ? SDL_CreateRenderer(
            rfswindow, -1, SDL_RENDERER_ACCELERATED |
            SDL_RENDERER_PRESENTVSYNC
        ) : NULL);
        if (!rfsrenderer) {
            #ifdef DEBUG_RENDERER
            fprintf(stderr,
                "rfsc/outputwindow.c: debug: creating "
                "software renderer.\n");
            #endif
            currently3dacc = 0;
            rfsrenderer = SDL_CreateRenderer(
                rfswindow, -1, SDL_RENDERER_SOFTWARE
            );
            global_smooth_textures = 0;
            SDL_SetHintWithPriority(
                "SDL_RENDER_SCALE_QUALITY", "0", SDL_HINT_OVERRIDE
            );
        } else {
            #ifdef DEBUG_RENDERER
            fprintf(stderr,
                "rfsc/outputwindow.c: debug: created "
                "3d acc renderer.\n");
            #endif
        }
        if (rfsrenderer) {
            SDL_RendererInfo info = {0};
            SDL_GetRendererInfo(rfsrenderer, &info);
            int copy = strlen(info.name) + 1;
            if (copy > (int)sizeof(currentrendername))
                copy = sizeof(currentrendername);
            memcpy(currentrendername, info.name, copy);
            currentrendername[sizeof(currentrendername) - 1] = '\0';
            #ifdef DEBUG_RENDERER
            fprintf(stderr,
                "rfsc/outputwindow.c: debug: new renderer: \"%s\"\n",
                info.name);
            #endif
        }
    }
    outputwindow_UpdateViewport(0);
}

SDL_Window *outputwindow_GetWindow() {
    outputwindow_EnsureOpenWindow(0);
    return rfswindow;
}

void outputwindow_UpdateViewport(int forceresize) {
    if (!rfsrenderer)
        return;
    if (!rfswindow) {
        #ifdef DEBUG_RENDERER
        fprintf(stderr,
            "rfsc/outputwindow.c: debug: "
            "outputwindow_UpdateViewport() "
            "-> missing window\n");
        #endif
        return;
    }
    int ww = -1;
    int wh = -1;
    SDL_GetWindowSize(
        rfswindow, &ww, &wh
    );
    if (ww < 0 || wh < 0) {
        #ifdef DEBUG_RENDERER
        fprintf(stderr,
            "rfsc/outputwindow.c: debug: "
            "outputwindow_UpdateViewport() "
            "-> invalid window sizes\n");
        #endif
        return;
    }
    if (_lastseenrfswindoww == ww &&
            _lastseenrfswindowh == wh && !forceresize)
        return;
    _lastseenrfswindoww = ww;
    _lastseenrfswindowh = wh;
    rfssurf_Free(current_winsrf);
    current_winsrf = NULL;

    int w = 0;
    int h = 0;
    if (SDL_GetRendererOutputSize(rfsrenderer, &w, &h) >= 0) {
        rfswindoww = w;
        rfswindowh = h;
        rfswindowdpiscaler = ((double)rfswindoww / (double)ww);
    }
    #ifdef DEBUG_RENDERER
    fprintf(stderr,
        "rfsc/outputwindow.c: debug: "
        "outputwindow_UpdateViewport() "
        "-> resizing to %d,%d with dpi scale=%f\n",
        rfswindoww, rfswindowh, rfswindowdpiscaler);
    #endif
}

rfssurf *outputwindow_GetSurface() {
    if (!rfswindow)
        return NULL;
    if (!current_winsrf) {
        current_winsrf = rfssurf_Create(
            (forcedoutputw >= 0 ? forcedoutputw :
             rfswindoww),
            (forcedoutputh >= 0 ? forcedoutputh :
             rfswindowh), 0
        );
        #ifdef DEBUG_RENDERER
        fprintf(stderr,
            "rfsc/outputwindow.c: debug: "
            "outputwindow_GetSurface() -> recreated\n");
        #endif
    }
    return current_winsrf;
}

void outputwindow_ForceScaledOutputSize(
        int w, int h) {
    if (w < 0 && h < 0) {
        if (forcedoutputw >= 0 && forcedoutputh >= 0) {
            rfssurf_Free(current_winsrf);
            current_winsrf = NULL;
        }
        forcedoutputw = -1;
        forcedoutputh = -1;
        return;
    }
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;
    forcedoutputw = w;
    forcedoutputh = h;
    if (current_winsrf && current_winsrf->w != w &&
            current_winsrf->h != h) {
        rfssurf_Free(current_winsrf);
        current_winsrf = NULL;
    }
}

int outputwindow_PresentSurface() {
    if (!current_winsrf || !rfswindow ||
            !rfsrenderer)
        return 0;
    SDL_Texture *srf = rfssurf_AsTex(
        rfsrenderer, current_winsrf,
        current_winsrf->hasalpha
    );
    if (!srf)
        return 0;
    SDL_Rect dest = {0};
    dest.w = rfswindoww;
    dest.h = rfswindowh;
    SDL_RenderCopy(rfsrenderer, srf, NULL, &dest);
    SDL_DestroyTexture(srf);
    SDL_RenderPresent(rfsrenderer);
    return 1;
}

void outputwindow_CloseWindow() {
    if (rfsrenderer) {
        SDL_DestroyRenderer(rfsrenderer);
        rfsrenderer = NULL;
    }
    rfssurf_Free(current_winsrf);
    current_winsrf = NULL;
    if (rfswindow) {
        SDL_DestroyWindow(rfswindow);
        rfswindow = NULL;
    }
}

