// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#include "compileconfig.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <math.h>
#if defined(HAVE_SDL)
#include <SDL2/SDL.h>
#endif
#include <stdlib.h>

#include "outputwindow.h"
#include "scriptcoreevents.h"
#include "scriptcorewindow.h"


int _window_no3dacc(ATTR_UNUSED lua_State *l) {
    outputwindow_ForceNo3DAcc();
    return 0;
}

int _window_setrelmouse(lua_State *l) {
    if (lua_gettop(l) < 1 ||
            lua_type(l, 1) != LUA_TBOOLEAN) {
        lua_pushstring(l, "expected 1 arg of type boolean");
        return lua_error(l);
    }
    scriptcoreevents_SetRelativeMouse(
        lua_toboolean(l, 1)
    );
    return 0;
}

int _window_quit(ATTR_UNUSED lua_State *l) {
    outputwindow_CloseWindow();
    #if defined(HAVE_SDL)
    SDL_Quit();
    #endif
    exit(0);
    return 0;
}

static int _window_getsize(lua_State *l) {
    lua_pushnumber(l, rfswindoww);
    lua_pushnumber(l, rfswindowh);
    return 2;
}

void scriptcorewindow_AddFunctions(lua_State *l) {
    lua_pushcfunction(l, _window_no3dacc);
    lua_setglobal(l, "_window_no3dacc");
    lua_pushcfunction(l, _window_quit);
    lua_setglobal(l, "_window_quit");
    lua_pushcfunction(l, _window_setrelmouse);
    lua_setglobal(l, "_window_setrelmouse");
    lua_pushcfunction(l, _window_getsize);
    lua_setglobal(l, "_window_getsize");
}
