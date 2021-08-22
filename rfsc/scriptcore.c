// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <math.h>
#if defined(HAVE_SDL)
#include <SDL2/SDL.h>
#endif

#include "filesys.h"
#include "outputwindow.h"
#include "scriptcore.h"
#include "scriptcorearchive.h"
#include "scriptcoreaudio.h"
#include "scriptcoreerror.h"
#include "scriptcoreevents.h"
#include "scriptcorefilesys.h"
#include "scriptcoregraphics.h"
#include "scriptcoregraphicsfont.h"
#include "scriptcorejson.h"
#include "scriptcoremath.h"
#include "scriptcorenetwork.h"
#include "scriptcoreplatform.h"
#include "scriptcoreroom.h"
#include "scriptcoretime.h"
#include "scriptcoreuuid.h"
#include "scriptcorewidechar.h"
#include "scriptcorewindow.h"
#include "vfs.h"


static lua_State *_mainstate = NULL;

char *stringescape(const char *value) {
    if (!value)
        return NULL;
    char *escaped = malloc(3 + strlen(value) * 2);
    if (!escaped)
        return NULL;
    int len = 0;
    escaped[0] = '\0';
    int i = 0;
    while (i < (int)strlen(value)) {
        uint8_t c = ((const uint8_t*)value)[i];
        if (c == '\r') {
            escaped[len] = '\\';
            escaped[len + 1] = '\r';
            i++;
            len += 2;
            continue;
        } else if (c == '\n') {
            escaped[len] = '\\';
            escaped[len + 1] = '\n';
            i++;
            len += 2;
            continue;
        } else if (c == '\\') {
            escaped[len] = '\\';
            escaped[len + 1] = '\\';
            i++;
            len += 2;
            continue;
        } else if (c == '"') {
            escaped[len] = '\\';
            escaped[len + 1] = '"';
            i++;
            len += 2;
            continue;
        }
        escaped[len] = c;
        i++; len++;
    }
    escaped[len] = '\0';
    return escaped;
}

char *_prefix__file__(
        const char *contents, const char *filepath
        ) {
    if (!contents)
        return NULL;
    char *escapedpath = stringescape(filepath);
    if (!escapedpath)
        return NULL;
    char prestr[] = "local __file__ = \"";
    char afterstr[] = "\"; ";
    char *buf = malloc(
        strlen(prestr) +
        strlen(escapedpath) + strlen(afterstr) +
        strlen(contents) + 1
    );
    if (!buf) {
        free(escapedpath);
        return NULL;
    }
    memcpy(buf, prestr, strlen(prestr));
    memcpy(buf + strlen(prestr), escapedpath, strlen(escapedpath));
    memcpy(buf + strlen(prestr) + strlen(escapedpath),
           afterstr, strlen(afterstr));
    memcpy(buf + strlen(prestr) + strlen(escapedpath) +
           strlen(afterstr),
           contents, strlen(contents) + 1);
    free(escapedpath);
    return buf;
}

static int _lua_pcall(lua_State *l) {
    if (lua_gettop(l) < 1) {
        lua_pushstring(l, "bad argument #1 to "
            "'pcall' (value expected)");
        return lua_error(l);
    }
    int args = lua_gettop(l) - 1;
    if (args < 0)
        args = 0;
    if (!lua_checkstack(l, 10)) {
        if (lua_checkstack(l, 1))
            lua_pushstring(l, "out of memory");
        return lua_error(l);
    }

    int errorhandlerindex = lua_gettop(l) - (args);
    assert(errorhandlerindex >= 1);
    lua_pushtracebackfunc(l);
    lua_insert(l, errorhandlerindex);

    int oldtop = lua_gettop(l);
    int result = lua_pcall(l, args, LUA_MULTRET, errorhandlerindex);
    oldtop -= args + 1;
    if (result) {
        lua_pushboolean(l, 0);
        if (result != LUA_ERRMEM) {
            const char *s = lua_tostring(l, -2);
            if (s)
                lua_pushstring(l, s);
            else
                lua_pushstring(l, "<internal error: "
                    "no error message>");
            return 2;
        }
        return 1;
    } else {
        int returned_values = lua_gettop(l) - oldtop;
        lua_pushboolean(l, 1);
        if (returned_values > 0)
            lua_insert(l, lua_gettop(l) - returned_values);
        return returned_values + 1;
    }
}

int scriptcore_Run(int argc, const char **argv) {
    lua_State *l = luaL_newstate();
    _mainstate = l;

    luaL_openlibs(l);

    const char *_loaderror_nofile = "no main.lua found";
    const char *loaderror = "generic I/O failure";
        
    // Set up script environment for RFS2:
    lua_pushcfunction(l, _lua_pcall);
    lua_setglobal(l, "pcall");
    scriptcoreplatform_AddFunctions(l);
    scriptcorefilesys_AddFunctions(l);
    scriptcorefilesys_RegisterVFS(l);
    scriptcoreevents_AddFunctions(l);
    scriptcorejson_AddFunctions(l);
    scriptcoremath_AddFunctions(l);
    scriptcoreaudio_AddFunctions(l);
    scriptcoretime_AddFunctions(l);
    scriptcorewindow_AddFunctions(l);
    scriptcoregraphics_AddFunctions(l);
    scriptcoregraphicsfont_AddFunctions(l);
    scriptcoreuuid_AddFunctions(l);
    scriptcorearchive_AddFunctions(l);
    scriptcorenetwork_AddFunctions(l);
    scriptcoreroom_AddFunctions(l);
    scriptcorewidechar_AddFunctions(l);

    int prev_stack = lua_gettop(l);
    lua_getglobal(l, "debug");
    if (lua_type(l, -1) == LUA_TTABLE) {
        lua_pushstring(l, "_debugtableref");
        lua_pushvalue(l, -2);
        lua_settable(l, LUA_REGISTRYINDEX);
    } else {
        lua_pop(l, 1);
    }
    lua_settop(l, prev_stack);

    lua_pushtracebackfunc(l);
    int errorhandlerindex = lua_gettop(l);
    assert(errorhandlerindex > 0);
    assert(lua_type(l, errorhandlerindex) == LUA_TFUNCTION);

    // Set up os.arg:
    lua_getglobal(l, "os");
    lua_pushstring(l, "args");
    lua_newtable(l);
    int i = 0;
    while (i < argc) {
        lua_pushnumber(l, i + 1);
        lua_pushstring(l, argv[i]);
        lua_settable(l, -3);
        i++;
    }
    lua_settable(l, -3);

    // Load up main.lua:
    lua_settop(l, errorhandlerindex);
    lua_getglobal(l, "_vfs_lua_loadfile");
    assert(lua_type(l, -1) == LUA_TFUNCTION);
    lua_pushstring(l, "rfslua/main.lua");
    assert(lua_type(l, -1) == LUA_TSTRING);
    assert(lua_type(l, errorhandlerindex) == LUA_TFUNCTION);
    int result = lua_pcall(l, 1, 1, errorhandlerindex);
    int result2 = 0;
    if (result == 0)
        result2 = lua_pcall(l, 0, 1, errorhandlerindex);
    if (result || result2) {
        char buf[1024];
        snprintf(
            buf, sizeof(buf) - 1,
            "FATAL STARTUP SCRIPT CRASH: %s",
            lua_tostring(l, -1)
        );
        fprintf(stderr, "rfsc/scriptcore.c: "
            "error: %s\n", buf);
        lua_close(l);
        outputwindow_EnsureOpenWindow(0);
        #if defined(HAVE_SDL)
        if (SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_ERROR,
                "RFS2 FATAL STARTUP SCRIPT CRASH",
                buf, outputwindow_GetWindow()
                ) != 0) {
            fprintf(stderr, "rfsc: error: "
                "SDL_ShowSimpleMessageBox() failed: "
                "%s\n", SDL_GetError());
        }
        #endif
        #if !defined(ANDROID) && !defined(__ANDROID__)
        outputwindow_CloseWindow();
        #endif
        return 1;
    }

    int luaresult = 0;
    if (lua_type(l, -1) == LUA_TNUMBER) {
        luaresult = lua_tonumber(l, -1);
    }

    return luaresult;
}

lua_State *scriptcore_MainState() {
    return _mainstate;
}
