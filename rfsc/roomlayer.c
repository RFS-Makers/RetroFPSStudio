// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "filesys.h"
#include "graphics.h"
#include "hash.h"
#include "room.h"
#include "roomcolmap.h"
#include "roomdefs.h"
#include "roomlayer.h"


static rfs2tex *missingtex = NULL;
static hashmap *roomlayer_by_id_map = NULL;
static uint64_t possibly_free_roomlayer_id = 1;


int roomlayer_RequireHashmap() {
    if (roomlayer_by_id_map != NULL)
        return 1;
    roomlayer_by_id_map = hash_NewBytesMap(128);
    return (roomlayer_by_id_map != NULL);
}

rfs2tex *roomlayer_GetTexOfRef(roomtexref *ref) {
    if (!ref) return NULL;
    if (ref->tex)
        return ref->tex;
    ref->tex = graphics_LoadTex(ref->diskpath);
    if (!ref->tex) {
        if (!missingtex)
            missingtex = graphics_LoadTex("rfslua/res/missingtex");
        return missingtex;
    }
    return ref->tex;
}

roomtexref *roomlayer_MakeTexRef(
        roomlayer *lr, const char *diskpath
        ) {
    char *p = (diskpath ? filesys_Normalize(diskpath) : NULL);
    if (!p)
        return NULL;
    uintptr_t hashptrval = 0;
    if (!hash_StringMapGet(
            lr->texref_by_path_map, p,
            (uint64_t *)&hashptrval)) {
        roomtexref **newrefs = realloc(
            lr->texref, sizeof(*lr->texref) *
                (lr->texrefcount + 1));
        if (!newrefs) {
            free(p);
            return NULL;
        }
        lr->texref = newrefs;
        roomtexref *newref = malloc(sizeof(*newref));
        if (!newref) {
            free(p);
            return NULL;
        }
        lr->texref[lr->texrefcount] = newref;
        memset(newref, 0, sizeof(*newref));
        newref->diskpath = p;
        p = NULL;
        newref->refcount = 1;
        if (!hash_StringMapSet(
                lr->texref_by_path_map, newref->diskpath,
                (uint64_t)(uintptr_t)newref)) {
            free(newref->diskpath);
            free(newref);
            return NULL;
        }
        lr->texrefcount++;
        return newref;
    }
    free(p);
    roomtexref *ref = (roomtexref *)hashptrval;
    ref->refcount++;
    return ref;
}

void roomlayer_UnmakeTexRef(
        roomlayer *lr, roomtexref *ref
        ) {
    if (!ref)
        return;
    ref->refcount--;
    assert(ref->refcount >= 0);
}

roomlayer *roomlayer_ById(uint64_t id) {
    if (!roomlayer_RequireHashmap())
        return NULL;
    uintptr_t hashptrval = 0;
    if (!hash_BytesMapGet(
            roomlayer_by_id_map,
            (char *)&id, sizeof(id),
            (uint64_t *)&hashptrval))
        return NULL;
    return (roomlayer *)(void*)hashptrval;
}

uint64_t roomlayer_GetNewId() {
    if (!roomlayer_RequireHashmap())
        return 0;
    uint64_t i = possibly_free_roomlayer_id;
    if (i < 1)
        i = 1;
    roomlayer *lr = roomlayer_ById(i);
    if (!lr)
        return i;
    i = 1;
    while (i < INT64_MAX) {
        roomlayer *lr = roomlayer_ById(i);
        if (!lr)
            return i;
        i++;
    }
    return 0;
}

roomlayer *roomlayer_Create(uint64_t id) {
    if (!roomlayer_RequireHashmap())
        return NULL;
    if (id <= 0)
        return NULL;
    if (roomlayer_ById(id) != NULL)
        return NULL;
    roomlayer *lr = malloc(sizeof(*lr));
    if (!lr)
        return NULL;
    memset(lr, 0, sizeof(*lr));
    lr->id = id;
    lr->colmap = roomcolmap_Create();
    if (!lr->colmap) {
        free(lr);
        return NULL;
    }
    lr->colmap->parentlayer = lr;
    lr->room_by_id_map = hash_NewBytesMap(128);
    if (!lr->room_by_id_map) {
        roomcolmap_Destroy(lr->colmap);
        free(lr);
        return NULL;
    }
    lr->texref_by_path_map = hash_NewStringMap(128);
    if (!lr->texref_by_path_map) {
        hash_FreeMap(lr->texref_by_path_map); 
        roomcolmap_Destroy(lr->colmap);
        free(lr);
        return NULL;
    }
    lr->possibly_free_room_id = 1;
    if (!hash_BytesMapSet(
            roomlayer_by_id_map,
            (char *)&lr->id, sizeof(lr->id),
            (uint64_t)(uintptr_t)lr)) {
        roomcolmap_Destroy(lr->colmap);
        hash_FreeMap(lr->room_by_id_map);
        hash_FreeMap(lr->texref_by_path_map);
        free(lr);
        return NULL;
    }
    possibly_free_roomlayer_id = lr->id + 1;
    return lr;
}

void roomlayer_Destroy(roomlayer *lr) {
    if (!lr)
        return;
    while (lr->roomcount > 0) {
        #ifndef NDEBUG
        int oldcount = lr->roomcount;
        #endif
        room_Destroy(lr->rooms[0]);
        assert(oldcount > lr->roomcount);
    }
    lr->roomcount = 0;
    int i = 0;
    while (i < lr->texrefcount) {
        if (lr->texref[i]) {
            free(lr->texref[i]->diskpath);
            free(lr->texref[i]);
        }
        i++;
    }
    free(lr->texref);
    roomcolmap_Destroy(lr->colmap);
    free(lr->rooms);
    hash_BytesMapUnset(
        roomlayer_by_id_map,
        (char *)&lr->id, sizeof(lr->id)
    );
    hash_FreeMap(lr->room_by_id_map);
    hash_FreeMap(lr->texref_by_path_map);
    free(lr);
}
