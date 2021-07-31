// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#ifndef RFS2_MATH3D_H_
#define RFS2_MATH3D_H_

#include <math.h>

/*double *vec3_rotate_quat(
    double *result, double *vec, double *quat
);

double *quat_from_euler(
    double *result, double *eulerangles
);*/

void math_polygon3d_normal(
    const double pos1[3], const double pos2[3],
    const double pos3[3],
    double result[3]
);

static int isNaN(double f) {
    return (isnan(f) || (f != f));
}

#endif  // RFS2_MATH3D_H_
