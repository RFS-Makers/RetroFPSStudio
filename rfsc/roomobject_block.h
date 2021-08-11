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
    int64_t *corner_x, *corner_y;
    roomtexinfo wall_tex[ROOM_MAX_CORNERS];
    roomtexinfo bottom_tex, top_tex;
} block;


block *block_Create();

#endif  // RFS2_ROOMOBJECT_BLOCK_H_
