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
    free(mov);
    movobj->objdata = NULL;
}


movable *movable_Create() {
    movable *mov = malloc(sizeof(*mov));
    if (!mov)
        return NULL;
    memset(mov, 0, sizeof(*mov));
    roomobj *obj = roomobj_Create(
        roomobj_GetNewId(), ROOMOBJ_MOVABLE,
        mov, &_movable_DestroyCallback
    );
    if (!obj) {
        free(mov);
        return NULL;
    }
    mov->obj = obj;
    return mov;
}

