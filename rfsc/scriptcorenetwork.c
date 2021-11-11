// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <lua.h>
#include <string.h>

#include "http.h"
#include "scriptcore.h"
#include "scriptcorenetwork.h"


static int _http_newdownload(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected 1 arg of type string");
        return lua_error(l);
    }
    const char *url = lua_tostring(l, 1);
    if (!url) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    int64_t maxbytes = 1024LL * 1024LL * 20LL;
    int retries = 0;
    if (lua_gettop(l) >= 2 && lua_type(l, 2) == LUA_TTABLE) {
        lua_pushstring(l, "retries");
        lua_gettable(l, 2);
        if (lua_type(l, -1) == LUA_TNUMBER) {
            retries = lua_tonumber(l, -1);
            if (retries < 0) retries = 0;
        }
        lua_pop(l, 1);
        lua_pushstring(l, "maxbytes");
        lua_gettable(l, 2);
        if (lua_type(l, -1) == LUA_TNUMBER) {
            if (lua_isinteger(l, -1)) maxbytes = lua_tointeger(l, -1);
            else maxbytes = lua_tonumber(l, -1);
            if (maxbytes < 0) maxbytes = -1;
        }
        lua_pop(l, 1);
    }
    httpdownload *h = http_NewDownload(
        lua_tostring(l, 1), maxbytes, retries
    );
    if (!h) {
        lua_pushstring(l, "failed to start download. "
            "missing libcurl?");
        return lua_error(l);
    }
    scriptobjref *ref = lua_newuserdata(l, sizeof(*ref));
    if (!ref) {
        http_FreeDownload(h);
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    memset(ref, 0, sizeof(*ref));
    ref->magic = OBJREFMAGIC;
    ref->type = OBJREF_DOWNLOAD;
    ref->value = (uintptr_t)h;
    return 1;
}


static int _http_getdownloadbytecount(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_DOWNLOAD) {
        lua_pushstring(l, "expected 1 arg of type download");
        return lua_error(l);
    }
    httpdownload *h = ((httpdownload *)
        (uintptr_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!h) {
        lua_pushstring(l, "expected 1 arg of type download");
        return lua_error(l);
    }
    lua_pushinteger(l, http_DownloadedByteCount(h));
    return 1;
}


static int _http_getdownloadcontents(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_DOWNLOAD) {
        lua_pushstring(l, "expected 1 arg of type download");
        return lua_error(l);
    }
    httpdownload *h = ((httpdownload *)
        (uintptr_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!h) {
        lua_pushstring(l, "expected 1 arg of type download");
        return lua_error(l);
    }
    if (!http_IsDownloadDone(h)) {
        lua_pushstring(l, "download in progress");
        return lua_error(l);
    }
    if (http_IsDownloadFailure(h)) {
        lua_pushnil(l);
        return 1;
    }
    int64_t c = 0;
    char *bytes = http_GetDownloadResultAsBytes(h, &c);
    if (!bytes || c < 0) {
        lua_pushnil(l);
        return 1;
    }
    lua_pushlstring(l, bytes, c);
    free(bytes);
    return 1;
}


static int _http_isdownloaddone(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_DOWNLOAD) {
        lua_pushstring(l, "expected 1 arg of type download");
        return lua_error(l);
    }
    httpdownload *h = ((httpdownload *)
        (uintptr_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!h) {
        lua_pushstring(l, "expected 1 arg of type download");
        return lua_error(l);
    }
    lua_pushboolean(l, http_IsDownloadDone(h));
    return 1;
}


static int _http_isdownloadfailure(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_DOWNLOAD) {
        lua_pushstring(l, "expected 1 arg of type download");
        return lua_error(l);
    }
    httpdownload *h = ((httpdownload *)
        (uintptr_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!h) {
        lua_pushstring(l, "expected 1 arg of type download");
        return lua_error(l);
    }
    lua_pushboolean(l, http_IsDownloadFailure(h));
    return 1;
}


static int _http_freedownload(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_DOWNLOAD) {
        lua_pushstring(l, "expected 1 arg of type download");
        return lua_error(l);
    }
    httpdownload *h = ((httpdownload *)
        (uintptr_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!h) {
        lua_pushstring(l, "expected 1 arg of type download");
        return lua_error(l);
    }
    http_FreeDownload(h);
    ((scriptobjref *)lua_touserdata(l, 1))->value = 0;
    return 1;
}


void scriptcorenetwork_AddFunctions(lua_State *l) {
    lua_pushcfunction(l, _http_newdownload);
    lua_setglobal(l, "_http_newdownload");
    lua_pushcfunction(l, _http_freedownload);
    lua_setglobal(l, "_http_freedownload");
    lua_pushcfunction(l, _http_isdownloaddone);
    lua_setglobal(l, "_http_isdownloaddone");
    lua_pushcfunction(l, _http_isdownloadfailure);
    lua_setglobal(l, "_http_isdownloadfailure");
    lua_pushcfunction(l, _http_getdownloadbytecount);
    lua_setglobal(l, "_http_getdownloadbytecount");
    lua_pushcfunction(l, _http_getdownloadcontents);
    lua_setglobal(l, "_http_getdownloadcontents");
}
