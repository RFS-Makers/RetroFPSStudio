// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_ROOMCAMRENDERSTATISTICS_H_
#define RFS2_ROOMCAMRENDERSTATISTICS_H_

#include "compileconfig.h"

#include <stdint.h>

typedef struct renderstatistics {
    int base_geometry_slices_rendered;
    int base_geometry_rays_cast;
    int base_geometry_rooms_recursed;
    int32_t last_canvas_width, last_canvas_height,
        last_fov, last_fovh, last_fovv;

    uint64_t fps_ts;
    int32_t frames_accumulator;
    int32_t fps;
} renderstatistics;


#endif  // RFS2_ROOMCAMRENDERSTATISTICS_H_
