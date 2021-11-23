// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_IMGALTER_H_
#define RFS2_IMGALTER_H_

#include "compileconfig.h"

#include <stdint.h>


void imgalter_32BitRawTo16Raw(
    const char *data, size_t datalen,
    int withalpha, char *output
);

void imgalter_16BitRawTo32Raw(
    const char *data, size_t datalen,
    int withalpha, char *output
);

#endif  // RFS2_IMGALTER_H_

