// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#include "compileconfig.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "datetime.h"
#include "graphics.h"
#include "math2d.h"
#include "room.h"
#include "roomcam.h"
#include "roomdefs.h"
#include "roomlayer.h"
#include "roomobject.h"
#include "rfssurf.h"

typedef struct renderstatistics renderstatistics;

typedef struct roomcamcache {
    int cachedangle, cachedvangle, cachedfov, cachedw, cachedh;

    int32_t cachedfovh, cachedfovv;
    int64_t planedist, planew, planeh;
    int64_t unrotatedplanevecs_left_x;
    int64_t unrotatedplanevecs_left_y;
    int64_t unrotatedplanevecs_right_x;
    int64_t unrotatedplanevecs_right_y;
    int64_t *planevecs_len;
    int32_t planevecs_alloc, vertivecs_alloc;

    int64_t *vertivecs_x, *vertivecs_z;
    int64_t *rotatedplanevecs_x, *rotatedplanevecs_y;
    int32_t rotatedalloc;

    renderstatistics stats;
} roomcamcache;


renderstatistics *roomcam_GetStats(roomcam *c) {
    if (c && c->cache)
        return &c->cache->stats;
    return NULL;
}

int32_t _roomcam_XYZToViewplaneY_NoRecalc(
        roomcam *cam,
        int64_t px, int64_t py, int64_t pz
        ) {
    const int w = cam->cache->cachedw;
    const int h = cam->cache->cachedh;

    // Rotate/move vec into camera-local space:
    px -= cam->obj->x;
    py -= cam->obj->y;
    pz -= cam->obj->z;
    math_rotate2di(&px, &py, -cam->obj->angle);

    // Special case of behind camera:
    if (px <= 0) {
        if (pz < 0)
            return h;
        if (pz > 0)
            return -1;
        return 0;
    }

    // Scale vec down to exactly viewplane distance:
    int64_t scalef = 10000LL * cam->cache->planedist / px;
    assert(cam->cache->vertivecs_x[0] == cam->cache->planedist);
    px = (px * scalef) / 10000LL;
    pz = (pz * scalef) / 10000LL;
    // (...we dont need 'py' anymore, so ignore that one)

    // If outside of viewplane, abort early:
    if (pz < cam->cache->vertivecs_z[0])
        return -1;
    if (pz > cam->cache->vertivecs_z[h - 1])
        return h;

    int res = (pz - cam->cache->vertivecs_z[0]) * h /
        (cam->cache->vertivecs_z[h - 1] -
        cam->cache->vertivecs_z[0]);
    if (res >= h) res = h - 1;
    if (res < 0) res = 0;
    return res;
}

int32_t _roomcam_XYToViewplaneX_NoRecalc(
        roomcam *cam, int64_t px, int64_t py
        ) {
    const int w = cam->cache->cachedw;

    // Rotate/move vec into camera-local space:
    px -= cam->obj->x;
    py -= cam->obj->y;
    math_rotate2di(&px, &py, -cam->obj->angle);

    // Special case of behind camera:
    if (px <= 0) {
        if (py < 0)
            return -1;
        if (py > 0)
            return w;
        return 0;
    }

    // Scale vec down to exactly viewplane distance:
    int64_t scalef = 10000LL * cam->cache->planedist / px;
    assert(cam->cache->unrotatedplanevecs_left_x == cam->cache->planedist);
    px = (px * scalef) / 10000LL;
    py = (py * scalef) / 10000LL;

    // If outside of viewplane, abort early:
    if (py < cam->cache->unrotatedplanevecs_left_y)
        return -1;
    if (py > cam->cache->unrotatedplanevecs_right_y)
        return w;

    int res = (py - cam->cache->unrotatedplanevecs_left_y) * w /
        (cam->cache->unrotatedplanevecs_right_y -
        cam->cache->unrotatedplanevecs_left_y);
    if (res >= w) res = w - 1;
    if (res < 0) res = 0;
    return res;
}


static void _roomcam_FreeCache(roomcamcache *cache) {
    if (!cache)
        return;
    free(cache->planevecs_len);
    free(cache->rotatedplanevecs_x);
    free(cache->rotatedplanevecs_y);
    free(cache->vertivecs_x);
    free(cache->vertivecs_z);
    free(cache);
}

static void _roomcam_DestroyCallback(roomobj *camobj) {
    roomcam *cam = ((roomcam *)camobj->objdata);
    if (!cam)
        return;
    _roomcam_FreeCache(cam->cache);
    free(cam);
    camobj->objdata = NULL;
}


roomcam *roomcam_Create() {
    roomcam *rc = malloc(sizeof(*rc));
    if (!rc)
        return NULL;
    memset(rc, 0, sizeof(*rc));
    rc->cache = malloc(sizeof(*rc->cache));
    if (!rc->cache) {
        free(rc);
        return NULL;
    }
    memset(rc->cache, 0, sizeof(*rc->cache));
    rc->cache->stats.fps_ts = datetime_Ticks();
    rc->cache->cachedangle = -99999;
    rc->cache->cachedvangle = -99999;
    rc->fovf = 70.0;
    rc->fov = round(
        rc->fovf * ANGLE_SCALAR
    );
    roomobj *obj = roomobj_Create(
        roomobj_GetNewId(), ROOMOBJ_CAMERA,
        rc, &_roomcam_DestroyCallback
    );
    if (!obj) {
        free(rc->cache);
        free(rc);
        return NULL;
    }
    rc->obj = obj;
    return rc;
}

int roomcam_CalculateViewPlane(roomcam *cam, int w, int h) {
    // Override mathematically unsafe values:
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;

    // Calculate viewplane:
    int force_recalculate_rotated_vecs = 0;
    cam->fov = math_fixanglei(cam->fov);
    cam->obj->angle = math_fixanglei(cam->obj->angle);
    cam->vangle = math_fixanglei(cam->vangle);
    if (cam->cache->cachedfov != cam->fov ||
            cam->cache->cachedw != w ||
            cam->cache->cachedh != h) {
        force_recalculate_rotated_vecs = 1;
        int32_t aspectscalar = (
            (w * ANGLE_SCALAR) / h
        );
        int32_t fov_h = (
            1 * (cam->fov * aspectscalar / ANGLE_SCALAR) +
            cam->fov * 10
        ) / 11;
        if (fov_h > 120 * ANGLE_SCALAR) fov_h = 120 * ANGLE_SCALAR;
        #if defined(DEBUG_3DRENDERER)
        fprintf(stderr,
            "rfsc/roomcam.c: debug: fov: "
            "input fov: %d, horizontal fov: %d\n",
            cam->fov, fov_h);
        #endif
        const int64_t plane_distance = (
            RAYCAST_LENGTH / VIEWPLANE_RAY_LENGTH_DIVIDER
        );
        int64_t plane_left_x = plane_distance;
        int64_t plane_left_y = 0;
        math_rotate2di(&plane_left_x, &plane_left_y, (fov_h / 2));
        int64_t scalar = 1000LL * plane_distance / plane_left_x;
        plane_left_x = plane_distance;
        plane_left_y = plane_left_y * scalar / 1000LL;
        int64_t plane_right_x = plane_distance;
        int64_t plane_right_y = 0;
        math_rotate2di(&plane_right_x, &plane_right_y, -(fov_h / 2));
        scalar = 1000LL * plane_distance / plane_right_x;
        plane_right_x = plane_distance;
        plane_right_y = plane_right_y * scalar / 1000LL;
        cam->cache->unrotatedplanevecs_left_x = plane_left_x;
        cam->cache->unrotatedplanevecs_left_y = plane_left_y;
        cam->cache->unrotatedplanevecs_right_x = plane_right_x;
        cam->cache->unrotatedplanevecs_right_y = plane_right_y;
        cam->cache->planedist = plane_distance;
        cam->cache->planew = llabs(plane_right_y - plane_left_y);
        cam->cache->planeh = (
            cam->cache->planew * h / w
        );
        if (cam->cache->rotatedalloc < w) {
            cam->cache->rotatedalloc = 0;
            free(cam->cache->rotatedplanevecs_x);
            cam->cache->rotatedplanevecs_x = NULL;
            free(cam->cache->rotatedplanevecs_y);
            cam->cache->rotatedplanevecs_y = NULL;
            cam->cache->rotatedplanevecs_x = malloc(
                sizeof(*cam->cache->rotatedplanevecs_x) * (w * 2)
            );
            if (!cam->cache->rotatedplanevecs_x)
                return 0;
            cam->cache->rotatedplanevecs_y = malloc(
                sizeof(*cam->cache->rotatedplanevecs_y) * (w * 2)
            );
            if (!cam->cache->rotatedplanevecs_y)
                return 0;
            cam->cache->rotatedalloc = w * 2;
        }
        if (cam->cache->planevecs_alloc < w) {
            cam->cache->planevecs_alloc = 0;
            free(cam->cache->planevecs_len);
            cam->cache->planevecs_len = NULL;
            cam->cache->planevecs_len = malloc(
                sizeof(*cam->cache->planevecs_len) * (w * 2)
            );
            if (!cam->cache->planevecs_len)
                return 0;
            cam->cache->planevecs_alloc = w * 2;
        }
        if (cam->cache->vertivecs_alloc < h) {
            cam->cache->vertivecs_alloc = 0;
            free(cam->cache->vertivecs_x);
            free(cam->cache->vertivecs_z);
            cam->cache->vertivecs_x = NULL;
            cam->cache->vertivecs_z = NULL;
            cam->cache->vertivecs_x = malloc(
                sizeof(*cam->cache->vertivecs_x) * (h * 2)
            );
            if (!cam->cache->vertivecs_x)
                return 0;
            cam->cache->vertivecs_z = malloc(
                sizeof(*cam->cache->vertivecs_z) * (h * 2)
            );
            if (!cam->cache->vertivecs_z)
                return 0;
            cam->cache->vertivecs_alloc = h * 2;
        }
        int64_t plane_world_width = llabs(
            plane_right_y - plane_left_y
        );
        int64_t plane_world_height = (
            plane_world_width * h / w
        );
        int i = 0;
        while (i < h) {
            cam->cache->vertivecs_z[i] = (
                plane_world_height / 2 -
                plane_world_height * i / imax(1, h - 1)
            );
            cam->cache->vertivecs_x[i] = plane_distance;
            i++;
        }
        int32_t fov_v = (math_angle2di(
            cam->cache->vertivecs_x[0],
            -cam->cache->vertivecs_z[0]
        ));
        cam->cache->cachedfovh = fov_h;
        cam->cache->cachedfovv = fov_v;
    }
    if (force_recalculate_rotated_vecs ||
            cam->cache->cachedangle != cam->obj->angle ||
            cam->cache->cachedvangle != cam->vangle) {
        int64_t plane_left_x = (
            cam->cache->unrotatedplanevecs_left_x
        );
        int64_t plane_left_y = (
            cam->cache->unrotatedplanevecs_left_y
        );
        int64_t plane_right_x = (
            cam->cache->unrotatedplanevecs_right_x
        );
        int64_t plane_right_y = (
            cam->cache->unrotatedplanevecs_right_y
        );
        math_rotate2di(&plane_left_x, &plane_left_y, cam->obj->angle);
        math_rotate2di(&plane_right_x, &plane_right_y, cam->obj->angle);
        int32_t i = 0;
        while (i < w) {
            int32_t scalar = (i * 10000) / (w > 1 ? (w - 1) : 1);
            cam->cache->rotatedplanevecs_x[i] = (
                plane_left_x * (10000 - scalar) +
                plane_right_x * scalar
            ) / 10000;
            cam->cache->rotatedplanevecs_y[i] = (
                plane_left_y * (10000 - scalar) +
                plane_right_y * scalar
            ) / 10000;
            cam->cache->planevecs_len[i] = math_veclen2di(
                cam->cache->rotatedplanevecs_x[i],
                cam->cache->rotatedplanevecs_y[i]
            );
            i++;
        }
    }
    cam->cache->cachedangle = cam->obj->angle;
    cam->cache->cachedvangle = cam->vangle;
    cam->cache->cachedfov = cam->fov;
    cam->cache->cachedw = w;
    cam->cache->cachedh = h;
    return 1;
}

int roomcam_FloorSliceHeight(
        roomcam *cam, int64_t geometry_z,
        int geometry_corners, int64_t *geometry_corner_x,
        int64_t *geometry_corner_y, int h, int xoffset,
        int isfloor,
        int64_t *topworldx, int64_t *topworldy,
        int64_t *bottomworldx, int64_t *bottomworldy,
        int32_t *topoffset, int32_t *bottomoffset
        ) {
    renderstatistics *stats = &cam->cache->stats;
    if (geometry_z == cam->obj->z)
        return 0;
    if (isfloor && geometry_z > cam->obj->z)
        return 0;
    else if (!isfloor && geometry_z < cam->obj->z)
        return 0;

    // First, get the angle along the viewing slice:
    int32_t angle = cam->obj->angle;
    int32_t vangle = cam->vangle;
    int64_t intersect_lx1 = cam->obj->x;
    int64_t intersect_ly1 = cam->obj->y;
    int64_t intersect_lx2 = intersect_lx1 +
        cam->cache->rotatedplanevecs_x[xoffset] *
        VIEWPLANE_RAY_LENGTH_DIVIDER;
    int64_t intersect_ly2 = intersect_ly1 +
        cam->cache->rotatedplanevecs_y[xoffset] *
        VIEWPLANE_RAY_LENGTH_DIVIDER;

    // Then, intersect test with the geometry polygon:
    stats->base_geometry_rays_cast++;
    int camtoaway_wallno;
    int64_t camtoaway_ix, camtoaway_iy;
    int camtoaway_intersect = math_polyintersect2di_ex(
        intersect_lx1, intersect_ly1,
        intersect_lx2, intersect_ly2,
        geometry_corners,
        geometry_corner_x, geometry_corner_y,
        -1, 0, &camtoaway_wallno, &camtoaway_ix,
        &camtoaway_iy
    );
    if (!camtoaway_intersect)
        return 0;
    stats->base_geometry_rays_cast++;
    int awaytocam_wallno;
    int64_t awaytocam_ix, awaytocam_iy;
    int awaytocam_intersect = math_polyintersect2di_ex(
        intersect_lx2, intersect_ly2,
        intersect_lx1, intersect_ly1,
        geometry_corners,
        geometry_corner_x, geometry_corner_y,
        -1, 0, &awaytocam_wallno, &awaytocam_ix,
        &awaytocam_iy
    );
    if (!awaytocam_intersect)
        return 0;
    // Compute "closer" geometry end point on screen:
    int32_t close_screen_offset = 0;
    int64_t close_world_x = intersect_lx1;
    int64_t close_world_y = intersect_ly1;
    if (awaytocam_wallno == camtoaway_wallno) {
        if (!math_polycontains2di(
                intersect_lx1, intersect_lx2,
                geometry_corners,
                geometry_corner_x, geometry_corner_y)) {
            return 0;
        }
        assert(h <= cam->cache->cachedh);
        if (isfloor) {
            assert(geometry_z < cam->obj->z);
            int64_t lowerscreen_x = cam->cache->vertivecs_x[h - 1];
            int64_t lowerscreen_z = cam->cache->vertivecs_z[h - 1];
            assert(lowerscreen_z < 0);
            int64_t scalar = 1000LL * (
                (geometry_z - cam->obj->z) / lowerscreen_z
            );
            close_world_x = lowerscreen_x * scalar / 1000LL;
            close_world_y = intersect_ly1;
        } else {
            assert(geometry_z > cam->obj->z);
            int64_t upperscreen_x = cam->cache->vertivecs_x[0];
            int64_t upperscreen_z = cam->cache->vertivecs_z[0];
            assert(upperscreen_z > 0);
            int64_t scalar = 1000LL * (
                (geometry_z - cam->obj->z) / upperscreen_z
            );
            close_world_x = upperscreen_x * scalar / 1000LL;
            close_world_y = intersect_ly1;
        }
        close_screen_offset = 0;
    } else {
        close_screen_offset = _roomcam_XYZToViewplaneY_NoRecalc(
            cam, close_world_x, close_world_y, geometry_z
        );
        if (close_screen_offset < 0) close_screen_offset = 0;
        if (close_screen_offset >= h) close_screen_offset = h - 1;
    }
    // Compute "farther" geometry point on screen:
    int64_t far_world_x = intersect_lx2;
    int64_t far_world_y = intersect_ly2;
    int32_t far_screen_offset = _roomcam_XYZToViewplaneY_NoRecalc(
        cam, far_world_x, far_world_y, geometry_z
    );
    if (far_screen_offset < 0) far_screen_offset = 0;
    if (far_screen_offset >= h) far_screen_offset = h - 1;
    // Figure out the proper order:
    if (far_screen_offset < close_screen_offset ||
            (far_screen_offset == close_screen_offset && !isfloor)) {
        assert(!isfloor);
        *topworldx = close_world_x;
        *topworldy = close_world_y;
        *topoffset = close_screen_offset;
        *bottomworldx = far_world_x;
        *bottomworldy = far_world_y;
        *bottomoffset = far_screen_offset;
    } else {
        assert(isfloor);
        *topworldx = far_world_x;
        *topworldy = far_world_y;
        *topoffset = far_screen_offset;
        *bottomworldx = close_world_x;
        *bottomworldy = close_world_y;
        *bottomoffset = close_screen_offset;
    }
    return 1;
}

int roomcam_WallSliceHeight(
        roomcam *cam, int64_t geometry_floor_z,
        int64_t geometry_height, int h, int64_t ix, int64_t iy,
        int xoffset, int64_t *topworldz, int64_t *bottomworldz,
        int32_t *topoffset, int32_t *bottomoffset
        ) {
    if (unlikely(geometry_height <= 0))
        return 0;
    int64_t dist = math_veclen2di(
        ix - cam->obj->x, iy - cam->obj->y
    );
    assert(xoffset >= 0 && xoffset < cam->cache->cachedw);
    int64_t planecoldist = cam->cache->planevecs_len[xoffset];
    int64_t planecoldistscalar = (dist * 100000LL) / planecoldist;
    int64_t screenheightatwall = (
        (cam->cache->planeh * planecoldistscalar) / 100000LL
    );
    /*printf("dist: %" PRId64 ", planecoldist: %" PRId64 ", "
        "planecoldistscalar: %" PRId64 ", "
        "screen height at plane: %" PRId64 ", "
        "screen height at wall: %" PRId64 ", wall "
        "height: %" PRId64 "\n",
        dist, planecoldist, planecoldistscalar,
        cam->cache->planeh,
        screenheightatwall,
        r->height);*/
    int64_t screentopatwall = (
        cam->obj->z + screenheightatwall / 2
    );
    if (unlikely(screentopatwall < geometry_floor_z ||
            screentopatwall - screenheightatwall >
            geometry_floor_z + geometry_height))
        return 0;
    int64_t world_to_pixel_scalar = (
        ((int64_t)h * 100000L) / screenheightatwall
    );
    int64_t topoff_world = (screentopatwall - (
        geometry_floor_z + geometry_height
    ));
    int64_t _topworldz = (
        (screentopatwall >= geometry_floor_z + geometry_height) ?
        (geometry_floor_z + geometry_height) : screentopatwall);
    if (topoff_world < 0) {
        topoff_world = 0;
    }
    int64_t topoff_screen = (
        (topoff_world * world_to_pixel_scalar) / 100000L
    );
    int64_t bottomoff_world = (screentopatwall - (
        geometry_floor_z
    ));
    int64_t bottomoff_screen = (
        (bottomoff_world * world_to_pixel_scalar) / 100000L
    );
    assert(bottomoff_world >= 0);
    if (bottomoff_screen > h) {
        bottomoff_screen = h;
        bottomoff_world = screentopatwall - screenheightatwall;
    }
    int64_t _bottomworldz = (_topworldz -
        ((bottomoff_screen - topoff_screen) * 100000L)
        / world_to_pixel_scalar
    );
    if (topoff_screen > bottomoff_screen)
        return 0;
    /*if (topoff_screen == 0 && 0)
        printf("render res %dx%d, screen top at wall: %d, "
            "screen height at wall: %d, "
            "world_to_pixel: %d, "
            "top offset world: %d, bottom offset world: %d, "
            "top offset screen: %d, bottom offset screen: %d, "
            "wall height world: %d, cam z: %d, floor z: %d, "
            "top world z: %d, bottom world z: %d,"
            "top world z below screen plane top: %d\n",
            (int)-1, (int)h,
            (int)screentopatwall, (int)screenheightatwall,
            (int)world_to_pixel_scalar,
            (int)topoff_world,
            (int)bottomoff_world,
            (int)topoff_screen, (int)bottomoff_screen,
            (int)geometry_height,
            (int)cam->obj->z, (int)geometry_floor_z, (int)_topworldz,
            (int)_bottomworldz,
            (int)(screentopatwall - _topworldz));*/
    if (unlikely(topoff_screen >= h || bottomoff_screen < 0))
        return 0;  // off-screen.
    *topoffset = topoff_screen;
    *bottomoffset = bottomoff_screen;
    *topworldz = _topworldz;
    *bottomworldz = _bottomworldz;
    return 1;
}

void roomcam_WallCubeMapping(
        room *r, int wallid, int64_t wx, int64_t wy,
        int aboveportal, roomtexinfo *texinfo,
        int64_t wtop, int64_t wsliceheight,
        int64_t *tx1, int64_t *ty1, int64_t *ty2
        ) {
    int64_t repeat_units_x = (TEX_REPEAT_UNITS * (
        ((int64_t)texinfo->tex_scaleintx) /
        TEX_FULLSCALE_INT
    ));
    if (repeat_units_x < 1) repeat_units_x = 1;
    int64_t repeat_units_y = (TEX_REPEAT_UNITS * (
        ((int64_t)texinfo->tex_scaleinty) /
        TEX_FULLSCALE_INT
    ));
    if (repeat_units_y < 1) repeat_units_y = 1;
    int nextwallid = wallid + 1;
    if (nextwallid >= r->corners)
        nextwallid = 0;
    if (llabs(r->corner_x[wallid] - r->corner_x[nextwallid]) >
            llabs(r->corner_y[wallid] - r->corner_y[nextwallid])) {
        if (r->wall[wallid].normal_y < 0)
            wx = -wx;
        int64_t reference_x = (
            (wx + (int64_t)texinfo->
                tex_gamestate_scrollx + (int64_t)texinfo->
                tex_shiftx)
        );
        if (reference_x > 0) {
            *tx1 = (reference_x % repeat_units_x) *
                TEX_COORD_SCALER / repeat_units_x;
        } else {
            *tx1 = TEX_COORD_SCALER - (
                ((-reference_x) % repeat_units_x) *
                TEX_COORD_SCALER/ repeat_units_x
            );
        }
    } else {
        if (r->wall[wallid].normal_x > 0)
            wy = -wy;
        int64_t reference_x = (
            (wy + (int64_t)texinfo->
                tex_gamestate_scrollx + (int64_t)texinfo->
                tex_shiftx)
        );
        if (reference_x > 0) {
            *tx1 = (reference_x % repeat_units_x) *
                TEX_COORD_SCALER / repeat_units_x;
        } else {
            *tx1 = TEX_COORD_SCALER - (
                ((-reference_x) % repeat_units_x) *
                TEX_COORD_SCALER / repeat_units_x
            );
        }
    }
    int64_t relevant_wall_segment_top = (r->floor_z + r->height);
    if (!aboveportal && r->wall[wallid].has_portal) {
        relevant_wall_segment_top = (
            r->wall[wallid].portal_targetroom->floor_z
        );
        if (relevant_wall_segment_top < r->floor_z)
            relevant_wall_segment_top = r->floor_z;
    }
    int64_t relevant_wall_segment_bottom = (r->floor_z);
    if (aboveportal && r->wall[wallid].has_portal) {
        relevant_wall_segment_bottom = (
            r->wall[wallid].portal_targetroom->floor_z +
                r->wall[wallid].portal_targetroom->height
        );
        if (relevant_wall_segment_bottom > r->floor_z +
                r->height)
            relevant_wall_segment_bottom = r->floor_z +
                r->height;
    }
    int64_t _wtop = wtop;
    if (r->wall[wallid].wall_tex.
            tex_stickyside == ROOM_DIR_UP) {
        _wtop = relevant_wall_segment_top - wtop;
    } else if (r->wall[wallid].wall_tex.
            tex_stickyside == ROOM_DIR_DOWN) {
        _wtop = relevant_wall_segment_bottom - wtop;
    }
    int64_t reference_z = (
        (-_wtop + (int64_t)texinfo->
            tex_gamestate_scrolly + (int64_t)texinfo->
            tex_shifty)
    );
    if (reference_z > 0) {
        *ty1 = (reference_z % repeat_units_y) *
            TEX_REPEAT_UNITS / repeat_units_y;
    } else {
        *ty1 = TEX_COORD_SCALER - (
            ((-reference_z) % repeat_units_y) *
            TEX_COORD_SCALER / repeat_units_y
        );
    }
    int64_t reference_zheight = (
        wsliceheight
    );
    *ty2 = *ty1 + (
        (reference_zheight * TEX_COORD_SCALER) / repeat_units_y
    );
}

int32_t roomcam_XYZToViewplaneY(
        roomcam *cam, int w, int h, int64_t px, int64_t py, int64_t pz,
        int32_t *result
        ) {
    if (w <= 0 || h <= 0)
        return 0;
    if (!roomcam_CalculateViewPlane(cam, w, h)) {
        return 0;
    }
    *result = _roomcam_XYZToViewplaneY_NoRecalc(cam, px, py, pz);
    return 1;
}

int roomcam_XYToViewplaneX(
        roomcam *cam, int w, int h, int64_t px, int64_t py,
        int32_t *result
        ) {
    if (w <= 0 || h <= 0)
        return 0;
    if (!roomcam_CalculateViewPlane(cam, w, h)) {
        return 0;
    }
    *result = _roomcam_XYToViewplaneX_NoRecalc(cam, px, py);
    return 1;
}

HOTSPOT static int roomcam_DrawWallSlice(
        rfssurf *rendertarget,
        room *r, int wallno, int aboveportal,
        roomtexinfo *texinfo,
        int64_t hit_x, int64_t hit_y,
        int screentop, int screenbottom,
        int64_t topworldz, int64_t bottomworldz,
        int x, int y, int h
        ) {
    rfs2tex *t = roomlayer_GetTexOfRef(texinfo->tex);
    rfssurf *srf = (
        t ? graphics_GetTexSideways(t) : NULL
    );
    if (!srf) {
        if (!graphics_DrawRectangle(
                0.8, 0.2, 0.7, 1,
                x,
                y + screentop,
                1, screenbottom - y - screentop
                ))
            return 0;
    } else {
        int64_t tx, ty1, ty2, ty;
        assert(bottomworldz <= topworldz);
        roomcam_WallCubeMapping(
            r, wallno, hit_x, hit_y, aboveportal,
            texinfo, topworldz,
            topworldz - bottomworldz,
            &tx, &ty1, &ty2
        );
        const int targetw = rendertarget->w;
        uint8_t *tgpixels = rendertarget->pixels;
        const uint8_t *srcpixels = srf->pixels;
        const int rendertargetcopylen = (
            rendertarget->hasalpha ? 4 : 3
        );
        const int srccopylen = (
            srf->hasalpha ? 4 : 3
        );
        int sourcey = srf->h - (
            srf->h * (tx % TEX_COORD_SCALER) / TEX_COORD_SCALER
        );
        if (sourcey < 0) sourcey = 0;
        if (sourcey >= srf->h) sourcey = srf->h - 1;
        int sourcey_multiplied_w = sourcey * srf->w;
        int k = screentop;
        assert(screenbottom >= screentop);
        assert(screenbottom <= h);
        assert(screentop >= 0);
        if (screenbottom == h)
            screenbottom--;
        const int slicepixellen = (screenbottom - screentop);
        const int32_t ty1toty2diff = (ty2 - ty1);
        while (k <= screenbottom) {
            ty = (
                ty1 + (ty1toty2diff * (screenbottom - k) /
                    slicepixellen)
            );
            assert(ty >= 0);
            // Remember, we're using the sideways tex.
            int sourcex = (
                (srf->w * (TEX_COORD_SCALER - 1 - (ty % TEX_COORD_SCALER)))
                / TEX_COORD_SCALER
            );
            const int tgoffset = (
                (x + (y + k) * targetw) *
                rendertargetcopylen
            );
            const int srcoffset = (
                sourcex + sourcey_multiplied_w) *
                srccopylen;
            assert(srcoffset >= 0 &&
                srcoffset < srf->w * srf->h * srccopylen);
            int r = srcpixels[srcoffset + 0];
            int g = srcpixels[srcoffset + 1];
            int b = srcpixels[srcoffset + 2];
            tgpixels[tgoffset + 0] = r;
            tgpixels[tgoffset + 1] = g;
            tgpixels[tgoffset + 2] = b;
            if (rendertarget->hasalpha)
                tgpixels[tgoffset + 3] = 255;
            k++;
        }
    }
    return 1;
}

// Helper to interpolate wall pos in roomcam_RenderRoom (x):
#define RENDERROOM_IPOL_X(z) (\
    hit_x + (endhit_x - hit_x) *\
    (z - col) / imax(1, endcol - col)\
);

// Helper to interpolate wall pos in roomcam_RenderRoom (y)
#define RENDERROOM_IPOL_Y(z) (\
    hit_y + (endhit_y - hit_y) *\
    (z - col) / imax(1, endcol - col)\
);

int roomcam_RenderRoom(
        roomcam *cam, room *r, int x, int y, int w, int h,
        int xoffset, int max_xoffset, int nestdepth,
        int ignorewall
        ) {
    if (nestdepth > RENDER_MAX_PORTAL_DEPTH)
        return -1;
    renderstatistics *stats = &cam->cache->stats;
    stats->base_geometry_rooms_recursed++;
    #if defined(DEBUG_3DRENDERER)
    fprintf(stderr,
        "rfsc/roomcam.c: roomcam_RenderRoom cam_id=%" PRId64
        ", RECURSE into room_id=%" PRId64 " "
        "(floor_z=%" PRId64 ",height=%" PRId64 " -> "
        "checking range %d<->%d\n",
        cam->obj->id, r->id, r->floor_z, r->height, xoffset,
        ((max_xoffset >= 0 && max_xoffset < w) ? max_xoffset : w));
    #endif
    rfssurf *rendertarget = graphics_GetTargetSrf();
    if (!rendertarget)
        return -1;
    const int rendertargetcopylen = (
        rendertarget->hasalpha ? 4 : 3
    );
    const int batchwidth = (
        imax(16, w / WALL_BATCH_DIVIDER)
    );
    int col = xoffset;
    while (col < w && col <= max_xoffset) {
        /*printf("---\n");
        printf("render col %d/%d cam x,y %d,%d along %d,%d, "
            "angle %f roomid=%d max_xoffset=%d\n",
            col, w,
            (int)cam->obj->x, (int)cam->obj->y,
            (int)cam->cache->rotatedplanevecs_x[col],
            (int)cam->cache->rotatedplanevecs_y[col],
            (double) math_angle2di(
                cam->cache->rotatedplanevecs_x[col],
                cam->cache->rotatedplanevecs_y[col]
            )/ (double)ANGLE_SCALAR, (int)r->id,
            (int)max_xoffset);*/
        int wallno = -1;
        int64_t hit_x, hit_y;
        int intersect = math_polyintersect2di_ex(
            cam->obj->x, cam->obj->y,
            cam->obj->x + cam->cache->rotatedplanevecs_x[col] *
                VIEWPLANE_RAY_LENGTH_DIVIDER,
            cam->obj->y + cam->cache->rotatedplanevecs_y[col] *
                VIEWPLANE_RAY_LENGTH_DIVIDER,
            r->corners, r->corner_x, r->corner_y, ignorewall, 1,
            &wallno, &hit_x, &hit_y
        );
        stats->base_geometry_rays_cast++;
        assert(!intersect || ignorewall < 0 ||
               wallno != ignorewall);
        //printf("intersect: %d, wallno: %d\n", intersect, wallno);
        if (!intersect || wallno < 0) {
            #if (defined(DEBUG_3DRENDERER) && \
                defined(DEBUG_3DRENDERER_EXTRA) && !defined(NDEBUG))
            if (cam->obj->parentroom != NULL) {
                fprintf(stderr,
                    "rfsc/roomcam.c: warning: roomcam_RenderRoom "
                    "cam_id=%" PRId64 ", room_id=%" PRId64 " "
                    "-> RAY FAIL at col %d/%d "
                    " while NOT out of bounds, "
                    "ray x,y %" PRId64 ",%" PRId64 " -> "
                    "%" PRId64 ",%" PRId64 " "
                    "polygon x,y",  // intentionally no EOL!
                    cam->obj->id, r->id, col, w,
                    cam->obj->x, cam->obj->y,
                    cam->obj->x + cam->cache->rotatedplanevecs_x[col] *
                        VIEWPLANE_RAY_LENGTH_DIVIDER,
                    cam->obj->y + cam->cache->rotatedplanevecs_y[col] *
                        VIEWPLANE_RAY_LENGTH_DIVIDER);
                int i = 0;
                while (i < r->corners) {
                    fprintf(stderr,
                        " %" PRId64 ",%" PRId64,
                        r->corner_x[i], r->corner_y[i]);
                    i++;
                }
                fprintf(stderr, "\n");
            }
            #endif
            col++;
            continue;
        }

        // Find out where wall segment ends (=faster):
        int endcol = _roomcam_XYToViewplaneX_NoRecalc(
            cam, r->corner_x[wallno], r->corner_y[wallno]
        );
        if (endcol < col)
            endcol = col;
        if (endcol >= w || endcol > max_xoffset)
            endcol = ((w - 1 < max_xoffset ||
                max_xoffset < 0) ? w - 1 : max_xoffset);
        if (endcol > col + batchwidth)
            endcol = col + batchwidth;
        assert(endcol >= col);

        // Get target hit pos at wall end:
        int64_t endhit_x, endhit_y;
        int endwallno = -1;
        int endintersect = math_polyintersect2di_ex(
            cam->obj->x, cam->obj->y,
            cam->obj->x + cam->cache->rotatedplanevecs_x[endcol] *
                VIEWPLANE_RAY_LENGTH_DIVIDER,
            cam->obj->y + cam->cache->rotatedplanevecs_y[endcol] *
                VIEWPLANE_RAY_LENGTH_DIVIDER,
            r->corners, r->corner_x, r->corner_y, ignorewall, 1,
            &endwallno, &endhit_x, &endhit_y
        );
        stats->base_geometry_rays_cast++;
        assert(!endintersect || ignorewall < 0 ||
               endwallno != ignorewall);
        if (!endintersect || endwallno < 0) {
            // Weird, that shouldn't happen. Just limit the advancement,
            // and ignore it:
            endcol = col;
        }
        #if defined(DEBUG_3DRENDERER)
        fprintf(stderr,
            "rfsc/roomcam.c: roomcam_RenderRoom cam_id=%" PRId64
            ", room_id=%" PRId64 " -> slice col=%d<->%d\n",
            cam->obj->id, r->id, col, endcol);
        #endif

        if (r->wall[wallno].has_portal) {
            assert(r->wall[wallno].portal_targetroom != NULL);
            room *target = r->wall[wallno].portal_targetroom;

            // Recurse into portal, or black it out if nested too deep:
            int rendered_portalcols = 0;
            if (nestdepth + 1 <= RENDER_MAX_PORTAL_DEPTH) {
                int innercols = roomcam_RenderRoom(
                    cam, target, x, y, w, h,
                    col, endcol, nestdepth + 1,
                    r->wall[wallno].portal_targetwall
                );
                if (innercols <= 0)
                    return -1;
                /*printf("nesting returned, col: %d "
                    "innercols: %d, endcol: %d\n",
                    (int)col, (int)innercols, (int)endcol);*/
                rendered_portalcols = innercols;
            }
            // Now draw the wall parts above & below portal.
            int draw_above = 0;
            int draw_below = 0;
            if (target->floor_z + target->height <
                    r->floor_z + r->height) {
                draw_above = 1;
            }
            if (target->floor_z > r->floor_z) {
                draw_below = 1;
            }
            if (draw_above || draw_below) {
                roomtexinfo *abovetexinfo = (
                    r->wall[wallno].has_aboveportal_tex ?
                    &r->wall[wallno].aboveportal_tex :
                    &r->wall[wallno].wall_tex
                );
                roomtexinfo *belowtexinfo = (
                    &r->wall[wallno].wall_tex
                );
                int32_t z = col;
                while (z <= endcol) {
                    int64_t ipolhit_x = RENDERROOM_IPOL_X(z);
                    int64_t ipolhit_y = RENDERROOM_IPOL_Y(z);

                    if (draw_above) {
                        // Calculate above wall slice height:
                        int32_t top, bottom;
                        int64_t topworldz, bottomworldz;
                        if (!roomcam_WallSliceHeight(
                                cam, target->floor_z + target->height,
                                (r->height + r->floor_z) -
                                (target->floor_z + target->height),
                                h, ipolhit_x, ipolhit_y,
                                z, &topworldz, &bottomworldz,
                                &top, &bottom
                                )) {
                            // Fully off-screen.
                            z++;
                            continue;
                        }

                        // Draw slice:
                        if (!roomcam_DrawWallSlice(
                                rendertarget, r, wallno, 1,
                                abovetexinfo,
                                ipolhit_x, ipolhit_y,
                                top, bottom, topworldz, bottomworldz,
                                x + z, y, h
                                ))
                            return -1;
                        stats->base_geometry_slices_rendered++;
                        z++;
                    }
                    if (draw_below) {
                        // Calculate below wall slice height:
                        int32_t top, bottom;
                        int64_t topworldz, bottomworldz;
                        if (!roomcam_WallSliceHeight(
                                cam, r->floor_z,
                                (target->floor_z - r->floor_z),
                                h, ipolhit_x, ipolhit_y,
                                z, &topworldz, &bottomworldz,
                                &top, &bottom
                                )) {
                            // Fully off-screen.
                            z++;
                            continue;
                        }

                        // Draw slice:
                        if (!roomcam_DrawWallSlice(
                                rendertarget, r, wallno, 0,
                                belowtexinfo,
                                ipolhit_x, ipolhit_y,
                                top, bottom, topworldz, bottomworldz,
                                x + z, y, h
                                ))
                            return -1;
                        stats->base_geometry_slices_rendered++;
                        z++;
                    }
                }
            }
        } else {
            roomtexinfo *texinfo = (
                &r->wall[wallno].wall_tex
            );
            int32_t z = col;
            while (z <= endcol) {
                int64_t ipolhit_x = RENDERROOM_IPOL_X(z);
                int64_t ipolhit_y = RENDERROOM_IPOL_Y(z);

                // Calculate full wall slice height:
                int32_t top, bottom;
                int64_t topworldz, bottomworldz;
                if (!roomcam_WallSliceHeight(
                        cam, r->floor_z, r->height,
                        h, ipolhit_x, ipolhit_y,
                        z, &topworldz, &bottomworldz,
                        &top, &bottom
                        )) {
                    // Fully off-screen.
                    z++;
                    continue;
                }

                // Draw slice:
                if (!roomcam_DrawWallSlice(
                        rendertarget, r, wallno, 0, texinfo,
                        ipolhit_x, ipolhit_y,
                        top, bottom, topworldz, bottomworldz,
                        x + z, y, h
                        ))
                    return -1;
                stats->base_geometry_slices_rendered++;
                z++;
            }
        }

        // Call ceiling + floor render:
        int32_t z = col;
        while (z <= endcol) {
            // Floor:
            int64_t topwx, topwy, bottomwx, bottomwy;
            int32_t topoffset, bottomoffset;
            if (roomcam_FloorSliceHeight(
                    cam, r->floor_z,
                    r->corners, r->corner_x, r->corner_y,
                    h, z, 1, &topwx, &topwy, &bottomwx, &bottomwy,
                    &topoffset, &bottomoffset
                    )) {
                graphics_DrawRectangle(
                    1, 0, 0.5, 1,
                    z, topoffset, 1, bottomoffset - topoffset + 1
                );
            }
            z++;
        }

        // Advance to next slice:
        assert(endcol >= col);
        col = endcol + 1;
    }
    return col - xoffset;
}

int roomcam_Render(
        roomcam *cam, int x, int y, int w, int h
        ) {
    if (!graphics_PushRenderScissors(x, y, w, h))
        return 0;
    if (!graphics_ClearTarget()) {
        graphics_PopRenderScissors();
        return 0;
    }

    // Prepare render stats first:
    renderstatistics *stats = &cam->cache->stats;
    uint64_t fps_ts = stats->fps_ts;
    int32_t frames_accumulator = stats->frames_accumulator;
    int32_t fps = stats->fps;
    memset(stats, 0, sizeof(*stats));
    stats->fps_ts = fps_ts;
    stats->frames_accumulator = frames_accumulator + 1;
    stats->fps = fps;
    stats->last_canvas_width = w;
    stats->last_canvas_height = h;
    uint64_t now = datetime_Ticks();
    if (stats->fps_ts < now) {  // FPS calculation update
        stats->fps_ts += 1000;
        if (stats->fps_ts >= now && stats->frames_accumulator > 0) {
            stats->fps = stats->frames_accumulator;
        } else {
            // Unusable numbers. Reset things.
            stats->fps_ts = now;
            stats->fps = 1;
        }
        stats->frames_accumulator = 1;
    }

    // Ok, we're ready to render now:
    #if defined(DEBUG_3DRENDERER)
    fprintf(stderr,
        "rfsc/roomcam.c: roomcam_Render cam_id=%" PRId64
        " START, w=%d,h=%d\n",
        cam->obj->id, w, h);
    #endif
    if (!cam->obj->parentroom) {
        #if defined(DEBUG_3DRENDERER)
        fprintf(stderr,
            "rfsc/roomcam.c: debug: cam%" PRIu64
            ": not in any room\n", cam->obj->id);
        #endif
        graphics_PopRenderScissors();
        return 1;       
    }

    if (!roomcam_CalculateViewPlane(cam, w, h)) {
        graphics_PopRenderScissors();
        return 0;
    }
    stats->last_fov = cam->cache->cachedfov;
    stats->last_fovh = cam->cache->cachedfovh;
    stats->last_fovv = cam->cache->cachedfovv;

    if (roomcam_RenderRoom(cam, cam->obj->parentroom,
            x, y, w, h, 0, w, 0, -1) < 0) {
        graphics_PopRenderScissors();
        return 0;
    }

    graphics_PopRenderScissors();
    return 1;
}


