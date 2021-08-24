// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_ROOMCAMCACHE_H_
#define RFS2_ROOMCAMCACHE_H_

#include "compileconfig.h"

#include <stdint.h>

#include "roomcamrenderstatistics.h"

typedef struct roomcamcache {
    int cachedangle, cachedvangle, cachedfov, cachedw, cachedh;

    int32_t cachedfovh, cachedfovv;
    int64_t planedist, planeheight, planezshift, planew, planeh;
    int64_t unrotatedplanevecs_left_x;
    int64_t unrotatedplanevecs_left_y;
    int64_t unrotatedplanevecs_right_x;
    int64_t unrotatedplanevecs_right_y;
    int64_t *planevecs_len;
    int32_t planevecs_alloc, vertivecs_alloc;

    int64_t *vertivecs_x, *vertivecs_z;
    int64_t *rotatedplanevecs_x, *rotatedplanevecs_y;
    int32_t rotatedalloc;

    renderstatistics stats;
} roomcamcache;


#endif  // RFS2_ROOMCAMCACHE_H_
