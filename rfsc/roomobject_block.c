// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details


#include "compileconfig.h"

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
    free(bl);
    obj->objdata = NULL;
}


block *block_Create(int corners) {
    block *bl = malloc(sizeof(*bl));
    if (!bl)
        return NULL;
    memset(bl, 0, sizeof(*bl));
    roomobj *obj = roomobj_Create(
        roomobj_GetNewId(), ROOMOBJ_BLOCK,
        bl, &_block_DestroyCallback
    );
    if (!obj) {
        free(bl);
        return NULL;
    }
    bl->obj = obj;
    return bl;
}

