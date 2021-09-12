// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_AUDIODECODER_H_
#define RFS2_AUDIODECODER_H_

#include <stdint.h>

typedef struct h3daudiodecoder h3daudiodecoder;

h3daudiodecoder *audiodecoder_NewFromFile(
    const char *filepath, int vfsflags
);

int audiodecoder_GetSourceSampleRate(
    h3daudiodecoder *decoder
);

int audiodecoder_GetSourceChannels(
    h3daudiodecoder *decoder
);

int audiodecoder_SetResampleTo(
    h3daudiodecoder *decoder, int samplerate
);

int audiodecoder_Decode(
    h3daudiodecoder *decoder, char *output, int frames,
    int *out_haderror
);

void audiodecoder_ResetToStart(h3daudiodecoder *decoder);

void audiodecoder_Destroy(h3daudiodecoder *decoder);


#endif  // RFS2_AUDIODECODER_H_
