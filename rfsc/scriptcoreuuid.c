
#include "compileconfig.h"

#include <lua.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "scriptcore.h"
#include "scriptcoreuuid.h"
#include "uuid.h"

static int _uuid_gen(lua_State *l) {
    char buf[UUIDLEN + 1];
    if (!uuid_gen(buf)) {
        lua_pushstring(l, "gen fail, out of memory or entropy");
        return lua_error(l);
    }
    lua_pushstring(l, buf);
    return 1;
}

void scriptcoreuuid_AddFunctions(lua_State *l) {
    lua_pushcfunction(l, _uuid_gen);
    lua_setglobal(l, "_uuid_gen");
}
