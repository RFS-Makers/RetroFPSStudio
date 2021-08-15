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

typedef struct roomcam {
    roomobj *obj;
    int32_t vangle, fov;  // degrees * ANGLE_SCALAR
    double vanglef, fovf;
    uint8_t gamma;  // default: 128

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

renderstatistics *roomcam_GetStats(roomcam *c);

#endif  // RFS2_ROOMCAM_H_
