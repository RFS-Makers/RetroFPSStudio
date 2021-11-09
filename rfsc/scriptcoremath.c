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
#include <string.h>
#include <unistd.h>

#include "mathdefs.h"
#include "math2d.h"
#include "math3d.h"
#include "scriptcore.h"
#include "scriptcoreerror.h"
#include "scriptcoremath.h"

static int _math_normalize(lua_State *l) {
    if (lua_gettop(l) != 3 ||
            lua_type(l, 1) != LUA_TNUMBER ||
            lua_type(l, 2) != LUA_TNUMBER ||
            lua_type(l, 3) != LUA_TNUMBER) {
        lua_pushstring(l, "expected 3 args of type number");
        return lua_error(l);
    }

    double v[3];
    v[0] = lua_tonumber(l, 1);
    v[1] = lua_tonumber(l, 2);
    v[2] = lua_tonumber(l, 3);

    lua_newtable(l);
    lua_pushnumber(l, 1);
    lua_pushnumber(l, v[0]);
    lua_settable(l, -3);
    lua_pushnumber(l, 2);
    lua_pushnumber(l, v[1]);
    lua_settable(l, -3);
    lua_pushnumber(l, 3);
    lua_pushnumber(l, v[2]);
    lua_settable(l, -3);
    return 1;
}


/*static int _math_quat_rotatevec(lua_State *l) {
    if (lua_gettop(l) != 2 ||
            lua_type(l, 1) != LUA_TTABLE ||
            lua_type(l, 2) != LUA_TTABLE) {
        lua_pushstring(l, "expected 2 args of type table");
        return lua_error(l);
    }

    double v[3];
    lua_pushnumber(l, 1);
    lua_gettable(l, 2);
    v[0] = lua_tonumber(l, -1);
    lua_pushnumber(l, 2);
    lua_gettable(l, 2);
    v[1] = lua_tonumber(l, -1);
    lua_pushnumber(l, 3);
    lua_gettable(l, 2);
    v[2] = lua_tonumber(l, -1);

    double quat[4];
    lua_pushnumber(l, 1);
    lua_gettable(l, 1);
    quat[0] = lua_tonumber(l, -1);
    lua_pushnumber(l, 2);
    lua_gettable(l, 1);
    quat[1] = lua_tonumber(l, -1);
    lua_pushnumber(l, 3);
    lua_gettable(l, 1);
    quat[2] = lua_tonumber(l, -1);
    lua_pushnumber(l, 4);
    lua_gettable(l, 1);
    quat[3] = lua_tonumber(l, -1);

    vec3_rotate_quat(v, v, quat);

    lua_newtable(l);
    lua_pushnumber(l, 1);
    lua_pushnumber(l, v[0]);
    lua_settable(l, -3);
    lua_pushnumber(l, 2);
    lua_pushnumber(l, v[1]);
    lua_settable(l, -3);
    lua_pushnumber(l, 3);
    lua_pushnumber(l, v[2]);
    lua_settable(l, -3);
    return 1;
}*/


/*static int _math_quat_fromeuler(lua_State *l) {
    if (lua_gettop(l) != 3 ||
            lua_type(l, 1) != LUA_TNUMBER ||
            lua_type(l, 2) != LUA_TNUMBER ||
            lua_type(l, 3) != LUA_TNUMBER) {
        lua_pushstring(l, "expected 3 args of type number");
        return lua_error(l);
    }

    double angles[3];
    angles[0] = lua_tonumber(l, 1) * M_PI / 180.0;
    angles[1] = lua_tonumber(l, 2) * M_PI / 180.0;
    angles[2] = lua_tonumber(l, 3) * M_PI / 180.0;

    double quat[4];
    quat_from_euler(quat, angles);

    lua_newtable(l);
    lua_pushnumber(l, 1);
    lua_pushnumber(l, quat[0]);
    lua_settable(l, -3);
    lua_pushnumber(l, 2);
    lua_pushnumber(l, quat[1]);
    lua_settable(l, -3);
    lua_pushnumber(l, 3);
    lua_pushnumber(l, quat[2]);
    lua_settable(l, -3);
    lua_pushnumber(l, 4);
    lua_pushnumber(l, quat[3]);
    lua_settable(l, -3);
    return 1;
}*/


static int _math_round(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TNUMBER) {
        lua_pushstring(l, "expected 1 arg of type number");
        return lua_error(l);
    }
    if (lua_isinteger(l, 1)) {
        lua_pushinteger(l, lua_tointeger(l, 1));
        return 1;
    }
    lua_pushinteger(l, round(lua_tonumber(l, 1)));
    return 1;
}

static int _math_angle2d(lua_State *l) {
    if (lua_gettop(l) < 2 || lua_type(l, 1) != LUA_TNUMBER ||
            lua_type(l, 2) != LUA_TNUMBER) {
        lua_pushstring(l, "expected 2 args of type number");
        return lua_error(l);
    }
    lua_pushnumber(l, math_angle2df(
        lua_tonumber(l, 1), lua_tonumber(l, 2)
    ));
    return 1;
}

static int _math_rotate2d(lua_State *l) {
    if (lua_gettop(l) < 3 || lua_type(l, 1) != LUA_TNUMBER ||
            lua_type(l, 2) != LUA_TNUMBER ||
            lua_type(l, 3) != LUA_TNUMBER) {
        lua_pushstring(l, "expected 3 args of type number");
        return lua_error(l);
    }
    if (lua_isinteger(l, 1) && lua_isinteger(l, 2)) {
        int64_t x,y;
        x = lua_tointeger(l, 1);
        y = lua_tointeger(l, 2);
        math_rotate2di(&x, &y, lua_tonumber(l, 3) * ANGLE_SCALAR);
        lua_pushinteger(l, x);
        lua_pushinteger(l, y);
        return 2;
    }
    double x,y;
    x = lua_tonumber(l, 1);
    y = lua_tonumber(l, 2);
    math_rotate2df(&x, &y, lua_tonumber(l, 3));
    lua_pushnumber(l, x);
    lua_pushnumber(l, y);
    return 2;
}

static int _math_lineintersect2d(lua_State *l) {
    if (lua_gettop(l) < 8 || lua_type(l, 1) != LUA_TNUMBER ||
            lua_type(l, 2) != LUA_TNUMBER ||
            lua_type(l, 3) != LUA_TNUMBER ||
            lua_type(l, 4) != LUA_TNUMBER ||
            lua_type(l, 5) != LUA_TNUMBER ||
            lua_type(l, 6) != LUA_TNUMBER ||
            lua_type(l, 7) != LUA_TNUMBER ||
            lua_type(l, 8) != LUA_TNUMBER) {
        lua_pushstring(l, "expected 8 args of type number");
        return lua_error(l);
    }
    if (lua_isinteger(l, 1) && lua_isinteger(l, 2) &&
            lua_isinteger(l, 3) && lua_isinteger(l, 4) &&
            lua_isinteger(l, 5) && lua_isinteger(l, 6) &&
            lua_isinteger(l, 7) && lua_isinteger(l, 8)) {
        int64_t x,y;
        if (math_lineintersect2di(
                lua_tointeger(l, 1), lua_tointeger(l, 2),
                lua_tointeger(l, 3), lua_tointeger(l, 4),
                lua_tointeger(l, 5), lua_tointeger(l, 6),
                lua_tointeger(l, 7), lua_tointeger(l, 8),
                &x, &y)) {
            lua_pushinteger(l, x);
            lua_pushinteger(l, y);
            return 1;
        }
        lua_pushnil(l);
        lua_pushnil(l);
        return 2;
    }
    double x,y;
    if (math_lineintersect2df(
            lua_tonumber(l, 1), lua_tonumber(l, 2),
            lua_tonumber(l, 3), lua_tonumber(l, 4),
            lua_tonumber(l, 5), lua_tonumber(l, 6),
            lua_tonumber(l, 7), lua_tonumber(l, 8),
            &x, &y)) {
        lua_pushnumber(l, x);
        lua_pushnumber(l, y);
        return 1;
    }
    lua_pushnil(l);
    lua_pushnil(l);
    return 2;
}

static int _math_polyintersect2d(lua_State *l) {
    if (lua_gettop(l) < 10) {
        lua_pushstring(l, "expected 10 or more "
            "args of type number");
        return lua_error(l);
    }
    int all_int = 1;
    {
        int i = 1;
        while (i <= lua_gettop(l)) {
            if (lua_type(l, i) != LUA_TNUMBER) {
                lua_pushstring(l, "expected 10 or more args "
                    "of type number");
                return lua_error(l);
            }
            if (all_int && !lua_isinteger(l, i) &&
                    lua_tonumber(l, i) !=
                    round(lua_tonumber(l, i))) all_int = 0;
            i++;
        }
    }
    int corner_count = (lua_gettop(l) - 4) / 2;
    assert(corner_count >= 3);
    if (all_int) {
        int64_t lx1, ly1, lx2, ly2;
        lx1 = (lua_isinteger(l, 1) ?
            lua_tointeger(l, 1) : round(lua_tonumber(l, 1)));
        ly1 = (lua_isinteger(l, 2) ?
            lua_tointeger(l, 2) : round(lua_tonumber(l, 2)));
        lx2 = (lua_isinteger(l, 3) ?
            lua_tointeger(l, 3) : round(lua_tonumber(l, 3)));
        ly2 = (lua_isinteger(l, 4) ?
            lua_tointeger(l, 4) : round(lua_tonumber(l, 4)));
        int64_t *cx = malloc(sizeof(*cx) * corner_count);
        int64_t *cy = malloc(sizeof(*cy) * corner_count);
        if (!cx || !cy) {
            free(cx); free(cy);
            fprintf(stderr, "rfsc/scriptcoremath.c: "
                "error: OUT OF MEMORY in _math_polyintersect2d\n");
            _exit(1);
            return 0;
        }
        int i = 0;
        while (i < corner_count) {
            cx[i] = (lua_isinteger(l, 4 + 2 * i) ?
                lua_tointeger(l, 4 + 2 * i) :
                round(lua_tonumber(l, 4 + 2 * i)));
            cy[i] = (lua_isinteger(l, 4 + 2 * i + 1) ?
                lua_tointeger(l, 4 + 2 * i + 1) :
                round(lua_tonumber(l, 4 + 2 * i + 1)));
            i++;
        }
        int iwall;
        int64_t ix, iy;
        int result = math_polyintersect2di(
            lx1, ly1, lx2, ly2, corner_count, cx, cy,
            &iwall, &ix, &iy
        );
        free(cx); free(cy);
        if (result) {
            lua_pushinteger(l, ix);
            lua_pushinteger(l, iy);
            lua_pushinteger(l, iwall);
            return 1;
        }
        return 0;
    } else {
        lua_pushstring(l, "the engine only implements this "
            "for non-fractional numbers");
        return lua_error(l);
    }
}

static int _math_polycontains2d(lua_State *l) {
    if (lua_gettop(l) < 8) {
        lua_pushstring(l, "expected 8 or more "
            "args of type number");
        return lua_error(l);
    }
    int all_int = 1;
    {
        int i = 1;
        while (i <= lua_gettop(l)) {
            if (lua_type(l, i) != LUA_TNUMBER) {
                lua_pushstring(l, "expected 8 or more args "
                    "of type number");
                return lua_error(l);
            }
            if (all_int && !lua_isinteger(l, i) &&
                    lua_tonumber(l, i) !=
                    round(lua_tonumber(l, i))) all_int = 0;
            i++;
        }
    }
    int corner_count = (lua_gettop(l) - 2) / 2;
    assert(corner_count >= 3);
    if (all_int) {
        int64_t px, py;
        px = (lua_isinteger(l, 1) ? lua_tointeger(l, 1) :
            round(lua_tonumber(l, 1)));
        py = (lua_isinteger(l, 2) ? lua_tointeger(l, 2) :
            round(lua_tonumber(l, 2)));
        int64_t *cx = malloc(sizeof(*cx) * corner_count);
        int64_t *cy = malloc(sizeof(*cy) * corner_count);
        if (!cx || !cy) {
            free(cx); free(cy);
            fprintf(stderr, "rfsc/scriptcoremath.c: "
                "error: OUT OF MEMORY in _math_polycontains2d\n");
            _exit(1);
            return 0;
        }
        int i = 0;
        while (i < corner_count) {
            cx[i] = (lua_isinteger(l, 2 + 2 * i) ?
                lua_tointeger(l, 2 + 2 * i) :
                round(lua_tonumber(l, 2 + 2 * i)));
            cy[i] = (lua_isinteger(l, 2 + 2 * i + 1) ?
                lua_tointeger(l, 2 + 2 * i + 1) :
                round(lua_tonumber(l, 2 + 2 * i)));
            i++;
        }
        int result = math_polycontains2di(
            px, py, corner_count, cx, cy
        );
        free(cx); free(cy);
        lua_pushboolean(l, result);
        return 1;
    } else {
        lua_pushstring(l, "the engine only implements this "
            "for non-fractional numbers");
        return lua_error(l);
    }
}


void scriptcoremath_AddFunctions(lua_State *l) {
    lua_pushcfunction(l, _math_round);
    lua_setglobal(l, "_math_round");
    lua_pushcfunction(l, _math_normalize);
    lua_setglobal(l, "_math_normalize");
    lua_pushcfunction(l, _math_angle2d);
    lua_setglobal(l, "_math_angle2d");
    lua_pushcfunction(l, _math_rotate2d);
    lua_setglobal(l, "_math_rotate2d");
    lua_pushcfunction(l, _math_lineintersect2d);
    lua_setglobal(l, "_math_lineintersect2d");
    lua_pushcfunction(l, _math_polyintersect2d);
    lua_setglobal(l, "_math_polyintersect2d");
    lua_pushcfunction(l, _math_polycontains2d);
    lua_setglobal(l, "_math_polycontains2d");
}
