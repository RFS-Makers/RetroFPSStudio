// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_RFSSURF_H_
#define RFS2_RFSSURF_H_

#include "compileconfig.h"

#include <stdint.h>
#if defined(HAVE_SDL)
#include <SDL2/SDL.h>
#endif


typedef struct rfssurf {
    int w,h;
    uint8_t *pixels;
    uint8_t hasalpha;
    #if defined(HAVE_SDL)
    SDL_Surface *sdlsrf;
    uint8_t sdlsrf_hasalpha;
    #endif
} rfssurf;


rfssurf *rfssurf_DuplicateNoAlpha(rfssurf *surf);

void rfssurf_Clear(rfssurf *surf);

rfssurf *rfssurf_Duplicate(rfssurf *surf);

HOTSPOT void rfssurf_Rect(rfssurf *target,
    int x, int y, int w, int h,
    double r, double g, double b, double a
);

HOTSPOT void rfssurf_BlitSimple(rfssurf *target, rfssurf *source,
    int tgx, int tgy, int clipx, int clipy, int clipw, int cliph
);

HOTSPOT void rfssurf_BlitColor(rfssurf *target, rfssurf *source,
    int tgx, int tgy, int clipx, int clipy, int clipw, int cliph,
    double r, double g, double b, double a
);

HOTSPOT void rfssurf_BlitScaled(rfssurf *target, rfssurf *source,
    int tgx, int tgy, int clipx, int clipy, int clipw, int cliph,
    double scalex, double scaley,
    double r, double g, double b, double a
);

rfssurf *rfssurf_Create(int w, int h, int alpha);

void rfssurf_Free(rfssurf *surf);

#if defined(HAVE_SDL)
#include <SDL2/SDL.h>
HOTSPOT SDL_Surface *rfssurf_AsSrf(rfssurf *srf, int withalpha);
SDL_Texture *rfssurf_AsTex(
    SDL_Renderer *r, rfssurf *srf, int withalpha
);
#endif

#endif  // RFS2_RFSSURF_H_
