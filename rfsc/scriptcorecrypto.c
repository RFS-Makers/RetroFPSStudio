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

#include "md5.h"
#include "scriptcore.h"
#include "scriptcorecrypto.h"
#include "sha2/sha2.h"
#include "sha512crypt/sha512crypt.h"


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
    MD5_CTX c = {0};
    MD5_Init(&c);
    MD5_Update(&c, p, plen);
    char hashdata[16];
    MD5_Final((uint8_t *)hashdata, &c);
    char dighex[16 * 2 + 1];
    int i = 0;
    while (i < 16) {
        char c[3];
        snprintf(c, 3, "%x", hashdata[i]);
        dighex[i * 2] = tolower(c[0]);
        dighex[i * 2 + 1] = tolower(c[1]);
        i++;
    }
    dighex[16 * 2] = '\0';
    lua_pushstring(l, dighex);
    return 0;
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
    char rounds_str[64];
    snprintf(rounds_str, sizeof(rounds_str) - 1,
        "$6$rounds=%" PRId64 "$", rounds);
    char *keynullterminated = malloc(keylen + 1);
    char *saltnullterminated_rounds = malloc(
        strlen(rounds_str) + saltlen + 1);
    if (!keynullterminated || !saltnullterminated_rounds) {
        free(keynullterminated);
        free(saltnullterminated_rounds);
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    memcpy(keynullterminated, key, keylen);
    keynullterminated[keylen] = '\0';
    memcpy(saltnullterminated_rounds,
           rounds_str, strlen(rounds_str));
    memcpy(saltnullterminated_rounds + strlen(rounds_str),
           salt, saltlen);
    saltnullterminated_rounds[
        strlen(rounds_str) + saltlen] = '\0';
    char *result = sha512_crypt(
        keynullterminated, saltnullterminated_rounds);
    free(keynullterminated);
    free(saltnullterminated_rounds);
    if (!result) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    int dollars_seen = 0;
    i = 0;
    while (i < (int)strlen(result)) {
        if (result[i] == '$') {
            dollars_seen++;
            if (dollars_seen == 4) {
                memmove(result, result + i + 1,
                        strlen(result) - (i + 1) + 1);
                break;
            }
        }
        i++;
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
    uint8_t dig[SHA512_DIGEST_LENGTH];
    SHA512((void *)p, plen, dig);
    char dighex[SHA512_DIGEST_LENGTH * 2 + 1];
    int i = 0;
    while (i < SHA512_DIGEST_LENGTH) {
        char c[3];
        snprintf(c, 3, "%x", dig[i]);
        dighex[i * 2] = tolower(c[0]);
        dighex[i * 2 + 1] = tolower(c[1]);
        i++;
    }
    dighex[SHA512_DIGEST_LENGTH * 2] = '\0';
    lua_pushstring(l, dighex);
    return 0;
}


void scriptcorecrypto_AddFunctions(lua_State *l) {
    lua_pushcfunction(l, _crypto_hashmd5);
    lua_setglobal(l, "_crypto_hashmd5");
    lua_pushcfunction(l, _crypto_hashsha512);
    lua_setglobal(l, "_crypto_hashsha512");
    lua_pushcfunction(l, _crypto_sha512crypt);
    lua_setglobal(l, "_crypto_sha512crypt");
}

