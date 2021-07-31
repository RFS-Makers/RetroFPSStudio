// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#ifndef RFS2_ROOMSERIALIZE_H_
#define RFS2_ROOMSERIALIZE_H_

#include "compileconfig.h"

#include <lua.h>
#include <stdint.h>

#include "room.h"
#include "roomdefs.h"

int roomserialize_lua_DeserializeRoomGeometries(
    lua_State *l, roomlayer *lr, int tblindex, char **err
);

int roomserialize_lua_SetRoomProperties(
    lua_State *l, roomlayer *lr, int tblindex, char **err
);

#endif  // RFS2_ROOMSERIALIZE_H_
