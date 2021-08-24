// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_ROOMOBJECT_BLOCK_H_
#define RFS2_ROOMOBJECT_BLOCK_H_

#include "compileconfig.h"

#include <stdint.h>

#include "room.h"

typedef struct roomobj roomobj;

typedef struct block {
    roomobj *obj;

    int64_t bottom_z, height;
    int corners;
    int64_t corner_x[ROOM_MAX_CORNERS];
    int64_t corner_y[ROOM_MAX_CORNERS];
    roomtexinfo wall_tex;
    roomtexinfo bottomtop_tex;
} block;


block *block_Create(
    int corners, int64_t *corner_x, int64_t *corner_y);
block *block_CreateById(uint64_t id, int corners,
    int64_t *corner_x, int64_t *corner_y);

#endif  // RFS2_ROOMOBJECT_BLOCK_H_
