// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_MATH2D_H_
#define RFS2_MATH2D_H_

#include "compileconfig.h"

#include <math.h>
#include <stdint.h>

#include "mathdefs.h"


static inline double math_angle2df(double x, double y) {
    // Angles: (1.0, 0.0) returns 0 degrees angle,
    // CCW rotation increases angle.
    return -((atan2(y, x) / M_PI) * 180.0);
}

HOTSPOT static inline int32_t math_angle2di(int64_t x, int64_t y) {
    // Angles: (1.0, 0.0) returns 0 degrees angle,
    // CCW rotation increases angle.
    return (int32_t)round((-(long double)((
        atan2l((long double)y, (long double)x) / M_PI)
    * 180.0)) * ANGLE_SCALAR);
}

static inline void math_rotate2df(
        double *x, double *y, double rot
        ) {
    // Axis: x right, y down, rot+ rotates CCW
    rot = (-rot / 180.0) * M_PI;
    double newy = (*y) * cos(rot) + (*x) * sin(rot);
    double newx = (*x) * cos(rot) - (*y) * sin(rot);
    *x = newx;
    *y = newy;
}

HOTSPOT static inline void math_rotate2di(
        int64_t *x, int64_t *y, int32_t rot
        ) {
    // Axis: x right, y down, rot+ rotates CCW
    double rotf = ((double)rot) / (double)ANGLE_SCALAR;
    rotf = (-rotf / 180.0) * M_PI;
    long double newy = ((long double)*y) * cosl(rotf) +
        ((long double)*x) * sinl(rotf);
    long double newx = ((long double)*x) * cosl(rotf) -
        ((long double)*y) * sinl(rotf);
    *x = (int64_t)newx;
    *y = (int64_t)newy;
}

HOTSPOT static inline int64_t math_veclen2di(int64_t x, int64_t y) {
    int64_t result = roundl(sqrtl(
        (long double)(x * x + y * y)
    ));
    return result;
}

static inline double math_fixanglef(double deg) {
    if (unlikely(deg > 180))
        deg = fmod((deg - 180.0), 360.0) - 180.0;
    else if (unlikely(deg < -180))
        deg = -(fmod(((-deg) - 180.0), 360.0) - 180.0);
    return deg;
}

HOTSPOT static inline int32_t math_fixanglei(int32_t deg) {
    if (unlikely(deg > 180 * ANGLE_SCALAR))
        deg = ((deg - 180 * ANGLE_SCALAR) % (360 * ANGLE_SCALAR)) -
            180 * ANGLE_SCALAR;
    else if (unlikely(deg < -180 * ANGLE_SCALAR))
        deg = -((((-deg) - 180 * ANGLE_SCALAR) %
            (360 * ANGLE_SCALAR)) - 180 * ANGLE_SCALAR);
    return deg;
}

int math_checkpolyccw2di(int corners, int64_t *x, int64_t *y);

double math_angle2df(double x, double y);

int32_t math_angle2di(int64_t x, int64_t y);

int64_t math_veclen2di(int64_t x, int64_t y);

void math_rotate2df(double *x, double *y, double rot);

void math_rotate2di(int64_t *x, int64_t *y, int32_t rot);

void math_nearestpointonsegment(
    int64_t nearesttox, int64_t nearesttoy,
    int64_t lx1, int64_t ly1, int64_t lx2, int64_t ly2,
    int64_t *resultx, int64_t *resulty
);

int64_t math_pointdisttosegment(
    int64_t px, int64_t py, int64_t lx1, int64_t ly1,
    int64_t lx2, int64_t ly2
);

int math_lineintersect2df(
    double l1x1, double l1y1, double l1x2, double l1y2,
    double l2x1, double l2y1, double l2x2, double l2y2,
    double *ix, double *iy
);

static int imin(int i1, int i2) {
    if (i1 < i2)
        return i1;
    return i2;
}

static int imax(int i1, int i2) {
    if (i1 > i2)
        return i1;
    return i2;
}

HOTSPOT int math_lineintersect2di(
    int64_t l1x1, int64_t l1y1, int64_t l1x2, int64_t l1y2,
    int64_t l2x1, int64_t l2y1, int64_t l2x2, int64_t l2y2,
    int64_t *ix, int64_t *iy
);

/*int math_polyintersect2df(
    double lx1, double ly1, double lx2, double ly2,
    int corner_count, const double *cx, const double *cy,
    int *iwall, double *ix, double *iy
);*/

HOTSPOT int math_pointalmostonline2di(
    int64_t lx1, int64_t ly1, int64_t lx2, int64_t ly2,
    int64_t px, int64_t py, int range
);

int math_polyintersect2di(
    int64_t lx1, int64_t ly1, int64_t lx2, int64_t ly2,
    int corner_count, const int64_t *cx, const int64_t *cy,
    int *iwall, int64_t *ix, int64_t *iy
);

int math_polyintersect2di_ex(
    int64_t lx1, int64_t ly1, int64_t lx2, int64_t ly2,
    int corner_count, const int64_t *cx, const int64_t *cy,
    int ignorewall, int loosefit,
    int *iwall, int64_t *ix, int64_t *iy
);

int math_polycontains2di(
    int64_t px, int64_t py,
    int corner_count, const int64_t *cx, const int64_t *cy
);

int math_vecsopposite2di(
    int64_t v1x, int64_t v1y, int64_t v2x, int64_t v2y
);

#endif  // RFS2_MATH2D_H_

