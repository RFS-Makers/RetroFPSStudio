// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "secrandom.h"
#include "uuid.h"

int uuid_gen(char *buf) {
    assert(UUIDLEN == 39);
    uint8_t randbytes[16];
    if (!secrandom_GetBytes((char *)randbytes, 16)) {
        return 0;
    }
    char nodashid[32];
    int i = 0;
    while ((unsigned int)i <
            sizeof(nodashid) / sizeof(*nodashid)) {
        char bytehex[3] = {0};
        snprintf(bytehex, 3, "%x", randbytes[i / 2]);
        if (strlen(bytehex) == 1) {
            nodashid[i] = '0';
            nodashid[i + 1] = toupper(bytehex[0]);
        } else {
            nodashid[i] = toupper(bytehex[0]);
            nodashid[i + 1] = toupper(bytehex[1]);
        }
        i += 2;
    }
    int k = 0;
    i = 0;
    while (i < UUIDLEN) {
        if ((i + 1) % 5 == 0) {
            buf[i] = '-';
            i++;
            continue;
        }
        assert(k < 32);
        buf[i] = nodashid[k];
        i++;
        k++;
    }
    buf[UUIDLEN] = '\0';
    return 1;
}
