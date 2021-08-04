// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#ifndef RFS2_ROOMOBJECT_COLMOV_H_
#define RFS2_ROOMOBJECT_COLMOV_H_

#include "compileconfig.h"

#include <stdint.h>

typedef struct roomobj roomobj;

typedef struct colmovobj {
    roomobj *obj;

    uint8_t is_solid;
    int32_t col_radius, height;
} colmovobj;


colmovobj *colmov_Create();

#endif  // RFS2_ROOMOBJECT_COLMOV_H_
