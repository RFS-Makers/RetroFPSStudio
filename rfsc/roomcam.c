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

#include "graphics.h"
#include "math2d.h"
#include "room.h"
#include "roomcam.h"
#include "roomdefs.h"
#include "roomlayer.h"
#include "roomobject.h"
#include "rfssurf.h"


typedef struct roomcamcache {
    int cachedangle, cachedvangle, cachedfov, cachedw, cachedh;

    int64_t planedist, planew, planeh;
    int64_t unrotatedplanevecs_left_x;
    int64_t unrotatedplanevecs_left_y;
    int64_t unrotatedplanevecs_right_x;
    int64_t unrotatedplanevecs_right_y;
    int64_t *planevecs_len;
    int32_t planevecs_alloc;

    int64_t *rotatedplanevecs_x, *rotatedplanevecs_y;
    int32_t rotatedalloc;
} roomcamcache;


static void _roomcam_FreeCache(roomcamcache *cache) {
    if (!cache)
        return;
    free(cache->planevecs_len);
    free(cache->rotatedplanevecs_x);
    free(cache->rotatedplanevecs_y);
    free(cache);
}

static void _roomcam_DestroyCallback(roomobj *camobj) {
    roomcam *cam = ((roomcam *)camobj->objdata);
    if (!cam)
        return;
    free(cam->cache);
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
            (h * ANGLE_SCALAR) / w
        );
        int32_t fov_h = (
            (cam->fov * aspectscalar / ANGLE_SCALAR) +
            cam->fov + cam->fov + cam->fov
        ) / 4;
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
        int64_t plane_right_x = plane_distance;
        int64_t plane_right_y = 0;
        math_rotate2di(&plane_right_x, &plane_right_y, -(fov_h / 2));
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
    }
    if (force_recalculate_rotated_vecs ||
            cam->cache->cachedangle != cam->obj->angle) {
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
            cam->cache->planevecs_len[i] = math_veclen(
                cam->cache->rotatedplanevecs_x[i],
                cam->cache->rotatedplanevecs_y[i]
            );
            i++;
        }
    }
    cam->cache->cachedangle = cam->obj->angle;
    cam->cache->cachedfov = cam->fov;
    cam->cache->cachedw = w;
    cam->cache->cachedh = h;
    return 1;
}

int roomcam_SliceHeight(
        roomcam *cam, room *r, int h, int64_t ix, int64_t iy,
        int xoffset, int64_t *topworldz, int64_t *bottomworldz,
        int32_t *topoffset, int32_t *bottomoffset
        ) {
    int64_t dist = math_veclen(
        ix - cam->obj->x, iy - cam->obj->y
    );
    int64_t planedist = cam->cache->planevecs_len[xoffset];
    int64_t planedistscalar = (dist * 100000LL) / planedist;
    int64_t screenheightatwall = (
        (cam->cache->planeh * planedistscalar) / 100000LL
    );
    /*printf("dist: %" PRId64 ", planedist: %" PRId64 ", "
        "planedistscalar: %" PRId64 ", "
        "screen height at plane: %" PRId64 ", "
        "screen height at wall: %" PRId64 ", wall "
        "height: %" PRId64 "\n",
        dist, planedist, planedistscalar,
        cam->cache->planeh,
        screenheightatwall,
        r->height);*/
    int64_t screentopatwall = (
        cam->obj->z + screenheightatwall / 2
    );
    if (screentopatwall < r->floor_z ||
            screentopatwall - screenheightatwall > r->floor_z + r->height)
        return 0;
    int64_t world_to_pixel_scalar = (
        ((int64_t)h * 100000L) / screenheightatwall
    );
    int64_t topoff_world = (screentopatwall - (
        r->floor_z + r->height
    ));
    int64_t _topworldz = (
        (screentopatwall >= r->floor_z + r->height) ?
        (r->floor_z + r->height) : screentopatwall);
    if (topoff_world < 0) {
        topoff_world = 0;
    }
    int64_t topoff_screen = (
        (topoff_world * world_to_pixel_scalar) / 100000L
    );
    int64_t bottomoff_world = (screentopatwall - (
        r->floor_z
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
            (int)r->height,
            (int)cam->obj->z, (int)r->floor_z, (int)_topworldz,
            (int)_bottomworldz,
            (int)(screentopatwall - _topworldz));*/
    if (topoff_screen >= h || bottomoff_screen < 0)
        return 0;  // off-screen.
    *topoffset = topoff_screen;
    *bottomoffset = bottomoff_screen;
    *topworldz = _topworldz;
    *bottomworldz = _bottomworldz;
    return 1;
}

void roomcam_WallCubeMapping(
        room *r, int wallid, int64_t wx, int64_t wy,
        int aboveportal,
        int64_t wtop, int64_t wsliceheight,
        int64_t *tx1, int64_t *ty1, int64_t *ty2
        ) {
    roomtexinfo *relevantinfo = (
        (aboveportal && r->wall[wallid].has_aboveportal_tex &&
        r->wall[wallid].has_portal) ?
        &r->wall[wallid].aboveportal_tex :
        &r->wall[wallid].wall_tex
    );
    int64_t repeat_units_x = (TEX_REPEAT_UNITS * (
        ((int64_t)relevantinfo->tex_scaleintx) /
        TEX_FULLSCALE_INT
    ));
    if (repeat_units_x < 1) repeat_units_x = 1;
    int64_t repeat_units_y = (TEX_REPEAT_UNITS * (
        ((int64_t)relevantinfo->tex_scaleinty) /
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
            (wx + (int64_t)relevantinfo->
                tex_gamestate_scrollx + (int64_t)relevantinfo->
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
            (wy + (int64_t)relevantinfo->
                tex_gamestate_scrollx + (int64_t)relevantinfo->
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
        (-_wtop + (int64_t)relevantinfo->
            tex_gamestate_scrolly + (int64_t)relevantinfo->
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

static int roomcam_DrawWallSlice(
        rfssurf *rendertarget,
        room *r, int wallno, int64_t hit_x, int64_t hit_y,
        int screentop, int screenbottom,
        int64_t topworldz, int64_t bottomworldz,
        int x, int y, int h
        ) {
    rfs2tex *t = roomlayer_GetTexOfRef(
        r->wall[wallno].wall_tex.tex
    );
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
            r, wallno, hit_x, hit_y,
            0, topworldz, topworldz - bottomworldz,
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
        while (k < screenbottom) {
            ty = (
                ty1 + ((ty2 - ty1) * (screenbottom - k) /
                    (screenbottom - screentop))
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
 
int roomcam_RenderRoom(
        roomcam *cam, room *r, int x, int y, int w, int h,
        int xoffset, int nestdepth
        ) {
    if (nestdepth > RENDER_MAX_PORTAL_DEPTH)
        return -1;
    rfssurf *rendertarget = graphics_GetTargetSrf();
    if (!rendertarget)
        return -1;
    const int rendertargetcopylen = (
        rendertarget->hasalpha ? 4 : 3
    );
    int col = xoffset;
    while (col < w) {
        /*printf("---\n");
        printf("render col %d/%d cam x,y %d,%d along %d,%d, "
            "angle %f\n",
            col, w,
            (int)cam->obj->x, (int)cam->obj->y,
            (int)cam->cache->rotatedplanevecs_x[col],
            (int)cam->cache->rotatedplanevecs_y[col],
            (double) math_angle2di(
                cam->cache->rotatedplanevecs_x[col],
                cam->cache->rotatedplanevecs_y[col]
            )/ (double)ANGLE_SCALAR);*/
        int wallno = -1;
        int64_t hit_x, hit_y;
        int intersect = math_polyintersect2di(
            cam->obj->x, cam->obj->y,
            cam->obj->x + cam->cache->rotatedplanevecs_x[col] *
                VIEWPLANE_RAY_LENGTH_DIVIDER,
            cam->obj->y + cam->cache->rotatedplanevecs_y[col] *
                VIEWPLANE_RAY_LENGTH_DIVIDER,
            r->corners, r->corner_x, r->corner_y,
            &wallno, &hit_x, &hit_y
        );
        /*printf("intersect %d wallno %d hit_x, hit_y %d, %d\n",
            intersect, wallno, (int)hit_x, (int)hit_y);*/
        if (!intersect || wallno < 0) {
            col++;
            continue;
        }
        if (r->wall[wallno].has_portal) {
            // Recurse into portal, or black it out if nested too deep:
            assert(r->wall[wallno].portal_targetroom != NULL);
            if (nestdepth + 1 <= RENDER_MAX_PORTAL_DEPTH) {
                int innercols = roomcam_RenderRoom(
                    cam, r->wall[wallno].portal_targetroom,
                    x, y, w, h,
                    col + xoffset, nestdepth + 1
                );
                if (innercols <= 0)
                    return -1;
                int k = 0;
                while (k < innercols) {
                    // Call ceiling + floor render:
                    // ...
                    k++;
                }
                col += innercols;
                continue;
            } else {
                // Calculate wall slice height:
                int32_t top, bottom;
                int64_t topworldz, bottomworldz;
                if (!roomcam_SliceHeight(
                        cam, r, h, hit_x, hit_y,
                        col + xoffset, &topworldz,
                        &bottomworldz, &top, &bottom
                        )) {
                    // Fully off-screen.
                    col++;
                    continue;
                }

                // Black it out:
                if (!graphics_DrawRectangle(
                        0, 0, 0, 1,
                        col + xoffset, y + top,
                        1, bottom - y - top
                        ))
                    return -1;
            }
        } else {
            // Calculate wall slice height:
            int32_t top, bottom;
            int64_t topworldz, bottomworldz;
            if (!roomcam_SliceHeight(
                    cam, r, h, hit_x, hit_y,
                    col + xoffset, &topworldz, &bottomworldz,
                    &top, &bottom
                    )) {
                // Fully off-screen.
                col++;
                continue;
            }

            // Draw slice:
            if (!roomcam_DrawWallSlice(
                    rendertarget, r, wallno, hit_x, hit_y,
                    top, bottom, topworldz, bottomworldz,
                    x + xoffset + col, y, h
                    ))
                return -1;
        }

        // Call ceiling + floor render:
        // ...

        // Advance to next slice:
        col++;
    }
    return col;
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

    if (roomcam_RenderRoom(cam, cam->obj->parentroom,
            x, y, w, h, 0, 0) < 0) {
        graphics_PopRenderScissors();
        return 0;
    }

    graphics_PopRenderScissors();
    return 1;
}


