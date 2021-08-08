// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_MATH3D_H_
#define RFS2_MATH3D_H_

#include <math.h>

/*double *vec3_rotate_quat(
    double *result, double *vec, double *quat
);

double *quat_from_euler(
    double *result, double *eulerangles
);*/

int64_t math_veclen3di(
    int64_t x, int64_t y, int64_t z
);

void math_polynormal3df(
    const double pos1[3], const double pos2[3],
    const double pos3[3],
    double result[3]
);

static int isNaN(double f) {
    return (isnan(f) || (f != f));
}

#endif  // RFS2_MATH3D_H_
