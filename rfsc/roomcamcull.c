// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "math2d.h"
#include "roomcam.h"
#include "roomcamcache.h"
#include "roomcamcull.h"
#include "roomdefs.h"
#include "roomobject.h"


int roomcamcull_ComputePolyCull(
        roomcam *cam, int poly_corners,
        int64_t *corner_x, int64_t *corner_y,
        int canvasw, int canvash, polycullinfo *output
        ) {
    memset(output, 0, sizeof(*output));
    assert(poly_corners >= 2 &&
        poly_corners <= ROOM_MAX_CORNERS);

    // (Note: without this margin we sometimes get render gaps
    // between wall segments due to precision issues.)
    const int outercolmargin = 3;

    // Calculate outer screen columns of the room's polygon:
    int evernotbehindcam = 0;
    int i = 0;
    while (i < poly_corners) {
        uint8_t resultbehindcam = 0;
        int32_t result;
        if (!roomcam_XYToViewplaneX(
                cam, canvasw, canvash,
                corner_x[i], corner_y[i],
                &result, &resultbehindcam
                ))
            return 0;
        if (!resultbehindcam) evernotbehindcam = 1;
        if (result >= 0 && result <= canvasw)
            output->on_screen = 1;
        output->corners_to_screenxcol[i] = result;
        if (i == 0) {
            output->gooddrawstart_xcol =
                imax(0, output->corners_to_screenxcol[i]);
            output->corners_minscreenxcol =
                output->corners_to_screenxcol[i] -
                    outercolmargin;
            output->corners_maxscreenxcol =
                output->corners_to_screenxcol[i] +
                    outercolmargin;
        } else {
            output->gooddrawstart_xcol = imin(
                output->gooddrawstart_xcol,
                imax(0, output->corners_to_screenxcol[i]));
            output->corners_minscreenxcol = imin(
                output->corners_minscreenxcol,
                output->corners_to_screenxcol[i] -
                    outercolmargin);
            output->corners_maxscreenxcol = imax(
                output->corners_maxscreenxcol,
                output->corners_to_screenxcol[i] +
                    outercolmargin);
        }
        i++;
    }
    if (output->corners_minscreenxcol !=
            output->corners_maxscreenxcol &&
            evernotbehindcam)  // crossing the screen in front.
        output->on_screen = 1;

    // The above gives wrong results for some polygons where
    // any edge crosses with the outer screen (x=0, x=screew - 1).
    // Therefore, detect whether that is the case for any edge:
    if (output->corners_minscreenxcol < 0 ||
            output->corners_maxscreenxcol >= canvasw
            ) {  // outside of screen
        int64_t screenedgel_x1 = cam->obj->x;
        int64_t screenedgel_y1 = cam->obj->y;
        int64_t screenedgel_x2 = cam->obj->x + cam->cache->
            rotatedplanevecs_x[0] * VIEWPLANE_RAY_LENGTH_DIVIDER;
        int64_t screenedgel_y2 = cam->obj->y + cam->cache->
            rotatedplanevecs_y[0] * VIEWPLANE_RAY_LENGTH_DIVIDER;
        int64_t screenedger_x1 = cam->obj->x;
        int64_t screenedger_y1 = cam->obj->y;
        assert(cam->cache->cachedw == canvasw);
        int64_t screenedger_x2 = cam->obj->x + cam->cache->
            rotatedplanevecs_x[canvasw - 1] * VIEWPLANE_RAY_LENGTH_DIVIDER;
        int64_t screenedger_y2 = cam->obj->y + cam->cache->
            rotatedplanevecs_y[canvasw - 1] * VIEWPLANE_RAY_LENGTH_DIVIDER;
        int inext = 1;
        i = 0;
        while (i < poly_corners) {
            int64_t ix,iy;
            if (output->corners_maxscreenxcol < canvasw &&
                    math_lineintersect2di(
                    corner_x[i], corner_y[i],
                    corner_x[inext], corner_y[inext],
                    screenedger_x1, screenedger_y1,
                    screenedger_x2, screenedger_y2,
                    &ix, &iy
                    )) {
                output->corners_maxscreenxcol = canvasw;
            }
            if (output->corners_minscreenxcol >= 0 &&
                    math_lineintersect2di(
                    corner_x[i], corner_y[i],
                    corner_x[inext], corner_y[inext],
                    screenedgel_x1, screenedgel_y1,
                    screenedgel_x2, screenedgel_y2,
                    &ix, &iy
                    )) {
                output->gooddrawstart_xcol = 0;
                output->corners_minscreenxcol = -1;
            }
            i++;
            inext++;
            if (inext >= poly_corners) inext = 0;
        }
    }
    return 1;
}
