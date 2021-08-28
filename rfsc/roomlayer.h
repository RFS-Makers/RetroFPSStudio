// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_ROOMLAYER_H_
#define RFS2_ROOMLAYER_H_

#include "compileconfig.h"

#include <stdint.h>

#include "hash.h"
#include "room.h"

typedef struct roomcolmap roomcolmap;

typedef struct roomtexref {
    rfs2tex *tex;
    char *diskpath;
    int refcount;
} roomtexref;

typedef struct roomlayer {
    uint64_t id;
    int texrefcount;
    roomtexref **texref;
    hashmap *texref_by_path_map;
    hashmap *room_by_id_map;
    uint64_t possibly_free_room_id;
    int roomcount;
    room **rooms;
    roomcolmap *colmap;
} roomlayer;


int roomlayer_RequireHashmap();

roomtexref *roomlayer_MakeTexRef(
    roomlayer *lr, const char *dispath
);

void roomlayer_UnmakeTexRef(
    roomlayer *lr, roomtexref *ref);

rfs2tex *roomlayer_GetTexOfRef(roomtexref *ref);

roomlayer *roomlayer_ById(uint64_t id);

uint64_t roomlayer_GetNewId();  // returns 0 on failure

roomlayer *roomlayer_Create(uint64_t id);

void roomlayer_Destroy(roomlayer *group);

#endif  // RFS2_ROOMLAYER_H_
