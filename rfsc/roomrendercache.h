// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef ROOMRENDERCACHE_H_
#define ROOMRENDERCACHE_H_

#include "compileconfig.h"

#include <stdint.h>

#include "roomdefs.h"

typedef struct room room;
typedef struct roomcam roomcam;

typedef struct roomrendercache {
    uint8_t screenxcols_set;
    int corners_to_screenxcol[ROOM_MAX_CORNERS];
    int corners_minscreenxcol,
        corners_maxscreenxcol;

    uint8_t upscaledcorners_set;
    int64_t upscaledcorner_x[ROOM_MAX_CORNERS];
    int64_t upscaledcorner_y[ROOM_MAX_CORNERS];
} roomrendercache;

void room_ClearRenderCache();

roomrendercache *room_GetRenderCache(uint64_t id);

int roomrendercache_SetXCols(
    roomrendercache *rcache, room *r, roomcam *cam,
    int canvasw, int canvash
);

int roomrendercache_FillUpscaledCoords(
    roomrendercache *rcache, room *r
);

#endif  // ROOMRENDERCACHE_H_

