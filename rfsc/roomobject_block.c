// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details


#include "compileconfig.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "room.h"
#include "roomobject.h"
#include "roomobject_block.h"


static void _block_DestroyCallback(roomobj *obj) {
    block *bl = ((block *)obj->objdata);
    if (!bl)
        return;
    roomobj_UnmakeTexRef(bl->wall_tex.tex);
    roomobj_UnmakeTexRef(bl->topbottom_tex.tex);
    free(bl);
    obj->objdata = NULL;
}


block *block_Create(int corners,
        int64_t *corner_x, int64_t *corner_y) {
    uint64_t id = roomobj_GetNewId();
    return block_CreateById(
        id, corners, corner_x, corner_y);
}


block *block_CreateById(uint64_t id, int corners,
        int64_t *corner_x, int64_t *corner_y) {
    if (id <= 0) return NULL;
    if (corners < 3 || corners > ROOM_MAX_CORNERS)
        return NULL;
    if (!room_VerifyBasicGeometryByPolygon(
            corners, corner_x, corner_y, 1,
            ADDITIVE_BLOCK_MAX_EXTENTS))
        return NULL;
    block *bl = malloc(sizeof(*bl));
    if (!bl)
        return NULL;
    memset(bl, 0, sizeof(*bl));
    roomobj *obj = roomobj_Create(
        id, ROOMOBJ_BLOCK,
        bl, &_block_DestroyCallback
    );
    if (!obj) {
        free(bl);
        return NULL;
    }
    bl->obj = obj;
    bl->corners = corners;
    memcpy(bl->corner_x, corner_x,
        sizeof(*corner_x) * bl->corners);
    memcpy(bl->corner_y, corner_y,
        sizeof(*corner_y) * bl->corners);
    bl->wall_tex.tex_scaleintx = TEX_FULLSCALE_INT;
    bl->wall_tex.tex_scaleinty = TEX_FULLSCALE_INT;
    bl->topbottom_tex.tex_scaleintx = TEX_FULLSCALE_INT;
    bl->topbottom_tex.tex_scaleinty = TEX_FULLSCALE_INT;
    block_RecomputeNormals(bl);
    return bl;
}

void block_RecomputeNormals(block *bl) {
    assert(bl != NULL && bl->corners >= 3);
    int inext = 1;
    int i = 0;
    while (i < bl->corners) {
        int64_t tx = (bl->corner_x[inext] - bl->corner_x[i]);
        int64_t ty = (bl->corner_y[inext] - bl->corner_y[i]);
        bl->normal_x[i] = ty;
        bl->normal_y[i] = -tx;
        i++;
        inext++;
        if (inext >= bl->corners) inext = 0;
    }
}
