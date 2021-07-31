// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#ifndef RFS2_VFSPARTIALFILEIO_H_
#define RFS2_VFSPARTIALFILEIO_H_

#include <stdint.h>
#include <stdio.h>

void *_PhysFS_Io_partialFileReadOnlyStruct(
    FILE *f, uint64_t start, uint64_t len
);

#endif  // RFS2_VFSPARTIALFILEIO_H_
