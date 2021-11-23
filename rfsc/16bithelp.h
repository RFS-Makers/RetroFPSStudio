// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_16BITHELP_H_
#define RFS2_16BITHELP_H_

#include "compileconfig.h"

#include <stdint.h>


static inline uint16_t leftc(uint16_t v) {
    return (v & (uint16_t)0xF0) >> 4;
}

static inline uint16_t rightc(uint16_t v) {
    return (v & (uint16_t)0x0F);
}


#endif  // RFS2_16BITHELP_H_
