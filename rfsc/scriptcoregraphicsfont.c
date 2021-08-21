// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "graphics.h"
#include "graphicsfont.h"
#include "outputwindow.h"
#include "scriptcore.h"
#include "scriptcoregraphicsfont.h"


static int _graphicsfont_get(lua_State *l) {
    if (lua_gettop(l) < 1 ||
            lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected first arg to "
            "be path to font");
        return lua_error(l);
    }
    const char *p = lua_tostring(l, 1);
    if (!p) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    rfsfont *f = graphicsfont_Get(p);
    if (!f) {
        lua_pushstring(l, "failed to get font");
        return lua_error(l);
    }
    scriptobjref *ref = lua_newuserdata(l, sizeof(*ref));
    if (!ref) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    memset(ref, 0, sizeof(*ref));
    ref->magic = OBJREFMAGIC;
    ref->type = OBJREF_FONT;
    ref->value = (uintptr_t)f;
    return 1;
}


void scriptcoregraphicsfont_AddFunctions(lua_State *l) {
    lua_pushcfunction(l, _graphicsfont_get);
    lua_setglobal(l, "_graphicsfont_get");
}
