// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_MIDMUS_H_
#define RFS2_MIDMUS_H_

#include "compileconfig.h"

#include <stdint.h>


typedef struct midmustrack midmustrack;

#define MIDMUS_PANRANGE 64
#define MIDMUS_TIMERSAMPLERES 256
#define MIDMUS_SAMPLERATE 48000
#define MIDMUS_MEASUREUNITS (4096 * 10)


enum midnusnotemodifier {
    MIDMUSMODIFY_UNKNOWN = 0,
    MIDMUSMODIFY_VOL = 1,
    MIDMUSMODIFY_PAN = 2
};

typedef struct midmusnotemodify {
    uint8_t type;
    uint8_t value;
} midmusnotemodify;

typedef struct midmusnote {
    int32_t sampleoffset, samplelen;
    uint8_t volume, pan;
    int32_t munitoffset, munitlen;
    int32_t modifiercount;
    midmusnotemodify *modifiers;
} midmusnote;

typedef struct midmusblock {
    midmustrack *parent;
    int32_t measurestart, measurelen, samplelen;
    int32_t notecount;
    midmusnote *note;
} midmusblock;

typedef struct midmustrack {
    int32_t instrument;
    uint8_t volume, pan;
    int32_t blockcount;
    midmusblock *block;
} midmus_track;

typedef struct midmussong {
    int32_t trackcount;
    midmustrack *track;
} midmussong;


midmussong *midmussong_Load(const char *bytes, int byteslen);

void midmussong_Free(midmussong *song);

#endif  // RFS2_MIDMUS_H_
