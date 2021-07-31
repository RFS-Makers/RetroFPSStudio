// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#ifndef RFS2_ROOMCOLMAP_H_
#define RFS2_ROOMCOLMAP_H_

#include "compileconfig.h"

#include <stdint.h>

#include "roomdefs.h"

typedef struct rfs2tex rfs2tex;
typedef struct room room;
typedef struct roomlayer roomlayer;
typedef struct roomobj roomobj;
typedef struct roomtexref roomtexref;


typedef struct colmapcell {
    int overlappingrooms;
    room *overlappingroom;
} colmapcell;


typedef struct roomcolmap {
    roomlayer *parentlayer;
    int32_t cells_x, cells_y;
    room ***cell_rooms;
    int *cell_rooms_count;
    roomobj ***cell_objects;
    int *cell_objects_count;
} roomcolmap;

roomcolmap *roomcolmap_Create();

void roolcolmap_Debug_AssertRoomIsNotRegistered(
    roomcolmap *colmap, room *r
);

void roolcolmap_Debug_AssertRoomIsRegistered(
    roomcolmap *colmap, room *r
);

void roomcolmap_Destroy(roomcolmap *colmap);

int roomcolmap_PosToCell(
    roomcolmap *colmap, int64_t x, int64_t y,
    int32_t *celx, int32_t *cely
);

int64_t roomcolmap_MinX(roomcolmap *colmap);

int64_t roomcolmap_MaxX(roomcolmap *colmap);

int64_t roomcolmap_MinY(roomcolmap *colmap);

int64_t roomcolmap_MaxY(roomcolmap *colmap);

void roomcolmap_RegisterRoom(roomcolmap *colmap, room *r);

void roomcolmap_UnregisterRoom(roomcolmap *colmap, room *r);

void roomcolmap_RegisterObject(roomcolmap *colmap, roomobj *obj);

void roomcolmap_UnregisterObject(roomcolmap *colmap, roomobj *obj);

void roomcolmap_MovedObject(roomcolmap *colmap, roomobj *obj);

room *roomcolmap_PickFromPos(
    roomcolmap *colmap, int64_t x, int64_t y
);

#endif  // RFS2_ROOMCOLMAP_H_
