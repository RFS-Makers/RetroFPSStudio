// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <math.h>

#include "datetime.h"
#include "scriptcore.h"
#include "scriptcoretime.h"


static int _time_getticks(lua_State *l) {
    lua_pushinteger(l, (int64_t)datetime_Ticks());
    return 1;
}

static int _time_sleepms(lua_State *l) {
    if (lua_gettop(l) < 1 ||
            lua_type(l, 1) != LUA_TNUMBER) {
        lua_pushstring(l, "expected arg of type number");
        return lua_error(l);
    }
    int64_t ms = 0;
    if (lua_isinteger(l, 1))
        ms = lua_tointeger(l, 1);
    else
        ms = lua_tonumber(l, 1);
    if (ms >= 0)
        datetime_Sleep(ms);
    return 0;
}

static int _time_schedulefunc(lua_State *l) {
    if (lua_gettop(l) < 2 ||
            lua_type(l, 1) != LUA_TNUMBER ||
            lua_type(l, 2) != LUA_TFUNCTION) {
        lua_pushstring(l, "expected args of types number, function");
        return lua_error(l);
    }
    lua_settop(l, 2);
    if (lua_tonumber(l, 1) < 0) {
        lua_pushstring(l, "scheduled waiting time must be zero or greater");
        return lua_error(l);
    }
    lua_pushstring(l, "scheduledfuncslist");
    lua_gettable(l, LUA_REGISTRYINDEX);
    if (lua_type(l, -1) != LUA_TTABLE) {
        lua_pushstring(l, "scheduledfuncslist");
        lua_newtable(l);
        lua_settable(l, LUA_REGISTRYINDEX);
        lua_pushstring(l, "scheduledfuncslist");
        lua_gettable(l, LUA_REGISTRYINDEX);
        if (lua_type(l, -1) != LUA_TTABLE) {
            lua_pushstring(l, "failed to create scheduling table");
            return lua_error(l);
        }
    }
    int entries = lua_rawlen(l, -1);
    lua_pushnumber(l, entries + 1);
    lua_newtable(l);
    lua_pushnumber(l, 1);
    if (lua_isinteger(l, 1))
        lua_pushinteger(l,
            (int64_t)datetime_Ticks() + lua_tointeger(l, 1));
    else
        lua_pushinteger(l,
            (int64_t)datetime_Ticks() + lua_tonumber(l, 1));
    lua_settable(l, -3);
    lua_pushnumber(l, 2);
    lua_pushvalue(l, 2);
    lua_settable(l, -3);
    lua_settable(l, -3);
    return 0;
}

static uint64_t last_framelimit_ts = 0;

int _time_framelimit(ATTR_UNUSED lua_State *l) {
    uint64_t now = datetime_Ticks();
    if (now < last_framelimit_ts + 15) {
        datetime_Sleep((last_framelimit_ts + 15) - now);
    }
    last_framelimit_ts = datetime_Ticks();
    return 0;
}

int _time_getscheduledfuncstable(lua_State *l) {
    lua_pushstring(l, "scheduledfuncslist");
    lua_gettable(l, LUA_REGISTRYINDEX);
    if (lua_type(l, -1) != LUA_TTABLE) {
        lua_pop(l, 1);
        lua_newtable(l);
        return 1;
    }
    return 1;
}

void scriptcoretime_AddFunctions(lua_State *l) {
    lua_pushcfunction(l, _time_getticks);
    lua_setglobal(l, "_time_getticks");
    lua_pushcfunction(l, _time_sleepms);
    lua_setglobal(l, "_time_sleepms");
    lua_pushcfunction(l, _time_getscheduledfuncstable);
    lua_setglobal(l, "_time_getscheduledfuncstable");
    lua_pushcfunction(l, _time_schedulefunc);
    lua_setglobal(l, "_time_schedulefunc");
    lua_pushcfunction(l, _time_framelimit);
    lua_setglobal(l, "_time_framelimit");
}
