// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_OUTPUTWINDOW_H_
#define RFS2_OUTPUTWINDOW_H_

#include "compileconfig.h"

#if defined(HAVE_SDL)
#include <SDL2/SDL.h>
#endif

#include "rfssurf.h"


extern double rfswindowdpiscaler;
extern int rfswindoww;
extern int rfswindowh;

void outputwindow_EnsureOpenWindow(int forceno3d);

#if defined(HAVE_SDL)
SDL_Window *outputwindow_GetWindow();
#endif

void outputwindow_SetFullscreen();

void outputwindow_ToggleFullscreen();

int outputwindow_GetIsFullscreen();

void outputwindow_UpdateViewport(int forceresize);

void outputwindow_CloseWindow();

void outputwindow_ForceNo3DAcc();

rfssurf *outputwindow_GetSurface();

int outputwindow_PresentSurface();

void outputwindow_ForceScaledOutputSize(
    int w, int h
);

int outputwindow_Is3DAcc();

const char *outputwindow_RendererName();

#endif  // RFS2_OUTPUTWINDOW_H_
