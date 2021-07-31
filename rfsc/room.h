// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#ifndef RFS2_ROOM_H_
#define RFS2_ROOM_H_

#include "compileconfig.h"

#include <stdint.h>

#include "roomdefs.h"

typedef struct rfs2tex rfs2tex;
typedef struct room room;
typedef struct roomlayer roomlayer;
typedef struct roomtexref roomtexref;

typedef struct roomtexinfo {
    roomtexref *tex;
    uint8_t tex_stickyside;  // ignored for floor/ceiling,
        // ^ must be ROOM_DIR_UP or ROOM_DIR_DOWN
    uint8_t tex_issky;
    int32_t tex_shiftx, tex_shifty;
    int32_t tex_scaleintx, tex_scaleinty;  // range: TEX_FULLSCALE_INT=1x
    int32_t tex_scrollspeedx, tex_scrollspeedy;
    int32_t tex_gamestate_scrollx,  // range: 0 to TEX_REPEAT_UNITS
            tex_gamestate_scrolly;
} roomtexinfo;


typedef struct roomdecal {
    roomtexinfo tex;
    // Decal position shift & scale are in tex!
} roomdecal;

typedef struct roomwall {
    roomtexinfo wall_tex, aboveportal_tex;
    uint8_t has_portal, has_aboveportal_tex;
    room *portal_targetroom;
    int portal_targetwall;

    uint8_t has_portaldoor;
    roomtexinfo portaldoor_tex;
    uint8_t portaldoor_opendir;
    int32_t portaldoor_openamount;

    uint8_t decals_count;
    roomdecal decal[DECALS_MAXCOUNT];
} roomwall;

typedef struct room {
    roomlayer *parentlayer;
    uint64_t id;
    int16_t corners;
    int64_t corner_x[ROOM_MAX_CORNERS], corner_y[ROOM_MAX_CORNERS];
    roomwall wall[ROOM_MAX_CORNERS];
    int32_t sector_light_r, sector_light_g, sector_light_b;
    int64_t floor_z, height;
    roomtexinfo floor_tex, ceiling_tex;

    int64_t center_x, center_y, max_radius;
} room;


uint64_t room_GetNewId(roomlayer *group);
room *room_ById(roomlayer *grp, uint64_t id);
room *room_Create(roomlayer *grp, uint64_t id);
void room_Destroy(room *r);
int room_RecomputePosExtent(room *room);
int room_ContainsPoint(
    room *room, int64_t x, int64_t y
);

#endif  // RFS2_ROOM_H_
