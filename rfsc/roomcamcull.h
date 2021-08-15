// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef ROOMCAMCULL_H_
#define ROOMCAMCULL_H_

#include "compileconfig.h"

#include <stdint.h>

#include "roomdefs.h"


typedef struct roomcam roomcam;

typedef struct polycullinfo {
    int corners_to_screenxcol[ROOM_MAX_CORNERS];
    int corners_minscreenxcol,
        corners_maxscreenxcol;
    int gooddrawstart_xcol;
    uint8_t on_screen;
} polycullinfo;


int roomcamcull_ComputePolyCull(
    roomcam *cam,
    int poly_corners, int64_t *corner_x, int64_t *corner_y,
    int canvasw, int canvash, polycullinfo *result
);

#endif  // ROOMCAMCULL_H_

