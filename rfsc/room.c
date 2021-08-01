// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#include "compileconfig.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hash.h"
#include "math2d.h"
#include "room.h"
#include "roomcolmap.h"
#include "roomdefs.h"
#include "roomlayer.h"


room *room_ById(roomlayer *lr, uint64_t id) {
    uintptr_t hashptrval = 0;
    if (!hash_BytesMapGet(
            lr->room_by_id_map,
            (char *)&id, sizeof(id),
            (uint64_t *)&hashptrval))
        return NULL;
    return (room *)(void*)hashptrval;
}

uint64_t room_GetNewId(roomlayer *lr) {
    uint64_t i = lr->possibly_free_room_id;
    if (i < 1)
        i = 1;
    room *r = room_ById(lr, i);
    if (!r)
        return i;
    i = 1;
    while (i < INT64_MAX) {
        room *r = room_ById(lr, i);
        if (!r)
            return i;
        i++;
    }
    return 0;
}

int room_RecomputePosExtent(room *r) {
    assert(r->corners > 0);
    int64_t min_x = r->corner_x[0];
    int64_t max_x = r->corner_x[0];
    int64_t min_y = r->corner_y[0];
    int64_t max_y = r->corner_y[0];

    int i = 0;
    while (i < r->corners) {
        if (r->corner_x[i] < min_x)
            min_x = r->corner_x[i];
        if (r->corner_x[i] > max_x)
            max_x = r->corner_x[i];
        if (r->corner_y[i] < min_y)
            min_y = r->corner_y[i];
        if (r->corner_y[i] > max_y)
            max_y = r->corner_y[i];
        i++;
    }
    int64_t center_x = (min_x + max_x) / 2;
    int64_t center_y = (min_y + max_y) / 2;
    int64_t max_radius_1 = (
        ceil(((double)llabs(min_x - max_x)) / 2.0)
    );
    int64_t max_radius_2 = (
        ceil(((double)llabs(min_y - max_y)) / 2.0)
    );
    int64_t max_radius = max_radius_1;
    if (max_radius_2 > max_radius)
        max_radius = max_radius_2;
    roomcolmap_UnregisterRoom(r->parentlayer->colmap, r);
    #ifndef NDEBUG
    roolcolmap_Debug_AssertRoomIsNotRegistered(
        r->parentlayer->colmap, r
    );
    #endif
    r->center_x = center_x;
    r->center_y = center_y;
    r->max_radius = max_radius;
    roomcolmap_RegisterRoom(r->parentlayer->colmap, r);
    #ifndef NDEBUG
    roolcolmap_Debug_AssertRoomIsRegistered(
        r->parentlayer->colmap, r
    );
    #endif

    int inext = 1;
    i = 0;
    while (i < r->corners) {
        int64_t tx = (r->corner_x[inext] - r->corner_x[i]);
        int64_t ty = (r->corner_y[inext] - r->corner_y[i]);
        int64_t normal_x = ty;
        int64_t normal_y = -tx;
        int64_t wallcenter_x = r->corner_x[i] + tx / 2;
        int64_t wallcenter_y = r->corner_y[i] + ty / 2;
        int64_t wall_to_roomcenter_x = (
            r->center_x - wallcenter_x
        );
        int64_t wall_to_roomcenter_y = (
            r->center_y - wallcenter_y
        );
        if (math_vecsopposite2di(
                wall_to_roomcenter_x, wall_to_roomcenter_y,
                normal_x, normal_y)) {
            normal_x = -normal_x;
            normal_y = -normal_y;
        }
        r->wall[i].normal_x = normal_x;
        r->wall[i].normal_y = normal_y;
        inext++;
        if (inext >= r->corners)
            inext = 0;
        i++;
    }
    return 1;
}

room *room_Create(roomlayer *lr, uint64_t id) {
    if (id <= 0 || !lr)
        return NULL;
    if (room_ById(lr, id) != NULL)
        return NULL;
    room *r = malloc(sizeof(*r));
    if (!r)
        return NULL;
    memset(r, 0, sizeof(*r));
    r->id = id;
    if (!hash_BytesMapSet(
            lr->room_by_id_map,
            (char *)&r->id, sizeof(r->id),
            (uint64_t)(uintptr_t)r)) {
        free(r);
        return NULL;
    }
    room **newrooms = realloc(lr->rooms,
        sizeof(*lr->rooms) * (lr->roomcount + 1)
    );
    if (!newrooms) {
        hash_BytesMapUnset(
            lr->room_by_id_map,
            (char *)&r->id, sizeof(r->id)
        );
        free(r);
        return NULL;
    }
    lr->rooms = newrooms;
    lr->rooms[lr->roomcount] = r;
    lr->roomcount++;
    r->parentlayer = lr;
    r->corners = 3;
    int i = 0;
    while (i < r->corners) {
        r->wall[i].wall_tex.tex_scaleintx = TEX_FULLSCALE_INT;
        r->wall[i].wall_tex.tex_scaleinty = TEX_FULLSCALE_INT;
        r->wall[i].wall_tex.tex = (
            roomlayer_MakeTexRef(lr, ROOM_DEF_WALL_TEX)
        );
        if (!r->wall[i].wall_tex.tex) {
            goto failandfreetextures;
        }
        i++;
    }
    r->corner_x[0] = -ONE_METER_IN_UNITS * 2;
    r->corner_y[0] = -ONE_METER_IN_UNITS * 2;
    r->corner_x[1] = ONE_METER_IN_UNITS * 2;
    r->corner_y[1] = -ONE_METER_IN_UNITS * 2;
    r->corner_x[2] = 0 * 2;
    r->corner_y[2] = ONE_METER_IN_UNITS * 2;
    r->height = ONE_METER_IN_UNITS * 2;
    r->floor_z = -r->height / 2;
    r->floor_tex.tex_scaleintx = TEX_FULLSCALE_INT;
    r->floor_tex.tex_scaleinty = TEX_FULLSCALE_INT;
    r->ceiling_tex.tex_scaleintx = TEX_FULLSCALE_INT;
    r->ceiling_tex.tex_scaleinty = TEX_FULLSCALE_INT;    
    r->sector_light_r = TEX_FULLSCALE_INT;
    r->sector_light_g = TEX_FULLSCALE_INT;
    r->sector_light_b = TEX_FULLSCALE_INT;
    r->floor_tex.tex = roomlayer_MakeTexRef(lr, ROOM_DEF_FLOOR_TEX);
    r->ceiling_tex.tex = roomlayer_MakeTexRef(lr, ROOM_DEF_CEILING_TEX);
    if (!r->floor_tex.tex || !r->ceiling_tex.tex)
        goto failandfreetextures;
    if (!room_RecomputePosExtent(r)) {
        failandfreetextures: ;
        lr->roomcount--;
        hash_BytesMapUnset(
            lr->room_by_id_map,
            (char *)&r->id, sizeof(r->id)
        );
        int k = 0;
        while (k < r->corners) {
            roomlayer_UnmakeTexRef(
                r->wall[i].wall_tex.tex
            );
            k++;
        }
        roomlayer_UnmakeTexRef(r->ceiling_tex.tex);
        roomlayer_UnmakeTexRef(r->floor_tex.tex);
        free(r);
        return NULL;
    }
    assert(r->max_radius >= (
        llabs(r->corner_x[0] - r->corner_x[1]) / 2)
    );
    lr->possibly_free_room_id = r->id + 1;
    return r;
}

void room_Destroy(room *r) {
    if (!r)
        return;
    roomcolmap_UnregisterRoom(r->parentlayer->colmap, r);
    int i = 0;
    while (i < r->corners) {
        if (r->wall[i].wall_tex.tex) {
            r->wall[i].wall_tex.tex->refcount--;
        }
        if (r->wall[i].has_aboveportal_tex &&
                r->wall[i].aboveportal_tex.tex) {
            r->wall[i].aboveportal_tex.tex->refcount--;
        }
        if (r->wall[i].has_portaldoor &&
                r->wall[i].portaldoor_tex.tex) {
            r->wall[i].portaldoor_tex.tex->refcount--;
        }
        if (r->wall[i].has_portal) {
            room *pr = r->wall[i].portal_targetroom;
            int i2 = r->wall[i].portal_targetwall;
            if (pr->wall[i2].has_portaldoor) {
                pr->wall[i2].has_portaldoor = 0;
                if (pr->wall[i2].portaldoor_tex.tex)
                    pr->wall[i2].portaldoor_tex.tex->refcount--;
                pr->wall[i2].portaldoor_tex.tex = NULL;
            }
            if (pr->wall[i2].has_aboveportal_tex) {
                pr->wall[i2].has_aboveportal_tex = 0;
                if (pr->wall[i2].aboveportal_tex.tex)
                    pr->wall[i2].aboveportal_tex.tex->refcount--;
                pr->wall[i2].aboveportal_tex.tex = NULL;
            }
        }
        int k = 0;
        while (k < r->wall[i].decals_count) {
            if (r->wall[i].decal[k].tex.tex) {
                r->wall[i].decal[k].tex.tex->refcount--;
            }
            k++;
        }
        i++;
    }
    if (r->floor_tex.tex)
        r->floor_tex.tex->refcount--;
    if (r->ceiling_tex.tex)
        r->ceiling_tex.tex->refcount--;
    hash_BytesMapUnset(
        r->parentlayer->room_by_id_map,
        (char *)&r->id, sizeof(r->id)
    );
    r->parentlayer->possibly_free_room_id = r->id;
    free(r);
}

int room_ContainsPoint(
        room *r, int64_t x, int64_t y
        ) {
    return math_polycontains2di(
        x, y, r->corners, r->corner_x, r->corner_y
    );
}
