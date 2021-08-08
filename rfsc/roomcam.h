// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

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

typedef struct renderstatistics {
    int base_geometry_slices_rendered;
    int base_geometry_rays_cast;
    int base_geometry_rooms_recursed;
    int last_canvas_width, last_canvas_height,
        last_fov, last_fovh, last_fovv;

    uint64_t fps_ts;
    int32_t frames_accumulator;
    int32_t fps;
} renderstatistics;

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
    int32_t *result
);

renderstatistics *roomcam_GetStats(roomcam *c);

#endif  // RFS2_ROOMCAM_H_
