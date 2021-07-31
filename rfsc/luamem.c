// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#include <assert.h>
#include <inttypes.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <lstate.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "scriptcore.h"


typedef struct luastateinfo {
    lua_State *l;
    int64_t memory_use;
} luastateinfo;


static void *lua_memalloc(
        void *userdata, void *ptr,
        size_t oldsize, size_t newsize
        ) {
    luastateinfo *lsinfo = (luastateinfo *)userdata;
    if (!lsinfo)
        return NULL;
    if (!ptr)
        oldsize = 0;
    void *result = NULL;
    if (newsize == 0) {
        if (ptr) {
            lsinfo->memory_use -= oldsize;
            free(ptr);
        }
    } else {
        lsinfo->memory_use += ((int)newsize - (int)oldsize);
        result = realloc(ptr, newsize);
    }
    assert(lsinfo->memory_use >= 0);
    return result;
}

int luamem_EnsureFreePools(lua_State *l) {
    if (!l)
        return 0;
    return 1;
}

int luamem_EnsureCanAllocSize(lua_State *l, size_t bytes) {
    if (!l)
        return 0;
    return 1;
}

lua_State *luamem_NewMemManagedState() {
    luastateinfo *lstateinfo = malloc(sizeof(*lstateinfo));
    if (!lstateinfo)
        return NULL;
    memset(lstateinfo, 0, sizeof(*lstateinfo));
    lua_State *l = NULL;
    l = luaL_newstate();
    if (!l) {
        free(lstateinfo);
        return NULL;
    }
    lstateinfo->l = l;
    return l;
}
