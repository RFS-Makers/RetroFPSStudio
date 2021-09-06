// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_MIDMUS_H_
#define RFS2_MIDMUS_H_

#include "compileconfig.h"

#include <stdint.h>


typedef struct midmustrack midmustrack;

#define MIDI_TIMERSAMPLERES 256
#define MIDI_SAMPLERATE 48000
#define MIDI_MEASUREUNITS (4096 * 10)


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
    int64_t sampleoffset, samplelen;
    uint8_t volume, pan;
    int munitoffset, munitlen;
    int modifycount;
    
} midmusnote;

typedef struct midmusblock {
    midmustrack *parent;
    int measurestart, measurelen, samplelen;
    int notecount;
    midmusnote *note;
} midmusblock;

typedef struct midmustrack {
    int instrument;
    uint8_t volume, pan;
    int blockcount;
    midmusblock *block;
} midmus_track;

typedef struct midmussong {
    int trackcount;
    midmustrack *track;
} midmussong;


#endif  // RFS2_MIDMUS_H_
