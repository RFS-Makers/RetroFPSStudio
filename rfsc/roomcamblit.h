// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_ROOMCAMBLIT_H_
#define RFS2_ROOMCAMBLIT_H_

#include "compileconfig.h"

#include <stdint.h>


typedef struct drawgeom drawgeom;
typedef struct geomtex geomtex;
typedef struct roomcam roomcam;


int roomcam_DrawWall(
    roomcam *cam, drawgeom *geom, int aboveportal,
    int canvasx, int canvasy,
    int xcol, int endcol,
    int originwall, geomtex *texinfo,
    int64_t wall_floor_z, int64_t wall_height
);

int roomcam_DrawFloorCeiling(
    roomcam *cam, drawgeom *geom,
    int canvasx, int canvasy,
    int xcol, int endcol
);

#endif  // RFS2_ROOMCAMBLIT_H_
