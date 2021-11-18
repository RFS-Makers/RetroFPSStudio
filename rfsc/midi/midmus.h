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


enum midnusmodifier {
    MIDMUSMODIFY_UNKNOWN = 0,
    MIDMUSMODIFY_VOL = 1,
    MIDMUSMODIFY_PAN = 2
};

typedef struct midmusmodify {
    uint8_t type;
    uint8_t value;
    int64_t munitoffset, frameoffsetinblock;
    int64_t nextsametypemunitoffset,
        nextsametypeframeoffset;
} midmusmodify;

typedef struct midmusnote {
    int32_t frameoffsetinblock, framelen;
    uint8_t volume, pan, key;
    int64_t munitoffset;
    int32_t munitlen;
    int32_t overlapindex;
} midmusnote;

typedef struct midmusblock {
    midmustrack *parent;
    int no;
    int32_t measurestart, measurelen;
    int32_t frameoffset, framelen;
    int32_t notecount, maxoverlappingnotes;
    uint8_t volume;
    midmusnote *note;

    int32_t modifiercount;
    midmusmodify *modifier;
} midmusblock;

typedef struct midmusmeasure {
    double beatspermeasure;
    int32_t signaturenom, signaturediv;

    double bpm;
    int32_t framelen;
} midmusmeasure;

typedef struct midmustrack {
    int16_t instrument;
    uint8_t volume, pan;

    int32_t blockcount;
    midmusblock *block;
    int32_t maxoverlappingnotes;

    midmussong *parent;
} midmustrack;

typedef struct midmussong {
    int32_t trackcount;
    midmustrack *track;

    int32_t measurecount;
    midmusmeasure *measure;
} midmussong;


midmussong *midmussong_Load(
    const char *bytes, int64_t byteslen);

void midmussong_Free(midmussong *song);

void midmussong_UpdateBlockSamplePos(
    midmusblock *bl);

void midmussong_UpdateMeasureTiming(
    midmusmeasure *measure);

int midmussong_EnsureMeasureCount(
    midmussong *song, int32_t count);

uint64_t midmussong_GetFramesLength(midmussong *s);

double midmussong_GetSecondsLength(midmussong *s);

void midmussong_SetMeasureBPM(
    midmussong *song, int measure, double bpm);

void midmussong_SetMeasureTimeSig(
    midmussong *song, int measure,
    int32_t nom, int32_t div);

void midmussong_GetMeasureStartLen(
    midmussong *song, int measure,
    int64_t *out_framestart, int64_t *out_framelen);

const char *midmussong_InstrumentNoToName(int instrno);

const char *midmussong_DrumKeyToName(int keyno);

int32_t midmussong_FrameOffsetToMeasure(
    midmussong *song, int64_t frameoffset);

void midmussong_MeasureTimeSig(
    midmussong *song, int measure,
    int32_t *out_signom, int32_t *out_sigdiv,
    double *out_beatspermeasure);

#endif  // RFS2_MIDMUS_H_
