// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#include "compileconfig.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "room.h"
#include "roomcolmap.h"
#include "roomdefs.h"
#include "roomlayer.h"


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
            int32_t i = ix + iy * colmap->cells_y;
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
        free(cmap);
        return 0;
    }
    cmap->cell_rooms_count = malloc(
        sizeof(*cmap->cell_rooms_count) *
        cmap->cells_x * cmap->cells_y
    );
    if (!cmap->cell_rooms_count) {
        free(cmap->cell_rooms);
        free(cmap);
        return 0;
    }
    cmap->cell_objects = malloc(
        sizeof(*cmap->cell_objects) *
        cmap->cells_x * cmap->cells_y
    );
    if (!cmap->cell_objects) {
        free(cmap->cell_rooms);
        free(cmap->cell_rooms_count);
        free(cmap);
        return 0;
    }
    cmap->cell_objects_count = malloc(
        sizeof(*cmap->cell_objects_count) *
        cmap->cells_x * cmap->cells_y
    );
    if (!cmap->cell_objects_count) {
        free(cmap->cell_rooms);
        free(cmap->cell_rooms_count);
        free(cmap->cell_objects);
        free(cmap);
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
    return cmap;
}

void roomcolmap_Destroy(roomcolmap *colmap) {

}

int roomcolmap_PosToCell(
        roomcolmap *colmap, int64_t x, int64_t y,
        int32_t *celx, int32_t *cely
        ) {
    x -= (-(colmap->cells_x / 2) * COLMAP_UNITS);
    y -= (-(colmap->cells_y / 2) * COLMAP_UNITS);
    if (x < 0 || y < 0)
        return 0;
    int64_t cx = x / COLMAP_UNITS;
    int64_t cy = y / COLMAP_UNITS;
    if (cx > colmap->cells_x || cy > colmap->cells_y)
        return 0;
    *celx = cx;
    *cely = cy;
    return 1;
}

int64_t roomcolmap_MinX(roomcolmap *colmap) {
    return -(colmap->cells_x / 2) * COLMAP_UNITS;
}

int64_t roomcolmap_MaxX(roomcolmap *colmap) {
    return (colmap->cells_x / 2) * COLMAP_UNITS;
}

int64_t roomcolmap_MinY(roomcolmap *colmap) {
    return -(colmap->cells_y / 2) * COLMAP_UNITS;
}

int64_t roomcolmap_MaxY(roomcolmap *colmap) {
    return (colmap->cells_y / 2) * COLMAP_UNITS;
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
            int32_t i = ix + iy * colmap->cells_y;
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

void roolcolmap_Debug_AssertRoomIsRegistered(
        roomcolmap *colmap, room *r
        ) {
    #ifdef NDEBUG
    return;
    #else
    int32_t ix = 0;
    while (ix < colmap->cells_x) {
        int32_t iy = 0;
        while (iy < colmap->cells_y) {
            int32_t i = ix + iy * colmap->cells_y;
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

void roolcolmap_Debug_AssertRoomIsNotRegistered(
        roomcolmap *colmap, room *r
        ) {
    #ifdef NDEBUG
    return;
    #else
    int32_t ix = 0;
    while (ix < colmap->cells_x) {
        int32_t iy = 0;
        while (iy < colmap->cells_y) {
            int32_t i = ix + iy * colmap->cells_y;
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

}

void roomcolmap_UnregisterObject(roomcolmap *colmap, roomobj *obj) {

}

void roomcolmap_MovedObject(roomcolmap *colmap, roomobj *obj) {

}

room *roomcolmap_PickFromPos(
        roomcolmap *colmap, int64_t x, int64_t y
        ) {
    int64_t orig_x = x;
    int64_t orig_y = y;
    x -= (-(colmap->cells_x / 2) * COLMAP_UNITS);
    y -= (-(colmap->cells_y / 2) * COLMAP_UNITS);
    int64_t cell_x = x / COLMAP_UNITS;
    int64_t cell_y = y / COLMAP_UNITS;
    if (cell_x < 0 || cell_y < 0 ||
            cell_x >= colmap->cells_x ||
            cell_y >= colmap->cells_y)
        return NULL;
    int32_t i = cell_x + cell_y * colmap->cells_y;
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

