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

int roomrendercache_SetXCols(
        roomrendercache *rcache, room *r, roomcam *cam,
        int canvasw, int canvash
        ) {
    if (!rcache->screenxcols_set) {
        // (Note: without this margin we sometimes get render gaps
        // between wall segments due to precision issues.)
        const int outercolmargin = 3;

        // Calculate outer screen columns of the room's polygon:
        rcache->screenxcols_set = 1;
        int i = 0;
        while (i < r->corners) {
            int32_t result;
            if (!roomcam_XYToViewplaneX(
                    cam, canvasw, canvash,
                    r->corner_x[i], r->corner_y[i],
                    &result
                    ))
                return 0;
            rcache->corners_to_screenxcol[i] = result;
            if (i == 0) {
                rcache->corners_minscreenxcol =
                    rcache->corners_to_screenxcol[i] -
                        outercolmargin;
                rcache->corners_maxscreenxcol =
                    rcache->corners_to_screenxcol[i] +
                        outercolmargin;
            } else {
                rcache->corners_minscreenxcol = imin(
                    rcache->corners_minscreenxcol,
                    rcache->corners_to_screenxcol[i] -
                        outercolmargin);
                rcache->corners_maxscreenxcol = imax(
                    rcache->corners_maxscreenxcol,
                    rcache->corners_to_screenxcol[i] +
                        outercolmargin);
            }
            i++;
        }
    }
    return 1;
}

typedef struct dynlight_cbinfo {
    roomrendercache *rcache;
    int64_t samplex, sampley;
    int calc_idx;
} dynlight_cbinfo;

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
    cachedlightcornerinfo *linfo = &(
        rcache->dynlight_sample[cbinfo->calc_idx]
    );
    movable *mov = (movable *)obj->objdata;
    if (mov->emit_range <= distance)
        return 1;
    int k = 0;
    while (k < linfo->light_count) {
        if (mov->emit_range - distance >
                linfo->light[k].range -
                linfo->light[k].dist) {
            break;
        }
        k++;
    }
    if (k >= MAX_DRAWN_LIGHTS_PER_ROOM)
        return 1;
    if (k + 1 < linfo->light_count)
        memmove(
            &linfo->light[k],
            &linfo->light[k + 1],
            sizeof(*linfo->light) * (
                MAX_DRAWN_LIGHTS_PER_ROOM - k - 1)
        );
    if (linfo->light_count <
            MAX_DRAWN_LIGHTS_PER_ROOM)
        linfo->light_count++;
    linfo->light[k].r = mov->emit_r;
    linfo->light[k].g = mov->emit_g;
    linfo->light[k].b = mov->emit_b;
    linfo->light[k].range = mov->emit_range;
    linfo->light[k].dist = distance;
    linfo->light[k].x = obj->x;
    linfo->light[k].y = obj->y;
    return 1;
}

int roomrendercache_SetDynLights(
        roomrendercache *rcache, room *r
        ) {
    if (rcache->dynlights_set ||
            !r->parentlayer)
        return 1;
    int i = 0;
    while (i < ROOM_MAX_CORNERS * 2 + 1) {
        int64_t samplepos_x = 0;
        int64_t samplepos_y = 0;
        if (i == (ROOM_MAX_CORNERS * 2 + 1) - 1) {
            samplepos_x = r->center_x;
            samplepos_y = r->center_y;
        } else {
            int corner = (i / 2);
            int corner_next = corner + 1;
            if (corner_next >= r->corners) corner_next = 0;
            if (i == corner * 2) {
                samplepos_x = r->corner_x[corner];
                samplepos_y = r->corner_y[corner];
            } else {
                samplepos_x = (r->corner_x[corner] +
                    r->corner_x[corner_next]) / 2;
                samplepos_y = (r->corner_y[corner] +
                    r->corner_x[corner_next]) / 2;
            }
        }
        dynlight_cbinfo c;
        c.samplex = samplepos_x;
        c.sampley = samplepos_x;
        c.calc_idx = i;
        c.rcache = rcache;
        int result = roomcolmap_IterateObjectsInRange(
            r->parentlayer->colmap, samplepos_x, samplepos_y,
            MAX_LIGHT_RANGE, 1, &rcache,
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
