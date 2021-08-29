// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details


#ifndef RFS2_DRAWGEOM_H_
#define RFS2_DRAWGEOM_H_

#include "compileconfig.h"

#include <stdint.h>

typedef struct room room;
typedef struct block block;

typedef enum geomtype {
    DRAWGEOM_UNKNOWN = 0,
    DRAWGEOM_ROOM = 1,
    DRAWGEOM_BLOCK = 2,
    DRAWGEOM_TOTAL
} geomtype;

typedef struct drawgeom {
    int type;
    union {
        room *r;
        block *bl;
    };
} drawgeom;

typedef enum geomtextype {
    GEOMTEX_UNKNOWN = 0,
    GEOMTEX_ROOM = 1,
    GEOMTEX_BLOCK = 2,
    GEOMTEX_SPRITE = 3,
    GEOMTEX_TOTAL
} geomtextype;

typedef struct roomtexinfo roomtexinfo;
typedef struct blocktexinfo blocktexinfo;
typedef struct spritetexinfo spritetexinfo;

typedef struct geomtex {
    int type;
    union {
        roomtexinfo *roomtex;
        blocktexinfo *blocktex; 
        spritetexinfo *spritetex;
    };
} geomtex;


#endif  // RFS2_DRAWGEOM_H_
