// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
#include <math.h>
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
static SDL_Texture *current_wintex = NULL;
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


void outputwindow_ToggleFullscreen() {
    int is_fullscreen = outputwindow_GetIsFullscreen();
    if (!is_fullscreen) {
        openinfullscreen = 1;
        if (rfswindow) {
            SDL_SetWindowFullscreen(rfswindow, SDL_WINDOW_FULLSCREEN);
        }
    } else {
        openinfullscreen = 0;
        if (rfswindow) {
            SDL_SetWindowFullscreen(rfswindow, 0);
        }
    }
}


int outputwindow_GetIsFullscreen() {
    return (openinfullscreen != 0);
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
            rfs2tex *tex = graphics_LoadTex("rfslua/res/ui/icon");
            SDL_Surface *srf = (
                tex ? rfssurf_AsSrf(tex->srfalpha, 1) : NULL
            );
            if (!tex || !srf) {
                fprintf(stderr, "rfsc/outputwindow.c: debug: "
                    "failed to access rfslua/res/ui/icon\n");
            } else {
                SDL_SetWindowIcon(rfswindow, srf);
            }
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
    SDL_DestroyTexture(current_wintex);
    current_wintex = NULL;

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
            SDL_DestroyTexture(current_wintex);
            current_wintex = NULL;
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
        SDL_DestroyTexture(current_wintex);
        current_wintex = NULL;
    }
}


int outputwindow_PresentSurface() {
    if (!current_winsrf || !rfswindow ||
            !rfsrenderer)
        return 0;
    current_wintex = rfssurf_AsTex_Update(
        rfsrenderer, current_winsrf,
        current_winsrf->hasalpha,
        current_wintex
    );
    if (!current_wintex)
        return 0;
    double aspect_ratios_max_mismatch = fabs(
        (100.0/100.0) - (100.0/101.0)
    );
    double windowaspect = (
        (double)rfswindoww / (double)rfswindowh
    );
    double forcerenderaspect = (
        (forcedoutputw >= 0 && forcedoutputh >= 0) ?
        ((double)forcedoutputw / (double)forcedoutputh) :
        windowaspect
    );
    if (forcedoutputw >= 0 && forcedoutputh >= 0 &&
            fabs(windowaspect - forcerenderaspect) >
                aspect_ratios_max_mismatch) {
        SDL_SetRenderDrawColor(rfsrenderer, 0, 0, 0, 255);
        SDL_RenderFillRect(rfsrenderer, NULL);
        SDL_SetRenderDrawColor(rfsrenderer, 255, 255, 255, 255);
        SDL_Rect dest = {0};
        if (windowaspect > forcerenderaspect) {
            dest.h = rfswindowh;
            dest.w = forcedoutputw * (
                (double)rfswindowh / (double)forcedoutputh
            );
            dest.x = (rfswindoww - dest.w) / 2;
        } else {
            dest.w = rfswindoww;
            dest.h = forcedoutputh * (
                (double)rfswindoww / (double)forcedoutputw
            );
            dest.y = (rfswindowh - dest.h) / 2;
        }
        SDL_RenderCopy(rfsrenderer, current_wintex, NULL, &dest);
        SDL_RenderPresent(rfsrenderer);
        return 1;
    } else {
        SDL_Rect dest = {0};
        dest.w = rfswindoww;
        dest.h = rfswindowh;
        SDL_RenderCopy(rfsrenderer, current_wintex, NULL, &dest);
        SDL_RenderPresent(rfsrenderer);
        return 1;
    }
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

