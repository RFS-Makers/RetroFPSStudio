// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#include "compileconfig.h"


#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "archiver.h"
#include "scriptcore.h"
#include "scriptcorearchive.h"
#include "vfs.h"


static int _archive_open(lua_State *l) {
    if (lua_gettop(l) < 1 ||
            lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected arg of type string");
        return lua_error(l);
    }
    const char *path = lua_tostring(l, 1);
    if (!path) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    h64archive *a = archive_FromFilePath(
        path, 0, VFSFLAG_NO_VIRTUALPAK_ACCESS,
        H64ARCHIVE_TYPE_ZIP
    );
    if (!a) {
        lua_pushstring(l, "failed to open archive");
        return lua_error(l);
    }
    scriptobjref *sobj = lua_newuserdata(l, sizeof(*sobj));
    if (!sobj) {
        h64archive_Close(a);
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    memset(sobj, 0, sizeof(*sobj));
    sobj->magic = OBJREFMAGIC;
    sobj->type = OBJREF_ARCHIVE;
    sobj->value = (int64_t)(uintptr_t)a;
    return 1;
}

static int _archive_getentrycount(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ARCHIVE) {
        lua_pushstring(l, "expected arg of type archive");
        return lua_error(l);
    }
    scriptobjref *fref = ((scriptobjref*)lua_touserdata(l, 1));
    h64archive *a = (h64archive*)(uintptr_t)fref->value;
    if (!a) {
        lua_pushstring(l, "archive already closed");
        return lua_error(l);
    }
    lua_pushinteger(l, h64archive_GetEntryCount(a));
    return 1;
}

static int _archive_close(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ARCHIVE) {
        lua_pushstring(l, "expected arg of type archive");
        return lua_error(l);
    }
    scriptobjref *fref = ((scriptobjref*)lua_touserdata(l, 1));
    h64archive *a = (h64archive*)(uintptr_t)fref->value;
    if (!a) {
        lua_pushstring(l, "archive already closed");
        return lua_error(l);
    }
    fref->value = 0;
    h64archive_Close(a);
    return 1;
}

static int _archive_isdir(lua_State *l) {
    if (lua_gettop(l) < 2 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ARCHIVE ||
            lua_type(l, 2) != LUA_TNUMBER) {
        lua_pushstring(l, "expected 2 args of type archive, "
            "number");
        return lua_error(l);
    }
    int64_t no = lua_tointeger(l, 2);
    scriptobjref *fref = ((scriptobjref*)lua_touserdata(l, 1));
    h64archive *a = (h64archive*)(uintptr_t)fref->value;
    if (!a) {
        lua_pushstring(l, "archive already closed");
        return lua_error(l);
    }
    if (no < 1 || no > h64archive_GetEntryCount(a)) {
        lua_pushstring(l, "entry number out of range");
        return lua_error(l);
    }
    lua_pushboolean(l, h64archive_GetEntryIsDir(a, no - 1));
    return 1;
}

static int _archive_getentrycontents(lua_State *l) {
    if (lua_gettop(l) < 2 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ARCHIVE ||
            lua_type(l, 2) != LUA_TNUMBER) {
        lua_pushstring(l, "expected 2 args of type archive, "
            "number");
        return lua_error(l);
    }
    int64_t no = lua_tointeger(l, 2);
    scriptobjref *fref = ((scriptobjref*)lua_touserdata(l, 1));
    h64archive *a = (h64archive*)(uintptr_t)fref->value;
    if (!a) {
        lua_pushstring(l, "archive already closed");
        return lua_error(l);
    }
    if (no < 1 || no > h64archive_GetEntryCount(a)) {
        lua_pushstring(l, "entry number out of range");
        return lua_error(l);
    }
    if (h64archive_GetEntryIsDir(a, no - 1)) {
        lua_pushstring(l, "entry is not a file");
        return lua_error(l);
    }
    int64_t size = h64archive_GetEntrySize(a, no - 1);
    char *data = malloc(size + 1);
    if (!data) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    if (size > 0 && !h64archive_ReadFileByteSlice(
            a, no - 1, 0, data, size)) {
        lua_pushstring(l, "I/O error or out of memory");
        return lua_error(l);
    }
    lua_pushlstring(l, data, size);
    free(data);
    return 1;
}

static int _archive_getentryname(lua_State *l) {
    if (lua_gettop(l) < 2 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ARCHIVE ||
            lua_type(l, 2) != LUA_TNUMBER) {
        lua_pushstring(l, "expected 2 args of type archive, "
            "number");
        return lua_error(l);
    }
    int64_t no = lua_tointeger(l, 2);
    scriptobjref *fref = ((scriptobjref*)lua_touserdata(l, 1));
    h64archive *a = (h64archive*)(uintptr_t)fref->value;
    if (!a) {
        lua_pushstring(l, "archive already closed");
        return lua_error(l);
    }
    if (no < 1 || no > h64archive_GetEntryCount(a)) {
        lua_pushstring(l, "entry number out of range");
        return lua_error(l);
    }
    const char *name = h64archive_GetEntryName(a, no - 1);
    if (!name) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    lua_pushstring(l, name);
    return 1;
}

static int _archive_adddirentry(lua_State *l) {
    if (lua_gettop(l) < 2 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ARCHIVE ||
            lua_type(l, 2) != LUA_TSTRING) {
        lua_pushstring(l, "expected 2 args of type archive, "
            "string");
        return lua_error(l);
    }
    const char *name = lua_tostring(l, 2);
    if (!name) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    scriptobjref *fref = ((scriptobjref*)lua_touserdata(l, 1));
    h64archive *a = (h64archive*)(uintptr_t)fref->value;
    if (!a) {
        lua_pushstring(l, "archive already closed");
        return lua_error(l);
    }
    int result = h64archive_AddDir(a, name);
    if (result != H64ARCHIVE_ADDERROR_SUCCESS) {
        lua_pushstring(l, "failed to add entry");
        return lua_error(l);
    }
    return 0;
}


static int _archive_addfileentry(lua_State *l) {
    if (lua_gettop(l) < 3 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ARCHIVE ||
            lua_type(l, 2) != LUA_TSTRING ||
            lua_type(l, 3) != LUA_TSTRING) {
        lua_pushstring(l, "expected 3 args of type archive, "
            "string, string");
        return lua_error(l);
    }
    const char *name = lua_tostring(l, 2);
    size_t datalen = 0;
    const char *data = lua_tolstring(l, 3, &datalen);
    if (!data || !name) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    scriptobjref *fref = ((scriptobjref*)lua_touserdata(l, 1));
    h64archive *a = (h64archive*)(uintptr_t)fref->value;
    if (!a) {
        lua_pushstring(l, "archive already closed");
        return lua_error(l);
    }
    int result = h64archive_AddFileFromMem(
        a, name, data, datalen
    );
    if (result != H64ARCHIVE_ADDERROR_SUCCESS) {
        lua_pushstring(l, "failed to add entry");
        return lua_error(l);
    }
    return 0;
}

void scriptcorearchive_AddFunctions(lua_State *l) {
    lua_pushcfunction(l, _archive_open);
    lua_setglobal(l, "_archive_open");
    lua_pushcfunction(l, _archive_getentryname);
    lua_setglobal(l, "_archive_getentryname");
    lua_pushcfunction(l, _archive_getentrycount);
    lua_setglobal(l, "_archive_getentrycount");
    lua_pushcfunction(l, _archive_close);
    lua_setglobal(l, "_archive_close");
    lua_pushcfunction(l, _archive_isdir);
    lua_setglobal(l, "_archive_isdir");
    lua_pushcfunction(l, _archive_getentrycontents);
    lua_setglobal(l, "_archive_getentrycontents");
    lua_pushcfunction(l, _archive_addfileentry);
    lua_setglobal(l, "_archive_addfileentry");
    lua_pushcfunction(l, _archive_adddirentry);
    lua_setglobal(l, "_archive_adddirentry");
}
