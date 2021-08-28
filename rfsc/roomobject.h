// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_ROOMOBJECT_H_
#define RFS2_ROOMOBJECT_H_

#include "compileconfig.h"

#include <stdint.h>

typedef struct rfs2tex rfs2tex;
typedef struct room room;
typedef struct roomobj roomobj;
typedef struct roomlayer roomlayer;


typedef enum roomobjtype {
    ROOMOBJ_UNKNOWN = 0,
    ROOMOBJ_CAMERA = 1,
    ROOMOBJ_MOVABLE = 2,
    ROOMOBJ_BLOCK = 3,
    ROOMOBJ_TOTAL
} roomobjtype;


typedef struct objtexref {
    rfs2tex *tex;
    char *diskpath;
    int refcount;
} objtexref;

typedef struct roomobj {
    uint64_t id;

    roomlayer *parentlayer;
    room *parentroom;
    int64_t x, y, z;
    int32_t angle;  // degrees * ANGLE_SCALAR
    double anglef;

    uint8_t has_collision;
    int32_t col_radius, height;

    int objtype;
    void *objdata;
    void (*objdestroy)(roomobj *obj);
} roomobj;


uint64_t roomobj_GetNewId();

objtexref *roomobj_MakeTexRef(
    const char *dispath
);

void roomobj_UnmakeTexRef(
    objtexref *ref);

roomobj *roomobj_ById(uint64_t id);

roomobj *roomobj_Create(
    uint64_t id, int objtype, void *objdata,
    void (*objdestroy)(roomobj *obj)
);

void roomobj_Destroy(roomobj *obj);

void roomobj_UpdatePos(
    roomobj *obj, int updateroom, int64_t oldx, int64_t oldy
);

void roomobj_SetLayer(roomobj *obj, roomlayer *lr);

#endif  // RFS2_ROOMOBJECT_H_
