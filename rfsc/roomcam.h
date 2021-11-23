// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_ROOMCAM_H_
#define RFS2_ROOMCAM_H_

#include "compileconfig.h"

#include <stdint.h>

#include "roomcamrenderstatistics.h"

typedef struct room room;
typedef struct roomobj roomobj;
typedef struct roomcamcache roomcamcache;
typedef struct roomrendercache roomrendercache;

typedef struct roomcam {
    roomobj *obj;
    int32_t vangle, fov;  // degrees * ANGLE_SCALAR
    double vanglef, fovf;
    uint8_t gamma;  // default: 8, 0 to 15

    roomcamcache *cache;
} roomcam;


roomcam *roomcam_Create();

int roomcam_Render(
    roomcam *cam, int x, int y, int w, int h
);

int32_t roomcam_XYZToViewplaneY(
    roomcam *cam, int w, int h, int64_t px, int64_t py, int64_t pz,
    int32_t *result
);

int roomcam_XYToViewplaneX(
    roomcam *cam, int w, int h, int64_t px, int64_t py,
    int32_t *result, uint8_t *resultbehindcam
);


int roomcam_DynamicLightAtXY(
    roomrendercache *rcache, room *r,
    int64_t x, int64_t y, int64_t z, int isatwallno,
    int isfacingup, int isfacingdown, int scaletorange,
    int32_t *cr, int32_t *cg, int32_t *cb
);

renderstatistics *roomcam_GetStats(roomcam *c);

#endif  // RFS2_ROOMCAM_H_
