// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <math.h>
#include <string.h>

#include "math2d.h"
#include "room.h"
#include "roomcam.h"
#include "roomcolmap.h"
#include "roomlayer.h"
#include "roomobject.h"
#include "roomobject_movable.h"
#include "roomserialize.h"
#include "scriptcore.h"
#include "scriptcoreroom.h"


static int _roomobj_newinvismovable(lua_State *l) {
    movable *mov = movable_Create();
    if (!mov) {
        lua_pushstring(l, "failed to create object");
        return lua_error(l);
    }
    scriptobjref *ref = lua_newuserdata(l, sizeof(*ref));
    if (!ref) {
        roomobj_Destroy(mov->obj);
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    memset(ref, 0, sizeof(*ref));
    ref->magic = OBJREFMAGIC;
    ref->type = OBJREF_ROOMOBJ;
    ref->value = (uintptr_t)mov->obj->id;
    return 1;
}

static int _movable_setemit(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ROOMOBJ) {
        wrongargs:;
        lua_pushstring(l, "expected arg of type movable");
        return lua_error(l);
    }
    roomobj *obj = roomobj_ById(
        (uint64_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!obj || obj->objtype != ROOMOBJ_MOVABLE)
        goto wrongargs;
    int32_t r = 0;
    int32_t g = 0;
    int32_t b = 0;
    int64_t radius = 0;
    if (lua_gettop(l) > 1) {
        if (lua_gettop(l) < 4 ||
                lua_type(l, 2) != LUA_TNUMBER ||
                lua_type(l, 3) != LUA_TNUMBER ||
                lua_type(l, 4) != LUA_TNUMBER) {
            lua_pushstring(l, "expected args #2, #3, #4 to "
                "be of type number if not omitted");
            return lua_error(l);
        }
        r = round(fmax(0.0, fmin(1.0, lua_tonumber(l, 2))) *
            LIGHT_COLOR_SCALAR);
        g = round(fmax(0.0, fmin(1.0, lua_tonumber(l, 3))) *
            LIGHT_COLOR_SCALAR);
        b = round(fmax(0.0, fmin(1.0, lua_tonumber(l, 4))) *
            LIGHT_COLOR_SCALAR);
        radius = ONE_METER_IN_UNITS * 6;
        if (radius > MAX_LIGHT_RANGE)
            radius = MAX_LIGHT_RANGE;
        if (r == 0 && g == 0 && b == 0)
            radius = 0;
    }
    if (lua_gettop(l) >= 5) {
        if (lua_type(l, 5) != LUA_TNUMBER) {
            lua_pushstring(l, "expected arg #5 to "
                "be of type number if not omitted");
            return lua_error(l);
        }
        if (lua_isinteger(l, 5))
            radius = lua_tointeger(l, 5);
        else
            radius = lua_tonumber(l, 5);
        if (radius < 0) radius = 0;
        if (radius > MAX_LIGHT_RANGE)
            radius = MAX_LIGHT_RANGE;
    }
    movable *cmov = (movable*)obj->objdata;
    cmov->does_emit = (radius > 0);
    cmov->emit_r = r;
    cmov->emit_g = g;
    cmov->emit_b = b;
    cmov->emit_range = radius;
    return 0;
}

static int _roomcam_renderstats(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ROOMOBJ) {
        wrongargs:;
        lua_pushstring(l, "expected arg of type roomcam");
        return lua_error(l);
    }
    roomobj *obj = roomobj_ById(
        (uint64_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!obj || obj->objtype != ROOMOBJ_CAMERA)
        goto wrongargs;
    roomcam *cam = (roomcam *)obj->objdata;
    renderstatistics *stats = roomcam_GetStats(cam);
    lua_newtable(l);
    lua_pushstring(l, "base_geometry_slices_rendered");
    lua_pushinteger(l,
        (stats ? stats->base_geometry_slices_rendered :
         (int64_t)0)
    );
    lua_settable(l, -3);
    lua_pushstring(l, "base_geometry_rays_cast");
    lua_pushinteger(l,
        (stats ? stats->base_geometry_rays_cast :
         (int64_t)0)
    );
    lua_settable(l, -3);
    lua_pushstring(l, "base_geometry_rooms_recursed");
    lua_pushinteger(l,
        (stats ? stats->base_geometry_rooms_recursed :
         (int64_t)0)
    );
    lua_settable(l, -3);
    lua_pushstring(l, "fps");
    lua_pushinteger(l,
        (stats ? stats->fps : (int64_t)0)
    );
    lua_settable(l, -3);
    lua_pushstring(l, "canvas_width");
    lua_pushinteger(l,
        (stats ? stats->last_canvas_width :
         (int64_t)0)
    );
    lua_settable(l, -3);
    lua_pushstring(l, "canvas_height");
    lua_pushinteger(l,
        (stats ? stats->last_canvas_height :
         (int64_t)0)
    );
    lua_settable(l, -3);
    lua_pushstring(l, "canvas_fov");
    lua_pushnumber(l,
        (double)(stats ? stats->last_fov :
         (int64_t)0) / (double)ANGLE_SCALAR
    );
    lua_settable(l, -3);
    lua_pushstring(l, "canvas_fovh");
    lua_pushinteger(l,
        (double)(stats ? stats->last_fovh :
         (int64_t)0) / (double)ANGLE_SCALAR
    );
    lua_settable(l, -3);
    lua_pushstring(l, "canvas_fovv");
    lua_pushinteger(l,
        (double)(stats ? stats->last_fovv :
         (int64_t)0) / (double)ANGLE_SCALAR
    );
    lua_settable(l, -3);
    return 1;
}

static int _roomlayer_deserializeroomstolayer(lua_State *l) {
    if (lua_gettop(l) < 2 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ROOMLAYER ||
            lua_type(l, 2) != LUA_TTABLE) {
        lua_pushstring(l, "expected args of type roomlayer, table");
        return lua_error(l);
    }
    roomlayer *lr = ((roomlayer *)
        (uintptr_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!lr) {
        lua_pushstring(l, "expected args of type roomlayer, table");
        return lua_error(l);
    }
    char *err = NULL;
    if (!roomserialize_lua_DeserializeRoomGeometries(
            l, lr, 2, &err
            )) {
        lua_pushstring(l, (err ? err : "unknown error"));
        free(err);
        return lua_error(l);
    }
    if (!roomserialize_lua_SetRoomProperties(
            l, lr, 2, &err
            )) {
        lua_pushstring(l, (err ? err : "unknown error"));
        free(err);
        return lua_error(l);
    }
    return 0;
}


static int _roomlayer_applyroomprops(lua_State *l) {
    if (lua_gettop(l) < 2 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ROOMLAYER ||
            lua_type(l, 2) != LUA_TTABLE) {
        lua_pushstring(l, "expected args of type roomlayer, table");
        return lua_error(l);
    }
    roomlayer *lr = ((roomlayer *)
        (uintptr_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!lr) {
        lua_pushstring(l, "expected args of type roomlayer, table");
        return lua_error(l);
    }
    char *err = NULL;
    if (!roomserialize_lua_SetRoomProperties(
            l, lr, 2, &err
            )) {
        lua_pushstring(l, (err ? err : "unknown error"));
        free(err);
        return lua_error(l);
    }
    return 0;
}

static int _roomlayer_addroom(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ROOMLAYER) {
        lua_pushstring(l, "expected 1 arg of type roomlayer");
        return lua_error(l);
    }
    roomlayer *lr = ((roomlayer *)
        (uintptr_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!lr) {
        lua_pushstring(l, "expected 1 arg of type roomlayer");
        return lua_error(l);
    }
    uint64_t id = 0;
    if (lua_gettop(l) >= 2 && lua_type(l, 2) == LUA_TNUMBER) {
        if (lua_isinteger(l, 2)) id = (
            lua_tointeger(l, 2) > 0 ? lua_tointeger(l, 2) : 0
        );
        else id = lua_tonumber(l, 2);
    }
    if (id == 0)
        id = room_GetNewId(lr);
    room *r = (id > 0 ? room_Create(lr, id) : NULL);
    if (id == 0 || !r) {
        lua_pushstring(l, "failed to create room");
        return lua_error(l);
    }
    lua_pushinteger(l, id);
    return 1;
}

static int _roomobj_getangle(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ROOMOBJ) {
        wrongargs:;
        lua_pushstring(l, "expected arg of type roomobj");
        return lua_error(l);
    }
    roomobj *obj = roomobj_ById(
        (uint64_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!obj)
        goto wrongargs;
    lua_pushnumber(l, obj->anglef);
    return 1;
}

static int _roomobj_setangle(lua_State *l) {
    if (lua_gettop(l) < 2 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ROOMOBJ ||
            lua_type(l, 2) != LUA_TNUMBER) {
        wrongargs:;
        lua_pushstring(l, "expected args of type roomobj, "
            "number");
        return lua_error(l);
    }
    roomobj *obj = roomobj_ById(
        (uint64_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!obj)
        goto wrongargs;

    double angle = math_fixanglef(lua_tonumber(l, 2));
    if (angle != obj->anglef) {
        obj->anglef = angle;
        obj->angle = round(
            ((double)ANGLE_SCALAR * (double)obj->anglef)
        );
    }
    return 0;
}



static int _roomcam_getvangle(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ROOMOBJ) {
        wrongargs:;
        lua_pushstring(l, "expected arg of type roomcam");
        return lua_error(l);
    }
    roomobj *obj = roomobj_ById(
        (uint64_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!obj || obj->objtype != ROOMOBJ_CAMERA)
        goto wrongargs;
    lua_pushnumber(l,
        ((roomcam *)obj->objdata)->vanglef
    );
    return 1;
}

static int _roomcam_setvangle(lua_State *l) {
    if (lua_gettop(l) < 2 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ROOMOBJ ||
            lua_type(l, 2) != LUA_TNUMBER) {
        wrongargs:;
        lua_pushstring(l, "expected args of type roomcam, "
            "number");
        return lua_error(l);
    }
    roomobj *obj = roomobj_ById(
        (uint64_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!obj || obj->objtype != ROOMOBJ_CAMERA)
        goto wrongargs;

    double vangle = math_fixanglef(lua_tonumber(l, 2));

    roomcam *cam = ((roomcam *)obj->objdata);
    if (vangle != cam->vanglef) {
        cam->vanglef = vangle;
        cam->vangle = round(
            ((int64_t)ANGLE_SCALAR) * cam->vanglef
        );
    }
    return 0;
}


static int _roomobj_getpos(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ROOMOBJ) {
        wrongargs:;
        lua_pushstring(l, "expected arg of type roomobj");
        return lua_error(l);
    }
    roomobj *obj = roomobj_ById(
        (uint64_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!obj)
        goto wrongargs;
    lua_pushinteger(l, obj->x);
    lua_pushinteger(l, obj->y);
    lua_pushinteger(l, obj->z);
    return 3;
}

static int _roomobj_setlayer(lua_State *l) {
    if (lua_gettop(l) < 2 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ROOMOBJ ||
            lua_type(l, 2) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 2))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 2))->type !=
                OBJREF_ROOMLAYER) {
        wrongargs:;
        lua_pushstring(l, "expected args of type roomobj, "
            "roomlayer");
        return lua_error(l);
    }
    roomobj *obj = roomobj_ById(
        (uint64_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!obj)
        goto wrongargs;
    roomlayer *lr = ((roomlayer *)
        (uintptr_t)((scriptobjref *)lua_touserdata(l, 2))->value
    );
    if (!lr)
        goto wrongargs;
    roomobj_SetLayer(obj, lr);
    return 0;
}

static int _roomobj_setpos(lua_State *l) {
    if (lua_gettop(l) < 4 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ROOMOBJ ||
            lua_type(l, 2) != LUA_TNUMBER ||
            lua_type(l, 3) != LUA_TNUMBER ||
            lua_type(l, 4) != LUA_TNUMBER) {
        wrongargs:;
        lua_pushstring(l, "expected args of type roomobj, "
            "number, number, number");
        return lua_error(l);
    }
    roomobj *obj = roomobj_ById(
        (uint64_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!obj)
        goto wrongargs;

    int64_t x = 0;
    if (lua_isinteger(l, 2)) x = lua_tointeger(l, 2);
    else x = round(lua_tonumber(l, 2));
    int64_t y = 0;
    if (lua_isinteger(l, 3)) y = lua_tointeger(l, 3);
    else y = round(lua_tonumber(l, 3));
    int64_t z = 0;
    if (lua_isinteger(l, 4)) z = lua_tointeger(l, 4);
    else z = round(lua_tonumber(l, 4));

    int64_t oldx = obj->x;
    int64_t oldy = obj->y;
    obj->x = x;
    obj->y = y;
    obj->z = z;
    roomobj_UpdatePos(obj, 1, oldx, oldy);
    #ifndef NDEBUG
    if (obj->parentlayer) {
        roomcolmap_Debug_AssertObjectIsRegistered(
            obj->parentlayer->colmap, obj
        );
    }
    #endif
    return 0;
}

static int _roomlayer_getroomcount(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ROOMLAYER) {
        lua_pushstring(l, "expected 1 arg of type roomlayer");
        return lua_error(l);
    }
    roomlayer *lr = ((roomlayer *)
        (uintptr_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!lr) {
        lua_pushstring(l, "expected 1 arg of type roomlayer");
        return lua_error(l);
    }
    lua_pushinteger(l, lr->roomcount);
    return 1;
}

static int _roomcam_new(lua_State *l) {
    roomcam *rc = roomcam_Create();
    if (!rc) {
        lua_pushstring(l, "failed to create cam");
        return lua_error(l);
    }
    scriptobjref *ref = lua_newuserdata(l, sizeof(*ref));
    if (!ref) {
        roomobj_Destroy(rc->obj);
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    memset(ref, 0, sizeof(*ref));
    ref->magic = OBJREFMAGIC;
    ref->type = OBJREF_ROOMOBJ;
    ref->value = (uintptr_t)rc->obj->id;
    return 1; 
}

static int _roomcam_render(lua_State *l) {
    if (lua_gettop(l) < 5 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ROOMOBJ ||
            lua_type(l, 2) != LUA_TNUMBER ||
            lua_type(l, 3) != LUA_TNUMBER ||
            lua_type(l, 4) != LUA_TNUMBER ||
            lua_type(l, 5) != LUA_TNUMBER) {
        wrongargs:;
        lua_pushstring(l, "expected args of type roomcam, "
            "number, number, number, number");
        return lua_error(l);
    }
    roomobj *obj = roomobj_ById(
        (uint64_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!obj || obj->objtype != ROOMOBJ_CAMERA)
        goto wrongargs;
    int x = round(lua_tonumber(l, 2));
    int y = round(lua_tonumber(l, 3));
    int w = round(lua_tonumber(l, 4));
    int h = round(lua_tonumber(l, 5));
    if (!roomcam_Render((roomcam *)obj->objdata, x, y, w, h)) {
        lua_pushstring(l, "roomcam render error");
        return lua_error(l);
    }
    return 0;
}


static int _roomobj_destroy(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ROOMOBJ) {
        lua_pushstring(l, "expected 1 arg of type roomobj");
        return lua_error(l);
    }
    roomobj *obj = roomobj_ById(
        (uint64_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!obj) {
        lua_pushstring(l, "expected 1 arg of type roomobj");
        return lua_error(l);
    }
    ((scriptobjref *)lua_touserdata(l, 1))->value = 0;
    roomobj_Destroy(obj);
    return 0;
}

static int _roomlayer_new(lua_State *l) {
    uint64_t id = 0;
    if (lua_gettop(l) >= 1 &&
            lua_type(l, 1) == LUA_TNUMBER) {
        if (lua_isinteger(l, 1)) id = lua_tointeger(l, 1);
        else id = round(lua_tonumber(l, 1));
        if (id < 1) {
            lua_pushstring(l, "roomlayer id (first arg) "
                "must be 1 or higher, or nil/unspecified");
            return lua_error(l);
        }
    }
    if (id == 0) {
        id = roomlayer_GetNewId();
        if (id == 0) {
            lua_pushstring(l, "failed to assign roomlayer id");
            return lua_error(l);
        }
    }
    roomlayer *lr = roomlayer_Create(id);
    if (!lr) {
        lua_pushstring(l, "failed to create roomlayer");
        return lua_error(l);
    } 
    scriptobjref *ref = lua_newuserdata(l, sizeof(*ref));
    if (!ref) {
        roomlayer_Destroy(lr);
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    memset(ref, 0, sizeof(*ref));
    ref->magic = OBJREFMAGIC;
    ref->type = OBJREF_ROOMLAYER;
    ref->value = (uintptr_t)lr;
    return 1;
}

static int _roomlayer_destroy(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ROOMLAYER) {
        lua_pushstring(l, "expected 1 arg of type roomlayer");
        return lua_error(l);
    }
    roomlayer *lr = ((roomlayer *)
        (uintptr_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!lr) {
        lua_pushstring(l, "expected 1 arg of type roomlayer");
        return lua_error(l);
    }
    ((scriptobjref *)lua_touserdata(l, 1))->value = 0;
    roomlayer_Destroy(lr);
    return 0;
}

static int _roomlayer_global_getmaxroomslimit(lua_State *l) {
    lua_pushinteger(l, MAX_ROOMS_PER_LAYER);
    return 1;
}

static int _roomlayer_global_getmaxwallslimit(lua_State *l) {
    lua_pushinteger(l, ROOM_MAX_CORNERS);
    return 1;
}

static int _roomlayer_global_getmaxdecalslimit(lua_State *l) {
    lua_pushinteger(l, DECALS_MAXCOUNT);
    return 1;
}

static int _roomlayer_global_getsmallestcolliderlimit(lua_State *l) {
    lua_pushinteger(l, SMALLEST_COLLIDER_UNITS);
    return 1;
}

static int _roomlayer_global_getlargestcolliderlimit(lua_State *l) {
    lua_pushinteger(l, LARGEST_COLLIDER_UNITS);
    return 1;
}

static int _roomlayer_getallrooms(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_ROOMLAYER) {
        lua_pushstring(l, "expected 1 arg of type roomlayer");
        return lua_error(l);
    }
    roomlayer *lr = ((roomlayer *)
        (uintptr_t)((scriptobjref *)lua_touserdata(l, 1))->value
    );
    if (!lr) {
        lua_pushstring(l, "expected 1 arg of type roomlayer");
        return lua_error(l);
    }
    lua_newtable(l);
    int32_t i = 0;
    while (i < lr->roomcount) {
        lua_pushnumber(l, i + 1);
        lua_pushinteger(l, lr->rooms[i]->id);
        lua_settable(l, -3);
        i++;
    }
    return 1;
}

static int _roomlayer_global_getonemeterunits(lua_State *l) {
    lua_pushinteger(l, ONE_METER_IN_UNITS);
    return 1;
}

void scriptcoreroom_AddFunctions(lua_State *l) {
    lua_pushcfunction(l, _roomlayer_addroom);
    lua_setglobal(l, "_roomlayer_addroom");
    lua_pushcfunction(l, _roomlayer_new);
    lua_setglobal(l, "_roomlayer_new");
    lua_pushcfunction(l, _roomlayer_destroy);
    lua_setglobal(l, "_roomlayer_destroy");
    lua_pushcfunction(l, _roomcam_new);
    lua_setglobal(l, "_roomcam_new");
    lua_pushcfunction(l, _roomobj_destroy);
    lua_setglobal(l, "_roomobj_destroy");
    lua_pushcfunction(l, _roomcam_render);
    lua_setglobal(l, "_roomcam_render");
    lua_pushcfunction(l, _roomobj_setpos);
    lua_setglobal(l, "_roomobj_setpos");
    lua_pushcfunction(l, _roomobj_getpos);
    lua_setglobal(l, "_roomobj_getpos");
    lua_pushcfunction(l, _roomobj_getangle);
    lua_setglobal(l, "_roomobj_getangle");
    lua_pushcfunction(l, _roomobj_setangle);
    lua_setglobal(l, "_roomobj_setangle");
    lua_pushcfunction(l, _roomobj_setlayer);
    lua_setglobal(l, "_roomobj_setlayer");
    lua_pushcfunction(l, _roomcam_getvangle);
    lua_setglobal(l, "_roomcam_getvangle");
    lua_pushcfunction(l, _roomcam_setvangle);
    lua_setglobal(l, "_roomcam_setvangle");
    lua_pushcfunction(l, _roomlayer_global_getmaxroomslimit);
    lua_setglobal(l, "_roomlayer_global_getmaxroomslimit");
    lua_pushcfunction(l, _roomlayer_global_getmaxwallslimit);
    lua_setglobal(l, "_roomlayer_global_getmaxwallslimit");
    lua_pushcfunction(l, _roomlayer_global_getmaxdecalslimit);
    lua_setglobal(l, "_roomlayer_global_getmaxdecalslimit");
    lua_pushcfunction(l, _roomlayer_global_getsmallestcolliderlimit);
    lua_setglobal(l, "_roomlayer_global_getsmallestcolliderlimit");
    lua_pushcfunction(l, _roomlayer_global_getlargestcolliderlimit);
    lua_setglobal(l, "_roomlayer_global_getlargestcolliderlimit");
    lua_pushcfunction(l, _roomlayer_getroomcount);
    lua_setglobal(l, "_roomlayer_getroomcount");
    lua_pushcfunction(l, _roomlayer_applyroomprops);
    lua_setglobal(l, "_roomlayer_applyroomprops");
    lua_pushcfunction(l, _roomlayer_getallrooms);
    lua_setglobal(l, "_roomlayer_getallrooms");
    lua_pushcfunction(l, _roomlayer_global_getonemeterunits);
    lua_setglobal(l, "_roomlayer_global_getonemeterunits");
    lua_pushcfunction(l, _roomlayer_deserializeroomstolayer);
    lua_setglobal(l, "_roomlayer_deserializeroomstolayer");
    lua_pushcfunction(l, _roomcam_renderstats);
    lua_setglobal(l, "_roomcam_renderstats");
    lua_pushcfunction(l, _roomobj_newinvismovable);
    lua_setglobal(l, "_roomobj_newinvismovable");
    lua_pushcfunction(l, _movable_setemit);
    lua_setglobal(l, "_movable_setemit");
}
