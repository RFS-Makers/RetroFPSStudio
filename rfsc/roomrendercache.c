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
