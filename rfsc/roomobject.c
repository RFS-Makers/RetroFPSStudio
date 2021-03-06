// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesys.h"
#include "graphics.h"
#include "hash.h"
#include "room.h"
#include "roomcolmap.h"
#include "roomlayer.h"
#include "roomobject.h"


roomobj **roomobj_list = NULL;
int roomobj_list_count = 0;
static hashmap *roomobj_by_id_map = NULL;
static uint64_t possibly_free_roomobj_id = 1;
static hashmap *obj_texref_by_path_map = NULL;
static objtexref **obj_texrefs = NULL;
static int obj_texrefs_count = 0;
static rfs2tex *missingtex = NULL;


rfs2tex *roomobj_GetTexOfRef(objtexref *ref) {
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


int roomobj_RequireHashmap() {
    if (roomobj_by_id_map != NULL)
        return 1;
    roomobj_by_id_map = hash_NewBytesMap(128);
    return (roomobj_by_id_map != NULL);
}


objtexref *roomobj_MakeTexRef(
        const char *diskpath
        ) {
    char *p = (diskpath ? filesys_Normalize(diskpath) : NULL);
    if (!p)
        return NULL;
    if (!obj_texref_by_path_map) {
        obj_texref_by_path_map = hash_NewStringMap(128);
        if (!obj_texref_by_path_map) {
            free(p);
            return NULL;
        }
    }
    uintptr_t hashptrval = 0;
    if (!hash_StringMapGet(
            obj_texref_by_path_map, p,
            (uint64_t *)&hashptrval)) {
        objtexref **newrefs = realloc(
            obj_texrefs, sizeof(*obj_texrefs) *
                (obj_texrefs_count + 1));
        if (!newrefs) {
            free(p);
            return NULL;
        }
        obj_texrefs = newrefs;
        objtexref *newref = malloc(sizeof(*newref));
        if (!newref) {
            free(p);
            return NULL;
        }
        obj_texrefs[obj_texrefs_count] = newref;
        memset(newref, 0, sizeof(*newref));
        newref->diskpath = p;
        p = NULL;
        newref->refcount = 1;
        if (!hash_StringMapSet(
                obj_texref_by_path_map, newref->diskpath,
                (uint64_t)(uintptr_t)newref)) {
            free(newref->diskpath);
            free(newref);
            return NULL;
        }
        obj_texrefs_count++;
        return newref;
    }
    free(p);
    objtexref *ref = (objtexref *)hashptrval;
    ref->refcount++;
    return ref;
}


void roomobj_UnmakeTexRef(
        objtexref *ref) {
    if (!ref)
        return;
    ref->refcount--;
    assert(ref->refcount >= 0);
}

uint64_t roomobj_GetNewId() {
    if (!roomobj_RequireHashmap())
        return 0;
    uint64_t i = possibly_free_roomobj_id;
    if (i < 1)
        i = 1;
    roomobj *obj = roomobj_ById(i);
    if (!obj)
        return i;
    i = 1;
    while (i < INT64_MAX) {
        roomobj *obj = roomobj_ById(i);
        if (!obj)
            return i;
        i++;
    }
    return 0;
}

roomobj *roomobj_ById(uint64_t id) {
    if (!roomobj_RequireHashmap() || id <= 0)
        return NULL;
    uintptr_t hashptrval = 0;
    if (!hash_BytesMapGet(
            roomobj_by_id_map,
            (char *)&id, sizeof(id),
            (uint64_t *)&hashptrval))
        return NULL;
    return (roomobj *)(void*)hashptrval;
}

void roomobj_SetLayer(
        roomobj *obj, roomlayer *lr
        ) {
    if (lr == obj->parentlayer)
        return;
    if (obj->parentlayer != NULL) {
        roomcolmap_UnregisterObject(
            obj->parentlayer->colmap, obj
        );
    } else {
        assert(obj->parentroom == NULL);
    }
    if (lr) {
        roomcolmap_RegisterObject(lr->colmap, obj);
        roomcolmap_Debug_AssertObjectIsRegistered(
            lr->colmap, obj
        );
    }
    obj->parentlayer = lr;
    if (lr) {
        obj->parentroom = roomcolmap_PickFromPos(
            obj->parentlayer->colmap, obj->x, obj->y
        );
        if (obj->parentroom) {
            assert(obj->parentroom->parentlayer == lr);
        }
    } else {
        obj->parentroom = NULL;
    }
}

roomobj *roomobj_Create(uint64_t id,
        int objtype, void *objdata,
        void (*objdestroy)(roomobj *obj)
        ) {
    if (!roomobj_RequireHashmap())
        return NULL;
    if (id <= 0)
        return NULL;
    if (roomobj_ById(id) != NULL)
        return NULL;

    roomobj **newlist = realloc(
        roomobj_list, sizeof(*roomobj_list) *
        (roomobj_list_count + 1)
    );
    if (!newlist)
        return NULL;
    roomobj_list = newlist;

    roomobj *obj = malloc(sizeof(*obj));
    if (!obj)
        return NULL;
    memset(obj, 0, sizeof(*obj));
    obj->id = id;
    obj->objtype = objtype;
    obj->objdata = objdata;
    obj->objdestroy = objdestroy;
    if (!hash_BytesMapSet(
            roomobj_by_id_map,
            (char *)&obj->id, sizeof(obj->id),
            (uint64_t)(uintptr_t)obj)) {
        free(obj);
        return NULL;
    }
    possibly_free_roomobj_id = obj->id + 1;
    roomobj_list[roomobj_list_count] = obj;
    roomobj_list_count++;
    return obj;
}

void roomobj_UpdatePos(
        roomobj *obj, int updateroom,
        int64_t oldx, int64_t oldy
        ) {
    if (obj->parentlayer) {
        roomcolmap_MovedObject(
            obj->parentlayer->colmap, obj, oldx, oldy
        );
        #ifndef NDEBUG
        roomcolmap_Debug_AssertObjectIsRegistered(
            obj->parentlayer->colmap, obj
        );
        #endif
        if (updateroom) {
            obj->parentroom = roomcolmap_PickFromPos(
                obj->parentlayer->colmap, obj->x, obj->y
            );
        }
    }
}

void roomobj_Destroy(roomobj *obj) {
    if (!obj)
        return;
    if (obj->objdestroy)
        obj->objdestroy(obj);
    int found = 0;
    int i = 0;
    while (i < roomobj_list_count) {
        if (roomobj_list[i] == obj) {
            found = 1;
            if (i + 1 < roomobj_list_count)
                memmove(
                    &roomobj_list[i], &roomobj_list[i + 1],
                    sizeof(*roomobj_list) * (
                        roomobj_list_count - i - 1
                    )
                );
            break;
        }
        i++;
    }
    if (!found) {
        fprintf(stderr, "rfsc/roomobj.c: error: "
            "object %p/id %" PRIu64
            " not found in global list\n",
            obj, obj->id);
    }
    if (obj->parentlayer) {
        roomcolmap_UnregisterObject(
            obj->parentlayer->colmap, obj
        );
    }
    hash_BytesMapUnset(
        roomobj_by_id_map,
        (char *)&obj->id, sizeof(obj->id)
    );
    free(obj);
}


