// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
#include <inttypes.h>
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


void room_ComputeExtentsByPolygon(
            int corners, int64_t *cx, int64_t *cy,
            int64_t *min_x, int64_t *min_y,
            int64_t *max_x, int64_t *max_y) {
    int64_t _min_x = cx[0];
    int64_t _max_x = cx[0];
    int64_t _min_y = cy[0];
    int64_t _max_y = cy[0];

    int i = 1;
    while (i < corners) {
        if (cx[i] < _min_x)
            _min_x = cx[i];
        if (cx[i] > _max_x)
            _max_x = cx[i];
        if (cy[i] < _min_y)
            _min_y = cy[i];
        if (cy[i] > _max_y)
            _max_y = cy[i];
        i++;
    }
    *min_x = _min_x;
    *min_y = _min_y;
    *max_x = _max_x;
    *max_y = _max_y;
}


void room_ComputeExtents(room *r, int64_t *min_x, int64_t *min_y,
            int64_t *max_x, int64_t *max_y) {
    room_ComputeExtentsByPolygon(
        r->corners, r->corner_x, r->corner_y,
        min_x, min_y, max_x, max_y);
}


int room_VerifyBasicGeometryByPolygon(
        int corners, int64_t *cx, int64_t *cy,
        int requireconvex, int64_t max_extents) {
    if (corners < 3 || corners > ROOM_MAX_CORNERS)
        return 0;

    // Make sure no corners share a spot:
    int iprev = corners - 1;
    int i = 0;
    while (i < corners) {
        int k = 0;
        while (k < corners) {
            if (k != i &&
                    cx[i] == cx[k] &&
                    cy[i] == cy[k]) {
                return 0;
            }
            k++;
        }
        i++;
    }

    // Ensure it is convex if requested:
    if (requireconvex) {
        int iprev = corners - 1;
        int inext = 1;
        int i = 0;
        while (i < corners) {
            int64_t walldir_x, walldir_y;
            walldir_x = (cx[inext] - cx[i]);
            walldir_y = (cy[inext] - cy[i]);
            if (walldir_x == 0 && walldir_y == 0)
                return 0;
            int64_t prevwalldir_x, prevwalldir_y;
            prevwalldir_x = (
                cx[i] - cx[iprev]
            );
            prevwalldir_y = (
                cy[i] - cy[iprev]
            );
            if (prevwalldir_x == 0 && prevwalldir_y == 0)
                return 0;
            int32_t angle_i = math_angle2di(
                walldir_x, walldir_y
            );
            int32_t angle_iprev = math_angle2di(
                prevwalldir_x, prevwalldir_y
            );
            int32_t anglediff = math_fixanglei(
                angle_i - angle_iprev
            );
            if (anglediff < 0)
                return 0;
            inext++;
            if (inext >= corners)
                inext = 0;
            iprev++;
            if (iprev >= corners)
                iprev = 0;
            i++;
        }
    }

    // Ensure it's not too large:
    int64_t min_x, min_y, max_x, max_y;
    room_ComputeExtentsByPolygon(
        corners, cx, cy,
        &min_x, &min_y, &max_x, &max_y);
    assert(max_x >= min_y && max_y >= min_y);
    if (max_x - min_x > max_extents ||
            max_y - min_y > max_extents) {
        return 0;
    }

    // Check it's counter-clockwise:
    if (!math_checkpolyccw2di(corners, cx, cy)) {
        return 0;
    }

    // Check walls don't intersect with each other:
    int inext = 1;
    i = 0;
    while (i < corners) {
        int knext = 1;
        int k = 0;
        while (k < i) {
            if (i - 1 == k || (k == 0 && i == corners - 1)) {
                // Neighboring walls, ignore.
                // (They'll collide with their corner spots.)
                knext++;
                if (knext >= corners) knext = 0;
                k++;
                continue;
            }
            // Collision check:
            int64_t wix, wiy;
            if (math_lineintersect2di(
                    cx[i], cy[i],
                    cx[inext], cy[inext],
                    cx[k], cy[k],
                    cx[knext], cy[knext],
                    &wix, &wiy
                    )) {
                return 0;
            }
            knext++;
            if (knext >= corners) knext = 0;
            k++;
        }
        inext++;
        if (inext >= corners) inext = 0;
        i++;
    }
    return 1;
}


int room_VerifyBasicGeometry(room *r) {
    if (!room_VerifyBasicGeometryByPolygon(
            r->corners, r->corner_x, r->corner_y,
            0, ROOM_MAX_EXTENTS_LEN
            ))
        return 0;

    // Ensure it doesn't exceed its colmap:
    if (r->parentlayer) {
        int64_t min_x, min_y, max_x, max_y;
        room_ComputeExtentsByPolygon(
            r->corners, r->corner_x, r->corner_y,
            &min_x, &min_y, &max_x, &max_y);
        assert(max_x >= min_y && max_y >= min_y);
        if (min_x < roomcolmap_MinX(r->parentlayer->colmap) ||
                max_x > roomcolmap_MaxX(r->parentlayer->colmap) ||
                min_y < roomcolmap_MinY(r->parentlayer->colmap) ||
                max_y > roomcolmap_MaxY(r->parentlayer->colmap)) {
            return 0;
        }
    }

    return 1;
}


int room_RecomputePosExtent(room *r) {
    assert(r->corners > 0);
    int64_t min_x, min_y, max_x, max_y;
    room_ComputeExtents(r, &min_x, &min_y, &max_x, &max_y);
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
    roomcolmap_Debug_AssertRoomIsNotRegistered(
        r->parentlayer->colmap, r
    );
    #endif
    r->center_x = center_x;
    r->center_y = center_y;
    r->max_radius = max_radius;
    roomcolmap_RegisterRoom(r->parentlayer->colmap, r);
    #ifndef NDEBUG
    roomcolmap_Debug_AssertRoomIsRegistered(
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
    r->corner_x[0] = ONE_METER_IN_UNITS * 2;
    r->corner_y[0] = -ONE_METER_IN_UNITS * 2;
    r->corner_x[1] = -ONE_METER_IN_UNITS * 2;
    r->corner_y[1] = -ONE_METER_IN_UNITS * 2;
    r->corner_x[2] = 0 * 2;
    r->corner_y[2] = ONE_METER_IN_UNITS * 2;
    r->height = ONE_METER_IN_UNITS * 2;
    r->floor_z = -r->height / 2;
    r->floor_tex.tex_scaleintx = TEX_FULLSCALE_INT;
    r->floor_tex.tex_scaleinty = TEX_FULLSCALE_INT;
    r->ceiling_tex.tex_scaleintx = TEX_FULLSCALE_INT;
    r->ceiling_tex.tex_scaleinty = TEX_FULLSCALE_INT;    
    r->sector_light_r = LIGHT_COLOR_SCALAR;
    r->sector_light_g = LIGHT_COLOR_SCALAR;
    r->sector_light_b = LIGHT_COLOR_SCALAR;
    r->floor_tex.tex = roomlayer_MakeTexRef(
        lr, ROOM_DEF_FLOOR_TEX);
    r->ceiling_tex.tex = roomlayer_MakeTexRef(
        lr, ROOM_DEF_CEILING_TEX);
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
    assert(room_VerifyBasicGeometry(r));
    return r;
}

void room_Destroy(room *r) {
    if (!r)
        return;
    if (r->parentlayer && r->parentlayer->colmap)
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
            assert(pr->wall[i2].portal_targetroom == r);
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
            pr->wall[i2].has_portal = 0;
            pr->wall[i2].portal_targetroom = NULL;
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
    if (r->parentlayer) {
        hash_BytesMapUnset(
            r->parentlayer->room_by_id_map,
            (char *)&r->id, sizeof(r->id)
        );
        r->parentlayer->possibly_free_room_id = r->id;
        int32_t i = 0;
        while (i < r->parentlayer->roomcount) {
            if (r->parentlayer->rooms[i] == r) {
                if (i + 1 < r->parentlayer->roomcount)
                    memmove(
                        &r->parentlayer->rooms[i],
                        &r->parentlayer->rooms[i + 1],
                        sizeof(*r->parentlayer->rooms) *
                            (r->parentlayer->roomcount - i - 1)
                    );
                r->parentlayer->roomcount--;
                break;
            }
            i++;
        }
    }
    free(r);
}

int room_ContainsPoint(
        room *r, int64_t x, int64_t y
        ) {
    return math_polycontains2di(
        x, y, r->corners, r->corner_x, r->corner_y
    );
}
