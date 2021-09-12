// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_ROOMDEFS_H_
#define RFS2_ROOMDEFS_H_

#include "compileconfig.h"

#include <stdint.h>

#include "mathdefs.h"

// Item limits:
#define DECALS_MAXCOUNT 2
#define ROOM_MAX_CORNERS 8
#define MAX_ROOMS_PER_LAYER 256

// Sound limits:
#define MAX_SOUND_SECONDS 6

// Draw limits:
#define RENDER_MAX_PORTAL_DEPTH 12

// General dimensions:
#define TEX_COORD_SCALER ((int64_t)4096LL)
#define TEX_FULLSCALE_INT ((int64_t)1024LL)
#define ONE_METER_IN_UNITS ((int64_t)128LL)
#define GRID_UNITS (ONE_METER_IN_UNITS/4LL)
#define TEX_REPEAT_UNITS ((int64_t)(\
    ONE_METER_IN_UNITS * 2LL\
))

// Default textures:
#define ROOM_DEF_WALL_TEX "rfslua/res/default-game-res/texture/brick1"
#define ROOM_DEF_CEILING_TEX "rfslua/res/default-game-res/texture/wood1"
#define ROOM_DEF_FLOOR_TEX "rfslua/res/default-game-res/texture/wood1"

// Collision:
#define SMALLEST_COLLIDER_UNITS (ONE_METER_IN_UNITS/4LL)
#define LARGEST_COLLIDER_UNITS (ONE_METER_IN_UNITS * 4LL)
#define DEFAULT_COLLIDER_UNITS ((int64_t)(ONE_METER_IN_UNITS * 0.75))
#define COLMAP_UNITS ((int64_t)(ONE_METER_IN_UNITS * (int64_t)4))
#define COLMAP_CELLS_PER_AXIS 64
#define ROOM_MAX_EXTENTS_LEN ((int64_t)(ONE_METER_IN_UNITS * (int64_t)8))
#define ADDITIVE_BLOCK_MAX_EXTENTS \
    ((int64_t)(ONE_METER_IN_UNITS * (int64_t)2))

// Drawing:
#define FIXED_ROOMTEX_SIZE 32
#define FIXED_ROOMTEX_SIZE_2 64
#define _RAYCAST_LENGTH_MAYBE ((int64_t)COLMAP_UNITS * 64LL)
#define RAYCAST_LENGTH ((int64_t)(\
    _RAYCAST_LENGTH_MAYBE > 6000LL ? _RAYCAST_LENGTH_MAYBE :\
    6000LL))
#define VIEWPLANE_RAY_LENGTH_DIVIDER 128
#define WALL_BATCH_DIVIDER 16
#define FLOORCEIL_BATCH_DIVIDER 16
#define MIN_WALL_BATCH_LEN 32
#define MAX_LIGHT_RANGE ((int64_t)(ONE_METER_IN_UNITS * 10))
#define LIGHT_COLOR_SCALAR 32
#define DRAW_CORNER_COORD_UPSCALE 16
#define MAX_DRAWN_LIGHTS_PER_ROOM 4
#if !defined(LOWRES_3DRENDERER)
#define DUPLICATE_WALL_PIX 1
#define DUPLICATE_FLOOR_PIX 2
#else
#define DUPLICATE_WALL_PIX 2
#define DUPLICATE_FLOOR_PIX 3
#endif

// Door and elevator directions:
#define ROOM_DIR_UP 1
#define ROOM_DIR_LEFT 2
#define ROOM_DIR_RIGHT 3
#define ROOM_DIR_DOWN 4


#endif  // RFS2_ROOMDFS_H_

