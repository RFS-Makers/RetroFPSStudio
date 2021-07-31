// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#include "compileconfig.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hash.h"
#include "math2d.h"
#include "room.h"
#include "roomcolmap.h"
#include "roomdefs.h"
#include "roomlayer.h"
#include "roomserialize.h"

static int _extract_apply_tex_props(
        lua_State *l, roomlayer *lr,
        roomtexinfo *rtex, int iswall, int isground
        ) {
    lua_pushstring(l, "texpath");
    lua_gettable(l, -2);
    if (lua_type(l, -1) == LUA_TSTRING) {
        rtex->tex = (
            roomlayer_MakeTexRef(lr, lua_tostring(l, -1))
        );
        if (!rtex->tex)
            return 0;
    }
    lua_pop(l, 1);
    lua_pushstring(l, "texscalex");
    lua_gettable(l, -2);
    if (lua_type(l, -1) == LUA_TNUMBER &&
            lua_tonumber(l, -1) > 0) {
        rtex->tex_scaleintx = round(
            lua_tonumber(l, -1) * TEX_FULLSCALE_INT
        );
        if (rtex->tex_scaleintx <= 0)
            rtex->tex_scaleintx = 1;
    }
    lua_pop(l, 1);
    lua_pushstring(l, "texscaley");
    lua_gettable(l, -2);
    if (lua_type(l, -1) == LUA_TNUMBER &&
            lua_tonumber(l, -1) > 0) {
        rtex->tex_scaleinty = round(
            lua_tonumber(l, -1) * TEX_FULLSCALE_INT
        );
        if (rtex->tex_scaleinty <= 0)
            rtex->tex_scaleinty = 1;
    }
    lua_pop(l, 1);
    lua_pushstring(l, "texscrollspeedx");
    lua_gettable(l, -2);
    if (lua_type(l, -1) == LUA_TNUMBER &&
            lua_tonumber(l, -1) > 0) {
        rtex->tex_scrollspeedx = round(
            lua_tonumber(l, -1) * TEX_FULLSCALE_INT
        );
    }
    lua_pop(l, 1);
    lua_pushstring(l, "texscrollspeedy");
    lua_gettable(l, -2);
    if (lua_type(l, -1) == LUA_TNUMBER &&
            lua_tonumber(l, -1) > 0) {
        rtex->tex_scrollspeedy = round(
            lua_tonumber(l, -1) * TEX_FULLSCALE_INT
        );
    }
    lua_pop(l, 1);
    lua_pushstring(l, "texshiftx");
    lua_gettable(l, -2);
    if (lua_type(l, -1) == LUA_TNUMBER &&
            lua_tonumber(l, -1) > 0) {
        rtex->tex_shiftx = round(
            lua_tonumber(l, -1)
        );
    }
    lua_pop(l, 1);
    lua_pushstring(l, "texshifty");
    lua_gettable(l, -2);
    if (lua_type(l, -1) == LUA_TNUMBER &&
            lua_tonumber(l, -1) > 0) {
        rtex->tex_shifty = round(
            lua_tonumber(l, -1)
        );
    }
    lua_pop(l, 1);
    lua_pushstring(l, "texstickyside");
    lua_gettable(l, -2);
    if (lua_type(l, -1) == LUA_TSTRING) {
        const char *s = lua_tostring(l, -1);
        if (s)
            rtex->tex_stickyside = (
                strcasecmp(s, "up") ? ROOM_DIR_UP :
                (strcasecmp(s, "down") ? ROOM_DIR_DOWN : 0)
            );
    }
    lua_pop(l, 1);
    lua_pushstring(l, "texissky");
    lua_gettable(l, -2);
    if (lua_type(l, -1) == LUA_TBOOLEAN) {
        rtex->tex_issky = round(
            lua_toboolean(l, -1)
        );
    }
    lua_pop(l, 1);
    if (!iswall) rtex->tex_stickyside = 0;
    if (isground) rtex->tex_issky = 0;
    if (rtex->tex_stickyside != 0) {
        rtex->tex_scrollspeedy = 0;
    }
    if (rtex->tex_issky) {
        rtex->tex_scrollspeedx = 0;
        rtex->tex_scrollspeedy = 0;
    }
    return 0;
}

int roomserialize_lua_DeserializeRoomGeometries(
        lua_State *l, roomlayer *lr, int tblindex, char **err
        ) {
    if (tblindex < 0)
        tblindex = lua_gettop(l) - 1 + tblindex;
    int startstack = lua_gettop(l);
    if (tblindex < 1 || tblindex > lua_gettop(l) ||
            lua_type(l, tblindex) != LUA_TTABLE) {
        *err = strdup("room property list not a table");
        lua_settop(l, startstack);
        return 0;
    }
    if (lr->roomcount > 0) {
        *err = strdup("deserialization requires an empty roomlayer");
        lua_settop(l, startstack);
        return 0;
    }
    int i = 1;
    while (1) {
        lua_pushinteger(l, i);
        lua_gettable(l, tblindex);
        if (lua_type(l, -1) != LUA_TTABLE &&
                lua_type(l, -1) != LUA_TNIL) {
            *err = strdup("room property entry not a table");
            lua_settop(l, startstack);
            return 0;
        }
        if (lua_type(l, -1) == LUA_TNIL) {
            lua_settop(l, startstack);
            break;
        }
        uint64_t room_id = -1;
        lua_pushstring(l, "id");
        lua_gettable(l, -2);
        if (lua_type(l, -1) != LUA_TNUMBER ||
                lua_tonumber(l, -1) < 1) {
            *err = strdup("room id not a number of 1 or larger");
            lua_settop(l, startstack);
            return 0;
        }
        if (lua_isinteger(l, -1)) room_id = lua_tointeger(l, -1);
            else room_id = round(lua_tonumber(l, -1));
        lua_pop(l, 1);
        room *r = room_ById(lr, room_id);
        if (r) {
            char buf[256];
            snprintf(buf, sizeof(buf) - 1,
                "duplicate room id %" PRIu64, room_id);
            *err = strdup(buf);
            lua_settop(l, startstack);
            return 0;
        }
        r = room_Create(lr, room_id);
        if (!r) {
            *err = strdup("room creation unexpectedly failed");
            lua_settop(l, startstack);
            return 0;
        }
        int i2 = 0;
        while (i2 < r->corners) {
            if (r->wall[i2].wall_tex.tex)
                roomlayer_UnmakeTexRef(r->wall[i2].wall_tex.tex);
            i2++;
        }
        r->corners = 0;
        lua_pushstring(l, "walls");
        lua_gettable(l, -2);
        if (lua_type(l, -1) != LUA_TTABLE &&
                lua_type(l, -1) != LUA_TNIL) {
            char buf[256];
            snprintf(buf, sizeof(buf) - 1,
                "room with id %" PRIu64 " has \"walls\" "
                "not being a table", room_id);
            *err = strdup(buf);
            lua_settop(l, startstack);
            return 0;
        } else if (lua_type(l, -1) == LUA_TTABLE) {
            // Read "walls" property
            int k = 0;
            while (1) {
                lua_pushinteger(l, k + 1);
                lua_gettable(l, -2);
                if (lua_type(l, -1) == LUA_TNIL) {
                    break;
                }
                if (r->corners >= ROOM_MAX_CORNERS) {
                    room_Destroy(r);
                    char buf[256];
                    snprintf(buf, sizeof(buf) - 1,
                        "room with id %" PRIu64 " has too "
                        "many room corners", room_id);
                    *err = strdup(buf);
                    lua_settop(l, startstack);
                    return 0;
                }
                if (lua_type(l, -1) != LUA_TTABLE) {
                    room_Destroy(r);
                    char buf[256];
                    snprintf(buf, sizeof(buf) - 1,
                        "room with id %" PRIu64 " has \"walls\"[%d] "
                        "not being a table", room_id, k + 1);
                    *err = strdup(buf);
                    lua_settop(l, startstack);
                    return 0;
                }
                int64_t corner_x = 0;
                lua_pushstring(l, "corner_x");
                lua_gettable(l, -2);
                if (lua_type(l, -1) != LUA_TNUMBER) {
                    room_Destroy(r);
                    char buf[256];
                    snprintf(buf, sizeof(buf) - 1,
                        "room with id %" PRIu64 " has invalid "
                        "or missing corner coordinates "
                        "for wall %d", room_id, k + 1);
                    *err = strdup(buf);
                    lua_settop(l, startstack);
                    return 0;
                }
                if (lua_isinteger(l, -1))
                    corner_x = lua_tointeger(l, -1);
                    else corner_x = round(lua_tonumber(l, -1));
                lua_pop(l, 1);
                int64_t corner_y = 0;
                lua_pushstring(l, "corner_y");
                lua_gettable(l, -2);
                if (lua_type(l, -1) != LUA_TNUMBER) {
                    room_Destroy(r);
                    char buf[256];
                    snprintf(buf, sizeof(buf) - 1,
                        "room with id %" PRIu64 " has invalid "
                        "or missing corner coordinates "
                        "for wall %d", room_id, k + 1);
                    *err = strdup(buf);
                    lua_settop(l, startstack);
                    return 0;
                }
                if (lua_isinteger(l, -1))
                    corner_y = lua_tointeger(l, -1);
                    else corner_y = round(lua_tonumber(l, -1));
                lua_pop(l, 1);
                lua_pop(l, 1);  // Remove "walls"[k + 1] entry
                r->corners++;
                memset(&r->wall[r->corners - 1], 0, sizeof(*r->wall));
                r->wall[r->corners - 1].wall_tex.tex_scaleintx = (
                    TEX_FULLSCALE_INT
                );
                r->wall[r->corners - 1].wall_tex.tex_scaleinty = (
                    TEX_FULLSCALE_INT
                );
                r->corner_x[r->corners - 1] = corner_x;
                r->corner_y[r->corners - 1] = corner_y;
                k++;
            }
        }
        lua_pop(l, 1);  // Remove "walls" property
        if (r->corners < 3) {
            room_Destroy(r);
            char buf[256];
            snprintf(buf, sizeof(buf) - 1,
                "room with id %" PRIu64 " has too few "
                "corners", room_id);
            *err = strdup(buf);
            lua_settop(l, startstack);
            return 0;
        }
        i++;
    }
    return 1;
}

int roomserialize_lua_SetRoomProperties(
        lua_State *l, roomlayer *lr, int tblindex, char **err
        ) {
    if (tblindex < 0)
        tblindex = lua_gettop(l) - 1 + tblindex;
    int startstack = lua_gettop(l);
    if (tblindex < 1 || tblindex > lua_gettop(l) ||
            lua_type(l, tblindex) != LUA_TTABLE) {
        *err = strdup("room property list not a table");
        lua_settop(l, startstack);
        return 0;
    }
    int i = 1;
    while (1) {
        lua_pushinteger(l, i);
        lua_gettable(l, tblindex);
        if (lua_type(l, -1) != LUA_TTABLE &&
                lua_type(l, -1) != LUA_TNIL) {
            *err = strdup("room property entry not a table");
            lua_settop(l, startstack);
            return 0;
        }
        if (lua_type(l, -1) == LUA_TNIL) {
            lua_settop(l, startstack);
            break;
        }
        uint64_t room_id = -1;
        lua_pushstring(l, "id");
        lua_gettable(l, -2);
        if (lua_type(l, -1) != LUA_TNUMBER ||
                lua_tonumber(l, -1) < 1) {
            *err = strdup("room id not a number of 1 or larger");
            lua_settop(l, startstack);
            return 0;
        }
        if (lua_isinteger(l, -1)) room_id = lua_tointeger(l, -1);
            else room_id = round(lua_tonumber(l, -1));
        lua_pop(l, 1);
        room *r = room_ById(lr, room_id);
        if (!r) {
            char buf[256];
            snprintf(buf, sizeof(buf) - 1,
                "room with id %" PRIu64 " not found", room_id);
            *err = strdup(buf);
            lua_settop(l, startstack);
            return 0;
        }
        lua_pushstring(l, "walls");
        lua_gettable(l, -2);
        if (lua_type(l, -1) != LUA_TTABLE &&
                lua_type(l, -1) != LUA_TNIL) {
            char buf[256];
            snprintf(buf, sizeof(buf) - 1,
                "room with id %" PRIu64 " has \"walls\" "
                "not being a table", room_id);
            *err = strdup(buf);
            lua_settop(l, startstack);
            return 0;
        } else if (lua_type(l, -1) == LUA_TTABLE) {
            // Read "walls" property
            int k = 0;
            while (k < r->corners) {
                lua_pushinteger(l, k + 1);
                lua_gettable(l, -2);
                if (lua_type(l, -1) != LUA_TTABLE) {
                    char buf[256];
                    snprintf(buf, sizeof(buf) - 1,
                        "room with id %" PRIu64 " has \"walls\"[%d] "
                        "not being a table", room_id, k + 1);
                    *err = strdup(buf);
                    lua_settop(l, startstack);
                    return 0;
                }
                if (!_extract_apply_tex_props(
                        l, lr, &r->wall[k].wall_tex, 1, 0
                        )) {
                    char buf[256];
                    snprintf(buf, sizeof(buf) - 1,
                        "room id %" PRIu64 " texture props for "
                        "wall %d didn't apply, out of memory?",
                        room_id, k + 1);
                    *err = strdup(buf);
                    lua_settop(l, startstack);
                    return 0;
                }
                lua_pop(l, 1);  // Remove "walls"[k + 1] entry
                k++;
            }
        }
        lua_pop(l, 1);  // Remove "walls" property
        lua_pushstring(l, "floor");
        lua_gettable(l, -2);
        if (lua_type(l, -1) != LUA_TTABLE &&
                lua_type(l, -1) != LUA_TNIL) {
            char buf[256];
            snprintf(buf, sizeof(buf) - 1,
                "room with id %" PRIu64 " has \"floor\" "
                "not being a table", room_id);
            *err = strdup(buf);
            lua_settop(l, startstack);
            return 0;
        } else if (lua_type(l, -1) == LUA_TTABLE) {
            if (!_extract_apply_tex_props(
                    l, lr, &r->floor_tex, 0, 1
                    )) {
                char buf[256];
                snprintf(buf, sizeof(buf) - 1,
                    "room id %" PRIu64 " texture props for floor "
                    "didn't apply, out of memory?", room_id);
                *err = strdup(buf);
                lua_settop(l, startstack);
                return 0;
            }
        }
        lua_pop(l, 1);  // Remove "floor" property
        lua_pushstring(l, "ceiling");
        lua_gettable(l, -2);
        if (lua_type(l, -1) != LUA_TTABLE &&
                lua_type(l, -1) != LUA_TNIL) {
            char buf[256];
            snprintf(buf, sizeof(buf) - 1,
                "room with id %" PRIu64 " has \"ceiling\" "
                "not being a table", room_id);
            *err = strdup(buf);
            lua_settop(l, startstack);
            return 0;
        } else if (lua_type(l, -1) == LUA_TTABLE) {
            if (!_extract_apply_tex_props(
                    l, lr, &r->ceiling_tex, 0, 0
                    )) {
                char buf[256];
                snprintf(buf, sizeof(buf) - 1,
                    "room id %" PRIu64 " texture props for ceiling "
                    "didn't apply, out of memory?", room_id);
                *err = strdup(buf);
                lua_settop(l, startstack);
                return 0;
            }
            lua_pushstring(l, "height");
        }
        lua_pop(l, 1);  // Remove "ceiling" property
        {
            lua_pushstring(l, "height");
            lua_gettable(l, -2);
            if (lua_type(l, -1) == LUA_TNUMBER) {
                int64_t h = 0;
                if (lua_isinteger(l, -1)) h = lua_tointeger(l, -1);
                    else h = round(lua_tonumber(l, -1));
                if (h < 1) h = 1;
                r->height = h;
            }
            lua_pop(l, 1);
        }
        {
            lua_pushstring(l, "floor_z");
            lua_gettable(l, -2);
            if (lua_type(l, -1) == LUA_TNUMBER) {
                int64_t z = 0;
                if (lua_isinteger(l, -1)) z = lua_tointeger(l, -1);
                    else z = round(lua_tonumber(l, -1));
                r->floor_z = z;
            }
            lua_pop(l, 1);
        }
        i++;
    }
    return 1;
}
