// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "gamma.h"


void gamma_GenerateMap(uint8_t gamma, uint8_t *map) {
    int i = 0;
    while (i < 15) {
        double targetv = (double)i;
        double shiftstrengthlinear = (
            targetv > 7.0 ?
            fmax(0, fmin(1, (7.0 - (targetv - 8.0)) / 7.0)) :
            fmin(1.0, targetv / 7.0)
        );
        double shiftstrength = 0.2 * sqrt(
            fmin(1.0, 2 * shiftstrengthlinear)
        ) + 0.8 * shiftstrengthlinear;
        double shift = (0.0 +
            ((((double)gamma) - 8.0) / 8.0) * 4.6);
        double shiftactual = shift * shiftstrength;
        targetv = round(fmin(15, fmax(0, targetv + shiftactual)));
        /*printf("i %d shiftstrengthlinear %f shiftstrength %f "
            "shift %f shiftactual %f targetv %f\n",
            i, shiftstrengthlinear, shiftstrength, shift,
            shiftactual, targetv);*/
        map[i] = targetv;
        i++;
    }
}
