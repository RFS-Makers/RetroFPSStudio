// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#ifndef RFS2_ROOMCAM_H_
#define RFS2_ROOMCAM_H_

#include "compileconfig.h"

#include <stdint.h>

typedef struct room room;
typedef struct roomobj roomobj;
typedef struct roomcamcache roomcamcache;

typedef struct roomcam {
    roomobj *obj;
    int16_t vangle, fov;  // degrees * ANGLE_SCALAR
    double vanglef, fovf;
    uint8_t gamma;  // default: 128

    roomcamcache *cache;
} roomcam;


roomcam *roomcam_Create();

int roomcam_Render(
    roomcam *cam, int x, int y, int w, int h
);


#endif  // RFS2_ROOMCAM_H_
