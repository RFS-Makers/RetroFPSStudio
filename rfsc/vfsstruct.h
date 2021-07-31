// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#ifndef RFS2_VFSSTRUCT_H_
#define RFS2_VFSSTRUCT_H_

#include <physfs.h>
#include <stdint.h>
#include <stdio.h>

typedef struct VFSFILE {
    uint8_t via_physfs, is_limited;
    union {
        PHYSFS_File *physfshandle;
        FILE *diskhandle;
    };
    int64_t size;
    uint64_t offset;
    uint64_t limit_start, limit_len;
    char *mode, *path;
} VFSFILE;

#endif  // RFS2_VFSSTRUCT_H_
