// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "math2d.h"
#include "room.h"
#include "roomcolmap.h"
#include "roomdefs.h"
#include "roomlayer.h"
#include "roomobject.h"


void roomcolmap_UnregisterRoom(roomcolmap *colmap, room *r) {
    int64_t min_x = r->center_x - r->max_radius;
    int64_t max_x = r->center_x + r->max_radius;
    int64_t min_y = r->center_y - r->max_radius;
    int64_t max_y = r->center_y + r->max_radius;
    int64_t min_map_x = (-(colmap->cells_x / 2) * COLMAP_UNITS);
    int64_t min_map_y = (-(colmap->cells_y / 2) * COLMAP_UNITS);
    int64_t cell_min_x = (min_x - min_map_x) / COLMAP_UNITS;
    if (cell_min_x < 0)
        cell_min_x = 0;
    if (cell_min_x >= colmap->cells_x - 1)
        cell_min_x = colmap->cells_x - 1;
    int64_t cell_max_x = (max_x - min_map_x) / COLMAP_UNITS;
    if (cell_min_x < 0)
        cell_min_x = 0;
    if (cell_min_x >= colmap->cells_x - 1)
        cell_min_x = colmap->cells_x - 1;
    int64_t cell_min_y = (min_y - min_map_y) / COLMAP_UNITS;
    if (cell_min_y < 0)
        cell_min_y = 0;
    if (cell_min_y >= colmap->cells_y - 1)
        cell_min_y = colmap->cells_y - 1;
    int64_t cell_max_y = (max_y - min_map_y) / COLMAP_UNITS;
    if (cell_min_y < 0)
        cell_min_y = 0;
    if (cell_min_y >= colmap->cells_y - 1)
        cell_min_y = colmap->cells_y - 1;
    int32_t ix = cell_min_x;
    while (ix <= cell_max_x) {
        int32_t iy = cell_min_y;
        while (iy <= cell_max_y) {
            int32_t i = ix + iy * colmap->cells_x;
            int k = 0;
            while (k < colmap->cell_rooms_count[i]) {
                if (colmap->cell_rooms[i][k] == r) {
                    if (k + 1 < colmap->cell_rooms_count[i])
                        memmove(
                            &colmap->cell_rooms[i][k],
                            &colmap->cell_rooms[i][k + 1],
                            sizeof(**colmap->cell_rooms) *
                                (colmap->cell_rooms_count[i] - k - 1)
                        );
                    colmap->cell_rooms_count[i]--;
                }
                k++;
            }
            iy++;
        }
        ix++;
    }
}

roomcolmap *roomcolmap_Create(roomlayer *lr) {
    roomcolmap *cmap = malloc(sizeof(*cmap));
    if (!cmap)
        return NULL;
    memset(cmap, 0, sizeof(*cmap));
    cmap->parentlayer = lr;
    cmap->cells_x = COLMAP_CELLS_PER_AXIS;
    cmap->cells_y = COLMAP_CELLS_PER_AXIS;
    assert(((int)(cmap->cells_x / 2)) * 2 == cmap->cells_x);
    assert(((int)(cmap->cells_y / 2)) * 2 == cmap->cells_y);
    cmap->cell_rooms = malloc(
        sizeof(*cmap->cell_rooms) *
        cmap->cells_x * cmap->cells_y
    );
    if (!cmap->cell_rooms) {
        roomcolmap_Destroy(cmap);
        return 0;
    }
    cmap->cell_rooms_count = malloc(
        sizeof(*cmap->cell_rooms_count) *
        cmap->cells_x * cmap->cells_y
    );
    if (!cmap->cell_rooms_count) {
        roomcolmap_Destroy(cmap);
        return 0;
    }
    cmap->cell_objects_alloc = malloc(
        sizeof(*cmap->cell_objects_alloc) *
        cmap->cells_x * cmap->cells_y
    );
    if (!cmap->cell_objects_alloc) {
        roomcolmap_Destroy(cmap);
        return 0;
    }
    cmap->cell_objects = malloc(
        sizeof(*cmap->cell_objects) *
        cmap->cells_x * cmap->cells_y
    );
    if (!cmap->cell_objects) {
        roomcolmap_Destroy(cmap);
        return 0;
    }
    cmap->cell_objects_count = malloc(
        sizeof(*cmap->cell_objects_count) *
        cmap->cells_x * cmap->cells_y
    );
    if (!cmap->cell_objects_count) {
        roomcolmap_Destroy(cmap);
        return 0;
    }
    memset(
        cmap->cell_rooms, 0,
        sizeof(*cmap->cell_rooms) *
        cmap->cells_x * cmap->cells_y
    );
    memset(
        cmap->cell_rooms_count, 0,
        sizeof(*cmap->cell_rooms_count) *
        cmap->cells_x * cmap->cells_y
    );
    memset(
        cmap->cell_objects, 0,
        sizeof(*cmap->cell_objects) *
        cmap->cells_x * cmap->cells_y
    );
    memset(
        cmap->cell_objects_count, 0,
        sizeof(*cmap->cell_objects_count) *
        cmap->cells_x * cmap->cells_y
    );
    memset(
        cmap->cell_objects_alloc, 0,
        sizeof(*cmap->cell_objects_alloc) *
        cmap->cells_x * cmap->cells_y
    );
    return cmap;
}

void roomcolmap_Destroy(roomcolmap *cmap) {
    if (!cmap) return;
    free(cmap->cell_rooms);
    free(cmap->cell_rooms_count);
    int i = 0;
    while (i < cmap->cells_x * cmap->cells_y) {
        if (cmap->cell_objects)
            free(cmap->cell_objects[i]);
        i++;
    }
    free(cmap->cell_objects);
    free(cmap->cell_objects_alloc);
    free(cmap);
}

int roomcolmap_PosToCell(
        roomcolmap *colmap, int64_t x, int64_t y,
        int32_t *celx, int32_t *cely
        ) {
    x += ((colmap->cells_x / 2) * COLMAP_UNITS);
    y += ((colmap->cells_y / 2) * COLMAP_UNITS);
    if (x < 0 || y < 0)
        return 0;
    int64_t cx = x / COLMAP_UNITS;
    int64_t cy = y / COLMAP_UNITS;
    if (cx >= colmap->cells_x || cy >= colmap->cells_y)
        return 0;
    *celx = cx;
    *cely = cy;
    return 1;
}

int64_t roomcolmap_MinX(roomcolmap *colmap) {
    return -(colmap->cells_x / 2) * COLMAP_UNITS;
}

int64_t roomcolmap_MaxX(roomcolmap *colmap) {
    return (colmap->cells_x / 2) * COLMAP_UNITS - 1;
}

int64_t roomcolmap_MinY(roomcolmap *colmap) {
    return -(colmap->cells_y / 2) * COLMAP_UNITS;
}

int64_t roomcolmap_MaxY(roomcolmap *colmap) {
    return (colmap->cells_y / 2) * COLMAP_UNITS - 1;
}

void roomcolmap_RegisterRoom(roomcolmap *colmap, room *r) {
    int64_t min_x = r->center_x - r->max_radius;
    int64_t max_x = r->center_x + r->max_radius;
    int64_t min_y = r->center_y - r->max_radius;
    int64_t max_y = r->center_y + r->max_radius;
    int64_t min_map_x = (-(colmap->cells_x / 2) * COLMAP_UNITS);
    int64_t min_map_y = (-(colmap->cells_y / 2) * COLMAP_UNITS);
    int64_t cell_min_x = (min_x - min_map_x) / COLMAP_UNITS;
    if (cell_min_x < 0)
        cell_min_x = 0;
    if (cell_min_x >= colmap->cells_x - 1)
        cell_min_x = colmap->cells_x - 1;
    int64_t cell_max_x = (max_x - min_map_x) / COLMAP_UNITS;
    if (cell_min_x < 0)
        cell_min_x = 0;
    if (cell_min_x >= colmap->cells_x - 1)
        cell_min_x = colmap->cells_x - 1;
    int64_t cell_min_y = (min_y - min_map_y) / COLMAP_UNITS;
    if (cell_min_y < 0)
        cell_min_y = 0;
    if (cell_min_y >= colmap->cells_y - 1)
        cell_min_y = colmap->cells_y - 1;
    int64_t cell_max_y = (max_y - min_map_y) / COLMAP_UNITS;
    if (cell_min_y < 0)
        cell_min_y = 0;
    if (cell_min_y >= colmap->cells_y - 1)
        cell_min_y = colmap->cells_y - 1;
    int32_t ix = cell_min_x;
    while (ix <= cell_max_x) {
        int32_t iy = cell_min_y;
        while (iy <= cell_max_y) {
            int32_t i = ix + iy * colmap->cells_x;
            room **new_rooms = realloc(
                colmap->cell_rooms[i], sizeof(
                    *colmap->cell_rooms) * (
                    colmap->cell_rooms_count[i] + 1));
            if (!new_rooms) {
                fprintf(stderr,
                    "rfsc/roomcolmap.c: error: out of memory\n");
                _exit(1);
                return;
            }
            colmap->cell_rooms[i] = new_rooms;
            colmap->cell_rooms[i][colmap->cell_rooms_count[i]] = r;
            colmap->cell_rooms_count[i]++;
            iy++;
        }
        ix++;
    }
}

void roomcolmap_Debug_AssertObjectIsRegistered(
        roomcolmap *colmap, roomobj *o
        ) {
    #ifdef NDEBUG
    return;
    #else
    int c = 0;
    int32_t ix = 0;
    while (ix < colmap->cells_x) {
        int32_t iy = 0;
        while (iy < colmap->cells_y) {
            int32_t i = ix + iy * colmap->cells_x;
            int32_t k = 0;
            while (k < colmap->cell_objects_count[i]) {
                if (colmap->cell_objects[i][k] == o)
                    c++;
                k++;
            }
            iy++;
        }
        ix++;
    }
    if (c != 1) {
        fprintf(stderr,
            "rfsc/roomcolmap.c: error: integrity "
            "error, object unexpectedly "
            "NOT registered exactly once but %d times\n",
            c);
        assert(0);
        _exit(1);
    }
    #endif
}


void roomcolmap_Debug_AssertRoomIsRegistered(
        roomcolmap *colmap, room *r
        ) {
    #ifdef NDEBUG
    return;
    #else
    int32_t ix = 0;
    while (ix < colmap->cells_x) {
        int32_t iy = 0;
        while (iy < colmap->cells_y) {
            int32_t i = ix + iy * colmap->cells_x;
            int32_t k = 0;
            while (k < colmap->cell_rooms_count[i]) {
                if (colmap->cell_rooms[i][k] == r)
                    return;
                k++;
            }
            iy++;
        }
        ix++;
    }
    fprintf(stderr,
        "rfsc/roomcolmap.c: error: integrity "
        "error, room unexpectedly "
        "NOT registered\n");
    assert(0);
    _exit(1);
    #endif
}

void roomcolmap_Debug_AssertRoomIsNotRegistered(
        roomcolmap *colmap, room *r
        ) {
    #ifdef NDEBUG
    return;
    #else
    int32_t ix = 0;
    while (ix < colmap->cells_x) {
        int32_t iy = 0;
        while (iy < colmap->cells_y) {
            int32_t i = ix + iy * colmap->cells_x;
            int32_t k = 0;
            while (k < colmap->cell_rooms_count[i]) {
                if (colmap->cell_rooms[i][k] == r) {
                    fprintf(stderr,
                        "rfsc/roomcolmap.c: error: integrity "
                        "error, room unexpectedly left "
                        "registered in %d, %d\n",
                        (int)ix, (int)iy);
                    assert(0);
                    _exit(1);
                }
                k++;
            }
            iy++;
        }
        ix++;
    }
    #endif
}

void roomcolmap_RegisterObject(roomcolmap *colmap, roomobj *obj) {
    int64_t x = obj->x;
    int64_t y = obj->y;
    x += ((colmap->cells_x / 2) * COLMAP_UNITS);
    y += ((colmap->cells_y / 2) * COLMAP_UNITS);
    int64_t cell_x = x / COLMAP_UNITS;
    int64_t cell_y = y / COLMAP_UNITS;
    if (cell_x < 0) cell_x = 0;
    if (cell_y < 0) cell_y = 0;
    if (cell_x >= colmap->cells_x) cell_x = colmap->cells_x - 1;
    if (cell_y >= colmap->cells_y) cell_y = colmap->cells_y - 1;

    int index = cell_x + cell_y * colmap->cells_x;
    assert(index >= 0 && index < colmap->cells_x * colmap->cells_y);
    #ifndef NDEBUG
    {
        const int c = colmap->cell_objects_count[index];
        int i = 0;
        while (i < c) {
            assert(colmap->cell_objects[index][i] != obj);
            i++;
        }
    }
    #endif
    if (colmap->cell_objects_count[index] >=
            colmap->cell_objects_alloc[index]) {
        int new_alloc = (
            colmap->cell_objects_count[index] * 2
        ) + 16;
        roomobj **new_objects = realloc(
            colmap->cell_objects[index],
            sizeof(*new_objects) * new_alloc
        );
        if (!new_objects) {
            fprintf(stderr,
                "rfsc/roomcolmap.c: error: out of memory\n");
            _exit(1);
            return;
        }
        colmap->cell_objects[index] = new_objects;
        colmap->cell_objects_alloc[index] = new_alloc;
    }
    colmap->cell_objects[index][
        colmap->cell_objects_count[index]
    ] = obj;
    colmap->cell_objects_count[index]++;
}

void roomcolmap_UnregisterObject(roomcolmap *colmap, roomobj *obj) {
    int64_t x = obj->x;
    int64_t y = obj->y;
    x += ((colmap->cells_x / 2) * COLMAP_UNITS);
    y += ((colmap->cells_y / 2) * COLMAP_UNITS);
    int64_t cell_x = x / COLMAP_UNITS;
    int64_t cell_y = y / COLMAP_UNITS;
    if (cell_x < 0) cell_x = 0;
    if (cell_y < 0) cell_y = 0;
    if (cell_x >= colmap->cells_x) cell_x = colmap->cells_x - 1;
    if (cell_y >= colmap->cells_y) cell_y = colmap->cells_y - 1;

    int index = cell_x + cell_y * colmap->cells_x;
    const int c = colmap->cell_objects_count[index];
    roomobj **objs_of_cell = colmap->cell_objects[index];
    int i = 0;
    while (i < c) {
        if (objs_of_cell[i] == obj) {
            objs_of_cell[i] = NULL;
            if (i + 1 < c)
                memmove(&objs_of_cell[i],
                    &objs_of_cell[i + 1],
                    sizeof(*objs_of_cell) * (c - i - 1));
            colmap->cell_objects_count[index]--;
            return;
        }
        i++;
    }
    assert(0);
}

void roomcolmap_MovedObject(
        roomcolmap *colmap, roomobj *obj,
        int64_t oldx, int64_t oldy
        ) {
    int64_t newx = obj->x;
    int64_t newy = obj->y;
    obj->x = oldx;
    obj->y = oldy;
    roomcolmap_UnregisterObject(colmap, obj);
    obj->x = newx;
    obj->y = newy;
    roomcolmap_RegisterObject(colmap, obj);
}

room *roomcolmap_PickFromPos(
        roomcolmap *colmap, int64_t x, int64_t y
        ) {
    int64_t orig_x = x;
    int64_t orig_y = y;
    x += ((colmap->cells_x / 2) * COLMAP_UNITS);
    y += ((colmap->cells_y / 2) * COLMAP_UNITS);
    int64_t cell_x = x / COLMAP_UNITS;
    int64_t cell_y = y / COLMAP_UNITS;
    if (cell_x < 0 || cell_y < 0 ||
            cell_x >= colmap->cells_x ||
            cell_y >= colmap->cells_y)
        return NULL;
    int32_t i = cell_x + cell_y * colmap->cells_x;
    int32_t k = 0;
    while (k < colmap->cell_rooms_count[i]) {
        if (room_ContainsPoint(
                colmap->cell_rooms[i][k], orig_x, orig_y
                ))
            return colmap->cell_rooms[i][k];
        k++;
    }
    return NULL;
}


int _roomcolmap_IterateObjectsInRange_Ex_Do(
        roomcolmap *colmap, int64_t x, int64_t y, int64_t radius,
        int enforce_exact_radius, int follow_layer_portals,
        int in_portal,
        void *userdata,
        int (*iter_callback)(roomlayer *layer,
            roomobj *obj, uint8_t reached_through_layer_portal,
            int64_t portal_x, int64_t portal_y,
            int64_t distance, void *userdata),
        roomobj ***seen_in_portals, int *seen_in_portals_count,
        int *seen_in_portals_alloc,
        uint8_t *seen_in_portals_onheap) {
    // Compute top-left cell to scan from:
    int64_t topleft_x = x - radius;
    int64_t topleft_y = y - radius;
    topleft_x += ((colmap->cells_x / 2) * COLMAP_UNITS);
    topleft_y += ((colmap->cells_y / 2) * COLMAP_UNITS);
    int64_t topleft_cell_x = topleft_x / COLMAP_UNITS;
    int64_t topleft_cell_y = topleft_y / COLMAP_UNITS;
    if (topleft_cell_x < 0) topleft_cell_x = 0;
    if (topleft_cell_y < 0) topleft_cell_y = 0;
    if (topleft_cell_x >= colmap->cells_x ||
            topleft_cell_y >= colmap->cells_y)
        return 1;

    // Compute bottom-right cell to scan from:
    int64_t bottomright_x = x + radius;
    int64_t bottomright_y = y + radius;
    bottomright_x += ((colmap->cells_x / 2) * COLMAP_UNITS);
    bottomright_y += ((colmap->cells_y / 2) * COLMAP_UNITS);
    int64_t bottomright_cell_x = bottomright_x / COLMAP_UNITS;
    int64_t bottomright_cell_y = bottomright_y / COLMAP_UNITS;
    if (bottomright_cell_x >= colmap->cells_x)
        bottomright_cell_x = colmap->cells_x - 1;
    if (bottomright_cell_y >= colmap->cells_y)
        bottomright_cell_y = colmap->cells_y - 1;
    if (bottomright_cell_x < 0 ||
            bottomright_cell_y < 0)
        return 1;

    assert((*seen_in_portals && *seen_in_portals_alloc > 0) ||
        !follow_layer_portals || !in_portal);

    // Go through all collision map cells covered by our query:
    int64_t ix = topleft_cell_x;
    int64_t iy = topleft_cell_y;
    while (ix <= bottomright_cell_x) {
        iy = topleft_cell_y;
        while (iy <= bottomright_cell_y) {
            int index = (
                ix + iy * colmap->cells_x
            );

            // Iterate all objects in the current cell:
            const int ocount = (
                colmap->cell_objects_count[index]
            );
            int i = 0;
            while (i < ocount) {
                roomobj *o = colmap->cell_objects[index][i];
                // See if object is near enough or we don't care:
                int64_t dist = 0;  // Intentionally remains zero
                                   // if enforce_exact_radius == 0.
                if (likely(!enforce_exact_radius ||
                        (dist = math_veclen2di(x - o->x,
                            y - o->y)) <= radius)) {
                    // If we're looking into a portal, make sure we
                    // haven't seen this object already:
                    if (unlikely(in_portal)) {
                        int duplicate = 0;
                        int k = 0;
                        while (k < *seen_in_portals_count) {
                            if (unlikely((*seen_in_portals)[k] == o)) {
                                duplicate = 1;
                                break;
                            }
                            k++;
                        }
                        if (duplicate) {
                            // We've seen this. Skip.
                            i++;
                            continue;
                        }
                    }
                    // Do callback on object:
                    int result = iter_callback(colmap->parentlayer,
                        o, 0, 0, 0, dist, userdata);
                    if (!result)
                        return 0;  // stop iteration
                    // If in layer portal, register object as seen:
                    if (in_portal) {
                        if (*seen_in_portals_count >=
                                *seen_in_portals_alloc) {
                            // Our seen list is full, reallocate:
                            int new_alloc = (
                                (*seen_in_portals_count) * 2
                            ) + 16;
                            roomobj **new_seen_in_portals = malloc(
                                sizeof(*new_seen_in_portals) *
                                new_alloc
                            );
                            if (!new_seen_in_portals)
                                return -1;  // oom
                            memcpy(
                                *seen_in_portals, new_seen_in_portals,
                                sizeof(*new_seen_in_portals) *
                                    (*seen_in_portals_count)
                            );
                            if (*seen_in_portals_onheap)
                                free(*seen_in_portals);
                            *seen_in_portals = new_seen_in_portals;
                            *seen_in_portals_onheap = 1;
                            *seen_in_portals_alloc = new_alloc;
                        }
                        // Add object to list:
                        (*seen_in_portals_count)++;
                        (*seen_in_portals)[
                            (*seen_in_portals_count) - 1
                        ] = o;
                    }
                }
                i++;
            }
            // If NOT supposed to scan portals, skip to next cell:
            if (!follow_layer_portals || in_portal) {
                iy++;
                continue;
            }
            // Look for layer portals to other layers;
            const int roomcount = (
                colmap->cell_rooms_count[index]
            );
            i = 0;
            while (i < roomcount) {
                room *r = colmap->cell_rooms[index][i];
                int k = 0;
                while (k < r->corners) {
                    // Recurse into any portals we find:
                    if (r->wall[k].has_portal &&
                            r->wall[k].portal_targetroom &&
                            r->wall[k].portal_targetroom->parentlayer !=
                            colmap->parentlayer) {
                        assert(r->wall[k].portal_targetroom->parentlayer);
                        // Find our distance to portal:
                        int k2 = k + 1;
                        if (k2 >= r->corners) k2 = 0;
                        int64_t portalx, portaly;
                        math_nearestpointonsegment(x, y,
                            r->corner_x[k], r->corner_y[k],
                            r->corner_x[k2], r->corner_y[k2],
                            &portalx, &portaly
                        );
                        int64_t remaining_radius = (radius -
                            math_veclen2di(x - portalx, y - portaly));
                        if (remaining_radius <= 0) {
                            // Oops, this portal is too far out!
                            k++;
                            continue;
                        }
                        // Do actual recurse:
                        int result = (
                            _roomcolmap_IterateObjectsInRange_Ex_Do(
                                r->wall[k].portal_targetroom->parentlayer->
                                    colmap, portalx, portaly,
                                remaining_radius, enforce_exact_radius,
                                1, 1, userdata, iter_callback,
                                seen_in_portals, seen_in_portals_count,
                                seen_in_portals_alloc,
                                seen_in_portals_onheap
                            )
                        );
                    }
                    k++;
                }
                i++;
            }
            iy++;
        }
        ix++;
    }
    return 1;
}

int roomcolmap_IterateObjectsInRange_Ex(
        roomcolmap *colmap, int64_t x, int64_t y, int64_t radius,
        int enforce_exact_radius, int follow_layer_portals,
        void *userdata,
        int (*iter_callback)(roomlayer *layer,
            roomobj *obj, uint8_t reached_through_layer_portal,
            int64_t portal_x, int64_t portal_y,
            int64_t distance, void *userdata)
        ) {
    roomobj *seen_in_portals_buf[256];
    roomobj **seen_in_portals = (roomobj**)seen_in_portals_buf;
    int seen_in_portals_alloc = 256;
    int seen_in_portals_count = 0;
    uint8_t seen_in_portals_onheap = 0;

    int result = _roomcolmap_IterateObjectsInRange_Ex_Do(
        colmap, x, y, radius,
        enforce_exact_radius, follow_layer_portals, 0,
        userdata, iter_callback,
        &seen_in_portals, &seen_in_portals_count,
        &seen_in_portals_alloc, &seen_in_portals_onheap
    );
    if (seen_in_portals_onheap) {
        assert(seen_in_portals != seen_in_portals_buf);
        free(seen_in_portals);
    } else {
        assert(seen_in_portals == seen_in_portals_buf);
    }
    return (result != -1);  // no oom
}

int roomcolmap_IterateObjectsInRange(
        roomcolmap *colmap, int64_t x, int64_t y, int64_t radius,
        int enforce_exact_radius, void *userdata,
        int (*iter_callback)(roomlayer *layer,
            roomobj *obj, uint8_t reached_through_layer_portal,
            int64_t portal_x, int64_t portal_y, int64_t distance,
            void *userdata)
        ) {
    return roomcolmap_IterateObjectsInRange_Ex(
        colmap, x, y, radius, enforce_exact_radius, 1,
        userdata, iter_callback
    );
}
