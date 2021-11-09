// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <ctype.h>
#include <inttypes.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "crypto.h"
#include "scriptcore.h"
#include "scriptcorecrypto.h"


static int _crypto_hashmd5(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected arg of type string");
        return lua_error(l);
    }
    size_t plen = 0;
    const char *p = lua_tolstring(l, 1, &plen);
    if (!p) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    char result[16 * 2 + 1];
    crypto_hashmd5(p, plen, result);
    lua_pushstring(l, result);
    return 1;
}


static int _crypto_sha512crypt(lua_State *l) {
    if (lua_gettop(l) < 2 || lua_type(l, 1) != LUA_TSTRING ||
            lua_type(l, 2) != LUA_TSTRING) {
        lua_pushstring(l, "expected args of type string, string");
        return lua_error(l);
    }
    size_t keylen = 0;
    const char *key = lua_tolstring(l, 1, &keylen);
    if (!key) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    int i = 0;
    while (i < (int)keylen) {
        if (key[i] == '\0') {
            lua_pushstring(l, "null bytes in key not supported");
            return lua_error(l);
        }
        i++;
    }
    size_t saltlen = 0;
    const char *salt = lua_tolstring(l, 2, &saltlen);
    if (!key) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    i = 0;
    while (i < (int)saltlen) {
        if (salt[i] == '\0') {
            lua_pushstring(l, "null bytes in salt not supported");
            return lua_error(l);
        }
        i++;
    }
    int64_t rounds = 5000;
    if (lua_gettop(l) >= 3 && lua_type(l, 3) == LUA_TNUMBER) {
        rounds = (
            lua_isinteger(l, 3) ? lua_tointeger(l, 3) :
            round(lua_tonumber(l, 3))
        );
    }
    if (rounds < 1) {
        lua_pushstring(l, "rounds must be positive");
        return lua_error(l);
    }
    char *result = crypto_sha512crypt(key, salt, rounds);
    if (!result) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    lua_pushstring(l, result);
    free(result);
    return 1;
}


static int _crypto_hashsha512(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected arg of type string");
        return lua_error(l);
    }
    size_t plen = 0;
    const char *p = lua_tolstring(l, 1, &plen);
    if (!p) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    char result[64 * 2 + 1];
    crypto_hashsha512(p, plen, result);
    lua_pushstring(l, result);
    return 1;
}


void scriptcorecrypto_AddFunctions(lua_State *l) {
    lua_pushcfunction(l, _crypto_hashmd5);
    lua_setglobal(l, "_crypto_hashmd5");
    lua_pushcfunction(l, _crypto_hashsha512);
    lua_setglobal(l, "_crypto_hashsha512");
    lua_pushcfunction(l, _crypto_sha512crypt);
    lua_setglobal(l, "_crypto_sha512crypt");
}

