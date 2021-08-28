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
#include "roomobject_movable.h"


static void _movable_DestroyCallback(roomobj *movobj) {
    movable *mov = ((movable *)movobj->objdata);
    if (!mov)
        return;
    roomobj_UnmakeTexRef(mov->sprite_ref);
    free(mov);
    movobj->objdata = NULL;
}


movable *movable_Create() {
    return movable_CreateWithId(
        roomobj_GetNewId()
    );
}


movable *movable_CreateWithId(uint64_t id) {
    if (id <= 0)
        return NULL;
    movable *mov = malloc(sizeof(*mov));
    if (!mov)
        return NULL;
    memset(mov, 0, sizeof(*mov));
    roomobj *obj = roomobj_Create(
        id, ROOMOBJ_MOVABLE,
        mov, &_movable_DestroyCallback
    );
    if (!obj) {
        free(mov);
        return NULL;
    }
    mov->obj = obj;
    mov->sprite_scaleintx = TEX_FULLSCALE_INT;
    mov->sprite_scaleinty = TEX_FULLSCALE_INT;
    return mov;
}
