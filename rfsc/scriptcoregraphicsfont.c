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
    char *emsg = NULL;
    rfsfont *f = graphicsfont_Get(p, &emsg);
    if (!f) {
        lua_pushstring(l, (emsg ? emsg :
            "unknown error loading font"));
        free(emsg);
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


static int _graphicsfont_calcoutlinepx(lua_State *l) {
    if (lua_gettop(l) < 3 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_FONT ||
            lua_type(l, 2) != LUA_TNUMBER ||
            lua_type(l, 3) != LUA_TNUMBER) {
        wrongargs: ;
        lua_pushstring(l, "expected 3 args of types font, "
            "number, number");
        return lua_error(l);
    }
    double pt_size = lua_tonumber(l, 2);
    double outline_size = lua_tonumber(l, 3);

    scriptobjref *fref = ((scriptobjref*)lua_touserdata(l, 1));
    rfsfont *f = (rfsfont *)(uintptr_t)fref->value;
    if (!f) goto wrongargs;
    int resultpx = (
        graphicsfont_RealOutlinePixelsAfterScaling(
            f, pt_size, outline_size));
    lua_pushinteger(l, resultpx);
    return 1;
}


static int _graphicsfont_calcwidth(lua_State *l) {
    if (lua_gettop(l) < 3 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_FONT ||
            lua_type(l, 2) != LUA_TSTRING ||
            lua_type(l, 3) != LUA_TNUMBER) {
        wrongargs: ;
        lua_pushstring(l, "expected 3 args of types font, "
            "string, number");
        return lua_error(l);
    }
    scriptobjref *fref = ((scriptobjref*)lua_touserdata(l, 1));
    rfsfont *f = (rfsfont *)(uintptr_t)fref->value;
    if (!f) goto wrongargs;

    const char *text = lua_tostring(l, 2);
    double pt_size = lua_tonumber(l, 3);
    double letterspacing = 0;
    if (lua_gettop(l) >= 4 && lua_type(l, 4) == LUA_TNUMBER)
        letterspacing = lua_tonumber(l, 4);
    double outlinesize = 0;
    if (lua_gettop(l) >= 5 && lua_type(l, 5) == LUA_TNUMBER)
        outlinesize = lua_tonumber(l, 5);
    if (!text) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    int width = graphicsfont_CalcWidth(
        f, text, pt_size, letterspacing, outlinesize
    );
    if (width < 0) {
        lua_pushstring(l, "out of memory or internal error");
        return lua_error(l);
    }
    lua_pushinteger(l, width);
    return 1;
}


static int _graphicsfont_calcheight(lua_State *l) {
    if (lua_gettop(l) < 4 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_FONT ||
            lua_type(l, 2) != LUA_TSTRING ||  // text
            (lua_type(l, 3) != LUA_TNUMBER &&  // width
             lua_type(l, 3) != LUA_TNIL) ||
            lua_type(l, 4) != LUA_TNUMBER  // pt_size
            ) {
        wrongargs: ;
        lua_pushstring(l, "expected 4 args of types font, "
            "string, number or nil, number");
        return lua_error(l);
    }
    scriptobjref *fref = ((scriptobjref*)lua_touserdata(l, 1));
    rfsfont *f = (rfsfont *)(uintptr_t)fref->value;
    if (!f) goto wrongargs;

    const char *text = lua_tostring(l, 2);
    int width = -1;
    if (lua_type(l, 3) == LUA_TNUMBER) {
        width = (lua_isinteger(l, 3) ? lua_tointeger(l, 3) :
            round(lua_tonumber(l, 3)));
    }
    double pt_size = lua_tonumber(l, 4);
    double letterspacing = 0;
    if (lua_gettop(l) >= 5 && lua_type(l, 5) == LUA_TNUMBER)
        letterspacing = lua_tonumber(l, 5);
    double outlinesize = 0;
    if (lua_gettop(l) >= 6 && lua_type(l, 6) == LUA_TNUMBER)
        outlinesize = lua_tonumber(l, 6);
    if (!text) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    int h = graphicsfont_CalcHeight(
        f, text, width, pt_size, letterspacing, outlinesize
    );
    if (h < 0) {
        lua_pushstring(l, "out of memory or internal error");
        return lua_error(l);
    }
    lua_pushinteger(l, h);
    return 1;
}


static int _graphicsfont_textwrap(lua_State *l) {
    if (lua_gettop(l) < 4 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_FONT ||
            lua_type(l, 2) != LUA_TSTRING ||
            (lua_type(l, 3) != LUA_TNUMBER &&
             lua_type(l, 3) != LUA_TNIL) ||
            lua_type(l, 4) != LUA_TNUMBER) {
        wrongargs: ;
        lua_pushstring(l, "expected 3 args of types font, "
            "string, number or nil, number");
        return lua_error(l);
    }
    scriptobjref *fref = ((scriptobjref*)lua_touserdata(l, 1));
    rfsfont *f = (rfsfont *)(uintptr_t)fref->value;
    if (!f) goto wrongargs;

    const char *text = lua_tostring(l, 2);
    int width = -1;
    if (lua_type(l, 3) == LUA_TNUMBER) {
        width = (lua_isinteger(l, 3) ? lua_tointeger(l, 3) :
            round(lua_tonumber(l, 3)));
    }
    double pt_size = lua_tonumber(l, 4);
    double letterspacing = 0;
    if (lua_gettop(l) >= 5 && lua_type(l, 5) == LUA_TNUMBER)
        letterspacing = lua_tonumber(l, 5);
    double outlinesize = 0;
    if (lua_gettop(l) >= 6 && lua_type(l, 6) == LUA_TNUMBER)
        outlinesize = lua_tonumber(l, 6);
    if (!text) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    char **lines = graphicsfont_TextWrap(
        f, text, width, pt_size, letterspacing, outlinesize
    );
    if (lines) {
        lua_pushstring(l, "out of memory or internal error");
        return lua_error(l);
    }
    lua_newtable(l);
    int i = 0;
    while (lines[i] != NULL) {
        lua_pushnumber(l, i + 1);
        lua_pushstring(l, lines[i]);
        lua_settable(l, -3);
        free(lines[i]);
        i++;
    }
    free(lines);
    return 1;
}


static int _graphicsfont_draw(lua_State *l) {
    if (lua_gettop(l) < 10 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_FONT ||
            lua_type(l, 2) != LUA_TSTRING ||  // text
            (lua_type(l, 3) != LUA_TNUMBER &&
             lua_type(l, 3) != LUA_TNIL) ||  // width
            lua_type(l, 4) != LUA_TNUMBER ||  // x
            lua_type(l, 5) != LUA_TNUMBER ||  // y
            (lua_type(l, 6) != LUA_TNUMBER &&
             lua_type(l, 6) != LUA_TNIL) ||  // r
            (lua_type(l, 7) != LUA_TNUMBER &&
             lua_type(l, 7) != LUA_TNIL) ||  // g
            (lua_type(l, 8) != LUA_TNUMBER &&
             lua_type(l, 8) != LUA_TNIL) ||  // b
            (lua_type(l, 9) != LUA_TNUMBER &&
             lua_type(l, 9) != LUA_TNIL) ||  // a
            lua_type(l, 10) != LUA_TNUMBER  // pt_size
            ) {  // (optional: letter_spacing)
        wrongargs: ;
        lua_pushstring(l, "expected 10+ args, namely "
            "\"font\" (font), "
            "\"text\" (string), \"width\" (number or nil), "
            "\"x\" (number), \"y\" (number), "
            "\"r\" (number or nil), "
            "\"g\" (number or nil), "
            "\"b\" (number or nil), "
            "\"a\" (number or nil), "
            "\"pt_size\" (number), "
            "\"letter_spacing\" (number or nil), "
            "\"outline_size\" (number or nil), "
            "\"invert_outline\" (boolean or nil)");
        return lua_error(l);
    }
    scriptobjref *fref = ((scriptobjref*)lua_touserdata(l, 1));
    rfsfont *f = (rfsfont *)(uintptr_t)fref->value;
    if (!f) goto wrongargs;

    const char *text = lua_tostring(l, 2);
    int width = -1;
    if (lua_type(l, 3) == LUA_TNUMBER) {
        width = (lua_isinteger(l, 3) ? lua_tointeger(l, 3) :
            round(lua_tonumber(l, 3)));
    }
    int x = (lua_isinteger(l, 4) ? lua_tointeger(l, 4) :
        round(lua_tonumber(l, 4)));
    int y = (lua_isinteger(l, 5) ? lua_tointeger(l, 5) :
        round(lua_tonumber(l, 5)));
    double r = 1; double g = 1; double b = 1; double a = 1;
    if (lua_type(l, 6) == LUA_TNUMBER)
        r = fmax(0, fmin(1, lua_tonumber(l, 6)));
    if (lua_type(l, 7) == LUA_TNUMBER)
        g = fmax(0, fmin(1, lua_tonumber(l, 7)));
    if (lua_type(l, 8) == LUA_TNUMBER)
        b = fmax(0, fmin(1, lua_tonumber(l, 8)));
    if (lua_type(l, 9) == LUA_TNUMBER)
        a = fmax(0, fmin(1, lua_tonumber(l, 9)));
    double pt_size = lua_tonumber(l, 10);
    double letterspacing = 0;
    if (lua_gettop(l) >= 11 && lua_type(l, 11) == LUA_TNUMBER)
        letterspacing = lua_tonumber(l, 11);
    double outlinesize = 0;
    if (lua_gettop(l) >= 12 && lua_type(l, 12) == LUA_TNUMBER)
        outlinesize = lua_tonumber(l, 12);
    int outlineinverted = 0;
    if (lua_gettop(l) >= 13 && lua_type(l, 13) == LUA_TBOOLEAN)
        outlineinverted = lua_toboolean(l, 13);
    if (!text) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    if (!graphicsfont_Draw(
            f, text, width, x, y,
            r, g, b, a, pt_size, letterspacing,
            outlinesize, outlineinverted
            )) {
        lua_pushstring(l, "internal error while drawing");
        return lua_error(l);
    }
    return 0;
}


void scriptcoregraphicsfont_AddFunctions(lua_State *l) {
    lua_pushcfunction(l, _graphicsfont_get);
    lua_setglobal(l, "_graphicsfont_get");
    lua_pushcfunction(l, _graphicsfont_calcwidth);
    lua_setglobal(l, "_graphicsfont_calcwidth");
    lua_pushcfunction(l, _graphicsfont_calcheight);
    lua_setglobal(l, "_graphicsfont_calcheight");
    lua_pushcfunction(l, _graphicsfont_textwrap);
    lua_setglobal(l, "_graphicsfont_textwrap");
    lua_pushcfunction(l, _graphicsfont_draw);
    lua_setglobal(l, "_graphicsfont_draw");
    lua_pushcfunction(l, _graphicsfont_calcoutlinepx);
    lua_setglobal(l, "_graphicsfont_calcoutlinepx");
}
