// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_LUAHELPERS_H_
#define RFS2_LUAHELPERS_H_

#include "compileconfig.h"

#include <assert.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "threading.h"


static int _unprotected_returnstringfromptr(lua_State *l) {
    const char *p = (const char *)lua_touserdata(l, 1);
    lua_pushstring(l, p);
    return 1;
}


static int lua_pushstring_s(lua_State *l, const char *s) {
    lua_pushcfunction(l, &_unprotected_returnstringfromptr);
    lua_pushlightuserdata(l, (char *)s);
    return (lua_pcall(l, 1, 1, 0) == LUA_OK);
}


static int _unprotected_returnnewtable(lua_State *l) {
    lua_newtable(l);
    return 1;
}


static int lua_newtable_s(lua_State *l) {
    lua_pushcfunction(l, &_unprotected_returnnewtable);
    return (lua_pcall(l, 0, 1, 0) == LUA_OK);
}


static int _unprotected_returnlstringfromptr(lua_State *l) {
    const char *p = (const char *)lua_touserdata(l, 1);
    int64_t size = lua_tointeger(l, 2);
    lua_pushlstring(l, p, size);
    return 1;
}


static int lua_pushlstring_s(
        lua_State *l, const char *s, size_t len
        ) {
    lua_pushcfunction(l, &_unprotected_returnlstringfromptr);
    lua_pushlightuserdata(l, (void *)s);
    lua_pushinteger(l, (int64_t)len);
    return (lua_pcall(l, 2, 1, 0) == LUA_OK);
}


static int _unprotected_createnewuserdata(lua_State *l) {
    void **udataptr_ref = (void **)lua_touserdata(l, 1);
    size_t blocksize = lua_tointeger(l, 2);
    *udataptr_ref = lua_newuserdatauv(l, blocksize, 0);
    return 1;
}


static void *lua_newuserdata_s(lua_State *l, size_t size) {
    void *udataptr = NULL;
    lua_pushcfunction(l, &_unprotected_createnewuserdata);
    lua_pushlightuserdata(l, (void *)&udataptr);
    lua_pushinteger(l, size);
    if (lua_pcall(l, 1, 1, 0) == LUA_OK) {
        return udataptr;
    }
    return NULL;
}

#endif  // RFS2_LUAHELPERS_H_

