// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_ROOMOBJECT_MOVABLE_H_
#define RFS2_ROOMOBJECT_MOVABLE_H_

#include "compileconfig.h"

#include <stdint.h>

typedef struct roomobj roomobj;

typedef struct movable {
    roomobj *obj;

    uint8_t is_solid;
    int32_t col_radius, height;

    uint8_t does_emit;
    int32_t emit_r, emit_g, emit_b, emit_range;
} movable;


movable *movable_Create();

#endif  // RFS2_ROOMOBJECT_MOVABLE_H_
