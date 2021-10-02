// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details


#include "compileconfig.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "graphics.h"
#include "room.h"
#include "roomobject.h"
#include "roomobject_movable.h"


static void _movable_DestroyCallback(roomobj *movobj) {
    movable *mov = ((movable *)movobj->objdata);
    if (!mov)
        return;
    roomobj_UnmakeTexRef(mov->sprite_tex.tex);
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
    mov->colscalar_hori = TEX_FULLSCALE_INT * 0.6;
    mov->colscalar_verti = TEX_FULLSCALE_INT;
    mov->sprite_tex.sprite_scaleintx = TEX_FULLSCALE_INT;
    mov->sprite_tex.sprite_scaleinty = TEX_FULLSCALE_INT;
    return mov;
}


movable *movable_CreateWithSingleSprite(const char *sprite) {
    return movable_CreateWithSingleSpriteAndId(
        sprite, roomobj_GetNewId());
}


movable *movable_CreateWithSingleSpriteAndId(
        const char *sprite, uint64_t sid) {
    movable *mov = movable_CreateWithId(sid);
    if (!mov) return NULL;
    mov->sprite_tex.tex = roomobj_MakeTexRef(sprite);
    if (!mov->sprite_tex.tex) {
        roomobj_Destroy(mov->obj);
        return NULL;
    }
    rfs2tex *t = roomobj_GetTexOfRef(
        mov->sprite_tex.tex);
    if (t && (t->w != FIXED_ROOMTEX_SIZE ||
            t->h != FIXED_ROOMTEX_SIZE) &&
            (t->w != FIXED_ROOMTEX_SIZE_2 ||
            t->h != FIXED_ROOMTEX_SIZE_2) &&
            (t->w != FIXED_SPRITETEX_EXTRASIZE ||
            t->h != FIXED_SPRITETEX_EXTRASIZE)) {
        roomobj_Destroy(mov->obj);
        return NULL;
    }
    return mov;
}
