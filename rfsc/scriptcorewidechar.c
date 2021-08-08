// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "scriptcore.h"
#include "scriptcoretime.h"
#include "scriptcorewidechar.h"
#include "widechar.h"


static int _widechar_utf8charlen(lua_State *l) {
    if (lua_gettop(l) < 1 ||
            lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected arg of type string");
        return lua_error(l);
    }
    size_t slen = 0;
    const char *s = lua_tolstring(l, 1, &slen);
    int64_t offset = 0;
    if (lua_gettop(l) >= 2 && lua_type(l, 2) == LUA_TNUMBER) {
        offset = lua_tointeger(l, 2) - 1;
    }
    if (!s) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    if (offset < 0 || offset >= (int64_t)slen) {
        lua_pushinteger(l, 0);
        return 0;
    }
    if (s[offset] == '\0') {
        lua_pushinteger(l, 1);
        return 0;
    }
    char *scopynullterminated = malloc(slen + 1);
    if (!scopynullterminated) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    memcpy(scopynullterminated, s + offset, slen - offset);
    scopynullterminated[slen - offset] = '\0';
    int64_t result = utf8_char_len((uint8_t*)scopynullterminated);
    free(scopynullterminated);
    lua_pushinteger(l, result);
    return 1;
}

static int _widechar_utf8cpcount(lua_State *l) {
    if (lua_gettop(l) < 1 ||
            lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected arg of type string");
        return lua_error(l);
    }
    size_t slen = 0;
    const char *s = lua_tolstring(l, 1, &slen);
    if (!s) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    char _scopybuf[512];
    int scopyneedsfree = 0;
    char *scopynullterminated = _scopybuf;
    if (slen + 1 > sizeof(_scopybuf)) {
        scopyneedsfree = 1;
        scopynullterminated = malloc(slen + 1);
        if (!scopynullterminated) {
            lua_pushstring(l, "out of memory");
            return lua_error(l);
        }
    }
    memcpy(scopynullterminated, s, slen);
    scopynullterminated[slen] = '\0';
    int64_t result = 0;
    int64_t offset = 0;
    while (offset < (int64_t)slen) {
        result++;
        int64_t len = utf8_char_len(
            (uint8_t*)scopynullterminated + offset
        );
        if (result > 0)
            offset += len;
        else
            offset++;
    }
    if (scopyneedsfree)
        free(scopynullterminated);
    lua_pushinteger(l, result);
    return 1;
}

void scriptcorewidechar_AddFunctions(lua_State *l) {
    lua_pushcfunction(l, _widechar_utf8charlen);
    lua_setglobal(l, "_widechar_utf8charlen");
    lua_pushcfunction(l, _widechar_utf8cpcount);
    lua_setglobal(l, "_widechar_utf8cpcount");
}
