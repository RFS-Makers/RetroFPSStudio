// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_ROOMOBJECT_MOVABLE_H_
#define RFS2_ROOMOBJECT_MOVABLE_H_

#include "compileconfig.h"

#include <stdint.h>

typedef struct objtexref objtexref;
typedef struct roomobj roomobj;

typedef struct spritetexinfo {
    objtexref *tex;
    int32_t sprite_scaleintx, sprite_scaleinty;
} spritetexinfo;

typedef struct movable {
    roomobj *obj;

    uint8_t is_solid;
    int32_t colscalar_hori,  // these use TEX_FULLSCALE_INT = 1.0
            colscalar_verti;
    spritetexinfo sprite_tex;

    uint8_t does_emit;
    int32_t emit_r, emit_g, emit_b, emit_range;
} movable;


movable *movable_Create();

movable *movable_CreateWithId(uint64_t id);

movable *movable_CreateWithSingleSprite(const char *sprite);

movable *movable_CreateWithSingleSpriteAndId(
    const char *sprite, uint64_t sid);

#endif  // RFS2_ROOMOBJECT_MOVABLE_H_
