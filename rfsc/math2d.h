// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#ifndef RFS2_MATH2D_H_
#define RFS2_MATH2D_H_

#include "compileconfig.h"

#include <stdint.h>


double math_fixanglef(double deg);

int32_t math_fixanglei(int32_t deg);  // input is deg * ANGLE_SCALAR

double math_angle2df(double x, double y);

int32_t math_angle2di(int64_t x, int64_t y);

int64_t math_veclen(int64_t x, int64_t y);

void math_rotate2df(double *x, double *y, double rot);

void math_rotate2di(int64_t *x, int64_t *y, int32_t rot);

int math_lineintersect2df(
    double l1x1, double l1y1, double l1x2, double l1y2,
    double l2x1, double l2y1, double l2x2, double l2y2,
    double *ix, double *iy
);

int math_lineintersect2di(
    int64_t l1x1, int64_t l1y1, int64_t l1x2, int64_t l1y2,
    int64_t l2x1, int64_t l2y1, int64_t l2x2, int64_t l2y2,
    int64_t *ix, int64_t *iy
);

/*int math_polyintersect2df(
    double lx1, double ly1, double lx2, double ly2,
    int corner_count, const double *cx, const double *cy,
    int *iwall, double *ix, double *iy
);*/

int math_polyintersect2di(
    int64_t lx1, int64_t ly1, int64_t lx2, int64_t ly2,
    int corner_count, const int64_t *cx, const int64_t *cy,
    int *iwall, int64_t *ix, int64_t *iy
);

int math_polycontains2di(
    int64_t px, int64_t py,
    int corner_count, const int64_t *cx, const int64_t *cy
);

#endif  // RFS2_MATH2D_H_

