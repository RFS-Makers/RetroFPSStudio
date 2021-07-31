// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#ifndef RFS2_GRAPHICS_H_
#define RFS2_GRAPHICS_H_

#include "compileconfig.h"

#include <stdint.h>


typedef struct rfssurf rfssurf;

typedef struct rfs2tex {
    int32_t id;
    char *diskpath;
    uint8_t isrendertarget, iswritecopy;
    int w,h;
    rfssurf *srf, *srfalpha, *_internal_srfsideways;
} rfs2tex;


char *graphics_FixTexturePath(const char *path);

rfssurf *graphics_GetTexSideways(rfs2tex *tex);

rfs2tex *graphics_GetTexById(int32_t id);

rfs2tex *graphics_LoadTex(const char *path);

rfs2tex *graphics_CreateTarget(int w, int h);

int graphics_ResizeTarget(rfs2tex *tex, int w, int h);

int graphics_SetTarget(rfs2tex *tex);

rfs2tex *graphics_GetTarget();

rfssurf *graphics_GetTargetSrf();

int graphics_ClearTarget();

int graphics_PresentTarget();

int graphics_DrawTex(rfs2tex *tex, int withalpha,
    int cropx, int cropy, int cropw, int croph,
    double r, double g, double b, double a,
    int x, int y, double scalex, double scaley
);

int graphics_DeleteTex(rfs2tex *tex);

int graphics_DrawRectangle(
    double r, double g, double b, double a,
    int x, int y, int w, int h
);

int graphics_DrawLine(
    double r, double g, double b, double a,
    int x1, int y1, int x2, int y2, int thickness
);

int graphics_PushRenderScissors(int x, int y, int w, int h);

void graphics_PopRenderScissors();


#endif  // RFS2_GRAPHICS_H_
