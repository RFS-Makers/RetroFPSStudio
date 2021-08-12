// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "datetime.h"
#include "drawgeom.h"
#include "graphics.h"
#include "math2d.h"
#include "room.h"
#include "roomcam.h"
#include "roomdefs.h"
#include "roomlayer.h"
#include "roomobject.h"
#include "roomobject_block.h"
#include "roomrendercache.h"
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


static HOTSPOT inline int pixclip(int v) {
    if (unlikely(v > 255))
        return 255;
    if (unlikely(v < 0))
        return 0;
    return v;
}

renderstatistics *roomcam_GetStats(roomcam *c) {
    if (c && c->cache)
        return &c->cache->stats;
    return NULL;
}

int32_t _roomcam_XYZToViewplaneY_NoRecalc(
        roomcam *cam,
        int64_t px, int64_t py, int64_t pz,
        int coordsextrascaler
        ) {
    const int w = cam->cache->cachedw;
    const int h = cam->cache->cachedh;

    // Rotate/move vec into camera-local space:
    px -= cam->obj->x * coordsextrascaler;
    py -= cam->obj->y * coordsextrascaler;
    pz -= cam->obj->z * coordsextrascaler;
    math_rotate2di(&px, &py, -cam->obj->angle);

    // Special case of behind camera:
    if (px <= 0) {
        if (pz < 0)
            return h + h * 10;
        if (pz > 0)
            return -(h * 10);
        return 0;
    }

    // Scale vec down to exactly viewplane distance:
    int64_t scalef = 10000LL * (
        cam->cache->planedist * coordsextrascaler) / px;
    assert(cam->cache->vertivecs_x[0] == cam->cache->planedist);
    pz = (pz * scalef) / 1000LL;
    // (...we dont need px and py anymore, so ignore those)

    int res = ((pz - cam->cache->vertivecs_z[0] *
        coordsextrascaler * (int64_t)10LL) * h) /
        ((cam->cache->vertivecs_z[h - 1] -
        cam->cache->vertivecs_z[0]) * coordsextrascaler *
        (int64_t)10LL);
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

int GetFloorCeilingSafeInterpolationColumns(
        roomcam *cam, room *r, roomrendercache *rcache,
        int xcol, int *result
        ) {
    if (!roomrendercache_SetXCols(
            rcache, r, cam, cam->cache->cachedw,
            cam->cache->cachedh))
        return 0;
    int max_cols_ahead = (
        cam->cache->cachedw / (WALL_BATCH_DIVIDER * 1)
    );
    int k = 0;
    while (k < r->corners) {
        if (rcache->corners_to_screenxcol[k] >= xcol) {
            int corner_ahead = (
                rcache->corners_to_screenxcol[k] - xcol
            ) + 1;
            if (max_cols_ahead > corner_ahead)
                max_cols_ahead = corner_ahead;
        }
        k++;
    }
    if (xcol + max_cols_ahead > cam->cache->cachedw - 1)
        max_cols_ahead = (cam->cache->cachedw - 1) - xcol;
    if (max_cols_ahead < 1) {
        *result = 1;
        return 1;
    }
    *result = max_cols_ahead;
    return 1;
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

void roomcam_FloorCeilingCubeMapping(
        roomtexinfo *texinfo,
        int64_t px1, int64_t py1,
        int64_t px2, int64_t py2,
        int64_t *tx1, int64_t *ty1,
        int64_t *tx2, int64_t *ty2
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
    {
        int64_t reference_x = (
            (px1 + (int64_t)texinfo->
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
    }
    {
        int64_t reference_y = (
            (py1 + (int64_t)texinfo->
                tex_gamestate_scrolly + (int64_t)texinfo->
                tex_shifty)
        );
        if (reference_y > 0) {
            *ty1 = (reference_y % repeat_units_y) *
                TEX_COORD_SCALER / repeat_units_y;
        } else {
            *ty1 = TEX_COORD_SCALER - (
                ((-reference_y) % repeat_units_y) *
                TEX_COORD_SCALER / repeat_units_y
            );
        }
    }
    *tx2 = (*tx1) + (
        ((px2 - px1) * TEX_COORD_SCALER) / repeat_units_x
    );
    *ty2 = (*ty1) + (
        ((py2 - py1) * TEX_COORD_SCALER) / repeat_units_y
    );
}

HOTSPOT int roomcam_DrawFloorCeilingSlice(
        rfssurf *rendertarget, room *r,
        drawgeom *geom, int isfacingup,
        int64_t top_world_x, int64_t top_world_y,
        int64_t top_world_z,
        int64_t bottom_world_x, int64_t bottom_world_y,
        int64_t bottom_world_z,
        roomtexinfo *texinfo,
        int screentop, int screenbottom,
        int x, int y, int h,
        int cr, int cg, int cb,
        int cr2, int cg2, int cb2
        ) {
    if (!rendertarget)
        return 0;
    assert(screenbottom <= h);
    assert(screentop >= 0);
    if (screenbottom == h)
        screenbottom--;
    if (screentop > screenbottom)
        return 1;

    rfs2tex *t = roomlayer_GetTexOfRef(texinfo->tex);
    rfssurf *srf = (t ? t->srfalpha : NULL);
    if (!srf) {
        if (!graphics_DrawRectangle(
                0.8, 0.2, 0.7, 1,
                x,
                y + screentop,
                1, screenbottom - y - screentop
                ))
            return 0;
        return 1;
    }

    /*if (x == 100) {
        printf("world top x,y %" PRId64 ",%" PRId64 " "
            "world bottom x,y %" PRId64 ",%" PRId64 "\n",
            bottom_world_x, bottom_world_y,
            top_world_x, top_world_y);
    }*/
    const int max_perspective_cheat_columns = h / 20;
    assert(screentop >= 0 && screentop <= h);
    assert(screenbottom >= 0 && screenbottom <= h);
    int row = screentop;
    int64_t past_world_x = top_world_x;
    int64_t past_world_y = top_world_y;
    while (row <= screenbottom) {
        int endrow = row + max_perspective_cheat_columns;
        if (endrow > screenbottom) endrow = screenbottom;
        assert(endrow >= row);
        int64_t endscalar = (4096 * (endrow - screentop + 1) /
            (screenbottom - screentop + 1));
        assert(endscalar >= 0 && endscalar <= 4096);
        int64_t target_world_x, target_world_y, target_world_z;
        target_world_x = top_world_x + ((
            (bottom_world_x - top_world_x) * endscalar) /
            (int64_t)4096);
        target_world_y = top_world_y + ((
            (bottom_world_y - top_world_y) * endscalar) /
            (int64_t)4096);
        int64_t toptx, topty, bottomtx, bottomty;
        roomcam_FloorCeilingCubeMapping(
            texinfo,
            past_world_x, past_world_y,
            target_world_x, target_world_y,
            &toptx, &topty, &bottomtx, &bottomty
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
        const int64_t tx1totx2diff = (bottomtx - toptx);
        const int64_t ty1toty2diff = (bottomty - topty);
        const int cr1to2diff = (cr2 - cr);
        const int cg1to2diff = (cg2 - cg);
        const int cb1to2diff = (cb2 - cb);
        const int slicepixellen = (endrow - row) + 1;
        while (likely(row <= endrow)) {
            int red = cr + ((cr1to2diff * (endrow - row)) /
                slicepixellen);
            int green = cg + ((cg1to2diff * (endrow - row)) /
                slicepixellen);
            int blue = cb + ((cb1to2diff * (endrow - row)) /
                slicepixellen);
            int64_t tx = (
                toptx + ((tx1totx2diff * (endrow - row)) /
                    slicepixellen)
            );
            if (tx < 0) tx = (TEX_COORD_SCALER - 1 - (((-tx) - 1) %
                TEX_COORD_SCALER));
            int64_t ty = (
                topty + ((ty1toty2diff * (endrow - row)) /
                    slicepixellen)
            );
            if (ty < 0) ty = (TEX_COORD_SCALER - 1 - (((-ty) - 1) %
                TEX_COORD_SCALER));
            // Here, we're using the non-sideways regular tex.
            const int srcoffset = (
                (((tx % TEX_COORD_SCALER) * srf->w / TEX_COORD_SCALER)) +
                srf->w * (((ty % TEX_COORD_SCALER) * srf->h) / TEX_COORD_SCALER)
            ) * srccopylen;
            const int tgoffset = (
                (x + (y + row) * targetw) *
                rendertargetcopylen
            );
            assert(srcoffset >= 0 &&
                srcoffset < srf->w * srf->h * srccopylen);
            assert(tgoffset >= 0 && tgoffset < rendertarget->w *
                rendertarget->h * rendertargetcopylen);
            int finalr = pixclip(
                srcpixels[srcoffset + 0] * red /
                LIGHT_COLOR_SCALAR);
            int finalg = pixclip(
                srcpixels[srcoffset + 1] * green /
                LIGHT_COLOR_SCALAR);
            int finalb = pixclip(
                srcpixels[srcoffset + 2] * blue /
                LIGHT_COLOR_SCALAR);
            tgpixels[tgoffset + 0] = finalr;
            tgpixels[tgoffset + 1] = finalg;
            tgpixels[tgoffset + 2] = finalb;
            if (rendertarget->hasalpha)
                tgpixels[tgoffset + 3] = 255;
            row++;
        }
        assert(row == endrow + 1);
        past_world_x = target_world_x;
        past_world_y = target_world_y;
    }
    return 1;
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
        assert(cam->fov > 0);
        int32_t aspectscalar = (
            (w * ANGLE_SCALAR) / h
        );
        int32_t fov_h = (int64_t)(
            (int64_t)1 * ((int64_t)cam->fov * (int64_t)aspectscalar) +
            (int64_t)cam->fov * 10 * (int64_t)ANGLE_SCALAR
        ) / ((int64_t)11 * (int64_t)ANGLE_SCALAR);
        if (fov_h > 120 * ANGLE_SCALAR) fov_h = 120 * ANGLE_SCALAR;
        assert(fov_h >= 0);
        if (fov_h < 30 * ANGLE_SCALAR) fov_h = 30 * ANGLE_SCALAR;
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

int roomcam_FloorCeilingSliceHeight(
        roomcam *cam, drawgeom *geom,
        int h, int xoffset,
        int isfacingup, int *onscreen,
        int64_t *topworldx, int64_t *topworldy,
        int64_t *bottomworldx, int64_t *bottomworldy,
        int32_t *topoffset, int32_t *bottomoffset
        ) {
    // First, basic checks and collecting info:
    renderstatistics *stats = &cam->cache->stats;
    int geometry_corners = 0;
    int64_t geometry_z = 0;
    int64_t *geometry_corner_x = NULL;
    int64_t *geometry_corner_y = NULL;
    roomrendercache *rcache = NULL;
    room *r = NULL;
    if (geom->type == DRAWGEOM_ROOM) {
        r = geom->r;
        rcache = room_GetRenderCache(r->id);
        if (!rcache || !roomrendercache_FillUpscaledCoords(
                rcache, r))
            return 0;
        geometry_corners = r->corners;
        geometry_z = r->floor_z;
        if (!isfacingup) geometry_z += r->height;
        geometry_corner_x = rcache->upscaledcorner_x;
        geometry_corner_y = rcache->upscaledcorner_y;
    } else if (geom->type == DRAWGEOM_BLOCK) {
        r = geom->bl->obj->parentroom;
    } else {
        return 0;
    }
    if (geometry_z == cam->obj->z)
        return 0;
    if (isfacingup && geometry_z > cam->obj->z)
        return 0;
    else if (!isfacingup && geometry_z < cam->obj->z)
        return 0;

    // First, get the angle along the viewing slice:
    int32_t angle = cam->obj->angle;
    int32_t vangle = cam->vangle;
    int64_t intersect_lx1 = cam->obj->x *
        DRAW_CORNER_COORD_UPSCALE;
    int64_t intersect_ly1 = cam->obj->y *
        DRAW_CORNER_COORD_UPSCALE;
    int64_t intersect_lx2 = intersect_lx1 +
        cam->cache->rotatedplanevecs_x[xoffset] *
        VIEWPLANE_RAY_LENGTH_DIVIDER *
        DRAW_CORNER_COORD_UPSCALE;
    int64_t intersect_ly2 = intersect_ly1 +
        cam->cache->rotatedplanevecs_y[xoffset] *
        VIEWPLANE_RAY_LENGTH_DIVIDER *
        DRAW_CORNER_COORD_UPSCALE;

    // Then, intersect test with the geometry polygon:
    stats->base_geometry_rays_cast++;
    int camtoaway_wallno;
    int64_t camtoaway_ix, camtoaway_iy;
    int camtoaway_intersect = math_polyintersect2di_ex(
        intersect_lx1, intersect_ly1,
        intersect_lx2, intersect_ly2,
        geometry_corners,
        geometry_corner_x, geometry_corner_y,
        -1, 1, &camtoaway_wallno, &camtoaway_ix,
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
        -1, 1, &awaytocam_wallno, &awaytocam_ix,
        &awaytocam_iy
    );
    if (!awaytocam_intersect)
        return 0;

    // Compute "closer" geometry end point on screen:
    int32_t close_screen_offset = 0;
    int64_t close_world_x = camtoaway_ix;
    int64_t close_world_y = camtoaway_iy;
    if (awaytocam_wallno == camtoaway_wallno &&
            math_polycontains2di(
            intersect_lx1, intersect_ly1,
            geometry_corners,
            geometry_corner_x, geometry_corner_y)) {
        // Camera is "inside" polygon, so closer border extends past screen
        assert(h <= cam->cache->cachedh);
        if (isfacingup) {
            assert(geometry_z < cam->obj->z);
            int64_t lowerscreen_x =
                cam->cache->rotatedplanevecs_x[xoffset];
            int64_t lowerscreen_y =
                cam->cache->rotatedplanevecs_y[xoffset];
            int64_t lowerscreen_z = cam->cache->vertivecs_z[h - 1];
            assert(lowerscreen_z < 0);
            int64_t scalar = ((int64_t)1000 *
                (geometry_z - cam->obj->z)) / lowerscreen_z;
            assert(llabs(
                (lowerscreen_z * scalar / (int64_t)1000LL) -
                (geometry_z - cam->obj->z)
            ) <= 5);
            close_world_x = lowerscreen_x * scalar / (int64_t)1000LL;
            close_world_y = lowerscreen_y * scalar / (int64_t)1000LL;
            math_rotate2di(
                &close_world_x, &close_world_y, cam->obj->angle
            );
            /*int testo = _roomcam_XYZToViewplaneY_NoRecalc(
                cam, close_world_x, close_world_y, geometry_z, 1
            );
            if (testo < h) {
                printf("cam z: %" PRId64 ", geo z: %" PRId64 "\n",
                    cam->obj->z, geometry_z);
                printf("h back project: %d\n", testo);
            }
            assert(1 || _roomcam_XYZToViewplaneY_NoRecalc(
                cam, close_world_x, close_world_y, geometry_z, 1
            ) >= h - 5);*/
            close_screen_offset = h - 1;
        } else {
            assert(geometry_z > cam->obj->z);
            int64_t upperscreen_x =
                cam->cache->rotatedplanevecs_x[xoffset];
            int64_t upperscreen_y =
                cam->cache->rotatedplanevecs_y[xoffset];
            int64_t upperscreen_z = cam->cache->vertivecs_z[0];
            assert(upperscreen_z > 0);
            int64_t scalar = ((int64_t)1000LL *
                (geometry_z - cam->obj->z)) / upperscreen_z;
            close_world_x = upperscreen_x * scalar / (int64_t)1000LL;
            close_world_y = upperscreen_y * scalar / (int64_t)1000LL;
            math_rotate2di(
                &close_world_x, &close_world_y, cam->obj->angle
            );
            close_screen_offset = 0;
        }
    } else {
        close_screen_offset = _roomcam_XYZToViewplaneY_NoRecalc(
            cam, close_world_x, close_world_y,
            geometry_z * DRAW_CORNER_COORD_UPSCALE,
            DRAW_CORNER_COORD_UPSCALE
        );
        close_world_x /= DRAW_CORNER_COORD_UPSCALE;
        close_world_y /= DRAW_CORNER_COORD_UPSCALE;
    }
    // Compute "farther" geometry point on screen:
    int64_t far_world_x = awaytocam_ix;
    int64_t far_world_y = awaytocam_iy;
    int32_t far_screen_offset = _roomcam_XYZToViewplaneY_NoRecalc(
        cam, far_world_x, far_world_y,
        geometry_z * DRAW_CORNER_COORD_UPSCALE,
        DRAW_CORNER_COORD_UPSCALE
    );
    far_world_x /= DRAW_CORNER_COORD_UPSCALE;
    far_world_y /= DRAW_CORNER_COORD_UPSCALE;
    /*if (d) {
        printf("close x,y,z,screen: %" PRId64
            ",%" PRId64 ",%" PRId64 ",%d\n",
            close_world_x, close_world_y, geometry_z,
            close_screen_offset);
        printf("far x,y,z,screen: %" PRId64
            ",%" PRId64 ",%" PRId64 ",%d\n",
            far_world_x, far_world_y, geometry_z,
            far_screen_offset);
    }*/
    if ((isfacingup && (far_screen_offset >
            close_screen_offset)) ||
            (!isfacingup && (far_screen_offset <
            close_screen_offset))) {
        // Segment is behind camera.
        *onscreen = 0;
    } else {
        *onscreen = 1;
    }
    // Return in proper order:
    if (!isfacingup) {
        *topworldx = close_world_x;
        *topworldy = close_world_y;
        *topoffset = close_screen_offset;
        *bottomworldx = far_world_x;
        *bottomworldy = far_world_y;
        *bottomoffset = far_screen_offset;
    } else {
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
    if (bottomoff_screen >= h) {
        bottomoff_screen = h - 1;
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
    *result = _roomcam_XYZToViewplaneY_NoRecalc(cam, px, py, pz, 1);
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
        int x, int y, int h, int cr, int cg, int cb
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
        while (likely(k <= screenbottom)) {
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
            int finalr = pixclip(
                srcpixels[srcoffset + 0] * cr /
                LIGHT_COLOR_SCALAR);
            int finalg = pixclip(
                srcpixels[srcoffset + 1] * cg /
                LIGHT_COLOR_SCALAR);
            int finalb = pixclip(
                srcpixels[srcoffset + 2] * cb /
                LIGHT_COLOR_SCALAR);
            tgpixels[tgoffset + 0] = finalr;
            tgpixels[tgoffset + 1] = finalg;
            tgpixels[tgoffset + 2] = finalb;
            if (rendertarget->hasalpha)
                tgpixels[tgoffset + 3] = 255;
            k++;
        }
    }
    return 1;
}

int roomcam_DrawFloorCeiling(
        roomcam *cam, drawgeom *geom,
        int canvasx, int canvasy,
        int xcol, int endcol
        ) {
    // Collect some info first:
    room *r = NULL;
    if (geom->type == DRAWGEOM_ROOM) {
        r = geom->r;
    } else if (geom->type == DRAWGEOM_BLOCK) {
        r = geom->bl->obj->parentroom;
    } else {
        return 0;
    }
    rfssurf *rendertarget = graphics_GetTargetSrf();
    if (!rendertarget)
        return 0;
    const int w = cam->cache->cachedw;
    const int h = cam->cache->cachedh;
    renderstatistics *stats = &cam->cache->stats;
    roomrendercache *rcache = room_GetRenderCache(r->id);

    // Do floor render:
    int prev_valuesset = 0;
    int64_t prev_topwx, prev_topwy, prev_bottomwx, prev_bottomwy;
    int32_t prev_topoffset, prev_bottomoffset;
    int prev_onscreen;
    int32_t z = xcol;
    while (likely(z <= endcol)) {
        // Check over which segment to interpolate the perspective:
        int max_safe_next_cols = -1;
        if (!GetFloorCeilingSafeInterpolationColumns(
                cam, r, rcache, z, &max_safe_next_cols
                )) {
            return 0;
        }
        assert(max_safe_next_cols >= 1);
        int innerendcol = max_safe_next_cols + z;
        if (innerendcol > endcol)
            innerendcol = endcol;
        assert(innerendcol >= z);
        int64_t top1wx, top1wy, bottom1wx, bottom1wy;
        int32_t top1offset, bottom1offset;
        int onscreen1 = 0;
        if (prev_valuesset) {
            prev_valuesset = 0;
            top1wx = prev_topwx;
            top1wy = prev_topwy;
            bottom1wx = prev_bottomwx;
            bottom1wy = prev_bottomwy;
            top1offset = prev_topoffset;
            bottom1offset = prev_bottomoffset;
            onscreen1 = prev_onscreen;
        } else {
            if (!roomcam_FloorCeilingSliceHeight(
                    cam, geom,
                    h, z, 1, &onscreen1,
                    &top1wx, &top1wy, &bottom1wx, &bottom1wy,
                    &top1offset, &bottom1offset
                    )) {
                prev_valuesset = 0;
                z = innerendcol + 1;
                break;
            }
        }
        int64_t top2wx, top2wy, bottom2wx, bottom2wy;
        int32_t top2offset, bottom2offset;
        int onscreen2 = 0;
        if (!roomcam_FloorCeilingSliceHeight(
                cam, geom,
                h, innerendcol, 1, &onscreen2,
                &top2wx, &top2wy, &bottom2wx, &bottom2wy,
                &top2offset, &bottom2offset
                )) {
            prev_valuesset = 0;
            z = innerendcol + 1;
            break;
        } else {
            prev_valuesset = 1;
            prev_topwx = top2wx;
            prev_topwy = top2wy;
            prev_bottomwx = bottom2wx;
            prev_bottomwy = bottom2wy;
            prev_topoffset = top2offset;
            prev_bottomoffset = bottom2offset;
            prev_onscreen = onscreen2;
        }

        // Render floor by interpolating over above info:
        roomtexinfo *texinfo = NULL;
        if (geom->type == DRAWGEOM_ROOM) {
            texinfo = &geom->r->floor_tex;
        } else if (geom->type == DRAWGEOM_BLOCK) {
            //texinfo =
        } else {
            assert(0);
            return 0;
        }
        const int startz = z;
        while (likely(z <= innerendcol)) {
            const int64_t scalarrange = (1024 * 8);
            const int64_t scalar = ((scalarrange *
                (z - startz)) / imax(1, innerendcol - startz));
            assert(scalar >= 0 && scalar <= scalarrange);
            int64_t topwx = (
                top1wx + (top2wx - top1wx) * scalar / scalarrange
            );
            int64_t topwy = (
                top1wy + (top2wy - top1wy) * scalar / scalarrange
            );
            int64_t bottomwx = (
                bottom1wx + (bottom2wx - bottom1wx) *
                scalar / scalarrange
            );
            int64_t bottomwy = (
                bottom1wy + (bottom2wy - bottom1wy) *
                scalar / scalarrange
            );
            int32_t topoffset = imin(h, imax(-1,
                top1offset + ((top2offset - top1offset) *
                scalar) / scalarrange
            ));
            int32_t bottomoffset = imin(h, imax(-1,
                bottom1offset + ((bottom2offset - bottom1offset) *
                scalar) / scalarrange
            ));
            /*if (1) {
                printf("topwx,topwy: %" PRId64 ",%" PRId64 " "
                    "bottomwx,bottomwy: %" PRId64 ",%" PRId64 " "
                    "top screen: %d, bottom screen: %d\n",
                    topwx, topwy, bottomwx, bottomwy,
                    (int)topoffset, (int)bottomoffset);
            }*/
            if (bottomoffset >= topoffset) {
                int cr = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                    r->sector_light_r));
                int cg = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                    r->sector_light_g));
                int cb = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                    r->sector_light_b));
                assert(
                    math_veclen2di(
                        topwx - cam->obj->x, topwy - cam->obj->y
                    ) >= math_veclen2di(
                        bottomwx - cam->obj->x, bottomwy - cam->obj->y
                    ) || llabs(bottomoffset == topoffset) <= 2
                );
                roomcam_DrawFloorCeilingSlice(
                    rendertarget, r, geom, 1,
                    topwx, topwy, r->floor_z,
                    bottomwx, bottomwy, r->floor_z,
                    texinfo, topoffset, bottomoffset,
                    canvasx + z, canvasy, h, cr, cg, cb,
                    cr, cg, cb
                );
                /*graphics_DrawRectangle(
                    1, fmin(1.0, ((double)r->id) * 0.2), 0.5, 1,
                    canvasx + z, canvasy + topoffset, 1,
                    bottomoffset - topoffset + 1
                );*/
            }
            z++;
        }
        assert(z > innerendcol);
    }

    // Do ceiling render:
    prev_valuesset = 0;
    z = xcol;
    while (likely(z <= endcol)) {
        // Check over which segment to interpolate the perspective:
        int max_safe_next_cols = -1;
        if (!GetFloorCeilingSafeInterpolationColumns(
                cam, r, rcache, z, &max_safe_next_cols
                )) {
            return 0;
        }
        assert(max_safe_next_cols >= 1);
        int innerendcol = max_safe_next_cols + z;
        if (innerendcol > endcol)
            innerendcol = endcol;
        assert(innerendcol >= z);
        int64_t top1wx, top1wy, bottom1wx, bottom1wy;
        int32_t top1offset, bottom1offset;
        int onscreen1 = 0;
        if (prev_valuesset) {
            prev_valuesset = 0;
            top1wx = prev_topwx;
            top1wy = prev_topwy;
            bottom1wx = prev_bottomwx;
            bottom1wy = prev_bottomwy;
            top1offset = prev_topoffset;
            bottom1offset = prev_bottomoffset;
            onscreen1 = prev_onscreen;
        } else {
            if (!roomcam_FloorCeilingSliceHeight(
                    cam, geom,
                    h, z, 0, &onscreen1,
                    &top1wx, &top1wy, &bottom1wx, &bottom1wy,
                    &top1offset, &bottom1offset
                    )) {
                prev_valuesset = 0;
                z = innerendcol + 1;
                break;
            }
        }
        int64_t top2wx, top2wy, bottom2wx, bottom2wy;
        int32_t top2offset, bottom2offset;
        int onscreen2 = 0;
        if (!roomcam_FloorCeilingSliceHeight(
                cam, geom,
                h, innerendcol, 0, &onscreen2,
                &top2wx, &top2wy, &bottom2wx, &bottom2wy,
                &top2offset, &bottom2offset
                )) {
            prev_valuesset = 0;
            z = innerendcol + 1;
            break;
        } else {
            prev_valuesset = 1;
            prev_topwx = top2wx;
            prev_topwy = top2wy;
            prev_bottomwx = bottom2wx;
            prev_bottomwy = bottom2wy;
            prev_topoffset = top2offset;
            prev_bottomoffset = bottom2offset;
            prev_onscreen = onscreen2;
        }

        // Render floor by interpolating over above info:
        roomtexinfo *texinfo = NULL;
        if (geom->type == DRAWGEOM_ROOM) {
            texinfo = &geom->r->floor_tex;
        } else if (geom->type == DRAWGEOM_BLOCK) {
            //texinfo =
        } else {
            assert(0);
            return 0;
        }
        const int startz = z;
        while (likely(z <= innerendcol)) {
            const int64_t scalarrange = (1024 * 8);
            const int64_t scalar = ((scalarrange *
                (z - startz)) / imax(1, innerendcol - startz));
            int64_t topwx = (
                top1wx + (top2wx - top1wx) * scalar / scalarrange
            );
            int64_t topwy = (
                top1wy + (top2wy - top1wy) * scalar / scalarrange
            );
            int64_t bottomwx = (
                bottom1wx + (bottom2wx - bottom1wx) *
                scalar / scalarrange
            );
            int64_t bottomwy = (
                bottom1wy + (bottom2wy - bottom1wy) *
                scalar / scalarrange
            );
            int32_t topoffset = imin(h, imax(-1,
                top1offset + ((top2offset - top1offset) *
                scalar) / scalarrange
            ));
            int32_t bottomoffset = imin(h, imax(-1,
                bottom1offset + ((bottom2offset - bottom1offset) *
                scalar) / scalarrange
            ));
            if (bottomoffset >= topoffset) {
                int cr = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                    r->sector_light_r));
                int cg = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                    r->sector_light_g));
                int cb = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                    r->sector_light_b));
                /*assert(
                    math_veclen2di(
                        topwx - cam->obj->x, topwy - cam->obj->y
                    ) <= math_veclen2di(
                        bottomwx - cam->obj->x, bottomwy - cam->obj->y
                    ) || llabs(bottomoffset - topoffset) <= 2
                );*/
                roomcam_DrawFloorCeilingSlice(
                    rendertarget, r, geom, 0,
                    topwx, topwy, r->floor_z,
                    bottomwx, bottomwy, r->floor_z,
                    texinfo, topoffset, bottomoffset,
                    canvasx + z, canvasy, h, cr, cg, cb,
                    cr, cg, cb
                );
            }
            z++;
        }
        assert(z > innerendcol);
    }
    return 1;
}

int roomcam_DrawWall(
        roomcam *cam, drawgeom *geom,
        int canvasx, int canvasy,
        int xcol, int endcol,
        int originwall, roomtexinfo *texinfo,
        int64_t wall_floor_z, int64_t wall_height
        ) {
    // First, extract some info:
    room *r = NULL;
    if (geom->type == DRAWGEOM_ROOM) {
        r = geom->r;
    } else if (geom->type == DRAWGEOM_BLOCK) {
        r = geom->bl->obj->parentroom;
    } else {
        return 0;
    }
    rfssurf *rendertarget = graphics_GetTargetSrf();
    if (!rendertarget)
        return 0;
    const int w = cam->cache->cachedw;
    const int h = cam->cache->cachedh;
    renderstatistics *stats = &cam->cache->stats;
    roomrendercache *rcache = room_GetRenderCache(r->id);
    #if defined(DEBUG_3DRENDERER) && defined(DEBUG_3DRENDERER_EXTRA)
    fprintf(stderr, "rfsc/roomcam.c: debug: "
        "roomcam_DrawWall r->id %" PRId64 " "
        "col=%d,%d\n",
        r->id, xcol, endcol);
    #endif

    // Some sanity checks:
    if (xcol < 0) xcol = 0;
    if (endcol < xcol) return 1;
    if (!rcache || !r)
        return 0;
    roomrendercache_SetXCols(  // require column data
        rcache, r, cam, cam->cache->cachedw, cam->cache->cachedh
    );
    if (endcol < rcache->corners_minscreenxcol ||
            xcol > rcache->corners_maxscreenxcol)
        return 1;
    if (xcol < rcache->corners_minscreenxcol)
        xcol = rcache->corners_minscreenxcol;
    if (endcol >= w)
        endcol = w - 1;
    if (endcol < xcol)
        return 1;

    // Do the actual drawing:
    const int orig_xcol = xcol;
    int prev_hitset = 0;
    int64_t prev_hitx, prev_hity;
    int prev_wallno = -1;
    while (xcol <= endcol) {
        // Calculate perspective data to interpolate between:
        int start_wallno = -1;
        int64_t start_hitx, start_hity;
        if (prev_hitset) {
            prev_hitset = 0;
            start_wallno = prev_wallno;
            start_hitx = prev_hitx; start_hity = prev_hity;
        } else {
            int _temp_xcol = xcol;
            while (1) {
                int intersect = math_polyintersect2di_ex(
                    cam->obj->x, cam->obj->y,
                    cam->obj->x + cam->cache->rotatedplanevecs_x[
                        _temp_xcol] *
                        VIEWPLANE_RAY_LENGTH_DIVIDER,
                    cam->obj->y + cam->cache->rotatedplanevecs_y[
                        _temp_xcol] *
                        VIEWPLANE_RAY_LENGTH_DIVIDER,
                    r->corners, r->corner_x, r->corner_y, originwall, 1,
                    &start_wallno, &start_hitx, &start_hity
                );
                stats->base_geometry_rays_cast++;
                if (!intersect) {
                    if (_temp_xcol < endcol && _temp_xcol < xcol + 3) {
                        _temp_xcol++;
                        continue;
                    } else {
                        start_wallno = -1;
                        break;
                    }
                }
                assert(start_wallno >= 0);
                break;
            }
            if (start_wallno < 0) {
                xcol++;
                prev_hitset = 0;
                continue;
            }
        }
        int max_safe_next_segment = -1;
        if (!GetFloorCeilingSafeInterpolationColumns(
                cam, r, rcache, xcol, &max_safe_next_segment
                )) {
            return 0;
        }
        int max_render_ahead = imax(1, imin(
            max_safe_next_segment + 1, endcol - xcol + 1));
        int _temp_max_render_ahead = max_render_ahead;
        int end_wallno = -1;
        int64_t end_hitx, end_hity;
        while (1) {
            int intersect = math_polyintersect2di_ex(
                cam->obj->x, cam->obj->y,
                cam->obj->x + cam->cache->rotatedplanevecs_x[
                    imin(w - 1, xcol + _temp_max_render_ahead - 1)] *
                    VIEWPLANE_RAY_LENGTH_DIVIDER,
                cam->obj->y + cam->cache->rotatedplanevecs_y[
                    imin(w - 1, xcol + _temp_max_render_ahead - 1)] *
                    VIEWPLANE_RAY_LENGTH_DIVIDER,
                r->corners, r->corner_x, r->corner_y, originwall, 1,
                &end_wallno, &end_hitx, &end_hity
            );
            stats->base_geometry_rays_cast++;
            if (!intersect) {
                if (_temp_max_render_ahead > 1 &&
                        _temp_max_render_ahead > max_render_ahead - 4) {
                    _temp_max_render_ahead--;
                    continue;
                } else if (_temp_max_render_ahead <= 1) {
                    end_wallno = start_wallno;
                    end_hitx = start_hitx;
                    end_hity = start_hity;
                    break;
                } else {
                    end_wallno = -1;
                    break;
                }
            } else {
                prev_hitset = 1;
                prev_wallno = start_wallno;  // intended. more reliable.
                prev_hitx = end_hitx; prev_hity = end_hity;
                assert(end_wallno >= 0);
                break;
            }
        }
        if (end_wallno < 0) {
            xcol++;
            prev_hitset = 0;
            continue;
        }

        // Do actual render:
        int z = xcol;
        while (likely(z < xcol + max_render_ahead)) {
            assert(z <= endcol);
            int64_t ix = (
                start_hitx + ((end_hitx - start_hitx) *
                (z - xcol)) / imax(1, endcol - xcol)
            );
            int64_t iy = (
                start_hity + ((end_hity - start_hity) *
                (z - xcol)) / imax(1, endcol - xcol)
            );

            // Calculate above wall slice height:
            int32_t top, bottom;
            int64_t topworldz, bottomworldz;
            if (unlikely(!roomcam_WallSliceHeight(
                    cam, wall_floor_z, wall_height,
                    h, ix, iy,
                    z, &topworldz, &bottomworldz,
                    &top, &bottom
                    ))) {
                // Fully off-screen.
                z++;
                continue;
            }

            // Calculate the color for this slice:
            int cr = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                r->sector_light_r));
            int cg = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                r->sector_light_g));
            int cb = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                r->sector_light_b));

            // Draw slice:
            assert(top >= 0 && bottom >= top && bottom <= h - 1);
            assert(start_wallno != originwall);
            if (unlikely(!roomcam_DrawWallSlice(
                    rendertarget, r, start_wallno, 1,
                    texinfo, ix, iy,
                    top, bottom, topworldz, bottomworldz,
                    canvasx + z, canvasy, h,
                    cr, cg, cb
                    )))
                return 0;
            stats->base_geometry_slices_rendered++;
            z++;
        }
        xcol = z;
    }
    assert(xcol == endcol + 1);
    return 1;
}

int roomcam_RenderRoom(
        roomcam *cam, room *r, int x, int y, int w, int h,
        int xoffset, int max_xoffset, int nestdepth,
        int ignorewall
        ) {
    if (nestdepth > RENDER_MAX_PORTAL_DEPTH)
        return -1;

    // Collect some important info first:
    renderstatistics *stats = &cam->cache->stats;
    stats->base_geometry_rooms_recursed++;
    #if defined(DEBUG_3DRENDERER)
    fprintf(stderr,
        "rfsc/roomcam.c: roomcam_RenderRoom cam_id=%" PRId64
        ", RECURSE into room_id=%" PRId64 " "
        "(floor_z=%" PRId64 ",height=%" PRId64 ","
        "ignorewall=%d) -> "
        "checking range %d<->%d\n",
        cam->obj->id, r->id, r->floor_z, r->height, ignorewall, xoffset,
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

    // Drawing loop over all screen columns:
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

        // Get hit info at the start of wall segment:
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
        if (!intersect || wallno < 0) {
            // Special super noisy debug of weird collision events:
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
                    "polygon x,y",  // Intentionally no EOL!
                    cam->obj->id, r->id, col, w,
                    cam->obj->x, cam->obj->y,
                    cam->obj->x + cam->cache->rotatedplanevecs_x[col] *
                        VIEWPLANE_RAY_LENGTH_DIVIDER,
                    cam->obj->y + cam->cache->rotatedplanevecs_y[col] *
                        VIEWPLANE_RAY_LENGTH_DIVIDER);
                int i = 0;
                while (i < r->corners) {  // Output polygon too.
                    fprintf(stderr,
                        " %" PRId64 ",%" PRId64,
                        r->corner_x[i], r->corner_y[i]);
                    i++;
                }
                fprintf(stderr, "\n");
            }
            #endif
            // We got nothing useful to draw with, therefore skip.
            col++;
            continue;
        }

        // Find out where wall segment ends:
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
        int _tempendcol = endcol;
        while (1) {
            int endintersect = math_polyintersect2di_ex(
                cam->obj->x, cam->obj->y,
                cam->obj->x + cam->cache->rotatedplanevecs_x[
                    _tempendcol] *
                    VIEWPLANE_RAY_LENGTH_DIVIDER,
                cam->obj->y + cam->cache->rotatedplanevecs_y[
                    _tempendcol] *
                    VIEWPLANE_RAY_LENGTH_DIVIDER,
                r->corners, r->corner_x, r->corner_y, ignorewall, 1,
                &endwallno, &endhit_x, &endhit_y
            );
            stats->base_geometry_rays_cast++;
            assert(!endintersect || ignorewall < 0 ||
                   endwallno != ignorewall);
            if (!endintersect || endwallno < 0) {
                // Oops, probably a precision error.
                // Cheat a little, and see if we can find the end:
                if (_tempendcol <= col ||
                        _tempendcol < col - 4) {
                    endwallno = -1;
                    break;
                }
                _tempendcol--;
                continue;
            }
            break;
        }
        if (endwallno < 0) {
            // Weird, that shouldn't happen. Limit the advancement,
            // and carry on:
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
            if (draw_above || draw_below) {  // Wall is visible!
                roomtexinfo *abovetexinfo = (
                    r->wall[wallno].has_aboveportal_tex ?
                    &r->wall[wallno].aboveportal_tex :
                    &r->wall[wallno].wall_tex
                );
                roomtexinfo *belowtexinfo = (
                    &r->wall[wallno].wall_tex
                );
                drawgeom geom = {0};
                geom.type = DRAWGEOM_ROOM;
                geom.r = r;
                if (draw_above) {  // Above portal drawing!
                    roomcam_DrawWall(
                        cam, &geom, x, y, col, endcol,
                        ignorewall, abovetexinfo,
                        target->floor_z + target->height,
                        (r->height + r->floor_z) -
                        (target->floor_z + target->height)
                    );
                }
                if (draw_below) {  // Below portal drawing!
                    roomcam_DrawWall(
                        cam, &geom, x, y, col, endcol,
                        ignorewall, belowtexinfo,
                        r->floor_z,
                        (target->floor_z - r->floor_z)
                    );
                }
            }
        } else {
            // Draw regular full height wall:
            roomtexinfo *texinfo = (
                &r->wall[wallno].wall_tex
            );
            drawgeom geom = {0};
            geom.type = DRAWGEOM_ROOM;
            geom.r = r;
            roomcam_DrawWall(
                cam, &geom, x, y, col, endcol,
                ignorewall, texinfo,
                r->floor_z, r->height
            );

            // Draw debug indicators for drawn walls:
            #if defined(DEBUG_3DRENDERER)
            int j = col;
            while (j <= endcol) {
                 graphics_DrawRectangle(
                    0, fmax(0, 1.0 - fmod(((double)r->id) * 0.2,
                    1.0)), 0, 1,
                    j, y + 65 + fmod(((double)r->id) * 0.05,
                    1.0) * 50, 1, 5
                );
                j++;
            }
            #endif
        }

        // Draw floor/ceiling:
        drawgeom geom = {0};
        geom.type = DRAWGEOM_ROOM;
        geom.r = r;
        roomcam_DrawFloorCeiling(
            cam, &geom, x, y, col, endcol
        );

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
    room_ClearRenderCache();
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


