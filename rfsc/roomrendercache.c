// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "math2d.h"
#include "room.h"
#include "roomcam.h"
#include "roomcamcache.h"
#include "roomcolmap.h"
#include "roomlayer.h"
#include "roomobject.h"
#include "roomobject_movable.h"
#include "roomrendercache.h"


typedef struct rrcachebucket {
    int entry_count, entry_alloc;
    uint64_t *entry_id;
    roomrendercache *entries;
} rrcachebucket;


#define RENDERCACHEBUCKETS 64

rrcachebucket roomrendercachemap[RENDERCACHEBUCKETS] = {0};


void room_ClearRenderCache() {
    int i = 0;
    while (i < RENDERCACHEBUCKETS) {
        roomrendercachemap[i].entry_count = 0;
        i++;
    }
}

roomrendercache *room_GetRenderCache(uint64_t id) {
    uint64_t bucketid = (id % RENDERCACHEBUCKETS);
    rrcachebucket *bucket = &roomrendercachemap[bucketid];
    int i = 0;
    while (i < bucket->entry_count) {
        if (bucket->entry_id[i] == id)
            return &bucket->entries[i];
        i++;
    }
    if (bucket->entry_count + 1 > bucket->entry_alloc) {
        int new_alloc = bucket->entry_alloc * 2 + 16;
        if (new_alloc < bucket->entry_count + 1)
            new_alloc = bucket->entry_count + 1;
        uint64_t *new_ids = realloc(
            bucket->entry_id, sizeof(*bucket->entry_id) *
            new_alloc
        );
        if (!new_ids)
            return NULL;
        bucket->entry_id = new_ids;
        roomrendercache *new_entries = realloc(
            bucket->entries, sizeof(*bucket->entries) *
            new_alloc
        );
        if (!new_entries)
            return NULL;
        bucket->entries = new_entries;
        bucket->entry_alloc = new_alloc;
    }
    assert(bucket->entry_alloc >= bucket->entry_count + 1);
    bucket->entry_count++;
    bucket->entry_id[bucket->entry_count - 1] = id;
    roomrendercache *rcache = (
        &bucket->entries[bucket->entry_count - 1]
    );
    memset(rcache, 0, sizeof(*rcache));
    return rcache;
}

int roomrendercache_FillUpscaledCoords(
        roomrendercache *rcache, room *r
        ) {
    if (rcache->upscaledcorners_set)
        return 1;
    rcache->upscaledcorners_set = 1;
    int i = 0;
    while (i < r->corners) {
        rcache->upscaledcorner_x[i] = (
            r->corner_x[i] * DRAW_CORNER_COORD_UPSCALE
        );
        rcache->upscaledcorner_y[i] = (
            r->corner_y[i] * DRAW_CORNER_COORD_UPSCALE
        );
        i++;
    }
    return 1;
}


int roomrendercache_SetCullInfo(
        roomrendercache *rcache, room *r, roomcam *cam,
        int canvasw, int canvash
        ) {
    if (!rcache->cullinfo_set) {
        if (!roomcamcull_ComputePolyCull(
                cam, r->corners, r->corner_x, r->corner_y,
                canvasw, canvash, &rcache->cullinfo
                ))
            return 0;
        rcache->cullinfo_set = 1;
    }
    return 1;
}

typedef struct dynlight_cbinfo {
    roomrendercache *rcache;
    roomlayer *ourlayer;
    int64_t samplex, sampley;
    int calc_idx;
} dynlight_cbinfo;

static int _add_lightinfo_to_list(
        roomobj *obj, int64_t distance,
        int *light_count, cachedlightinfo *light_list
        ) {
    movable *mov = (movable *)obj->objdata;
    if (mov->emit_range <= distance)
        return 1;
    int k = 0;  // Check duplicates:
    while (k < *light_count) {
        if (obj == light_list[k].obj)
            return 1;
        k++;
    }
    // Find insert pos:
    k = 0;
    while (k < *light_count) {
        if (mov->emit_range - distance >
                light_list[k].range -
                light_list[k].dist) {
            break;
        }
        k++;
    }
    if (k >= MAX_DRAWN_LIGHTS_PER_ROOM)
        return 1;
    if (k + 1 < *light_count)
        memmove(
            &light_list[k],
            &light_list[k + 1],
            sizeof(*light_list) * (
                MAX_DRAWN_LIGHTS_PER_ROOM - k - 1)
        );
    if (*light_count <
            MAX_DRAWN_LIGHTS_PER_ROOM)
        (*light_count)++;
    light_list[k].obj = obj;
    light_list[k].r = mov->emit_r;
    light_list[k].g = mov->emit_g;
    light_list[k].b = mov->emit_b;
    light_list[k].range = mov->emit_range;
    light_list[k].dist = distance;
    light_list[k].x = obj->x;
    light_list[k].y = obj->y;
    light_list[k].z = obj->z;
    return 1;
} 

int roomrendercache_CallBack_AddDynLight(
        ATTR_UNUSED roomlayer *layer,
        roomobj *obj,
        ATTR_UNUSED uint8_t reached_through_layer_portal,
        ATTR_UNUSED int64_t portal_x,
        ATTR_UNUSED int64_t portal_y,
        int64_t distance, void *userdata) {
    dynlight_cbinfo *cbinfo = (dynlight_cbinfo *)userdata;
    roomrendercache *rcache = cbinfo->rcache;
    if (!obj->parentlayer ||
            obj->objtype != ROOMOBJ_MOVABLE ||
            !((movable *)obj->objdata)->does_emit)
        return 1;
    if (!_add_lightinfo_to_list(
            obj, distance,
            &rcache->light_count, rcache->light)) {
        return 0;
    }
    return 1;
}

int roomrendercache_SetDynLights(
        roomrendercache *rcache, room *r
        ) {
    if (rcache->dynlights_set ||
            !r->parentlayer)
        return 1;
    dynlight_cbinfo c;
    c.rcache = rcache;
    c.ourlayer = r->parentlayer;
    int i = 0;
    while (i <= r->corners) {
        int64_t sample_x = (
            (i < r->corners) ? r->corner_x : r->center_x
        );
        int64_t sample_y = (
            (i < r->corners) ? r->corner_y : r->center_y
        );
        int result = roomcolmap_IterateObjectsInRange(
            r->parentlayer->colmap, sample_x, sample_y,
            MAX_LIGHT_RANGE * 10 / 12, 1, &c,
            roomrendercache_CallBack_AddDynLight
        );
        if (!result) {
            return 0;
        }
        i++;
    }
    rcache->dynlights_set = 1;
    return 1;
}
