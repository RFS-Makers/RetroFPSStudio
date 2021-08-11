// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

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
    return 1;
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
    room *previousroom = NULL;
    int i = 1;
    while (1) {
        // If we successfully dealt with a previous room,
        // make sure it's properly registered:
        if (previousroom) {
            room_RecomputePosExtent(previousroom);
            previousroom = NULL;
        }
        // Get info on next room to be added:
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

        // Make sure the room we're adding doesn't exist yet:
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
        previousroom = r;
        
        // Clear out all the corners & wall info the room
        // has set by default:
        int i2 = 0;
        while (i2 < r->corners) {
            if (r->wall[i2].wall_tex.tex)
                roomlayer_UnmakeTexRef(r->wall[i2].wall_tex.tex);
            i2++;
        }
        roomlayer_UnmakeTexRef(r->floor_tex.tex);
        r->floor_tex.tex = NULL;
        roomlayer_UnmakeTexRef(r->ceiling_tex.tex);
        r->ceiling_tex.tex = NULL;
        r->corners = 0;

        // Now, read out actual corners/walls geometry:
        lua_pushstring(l, "walls");
        lua_gettable(l, -2);
        if (lua_type(l, -1) != LUA_TTABLE) {
            room_Destroy(r); previousroom = NULL;
            char buf[256];
            snprintf(buf, sizeof(buf) - 1,
                "room with id %" PRIu64 " has \"walls\" "
                "not being a table", room_id);
            *err = strdup(buf);
            lua_settop(l, startstack);
            return 0;
        } else {
            // Read "walls" property
            int k = 0;
            while (1) {
                lua_pushinteger(l, k + 1);
                lua_gettable(l, -2);
                if (lua_type(l, -1) == LUA_TNIL) {
                    break;
                }
                // Ensure we don't have excess corners already:
                if (r->corners >= ROOM_MAX_CORNERS) {
                    room_Destroy(r); previousroom = NULL;
                    char buf[256];
                    snprintf(buf, sizeof(buf) - 1,
                        "room with id %" PRIu64 " has too "
                        "many room corners", room_id);
                    *err = strdup(buf);
                    lua_settop(l, startstack);
                    return 0;
                }
                // Make sure wall info is a table:
                if (lua_type(l, -1) != LUA_TTABLE) {
                    room_Destroy(r); previousroom = NULL;
                    char buf[256];
                    snprintf(buf, sizeof(buf) - 1,
                        "room with id %" PRIu64 " "
                        "has \"walls\"[%d] "
                        "not being a table", room_id, k + 1);
                    *err = strdup(buf);
                    lua_settop(l, startstack);
                    return 0;
                }
                // Extract the two corner coordinates:
                int64_t corner_x = 0;
                lua_pushstring(l, "corner_x");
                lua_gettable(l, -2);
                if (lua_type(l, -1) != LUA_TNUMBER) {
                    room_Destroy(r); previousroom = NULL;
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
                    room_Destroy(r); previousroom = NULL;
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

                // Set corner data, now that we got it:
                r->corners++;
                memset(&r->wall[r->corners - 1], 0,
                    sizeof(*r->wall));
                r->wall[r->corners - 1].wall_tex.
                    tex_scaleintx = (
                        TEX_FULLSCALE_INT
                    );
                r->wall[r->corners - 1].wall_tex.
                    tex_scaleinty = (
                        TEX_FULLSCALE_INT
                    );
                r->corner_x[r->corners - 1] = corner_x;
                r->corner_y[r->corners - 1] = corner_y;

                k++;
            }
        }
        lua_pop(l, 1);  // Remove "walls" property
        lua_pop(l, 1);  // Remove room[i]
        // Verify amount of corners:
        if (r->corners < 3) {
            room_Destroy(r); previousroom = NULL;
            char buf[256];
            snprintf(buf, sizeof(buf) - 1,
                "room with id %" PRIu64 " has too few "
                "corners", room_id);
            *err = strdup(buf);
            lua_settop(l, startstack);
            return 0;
        }
        // Verify room shape:
        if (!room_VerifyBasicGeometry(r)) {
            room_Destroy(r); previousroom = NULL;
            char buf[256];
            snprintf(buf, sizeof(buf) - 1,
                "room with id %" PRIu64 " has invalid "
                "corner geometry that has wall overlaps, or "
                "not specified counter-clockwise, or "
                "with two corners merged in one spot",
                room_id);
            *err = strdup(buf);
            lua_settop(l, startstack);
            return 0;
        }
        i++;
    }

    // Now, iterate again for the portals:
    i = 1;
    while (1) {
        // Get i'th room:
        lua_pushinteger(l, i);
        lua_gettable(l, tblindex);
        assert(lua_type(l, -1) == LUA_TTABLE ||
            lua_type(l, -1) == LUA_TNIL);
            // ^ checked in loop above
        if (lua_type(l, -1) == LUA_TNIL) {
            // End of room list.
            lua_settop(l, startstack);
            break;
        }
        // Get room id:
        uint64_t room_id = -1;
        lua_pushstring(l, "id");
        lua_gettable(l, -2);
        assert(lua_type(l, -1) == LUA_TNUMBER &&
            lua_tonumber(l, -1) >= 1);
            // ^ checked in loop above;
        if (lua_isinteger(l, -1)) room_id = lua_tointeger(l, -1);
            else room_id = round(lua_tonumber(l, -1));
        lua_pop(l, 1);

        // Obtain the corresponding room
        room *r = room_ById(lr, room_id);
        assert(r != NULL);  // Was created in loop above,
                            // so it should really exist.
        lua_pushstring(l, "walls");
        lua_gettable(l, -2);
        assert(lua_type(l, -1) == LUA_TTABLE);
        // ^ checked in loop above

        // Iterate over "walls" property:
        int nextk = 1;
        int k = 0;
        while (1) {
            lua_pushinteger(l, k + 1);
            lua_gettable(l, -2);
            if (lua_type(l, -1) == LUA_TNIL) {
                break;
            }
            assert(lua_type(l, -1) == LUA_TTABLE);

            // Try to load up "portal_to" property:
            lua_pushstring(l, "portal_to");
            lua_gettable(l, -2);
            if (lua_type(l, -1) != LUA_TNUMBER &&
                    lua_type(l, -1) != LUA_TNIL) {
                char buf[256];
                snprintf(buf, sizeof(buf) - 1,
                    "room with id %" PRIu64 " has invalid "
                    "type for \"portal_to\" for wall %d\n",
                    room_id, k + 1);
                *err = strdup(buf);
                lua_settop(l, startstack);
                return 0;
            } else if (lua_type(l, -1) == LUA_TNUMBER) {
                // Extract room id for portal target:
                uint64_t portal_target_id = -1;
                if (lua_isinteger(l, -1))
                    portal_target_id = lua_tointeger(l, -1);
                    else portal_target_id =
                        round(lua_tonumber(l, -1));
                room *portal_target = NULL;
                if (portal_target_id < 1 ||
                        (portal_target = room_ById(
                            lr, portal_target_id
                        )) == NULL) {
                    char buf[256];
                    snprintf(buf, sizeof(buf) - 1,
                        "room with id %" PRIu64 " has invalid "
                        "portal target for wall %d - "
                        "target room "
                        "with id %" PRId64 " not found",
                        room_id,
                        k + 1, portal_target_id);
                    *err = strdup(buf);
                    lua_settop(l, startstack);
                    return 0;
                }

                // Find the matching wall in the target room:
                // (we don't store this in the serialized data)
                int opposite_portal_wall_idx = -1;
                int nextj = 1;
                int j = 0;
                while (j < portal_target->corners) {
                    if (portal_target->corner_x[j] ==
                            r->corner_x[nextk] &&
                            portal_target->corner_y[j] ==
                            r->corner_y[nextk] &&
                            portal_target->corner_x[nextj] ==
                            r->corner_x[k] &&
                            portal_target->corner_y[nextj] ==
                            r->corner_y[k]) {
                        opposite_portal_wall_idx = j;
                        break;
                    }
                    nextj++;
                    if (nextj >= portal_target->corners)
                        nextj = 0;
                    j++;
                }
                // Ensure we found the target wall:
                if (opposite_portal_wall_idx < 0) {
                    char buf[256];
                    snprintf(buf, sizeof(buf) - 1,
                        "room with id %" PRIu64 " has invalid "
                        "portal target for wall %d - "
                        "target room "
                        "with id %" PRId64 " "
                        "has no matching wall lined up",
                        room_id,
                        k + 1, portal_target_id);
                    *err = strdup(buf);
                    lua_settop(l, startstack);
                    return 0;
                }
                // Ensure there is not already a
                // conflicting portal heading back to us:
                if (portal_target->wall[
                            opposite_portal_wall_idx].
                            has_portal &&
                        portal_target->wall[
                            opposite_portal_wall_idx].
                            portal_targetroom != r) {
                    char buf[256];
                    snprintf(buf, sizeof(buf) - 1,
                        "room with id %" PRIu64 " has invalid "
                        "portal target for wall %d - "
                        "target room "
                        "with id %" PRId64 " "
                        "already has portal elsewhere on "
                        "opposite wall %d", room_id,
                        k + 1, portal_target_id,
                        opposite_portal_wall_idx + 1);
                    *err = strdup(buf);
                    lua_settop(l, startstack);
                    return 0;
                }
                // Finally, set the portal info on both ends:
                r->wall[k].has_portal = 1;
                r->wall[k].portal_targetroom = portal_target;
                r->wall[k].portal_targetwall = (
                    opposite_portal_wall_idx
                );
                portal_target->wall[opposite_portal_wall_idx].
                    has_portal = 1;
                portal_target->wall[opposite_portal_wall_idx].
                    portal_targetroom = r;
                portal_target->wall[opposite_portal_wall_idx].
                    portal_targetwall = k;
            }
            lua_pop(l, 1);  // Remove "portal_target_id"
            lua_pop(l, 1);  // Remove wall[k + 1]
            nextk++;
            if (nextk >= r->corners)
                nextk = 0;
            k++;
        }
        lua_pop(l, 1);  // Remove "walls" property
        lua_pop(l, 1);  // Remove room[i]
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
        lua_pushstring(l, "light");
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
            double cr = 1;
            double cg = 1;
            double cb = 1;
            int i = 1;
            while (i <= 3) {
                lua_pushinteger(l, i);
                lua_gettable(l, -2);
                if (lua_type(l, -1) != LUA_TNUMBER) {
                    char buf[256];
                    snprintf(buf, sizeof(buf) - 1,
                        "room with id %" PRIu64 " has \"light\" "
                        "not containing 3 numbers", room_id);
                    *err = strdup(buf);
                    lua_settop(l, startstack);
                    return 0;
                }
                if (i == 1) cr = fmax(0.0, fmin(2.0, lua_tonumber(l, -1)));
                if (i == 2) cg = fmax(0.0, fmin(2.0, lua_tonumber(l, -1)));
                if (i == 3) cb = fmax(0.0, fmin(2.0, lua_tonumber(l, -1)));
                lua_pop(l, 1);  // Remove current light value
                i++;
            }
            r->sector_light_r = round(cr * LIGHT_COLOR_SCALAR);
            r->sector_light_g = round(cg * LIGHT_COLOR_SCALAR);
            r->sector_light_b = round(cb * LIGHT_COLOR_SCALAR);
        }
        lua_pop(l, 1);  // Remove "light" property
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
        lua_pop(l, 1);  // Remove room[i + 1]
        i++;
    }
    return 1;
}
