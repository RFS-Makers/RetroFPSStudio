// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_SCRIPTCOREERROR_H_
#define RFS2_SCRIPTCOREERROR_H_

#include <lua.h>
#include <stdlib.h>
#include <string.h>


static int lua_pushtracebackfunc(lua_State *l) {
    lua_pushstring(l, "_debugtableref");
    lua_gettable(l, LUA_REGISTRYINDEX);
    if (lua_type(l, -1) != LUA_TTABLE) {
        fprintf(stderr,
            "rfsc/scriptcoreerror.h: warning: "
            "_debugtableref not set, "
            "cannot set traceback function\n");
        lua_pop(l, -1);
        lua_pushnil(l);
        return 0;
    }
    lua_pushstring(l, "traceback");
    lua_gettable(l, -2);
    lua_remove(l, -2);
    if (lua_type(l, -1) != LUA_TFUNCTION) {
        fprintf(stderr,
            "rfsc/scriptcoreerror.h: warning: "
            "_debugtableref.traceback is not "
            "of type function\n");
    }
    return 0;
}

#endif  // RFS2_SCRIPTCOREERROR_H_
