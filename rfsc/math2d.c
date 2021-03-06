// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "math2d.h"
#include "mathdefs.h"


void math_nearestpointonsegment(
        int64_t nearesttox, int64_t nearesttoy,
        int64_t lx1, int64_t ly1, int64_t lx2, int64_t ly2,
        int64_t *resultx, int64_t *resulty
        ) {
    if (lx1 == lx2 && ly1 == ly2) {
        *resultx = lx1;
        *resulty = ly1;
        return;
    }
    // Rotate things to be seen locally to the line:
    int32_t angle_segment = math_angle2di(
        lx2 - lx1, ly2 - ly1
    );
    int64_t len_segment = math_veclen2di(lx2 - lx1, ly2 - ly1);
    assert(len_segment >= 0);
    nearesttox -= lx1;
    nearesttoy -= ly1;
    math_rotate2di(&nearesttox, &nearesttoy, -angle_segment);
    if (nearesttox < 0) {
        *resultx = lx1;
        *resulty = ly1;
    } else if (nearesttox > len_segment) {
        *resultx = lx2;
        *resulty = ly2;
    } else {
        // Rotate result back into world space:
        *resultx = nearesttox;
        *resulty = 0;
        math_rotate2di(resultx, resulty, angle_segment);
        (*resultx) += lx1;
        (*resulty) += ly1;
    }
}

int64_t math_pointdisttosegment(
        int64_t px, int64_t py, int64_t lx1, int64_t ly1,
        int64_t lx2, int64_t ly2
        ) {
    int64_t wallx, wally;
    math_nearestpointonsegment(
        px, py, lx1, ly1, lx2, ly2, &wallx, &wally
    );
    return math_veclen2di((px - wallx), (py - wally));
}


int math_lineintersect2df(
        double l1x1, double l1y1, double l1x2, double l1y2,
        double l2x1, double l2y1, double l2x2, double l2y2,
        double *ix, double *iy) {
    // If the two line segments intersect,
    // returns 1 and sets ix, iy coordinates of intersection.
    // Otherwise, returns 0.

    // Based on https://github.com/psalaets/line-intersect,
    // also see 3RDPARTYCREDITS.md:
    double dval = ((l2y2 - l2y1) * (l1x2 - l1x1)) -
        ((l2x2 - l2x1) * (l1y2 - l1y1));
    double valA = ((l2x2 - l2x1) * (l1y1 - l2y1)) -
        ((l2y2 - l2y1) * (l1x1 - l2x1));
    double valB = ((l1x2 - l1x1) * (l1y1 - l2y1)) -
        ((l1y2 - l1y1) * (l1x1 - l2x1));
    if (dval == 0) {
        return 0;
    }
    double slopeA = valA / (double)dval;
    double slopeB = valB / (double)dval;
    if (slopeA >= 0 && slopeA <= 1 &&
            slopeB >= 0 && slopeB <= 1) {
        *ix = (l1x1 + (slopeA * (l1x2 - l1x1)));
        *iy = (l1y1 + (slopeA * (l1y2 - l1y1)));
        return 1;
    }
    return 0;
}

static void __attribute__((constructor))
        _test_pointalmostonline() {
    int result = math_pointalmostonline2di(
        -10, 0, 10, 1,
        0, 0, 1
    );
    assert(result != 0);
    result = math_pointalmostonline2di(
        -10, 0, 10, 1,
        0, -2, 1
    );
    assert(result == 0);
    result = math_pointalmostonline2di(
        -10, -10, 10, 10,
        0, 1, 1
    );
    assert(result != 0);
    result = math_pointalmostonline2di(
        -10, -10, 10, 10,
        0, 0, 1
    );
    assert(result != 0);
}

int math_checkpolyccw2di(int corners, int64_t *x, int64_t *y) {
    int64_t sum = 0;
    if (corners < 3)
        return 0;
    int inext = 1;
    int i = 0;
    while (i < corners) {
        sum += (x[inext] - x[i]) * (y[inext] + y[i]);
        inext++;
        if (inext >= corners) inext = 0;
        i++;
    }
    return (sum > 0);
}

HOTSPOT int math_pointalmostonline2di(
        int64_t lx1, int64_t ly1, int64_t lx2, int64_t ly2,
        int64_t px, int64_t py, int range
        ) {
    if (llabs(ly2 - ly1) > llabs(lx2 - lx1)) {
        if (unlikely((py >= ly2 && py <= ly1) ||
                 (py >= ly1 && py <= ly2))) {
            int64_t expected_x = -1;
            expected_x = (
                lx1 + (lx2 - lx1) *
                    (py - ly1) / (ly2 - ly1)
            );
            if (llabs(expected_x - px) <= range)
                return 1;
        }
    } else {
        if (llabs(lx2 - lx1) == 0)
            return 0;
        if (unlikely((px >= lx2 && px <= lx1) ||
                 (px >= lx1 && px <= lx2))) {
            int64_t expected_y = -1;
            expected_y = (
                ly1 + (ly2 - ly1) *
                    (px - lx1) / (lx2 - lx1)
            );
            if (llabs(expected_y - py) <= range)
                return 1;
        }
    }
    return 0;
}

HOTSPOT int math_lineintersect2di(
        int64_t l1x1, int64_t l1y1, int64_t l1x2, int64_t l1y2,
        int64_t l2x1, int64_t l2y1, int64_t l2x2, int64_t l2y2,
        int64_t *ix, int64_t *iy
        ) {
    // If the two line segments intersect,
    // returns 1 and sets ix, iy coordinates of intersection.
    // Otherwise, returns 0.

    // Based on https://github.com/psalaets/line-intersect,
    // also see 3RDPARTYCREDITS.md:
    int64_t dval = ((l2y2 - l2y1) * (l1x2 - l1x1)) -
        ((l2x2 - l2x1) * (l1y2 - l1y1));
    int64_t valA = ((l2x2 - l2x1) * (l1y1 - l2y1)) -
        ((l2y2 - l2y1) * (l1x1 - l2x1));
    int64_t valB = ((l1x2 - l1x1) * (l1y1 - l2y1)) -
        ((l1y2 - l1y1) * (l1x1 - l2x1));
    if (dval == 0) {
        return 0;
    }
    const int64_t precisionf = 4096;
    int64_t slopeA = valA * precisionf / dval;
    int64_t slopeB = valB * precisionf / dval;
    if (slopeA >= 0 && slopeA <= 1 * precisionf &&
            slopeB >= 0 && slopeB <= 1 * precisionf) {
        *ix = (l1x1 + (slopeA * (l1x2 - l1x1) / precisionf));
        *iy = (l1y1 + (slopeA * (l1y2 - l1y1) / precisionf));
        return 1;
    }
    return 0;
}

static __attribute__((constructor)) void _test_math_lineintersect2d() {
    // x,y_x,y 128,-128_0,128   AND  x,y_x,y 0,0_17363,9924
    #if !defined(NDEBUG)
    int64_t ix, iy;
    int result;

    result = math_lineintersect2di(
        0,448,448,-576,
        63,0,-3265,40960,
        &ix, &iy
    );
    assert(result != 0);
    assert(llabs(ix - 33) <= 5);
    assert(llabs(iy - 373) <= 5);
    result = math_lineintersect2di(
        448,-576,448,-896,
        0,0,22144,-34560,
        &ix, &iy
    );
    assert(result != 0);
    assert(llabs(ix - 448) <= 5);
    assert(llabs(iy - (-699)) <= 5);
    result = math_lineintersect2di(
        128,-128,0,128,
        0,0,17363,9924,
        &ix, &iy
    );
    assert(result != 0);
    #endif
}

/*int math_polyintersect2df(
        double lx1, double ly1, double lx2, double ly2,
        int corner_count, const double *cx, const double *cy,
        int *iwall, double *ix, double *iy
        ) {
    // If intersection found, returns 1 and sets
    // iwall to intersected section number, and
    // ix/iy to the intersection point.
    // Otherwise, returns 0.

    int contains1 = math_polycontains2df(
        lx1, ly1, corner_count, cx, cy
    );
    int contains2 = math_polycontains2df(
        lx2, ly2, corner_count, cx, cy
    );
    if (contains1 == contains2) {
        return 0;
    }
    if (!contains1) {
        double tx = lx1;
        double ty = ly1;
        lx1 = lx2;
        ly1 = ly2;
        lx2 = tx;
        ly2 = ty;
        contains1 = 1;
        contains2 = 0;
    }
    int i = 0;
    int imax = corner_count;
    int iprev = imax - 1;
    while (i < imax) {
        double wix, wiy;
        if (math_lineintersect2df(
                cx[iprev],
                cy[iprev],
                cx[i],
                cy[i],
                lx1, ly2, lx2, ly2,
                &wix, &wiy
                )) {
            *iwall = iprev;
            *ix = wix;
            *iy = wiy;
            return 1;
        }
        iprev = i;
        i++;
    }
    return 0;
}*/

int math_polycontains2di(int64_t px, int64_t py,
        int corner_count, const int64_t *cx, const int64_t *cy) {
    // Based on an algorithm by Randolph Franklin.
    const int imax = corner_count;
    int i = 0;
    int j = imax - 1;
    int c = 0;
    while (i < imax) {
        int cond1 = ((cy[i] > py) !=
            (cy[j] > py));
        int cond2 = 0;
        if (cy[j] - cy[i] != 0)  // Dividing by this won't be infinity
            cond2 = (px < (cx[j] - cx[i]) *
                (py - cy[i]) /
                (cy[j] - cy[i]) + cx[i]);
        if (cond1 && cond2) {
            c = !c;
        }
        j = i;
        i++;
    }
    return c;
}

static __attribute__((constructor)) void _test_math_polycontains2d() {
    double corner_x_f[3];
    double corner_y_f[3];
    int64_t corner_x[3];
    int64_t corner_y[3];
    const int ONE_METER_IN_UNITS_c = 128;
    corner_x_f[0] = -ONE_METER_IN_UNITS_c;
    corner_y_f[0] = -ONE_METER_IN_UNITS_c;
    corner_x_f[1] = ONE_METER_IN_UNITS_c;
    corner_y_f[1] = -ONE_METER_IN_UNITS_c;
    corner_x_f[2] = 0;
    corner_y_f[2] = ONE_METER_IN_UNITS_c;
    int result = 1; /*math_polycontains2df(
        0, 0, 3, corner_x_f, corner_y_f
    );*/
    assert(result != 0);
    corner_x[0] = -ONE_METER_IN_UNITS_c;
    corner_y[0] = -ONE_METER_IN_UNITS_c;
    corner_x[1] = ONE_METER_IN_UNITS_c;
    corner_y[1] = -ONE_METER_IN_UNITS_c;
    corner_x[2] = 0;
    corner_y[2] = ONE_METER_IN_UNITS_c;
    result = math_polycontains2di(0, 0, 3, corner_x, corner_y);
    assert(result != 0);
    corner_x[0] = -10240;
    corner_y[0] = -10240;
    corner_x[1] = 10240;
    corner_y[1] = -10240;
    corner_x[2] = 0;
    corner_y[2] = 10240;
    result = math_polycontains2di(
        22460, -61540, 3, corner_x, corner_y
    );
    assert(!result);
}

int math_vecsopposite2di(
        int64_t v1x, int64_t v1y, int64_t v2x, int64_t v2y
        ) {
    int64_t result = (v1x * v2x + v1y * v2y);
    return (result < 0);
}

static __attribute__((constructor))
        void _test_math_vecsopposite2di() {
    int result;
    result = math_vecsopposite2di(
        1, 0, -1, -1
    );
    assert(result != 0);
    result = math_vecsopposite2di(
        1, 0, 1, -1
    );
    assert(result == 0);
}

int math_polyintersect2di(
        int64_t lx1, int64_t ly1, int64_t lx2, int64_t ly2,
        int corner_count, const int64_t *cx, const int64_t *cy,
        int *iwall, int64_t *ix, int64_t *iy
        ) {
    return math_polyintersect2di_ex(
        lx1, ly1, lx2, ly2, corner_count, cx, cy, -1, 0,
        iwall, ix, iy
    );
}


int math_polyintersect2di_ex(
        int64_t lx1, int64_t ly1, int64_t lx2, int64_t ly2,
        int corner_count, const int64_t *cx, const int64_t *cy,
        int passable_wall_id, int loosefit,
        int *iwall, int64_t *ix, int64_t *iy
        ) {
    // If intersection found, returns 1 and sets
    // iwall to intersected section number, and
    // ix/iy to the intersection point.
    // Otherwise, returns 0.
    // Convex/concave polygons are both supported.

    if (corner_count < 3) return 0;
    int contains1 = math_polycontains2di(
        lx1, ly1, corner_count, cx, cy
    );
    int contains2 = math_polycontains2di(
        lx2, ly2, corner_count, cx, cy
    );
    if (unlikely(contains1 && contains2 && corner_count == 3))
        return 0;
    if (!contains1 && contains2) {
        int64_t tx = lx1;
        int64_t ty = ly1;
        lx1 = lx2;
        ly1 = ly2;
        lx2 = tx;
        ly2 = ty;
        contains1 = 1;
        contains2 = 0;
    }

    int bestwall = -1;
    int64_t bestx, besty;
    int64_t bestdist = -1;

    // Find the closest intersection:
    int i = 0;
    int imax = corner_count - 1;
    int iprev = imax;
    while (i <= imax) {
        if (passable_wall_id >= 0 &&
                iprev == passable_wall_id) {
            iprev = i;
            i++;
            continue;
        }
        int64_t wix, wiy;
        /*printf("checking %" PRId64 ",%" PRId64 " "
            "to %" PRId64 ",%" PRId64 "  VS  "
            "%" PRId64 ",%" PRId64 " to "
            "%" PRId64 ",%" PRId64 "\n",
            cx[iprev], cy[iprev],
            cx[i], cy[i],
            lx1, ly1, lx2, ly2);*/
        if (math_lineintersect2di(
                cx[iprev],
                cy[iprev],
                cx[i],
                cy[i],
                lx1, ly1, lx2, ly2,
                &wix, &wiy
                )) {
            if (likely(corner_count == 3 && contains1 && !contains2)) {
                // No other collision possible, since convex polygon.
                assert(iprev >= 0);
                *iwall = iprev;
                *ix = wix;
                *iy = wiy;
                return 1;
            }
            int64_t intersectdist = (
                math_veclen2di(lx1 - wix, ly1 - wiy)
            );
            if (bestwall < 0 || intersectdist < bestdist) {
                assert(iprev >= 0);
                bestwall = iprev;
                bestx = wix;
                besty = wiy;
                bestdist = intersectdist;
            }
        }
        iprev = i;
        i++;
    }
    if (bestwall >= 0) {
        // We got a result!
        *iwall = bestwall;
        *ix = bestx;
        *iy = besty;
        return 1;
    }
    // Sometimes, precision issues let a ray slip through a corner.
    // We want to return a collision in that case:
    const int cornercheckrange = (
        (contains1 != contains2) ? 10 :
        (loosefit ? 5 : 1));
    i = 0;
    while (i <= imax) {
        if (i != passable_wall_id && math_pointalmostonline2di(
                lx1, ly1, lx2, ly2, cx[i], cy[i],
                cornercheckrange
                )) {
            assert(i >= 0);
            *iwall = i;
            *ix = cx[i];
            *iy = cy[i];
            return 1;
        }
        i++;
    }
    return 0;
}

static void __attribute__((constructor))
        _test_polyintersect() {
    // Line: 0,0 -> 26100,-27000
    // Polygon: 128,-640 640,-640 384,-1920

    int64_t lx1 = 0;
    int64_t ly1 = 0;
    int64_t lx2 = 26100;
    int64_t ly2 = -27000;
    int64_t cx[] = {128, 640, 384};
    int64_t cy[] = {-640, -640, -1920};

    int iwall;
    int64_t ix, iy;
    int result = math_polyintersect2di_ex(
        lx1, ly1, lx2, ly2, 3, cx, cy, -1, 0,
        &iwall, &ix, &iy
    );
    assert(result != 0);
    assert(iwall == 0);
    assert(llabs(ix - 618) <= 10);
    assert(llabs(iy - (-640)) <= 10);
}
