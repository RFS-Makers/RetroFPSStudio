// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#ifndef RFS2_ROOMDEFS_H_
#define RFS2_ROOMDEFS_H_

#include "compileconfig.h"

#include <stdint.h>

#include "mathdefs.h"

// Item limits:
#define DECALS_MAXCOUNT 2
#define ROOM_MAX_CORNERS 8
#define MAX_ROOMS_PER_GROUP 256

// Draw limits
#define RENDER_MAX_PORTAL_DEPTH 12

// General dimensions:
#define TEX_COORD_SCALER ((int64_t)10000LL)
#define TEX_FULLSCALE_INT ((int64_t)1000LL)
#define ONE_METER_IN_UNITS (128LL)
#define GRID_UNITS (ONE_METER_IN_UNITS/4LL)
#define TEX_REPEAT_UNITS (ONE_METER_IN_UNITS * 1LL)

// Default textures:
#define ROOM_DEF_WALL_TEX "rfslua/res/default-game-res/texture/brick1"
#define ROOM_DEF_CEILING_TEX "rfslua/res/default-game-res/texture/wood1"
#define ROOM_DEF_FLOOR_TEX "rfslua/res/default-game-res/texture/wood1"

// Collision:
#define SMALLEST_COLLIDER_UNITS (ONE_METER_IN_UNITS/4LL)
#define LARGEST_COLLIDER_UNITS (ONE_METER_IN_UNITS * 4LL)
#define DEFAULT_COLLIDER_UNITS ((int64_t)(ONE_METER_IN_UNITS * 0.75))
#define COLMAP_UNITS (ONE_METER_IN_UNITS * 4LL)
#define COLMAP_CELLS_PER_AXIS 64
#define _RAYCAST_LENGTH_MAYBE ((int64_t)COLMAP_UNITS * 16LL)
#define RAYCAST_LENGTH ((int64_t)(\
    _RAYCAST_LENGTH_MAYBE > 6000LL ? _RAYCAST_LENGTH_MAYBE :\
    6000LL))
#define VIEWPLANE_RAY_LENGTH_DIVIDER 10

// Door and elevator directions:
#define ROOM_DIR_UP 1
#define ROOM_DIR_LEFT 2
#define ROOM_DIR_RIGHT 3
#define ROOM_DIR_DOWN 4


#endif  // RFS2_ROOMDFS_H_

