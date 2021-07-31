// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#include "compileconfig.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "graphics.h"
#include "outputwindow.h"
#include "scriptcore.h"
#include "scriptcoregraphics.h"


static int _graphics_is3dacc(lua_State *l) {
    lua_pushboolean(l, outputwindow_Is3DAcc());
    return 1;
}

static int _graphics_setfullscreen(ATTR_UNUSED lua_State *l) {
    outputwindow_SetFullscreen();
    return 0;
}

static int _graphics_forceoutputsize(lua_State *l) {
    if (lua_gettop(l) < 2 || lua_type(l, 1) != LUA_TNUMBER ||
            lua_type(l, 2) != LUA_TNUMBER) {
        lua_pushstring(l, "expected two args of type number");
        return lua_error(l);
    }
    int64_t w = (
        lua_isinteger(l, 1) ? lua_tointeger(l, 1) :
        round(lua_tonumber(l, 1))
    );
    int64_t h = (
        lua_isinteger(l, 2) ? lua_tointeger(l, 2) :
        round(lua_tonumber(l, 2))
    );
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    outputwindow_ForceScaledOutputSize(w, h);
    return 0;
}

static int _graphics_renderername(lua_State *l) {
    const char *p = outputwindow_RendererName();
    if (!p || strlen(p) == 0) {
        lua_pushstring(l, "none");
        return 1;
    }
    lua_pushstring(l, p);
    return 1;
}

static int _graphics_gettex(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected 1 arg of type string");
        return lua_error(l);
    }
    rfs2tex *t = graphics_LoadTex(lua_tostring(l, 1));
    if (!t) {
        char buf[1024];
        snprintf(buf, sizeof(buf) - 1, "failed to load texture: %s",
            lua_tostring(l, 1));
        lua_pushstring(l, buf);
        return lua_error(l);
    }
    scriptobjref *ref = lua_newuserdata(l, sizeof(*ref));
    if (!ref) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    memset(ref, 0, sizeof(*ref));
    ref->magic = OBJREFMAGIC;
    ref->type = OBJREF_TEXTURE;
    ref->value = t->id;
    return 1;
}

static int _graphics_drawrectangle(lua_State *l) {
    if (lua_gettop(l) < 7 || lua_type(l, 1) != LUA_TNUMBER ||
            lua_type(l, 2) != LUA_TNUMBER ||
            lua_type(l, 3) != LUA_TNUMBER ||
            lua_type(l, 4) != LUA_TNUMBER ||
            lua_type(l, 5) != LUA_TNUMBER ||
            lua_type(l, 6) != LUA_TNUMBER ||
            lua_type(l, 7) != LUA_TNUMBER) {
        lua_pushstring(l, "expected 7 args of type number");
        return lua_error(l);
    }
    double alpha = 1.0f;
    if (lua_gettop(l) >= 8 && lua_type(l, 8) == LUA_TNUMBER)
        alpha = lua_tonumber(l, 8);
    int result = graphics_DrawRectangle(
        lua_tonumber(l, 5), lua_tonumber(l, 6),
        lua_tonumber(l, 7), alpha,
        lua_tonumber(l, 1), lua_tonumber(l, 2),
        lua_tonumber(l, 3), lua_tonumber(l, 4)
    );
    if (!result) {
        lua_pushstring(l, "draw error");
        return lua_error(l);
    }
    return 0;
}

static int _graphics_drawline(lua_State *l) {
    if (lua_gettop(l) < 7 || lua_type(l, 1) != LUA_TNUMBER ||
            lua_type(l, 2) != LUA_TNUMBER ||
            lua_type(l, 3) != LUA_TNUMBER ||
            lua_type(l, 4) != LUA_TNUMBER ||
            lua_type(l, 5) != LUA_TNUMBER ||
            lua_type(l, 6) != LUA_TNUMBER ||
            lua_type(l, 7) != LUA_TNUMBER) {
        lua_pushstring(l, "expected 7 args of type number");
        return lua_error(l);
    }
    int thickness = 1;
    if (lua_gettop(l) >= 8 && lua_type(l, 8) == LUA_TNUMBER)
        thickness = round(lua_tonumber(l, 8));
    double alpha = 1.0f;
    if (lua_gettop(l) >= 9 && lua_type(l, 9) == LUA_TNUMBER)
        alpha = lua_tonumber(l, 9);
    int result = graphics_DrawLine(
        lua_tonumber(l, 5), lua_tonumber(l, 6),
        lua_tonumber(l, 7), alpha,
        lua_tonumber(l, 1), lua_tonumber(l, 2),
        lua_tonumber(l, 3), lua_tonumber(l, 4),
        thickness
    );
    if (!result) {
        lua_pushstring(l, "draw error");
        return lua_error(l);
    }
    return 0;
}

static int _graphics_idtotex(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TNUMBER) {
        lua_pushstring(l, "expected 1 arg of type number");
        return lua_error(l);
    }
    int64_t id = lua_tointeger(l, 1);
    rfs2tex *t = graphics_GetTexById(id);
    if (!t) {
        lua_pushnil(l);
        return 1;
    }
    scriptobjref *ref = lua_newuserdata(l, sizeof(*ref));
    if (!ref) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    memset(ref, 0, sizeof(*ref));
    ref->magic = OBJREFMAGIC;
    ref->type = OBJREF_TEXTURE;
    ref->value = t->id;
    return 1;
}

static int _graphics_textoid(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_TEXTURE) {
        lua_pushstring(l, "expected 1 arg of type texture");
        return lua_error(l);
    }
    rfs2tex *t = graphics_GetTexById(
        ((scriptobjref*)lua_touserdata(l, 1))->value
    );
    if (!t) {
        lua_pushstring(l, "expected 1 arg of type texture");
        return lua_error(l);
    }
    lua_pushinteger(l, t->id);
    return 1;
}

static int _graphics_creatert(lua_State *l) {
    if (lua_gettop(l) < 2 || lua_type(l, 1) != LUA_TNUMBER ||
            lua_type(l, 2) != LUA_TNUMBER) {
        lua_pushstring(l, "expected 2 args of type number");
        return lua_error(l);
    }
    int64_t rtw = round(lua_tonumber(l, 1));
    int64_t rth = round(lua_tonumber(l, 2));
    if (rtw < 0 || rth < 0) {
        lua_pushstring(l, "invalid render target size");
        return lua_error(l);
    }
    rfs2tex *t = graphics_CreateTarget(rtw, rth);
    if (!t) {
        lua_pushstring(l, "unexpected failure to create "
            "render target");
        return lua_error(l);
    }
    scriptobjref *ref = lua_newuserdata(l, sizeof(*ref));
    if (!ref) {
        graphics_DeleteTex(t);
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    memset(ref, 0, sizeof(*ref));
    ref->magic = OBJREFMAGIC;
    ref->type = OBJREF_TEXTURE;
    ref->value = t->id;
    return 1;
}

static int _graphics_clearrt(lua_State *l) {
    if (!graphics_ClearTarget()) {
        lua_pushstring(l, "clear error");
        return lua_error(l);
    }
    return 0;
}

static int _graphics_updatert(lua_State *l) {
    if (!graphics_PresentTarget()) {
        lua_pushstring(l, "present error");
        return lua_error(l);
    }
    return 0;
}

static int _graphics_getrt(lua_State *l) {
    rfs2tex *t = graphics_GetTarget();
    if (!t) {
        lua_pushnil(l);
        return 1;
    }
    scriptobjref *ref = lua_newuserdata(l, sizeof(*ref));
    if (!ref) {
        graphics_DeleteTex(t);
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    memset(ref, 0, sizeof(*ref));
    ref->magic = OBJREFMAGIC;
    ref->type = OBJREF_TEXTURE;
    ref->value = t->id;
    return 1;
}

static int _graphics_setrt(lua_State *l) {
    if ((lua_gettop(l) >= 1 && (lua_type(l, 1) != LUA_TUSERDATA
            && lua_type(l, 1) != LUA_TNIL)) || (
            lua_gettop(l) >= 1 && lua_type(l, 1) == LUA_TUSERDATA
            && (
            ((scriptobjref *)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref *)lua_touserdata(l, 1))->type !=
                OBJREF_TEXTURE))) {
        lua_pushstring(l, "expected 1 arg of type "
            "render target or nil");
        return lua_error(l);
    }
    if (lua_gettop(l) == 0 || lua_type(l, 1) == LUA_TNIL) {
        if (!graphics_SetTarget(NULL)) {
            lua_pushstring(l, "failed to set render target");
            return lua_error(l);
        }
        return 0;
    }
    rfs2tex *t = graphics_GetTexById(
        ((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!t || !t->isrendertarget) {
        lua_pushstring(l, "expected 1 arg of type "
            "render target or nil");
        return lua_error(l);
    }
    if (!graphics_SetTarget(t)) {
        lua_pushstring(l, "failed to set render target");
        return lua_error(l);
    }
    return 0;
}

static int _graphics_resizert(lua_State *l) {
    if (lua_gettop(l) < 3 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_TEXTURE ||
            lua_type(l, 2) != LUA_TNUMBER ||
            lua_type(l, 3) != LUA_TNUMBER) {
        lua_pushstring(l, "expected 3 args of types "
            "render target, number, number");
        return lua_error(l);
    }
    rfs2tex *t = graphics_GetTexById(
        ((scriptobjref*)lua_touserdata(l, 1))->value
    );
    if (!t || !t->isrendertarget) {
        lua_pushstring(l, "expected 3 args of types "
            "render target, number, number");
        return lua_error(l);
    }
    int64_t rtw = round(lua_tonumber(l, 2));
    int64_t rth = round(lua_tonumber(l, 3));
    if (rtw < 0 || rth < 0) {
        lua_pushstring(l, "invalid render target size");
        return lua_error(l);
    }
    if (!graphics_ResizeTarget(t, rtw, rth)) {
        lua_pushstring(l, "resize error");
        return lua_error(l);
    }
    return 0;
}

static int _graphics_deletetex(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_TEXTURE) {
        lua_pushstring(l, "expected 1 arg of type "
            "texture");
        return lua_error(l);
    }
    rfs2tex *t = graphics_GetTexById(
        ((scriptobjref*)lua_touserdata(l, 1))->value
    );
    if (!t) {
        lua_pushstring(l, "expected 1 arg of type "
            "texture");
        return lua_error(l);
    }
    if (!t->isrendertarget && !t->iswritecopy) {
        lua_pushstring(l, "can only delete writable "
            "textures or render targets");
        return lua_error(l);
    }
    if (!graphics_DeleteTex(t)) {
        lua_pushstring(l, "unexpected deletion failure");
        return lua_error(l);
    }
    return 0;
}

static int _graphics_gettexsize(lua_State *l) {
    if (lua_gettop(l) < 1 || ((lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_TEXTURE) && lua_type(l, 1) != LUA_TSTRING)) {
        lua_pushstring(l, "expected arg of type "
            "texture or string");
        return lua_error(l);
    }
    rfs2tex *t = NULL;
    if (lua_type(l, 1) == LUA_TSTRING) {
        t = graphics_LoadTex(lua_tostring(l, 1));
        if (!t) {
            char buf[1024];
            snprintf(buf, sizeof(buf) - 1, "failed to load texture: %s",
                lua_tostring(l, 1));
            lua_pushstring(l, buf);
            return lua_error(l);
        }
    } else {
        t = graphics_GetTexById(
            ((scriptobjref*)lua_touserdata(l, 1))->value
        );
        if (!t) {
            lua_pushstring(l, "expected arg of type "
                "texture");
            return lua_error(l);
        }
    }
    lua_pushinteger(l, t->w);
    lua_pushinteger(l, t->h);
    return 2;
}

static int _graphics_pushscissors(lua_State *l) {
    if (lua_gettop(l) < 4 || lua_type(l, 1) != LUA_TNUMBER ||
            lua_type(l, 2) != LUA_TNUMBER ||
            lua_type(l, 3) != LUA_TNUMBER ||
            lua_type(l, 4) != LUA_TNUMBER) {
        lua_pushstring(l, "expected four args of type number");
        return lua_error(l);
    }
    if (!graphics_PushRenderScissors(
            round(lua_tonumber(l, 1)), round(lua_tonumber(l, 2)),
            round(lua_tonumber(l, 3)), round(lua_tonumber(l, 4)))) {
        lua_pushstring(l, "failed to push scissors");
        return lua_error(l);
    }
    return 0;
}

static int _graphics_popscissors(ATTR_UNUSED lua_State *l) {
    graphics_PopRenderScissors();
    return 0;
}

static int _graphics_drawtex(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_TEXTURE) {
        lua_pushstring(l, "expected 1+ args with #1 of type "
            "texture");
        return lua_error(l);
    }
    rfs2tex *t = graphics_GetTexById(
        ((scriptobjref*)lua_touserdata(l, 1))->value
    );
    if (!t) {
        lua_pushstring(l, "expected 1+ args with #1 of type "
            "texture");
        return lua_error(l);
    }
    int x = 0;
    int y = 0;
    if (lua_gettop(l) >= 3 && lua_type(l, 2) == LUA_TNUMBER &&
            lua_type(l, 3) == LUA_TNUMBER) {
        x = round(lua_tonumber(l, 2));
        y = round(lua_tonumber(l, 3));
    }
    double scalex = 1.0;
    double scaley = 1.0;
    if (lua_gettop(l) >= 4 && lua_type(l, 4) == LUA_TNUMBER) {
        if (lua_gettop(l) >= 5 && lua_type(l, 5) == LUA_TNUMBER) {
            scalex = lua_tonumber(l, 4);
            scaley = lua_tonumber(l, 5);
        } else {
            scalex = lua_tonumber(l, 4);
            scaley = lua_tonumber(l, 4);
        }
    }
    double r = 1.0f;
    double g = 1.0f;
    double b = 1.0f;
    double a = 1.0f;
    if (lua_gettop(l) >= 9 && lua_type(l, 6) == LUA_TNUMBER &&
            lua_type(l, 7) == LUA_TNUMBER &&
            lua_type(l, 8) == LUA_TNUMBER &&
            lua_type(l, 9) == LUA_TNUMBER) {
        r = fmax(0.0, fmin(1.0, lua_tonumber(l, 6)));
        g = fmax(0.0, fmin(1.0, lua_tonumber(l, 7)));
        b = fmax(0.0, fmin(1.0, lua_tonumber(l, 8)));
        a = fmax(0.0, fmin(1.0, lua_tonumber(l, 9)));
    }
    int cropx = 0;
    int cropy = 0;
    int cropw = t->w;
    int croph = t->h;
    if (lua_gettop(l) >= 13 && lua_type(l, 10) == LUA_TNUMBER &&
            lua_type(l, 11) == LUA_TNUMBER &&
            lua_type(l, 12) == LUA_TNUMBER &&
            lua_type(l, 13) == LUA_TNUMBER) {
        cropx = round(lua_tonumber(l, 10));
        cropy = round(lua_tonumber(l, 11));
        cropw = round(lua_tonumber(l, 12));
        croph = round(lua_tonumber(l, 13));
    }
    if (!graphics_DrawTex(t, 1,
            cropx, cropy, cropw, croph,
            r, g, b, a, x, y, scalex, scaley
            )) {
        lua_pushstring(l, "unexpected draw failure");
        return lua_error(l);
    }
    return 0;
}

void scriptcoregraphics_AddFunctions(lua_State *l) {
    lua_pushcfunction(l, _graphics_gettex);
    lua_setglobal(l, "_graphics_gettex");
    lua_pushcfunction(l, _graphics_textoid);
    lua_setglobal(l, "_graphics_textoid");
    lua_pushcfunction(l, _graphics_idtotex);
    lua_setglobal(l, "_graphics_idtotex");
    lua_pushcfunction(l, _graphics_creatert);
    lua_setglobal(l, "_graphics_creatert");
    lua_pushcfunction(l, _graphics_clearrt);
    lua_setglobal(l, "_graphics_clearrt");
    lua_pushcfunction(l, _graphics_setrt);
    lua_setglobal(l, "_graphics_setrt");
    lua_pushcfunction(l, _graphics_getrt);
    lua_setglobal(l, "_graphics_getrt");
    lua_pushcfunction(l, _graphics_updatert);
    lua_setglobal(l, "_graphics_updatert");
    lua_pushcfunction(l, _graphics_resizert);
    lua_setglobal(l, "_graphics_resizert");
    lua_pushcfunction(l, _graphics_drawtex);
    lua_setglobal(l, "_graphics_drawtex");
    lua_pushcfunction(l, _graphics_deletetex);
    lua_setglobal(l, "_graphics_deletetex");
    lua_pushcfunction(l, _graphics_gettexsize);
    lua_setglobal(l, "_graphics_gettexsize");
    lua_pushcfunction(l, _graphics_drawrectangle);
    lua_setglobal(l, "_graphics_drawrectangle");
    lua_pushcfunction(l, _graphics_drawline);
    lua_setglobal(l, "_graphics_drawline");
    lua_pushcfunction(l, _graphics_is3dacc);
    lua_setglobal(l, "_graphics_is3dacc");
    lua_pushcfunction(l, _graphics_renderername);
    lua_setglobal(l, "_graphics_renderername");
    lua_pushcfunction(l, _graphics_pushscissors);
    lua_setglobal(l, "_graphics_pushscissors");
    lua_pushcfunction(l, _graphics_popscissors);
    lua_setglobal(l, "_graphics_popscissors");
    lua_pushcfunction(l, _graphics_forceoutputsize);
    lua_setglobal(l, "_graphics_forceoutputsize");
    lua_pushcfunction(l, _graphics_setfullscreen);
    lua_setglobal(l, "_graphics_setfullscreen");
}
