// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#include "compileconfig.h"

#include <lua.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "scriptcore.h"
#include "scriptcoreplatform.h"
#include "uuid.h"

static int _platform_osname(lua_State *l) {
    #if defined(ANDROID) || defined(__ANDROID__)
    lua_pushstring(l, "Android");
    #elif defined(_WIN32) || defined(_WIN64)
    lua_pushstring(l, "Windows");
    #elif defined(__FreeBSD__) || defined(__FREEBSD__)
    lua_pushstring(l, "FreeBSD");
    #elif defined(__linux__)
    lua_pushstring(l, "Linux");
    #elif defined(__APPLE__) || defined(__OSX__)
    lua_pushstring(l, "macOS");
    #else
    lua_pushstring(l, "Unknown");
    #endif
    return 1;
}

static int _platform_binarch(lua_State *l) {
    #if defined(ARCH_X64)
    lua_pushstring(l, "x64");
    #elif defined(ARCH_ARM64)
    lua_pushstring(l, "ARM64");
    #elif defined(ARCH_X86)
    lua_pushstring(l, "x86");
    #elif defined(ARCH_ARMV7)
    lua_pushstring(l, "ARMv7");
    #else
    lua_pushstring(l, "Unknown");
    #endif
    return 1;
}

void scriptcoreplatform_AddFunctions(lua_State *l) {
    lua_pushcfunction(l, _platform_osname);
    lua_setglobal(l, "_platform_osname");
    lua_pushcfunction(l, _platform_binarch);
    lua_setglobal(l, "_platform_binarch");
}


