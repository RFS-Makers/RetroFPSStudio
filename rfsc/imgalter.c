// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#define STBI_NO_STDIO
#include <stb/stb_image.h>

#include "16bithelp.h"
#include "filesys.h"
#include "hash.h"
#include "imgalter.h"
#include "math2d.h"
#include "vfs.h"



void imgalter_32BitRawTo16Raw(
        const char *data, size_t datalen,
        int withalpha, char *output) {
    if (withalpha) {
        assert((datalen / 4) * 4 == datalen);
        uint8_t *writep = (uint8_t *)output;
        const uint8_t *readp = (const uint8_t *)data;
        uint8_t *endwritep = writep + (datalen / 4) * 2;
        while (writep != endwritep) {
            assert(writep < endwritep);
            uint16_t new_r = ((*readp * 256) / 16) / 256;
            assert(new_r < 16);
            readp++;
            uint16_t new_g = ((*readp * 256) / 16) / 256;
            assert(new_g < 16);
            readp++;
            uint16_t new_b = ((*readp * 256) / 16) / 256;
            assert(new_b < 16);
            readp++;
            uint16_t new_a = ((*readp * 256) / 16) / 256;
            assert(new_a < 16);
            readp++;
            *writep = (new_g << (uint16_t)4) + new_b;
            writep++;
            *writep = (new_a << (uint16_t)4) + new_r;
            writep++;
        }
    } else {
        assert((datalen / 3) * 3 == datalen);
        uint8_t *writep = (uint8_t *)output;
        const uint8_t *readp = (const uint8_t *)data;
        uint8_t *endwritep = writep + (datalen / 3) * 2;
        while (writep != endwritep) {
            assert(writep < endwritep);
            uint16_t new_r = round(*readp / 17.0);
            assert(new_r < 16);
            readp++;
            uint16_t new_g = round(*readp / 17.0);
            assert(new_g < 16);
            readp++;
            uint16_t new_b = round(*readp / 17.0);
            assert(new_b < 16);
            readp++;
            *writep = (new_g << (uint16_t)4) + new_b;
            writep++;
            *writep = new_r;
            writep++;
        }
    }
}


void imgalter_16BitRawTo32Raw(
        const char *data, size_t datalen,
        int withalpha, char *output
        ) {
    assert((datalen / 2) * 2 == datalen);
    uint8_t *writep = (uint8_t *)output;
    const uint8_t *readp = (const uint8_t *)data;
    uint8_t *endwritep = writep + (datalen / 2) *
        (withalpha ? 4 : 3);
    while (writep != endwritep) {
        assert(writep < endwritep);
        uint8_t new_r = rightc(*readp) * (uint16_t)17;
        uint8_t new_g = leftc(*readp) * (uint16_t)17;
        readp++;
        uint8_t new_b = rightc(*readp) * (uint16_t)17;
        uint8_t new_a = ((!withalpha) ?
            (uint8_t)255 : leftc(*readp) * (uint16_t)17);
        readp++;
        *writep = new_r;
        writep++;
        *writep = new_g;
        writep++;
        *writep = new_b;
        writep++;
        if (withalpha) {
            *writep = new_a;
            writep++;
        }
    }
}
