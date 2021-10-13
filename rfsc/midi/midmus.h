// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_MIDMUS_H_
#define RFS2_MIDMUS_H_

#include "compileconfig.h"

#include <stdint.h>


typedef struct midmustrack midmustrack;
typedef struct midmussong midmussong;

#define MIDMUS_PANRANGE 32
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
    int32_t sampleoffsetinblock, samplelen;
    uint8_t volume, pan;
    int32_t munitoffset, munitlen;
    int32_t modifiercount;
    midmusnotemodify *modifiers;
} midmusnote;

typedef struct midmusblock {
    midmustrack *parent;
    int no;
    int32_t measurestart, measurelen;
    int32_t sampleoffset, samplelen;
    int32_t notecount;
    midmusnote *note;
} midmusblock;

typedef struct midmusmeasure {
    double beatpermeasure;
    int32_t signaturenom, signaturediv;

    double bpm;
    int32_t samplelen;
} midmusmeasure;

typedef struct midmustrack {
    int32_t instrument;
    uint8_t volume, pan;

    int32_t blockcount;
    midmusblock *block;

    midmussong *parent;
} midmustrack;

typedef struct midmussong {
    int32_t trackcount;
    midmustrack *track;

    int32_t measurecount;
    midmusmeasure *measure;
} midmussong;


midmussong *midmussong_Load(const char *bytes, int byteslen);

void midmussong_Free(midmussong *song);

void midmussong_UpdateBlockSamplePos(
    midmusblock *bl);

void midmussong_UpdateMeasureTiming(
    midmusmeasure *measure);

int midmussong_EnsureMeasureCount(
    midmussong *song, int count);

#endif  // RFS2_MIDMUS_H_
