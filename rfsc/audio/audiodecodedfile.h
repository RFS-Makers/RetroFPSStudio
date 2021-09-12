// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_AUDIODECODEDFILE_H_
#define RFS2_AUDIODECODEDFILE_H_

#include "compileconfig.h"

#include <stdint.h>


typedef struct h3daudiodecodedfile h3daudiodecodedfile;


h3daudiodecodedfile *audiodecodedfile_LoadEx(
    const char *path, int vfsflags,
    int override_samplerate,
    double may_stop_early_past_seconds,
    char **error
);

void audiodecodedfile_Free(h3daudiodecodedfile *f);

char *audiodecodedfile_GetBuffer(h3daudiodecodedfile *f);

h3daudiodecodedfile *audiodecodedfile_Load(
    const char *path, int vfsflags, char **error
);

int audiodecodedfile_GetFrameCount(h3daudiodecodedfile *f);

int audiodecodedfile_GetSamplerate(h3daudiodecodedfile *f);

double audiodecodedfile_GetLengthInSeconds(
    h3daudiodecodedfile *f);

#endif  // #ifndef RFS2_AUDIODECODEDFILE_H
