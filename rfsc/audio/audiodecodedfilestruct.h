// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details


#ifndef RFS2_AUDIODECODEDFILESTRUCT_H_
#define RFS2_AUDIODECODEDFILESTRUCT_H_

#include "compileconfig.h"

#include <stdint.h>


typedef struct h3daudiodecodedfile {
    int samplerate, channels;
    int total_frames;
    char *audiobuf;
    int audiobuf_frames_alloc;
    int playback_refcount;
} h3daudiodecodedfile;


#endif  // RFS2_AUDIODECODEDFILESTRUCT_H_
