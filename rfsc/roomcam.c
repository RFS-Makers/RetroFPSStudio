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
#include "roomcamcache.h"
#include "roomcamcull.h"
#include "roomcamrenderstatistics.h"
#include "roomdefs.h"
#include "roomlayer.h"
#include "roomobject.h"
#include "roomobject_block.h"
#include "roomrendercache.h"
#include "rfssurf.h"


renderstatistics *roomcam_GetStats(roomcam *c) {
    if (c && c->cache)
        return &c->cache->stats;
    return NULL;
}

int _roomcam_ViewplaneXYToXYOnPlane_NoRecalc(
        roomcam *cam,
        int viewx, int viewy,
        int64_t plane_z,
        int64_t *px, int64_t *py
        ) {
    if (plane_z == cam->obj->z)
        return 0;
    const int w = cam->cache->cachedw;
    const int h = cam->cache->cachedh;
    assert(viewx >= 0 && viewx < w &&
        viewy >= 0 && viewy < h);

    int64_t isectvec_x = cam->cache->
        rotatedplanevecs_x[viewx];
    int64_t isectvec_y = cam->cache->
        rotatedplanevecs_y[viewx];
    int64_t isectvec_z = cam->cache->
        vertivecs_z[viewy];
    if (isectvec_z == 0) return 0;
    if ((isectvec_z < 0 && plane_z > cam->obj->z) ||
            (isectvec_z > 0 && plane_z < cam->obj->z))
        return 0;

    int64_t scalar = ((int64_t)1000 *
        (plane_z - cam->obj->z)) / isectvec_z;
    isectvec_x = (isectvec_x * scalar) / (int64_t)1000;
    isectvec_y = (isectvec_y * scalar) / (int64_t)1000;
    isectvec_x += cam->obj->x;
    isectvec_y += cam->obj->y;
    *px = isectvec_x;
    *py = isectvec_y;
    return 1;
}

int32_t _roomcam_XYZToViewplaneY_NoRecalc(
        roomcam *cam,
        int64_t px, int64_t py, int64_t pz,
        int coordsextrascaler
        ) {
    const int w = imax(1, cam->cache->cachedw);
    const int h = imax(1, cam->cache->cachedh);

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

    if (unlikely(cam->cache->vertivecs_z[h - 1] ==
            cam->cache->vertivecs_z[0])) {
        // Viewplane has no height (e.g. output window 0,0 sized).
        if (pz > cam->cache->vertivecs_z[0])
            return -(h * 10);
        else
            return h + h * 10;
    }
    int res = ((pz - cam->cache->vertivecs_z[0] *
        coordsextrascaler * (int64_t)10LL) * h) /
        ((cam->cache->vertivecs_z[h - 1] -
        cam->cache->vertivecs_z[0]) * coordsextrascaler *
        (int64_t)10LL);
    return res;
}


int32_t _roomcam_XYToViewplaneX_NoRecalc(
        roomcam *cam, int64_t px, int64_t py,
        uint8_t *isbehindcam
        ) {
    const int w = cam->cache->cachedw;

    // Rotate/move vec into camera-local space:
    px -= cam->obj->x;
    py -= cam->obj->y;
    math_rotate2di(&px, &py, -cam->obj->angle);

    // Special case of behind camera:
    if (px <= 0) {
        if (isbehindcam) *isbehindcam = 1;
        if (py < 0)
            return -1;
        if (py > 0)
            return w;
        return 0;
    }
    if (isbehindcam) *isbehindcam = 0;

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
        int xcol, int wallno_if_known, int *result
        ) {
    if (!roomrendercache_SetCullInfo(
            rcache, r, cam, cam->cache->cachedw,
            cam->cache->cachedh))
        return 0;
    int max_cols_ahead = (
        (cam->cache->cachedw / WALL_BATCH_DIVIDER) > 0 ?
        (cam->cache->cachedw / WALL_BATCH_DIVIDER) : 1
    );
    if (wallno_if_known >= 0) {
        if (rcache->cullinfo.corners_to_screenxcol[
                wallno_if_known] > xcol) {
            *result = imin(cam->cache->cachedw - 1 - xcol,
                imin(max_cols_ahead,
                rcache->cullinfo.
                    corners_to_screenxcol[
                    wallno_if_known] - xcol));
        } else {
            *result = (xcol < cam->cache->cachedw ? 1 : 0);
        }
        return 1;
    }
    int k = 0;
    while (k < r->corners) {
        if (rcache->cullinfo.
                corners_to_screenxcol[k] >= xcol) {
            int corner_ahead = (
                rcache->cullinfo.
                corners_to_screenxcol[k] - xcol
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


int roomcam_DynlightFalloff(
        int64_t dist, int64_t light_range
        ) {
    assert(dist >= 0 && light_range >= 0);
    if (dist >= light_range)
        return 0;
    int64_t linear = 1000 * (light_range - dist) / light_range;
    int64_t exponential = 1000 * fmin(1.0,
        pow(((int64_t)(light_range - dist + 1)) /
            (double)(light_range + 1), 2.0)
    );
    assert(linear <= 1000 && exponential <= 1000);
    int mixtolinear = imax(300, (linear - 500) * 2);
    if (dist < light_range / 10)
        linear = imax(linear,
            (light_range / 10 - dist) * 1000 / imax(1,
            light_range / 10));
    int antimixtolinear = 1000 - mixtolinear;
    int result = (
        (linear * mixtolinear +
        exponential * antimixtolinear) / 1000
    );
    assert(result >= 0 && result <= 1000);
    return result;
}

int roomcam_DynlightFactorFloorCeilOrPoint(
        int64_t light_z, int64_t light_range,
        int64_t light_dist,
        int64_t geo_z, int facing_up, int facing_down
        ) {
    if (facing_up && geo_z >= light_z)
        return 0;
    if (facing_down && geo_z <= light_z)
        return 0;
    return roomcam_DynlightFalloff(
        light_dist, light_range
    );
}

int roomcam_DynlightFactorWall(
        int64_t light_x, int64_t light_y, int64_t light_range,
        int64_t light_dist,
        int64_t wx1, int64_t wy1, int64_t wx2, int64_t wy2,
        int64_t wallnormalx, int64_t wallnormaly
        ) {
    int64_t wallcenterx = (wx1 + wx2) / 2;
    int64_t wallcentery = (wy1 + wy2) / 2;
    int32_t anglenormal = math_angle2di(
        wallnormalx, wallnormaly
    );
    int32_t anglelight = math_angle2di(
        light_x - wallcenterx, light_y - wallcentery
    );
    if (abs(math_fixanglei(anglenormal - anglelight)) >
            ANGLE_SCALAR * 90) {
        return 0;
    }
    return roomcam_DynlightFalloff(
        light_dist, light_range
    );
}

int roomcam_DynamicLightAtXY(
        roomrendercache *rcache, room *r,
        int64_t x, int64_t y, int64_t z, int isatwallno,
        int isfacingup, int isfacingdown, int scaletorange,
        int32_t *cr, int32_t *cg, int32_t *cb
        ) {
    assert(isatwallno < 0 || (!isfacingup && !isfacingdown));
    assert(isatwallno < r->corners);
    if (!roomrendercache_SetDynLights(
            rcache, r
            )) {
        return 0;
    }
    int64_t result_r = 0;
    int64_t result_g = 0;
    int64_t result_b = 0;
    int64_t wallnormal_x = 0;
    int64_t wallnormal_y = 0;
    int wallid = isatwallno;
    int wallidnext = (isatwallno >= 0 ? isatwallno + 1 : -1);
    if (wallidnext >= r->corners) wallidnext = 0;
    if (isatwallno >= 0) {
        // We're sampling for light at a wall.
        wallnormal_x = r->wall[wallid].normal_x;
        wallnormal_y = r->wall[wallid].normal_y;
    } else {
        // We're sampling somewhere inside the room.
    }
    // Just go through entire light list:
    int64_t mix_r = 0;
    int64_t mix_g = 0;
    int64_t mix_b = 0;
    int i = 0;
    while (i < rcache->light_count) {
        cachedlightinfo *linfo = &(
            rcache->light[i]
        );
        const int64_t ldist = math_veclen2di(
            linfo->x - x, linfo->y - y
        );
        int64_t strength_scalar = (isatwallno < 0 ?
            roomcam_DynlightFactorFloorCeilOrPoint(
                linfo->z, linfo->range,
                ldist, z, isfacingup, isfacingdown
            ) : roomcam_DynlightFactorWall(
                linfo->x, linfo->y, linfo->range,
                ldist,
                r->corner_x[wallid], r->corner_y[wallid],
                r->corner_x[wallidnext], r->corner_y[wallidnext],
                r->wall[wallid].normal_x,
                r->wall[wallid].normal_y
            ));
        assert(linfo->r >= 0 && linfo->r <= LIGHT_COLOR_SCALAR);
        assert(linfo->g >= 0 && linfo->g <= LIGHT_COLOR_SCALAR);
        assert(linfo->b >= 0 && linfo->b <= LIGHT_COLOR_SCALAR);
        mix_r += (strength_scalar * linfo->r /
            LIGHT_COLOR_SCALAR);
        mix_g += (strength_scalar * linfo->g /
            LIGHT_COLOR_SCALAR);
        mix_b += (strength_scalar * linfo->b /
            LIGHT_COLOR_SCALAR);
        i++;
    }
    result_r = imax(0,
        imin(scaletorange, (mix_r * scaletorange) / 1000));
    result_g = imax(0,
        imin(scaletorange, (mix_g * scaletorange) / 1000));
    result_b = imax(0,
        imin(scaletorange, (mix_b * scaletorange) / 1000));
    assert(result_r >= 0 && result_r <= scaletorange);
    assert(result_g >= 0 && result_g <= scaletorange);
    assert(result_b >= 0 && result_b <= scaletorange);
    *cr = result_r;
    *cg = result_g;
    *cb = result_b;
    return 1;
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
        int64_t reference_x = (px1 + (int64_t)texinfo->
                tex_gamestate_scrollx + (int64_t)texinfo->
                tex_shiftx);
        if (reference_x > 0) {
            *tx1 = (reference_x % repeat_units_x) *
                TEX_COORD_SCALER / repeat_units_x;
        } else {
            *tx1 = (repeat_units_x - 1 -
                (((-reference_x) - 1) % repeat_units_x)) *
                TEX_COORD_SCALER/ repeat_units_x;
        }
    }
    {
        int64_t reference_y = (py1 + (int64_t)texinfo->
                tex_gamestate_scrolly + (int64_t)texinfo->
                tex_shifty);
        if (reference_y > 0) {
            *ty1 = (reference_y % repeat_units_y) *
                TEX_COORD_SCALER / repeat_units_y;
        } else {
            *ty1 = (repeat_units_y - 1 -
                (((-reference_y) - 1) % repeat_units_y)) *
                TEX_COORD_SCALER / repeat_units_y;
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
        rfssurf *rendertarget, roomcam *cam, room *r,
        drawgeom *geom, int isfacingup, int xoffset,
        int64_t top_world_x, int64_t top_world_y,
        ATTR_UNUSED int64_t top_world_z,
        int64_t bottom_world_x, int64_t bottom_world_y,
        ATTR_UNUSED int64_t bottom_world_z,
        roomtexinfo *texinfo,
        int screentop, int screenbottom,
        int x, int y, int h,
        int cr, int cg, int cb,
        int cr2, int cg2, int cb2
        ) {
    // Some consistency checks:
    if (!rendertarget)
        return 0;
    assert(screenbottom <= h);
    if (screentop <= 0 && screenbottom <= 0)
        return 0;
    assert(screentop >= -1);
    if (screentop == -1)
        screentop++;
    if (screenbottom == h)
        screenbottom--;
    if (screentop > screenbottom)
        return 1;

    // Get a few things we need:
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
    int64_t plane_z = 0;
    if (geom->type == DRAWGEOM_ROOM) {
        plane_z = geom->r->floor_z;
        if (!isfacingup) plane_z += r->height;
    } else if (geom->type == DRAWGEOM_BLOCK) {
        assert(0);
    } else {
        return 0;
    }

    /*if (x == 100) {
        printf("world top x,y %" PRId64 ",%" PRId64 " "
            "world bottom x,y %" PRId64 ",%" PRId64 "\n",
            bottom_world_x, bottom_world_y,
            top_world_x, top_world_y);
    }*/
    const int max_perspective_cheat_columns = (
        (h / FLOORCEIL_BATCH_DIVIDER) > 0 ?
        (h / FLOORCEIL_BATCH_DIVIDER) : 1
    );
    assert(screentop >= 0 && screentop <= h);
    assert(screenbottom >= 0 && screenbottom <= h);
    assert(TEX_COORD_SCALER > 0 &&
        !math_isnpot(TEX_COORD_SCALER));
    const int32_t texcoord_modulo_mask = (
        math_one_bits_from_right(
            math_count_bits_until_zeros(TEX_COORD_SCALER) - 1
        )
    );
    int row = screentop;
    int64_t past_world_x = top_world_x;
    int64_t past_world_y = top_world_y;
    while (likely(row <= screenbottom)) {
        int endrow = row + max_perspective_cheat_columns;
        if (endrow > screenbottom) endrow = screenbottom;
        assert(endrow >= row);
        int64_t endscalar = ((4096 * (endrow - screentop)) /
            (screenbottom - screentop + 1));
        assert(endscalar >= 0 && endscalar <= 4096);
        int64_t target_world_x, target_world_y;
        if (unlikely(endrow == screenbottom)) {
            target_world_x = bottom_world_x;
            target_world_y = bottom_world_y;
        } else {
            if (!_roomcam_ViewplaneXYToXYOnPlane_NoRecalc(
                    cam, xoffset, endrow, plane_z,
                    &target_world_x, &target_world_y
                    ))
                return 1;
        }
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
        const int startrow = row;
        const int64_t tx1totx2diff = (bottomtx - toptx);
        const int64_t ty1toty2diff = (bottomty - topty);
        const int cr1to2diff = (cr2 - cr);
        const int cg1to2diff = (cg2 - cg);
        const int cb1to2diff = (cb2 - cb);
        const int slicepixellen = (endrow - row) + 1;
        int tgoffset = (
            (x + xoffset + (y + row) * targetw) *
            rendertargetcopylen
        );
        const int origtgoffset = tgoffset;
        const int fullslicelen = (screenbottom - screentop + 1);
        assert(fullslicelen >= 1);
        const int tgoffsetplus = targetw * rendertargetcopylen;
        uint8_t *writepointer = (
            tgpixels + (x + xoffset +
            (y + row) * targetw) * rendertargetcopylen
        );
        const int writepointerplus = (
            targetw * rendertargetcopylen - 3
        );
        int red = 0;
        int green = 0;
        int blue = 0;
        int updatecolorcounter = 0;
        const int srfw = srf->w;
        const int srfh = srf->h;
        const int colorupdateinterval = imax(32,
            h / 16 / (1 + DUPLICATE_FLOOR_PIX));
        const int tghasalpha = rendertarget->hasalpha;
        int rowoffset = 0;
        int maxrowoffset = endrow - row;
        const int islowres = (
            (cam->cache->cachedw + cam->cache->cachedh) <
            (370 + 370)
        );
        #if defined(DUPLICATE_FLOOR_PIX) && \
                    DUPLICATE_FLOOR_PIX >= 1
        const int extradups = (islowres ? 1 : DUPLICATE_FLOOR_PIX);
        #else
        const int extradups = 0;
        #endif
        int tx1totx2diff_mult_rowoffset = 0;
        int ty1toty2diff_mult_rowoffset = 0;
        while (likely(rowoffset <= maxrowoffset)) {
            int64_t tx = ((int32_t)(
                toptx + (tx1totx2diff_mult_rowoffset /
                    slicepixellen))) & texcoord_modulo_mask;
            int64_t ty = ((int32_t)(
                topty + (ty1toty2diff_mult_rowoffset /
                    slicepixellen))) & texcoord_modulo_mask;
            // Here, we're using the non-sideways regular tex.
            const int srcoffset = (
                ((tx * srfw) / TEX_COORD_SCALER) +
                (srfw * ((ty * srfh) / TEX_COORD_SCALER))
            ) * srccopylen;
            assert(srcoffset >= 0 &&
                srcoffset < srfw * srfh * srccopylen);
            assert(tgoffset >= 0 && tgoffset < rendertarget->w *
                rendertarget->h * rendertargetcopylen);

            if (unlikely(updatecolorcounter %
                    colorupdateinterval == 0)) {
                const int row = startrow + rowoffset;
                const int screentoprowoffset = row - screentop;
                red = cr + (cr1to2diff * screentoprowoffset) /
                    fullslicelen;
                green = cg + (cg1to2diff * screentoprowoffset) /
                    fullslicelen;
                blue = cb + (cb1to2diff * screentoprowoffset) /
                    fullslicelen;
                red = imax(0, imin(LIGHT_COLOR_SCALAR * 2, red));
                green = imax(0, imin(LIGHT_COLOR_SCALAR * 2, green));
                blue = imax(0, imin(LIGHT_COLOR_SCALAR * 2, blue));
                assert(red >= cr || red >= cr2);
                assert(red <= cr || red <= cr2);
            }
            *writepointer = math_pixcliptop(
                srcpixels[srcoffset + 0] * red /
                LIGHT_COLOR_SCALAR);
            writepointer++;
            *writepointer = math_pixcliptop(
                srcpixels[srcoffset + 1] * green /
                LIGHT_COLOR_SCALAR);
            writepointer++;
            *writepointer = math_pixcliptop(
                srcpixels[srcoffset + 2] * blue /
                LIGHT_COLOR_SCALAR);
            writepointer++;
            if (tghasalpha) *writepointer = 255;
            updatecolorcounter++;
            rowoffset++;
            tx1totx2diff_mult_rowoffset += tx1totx2diff;
            ty1toty2diff_mult_rowoffset += ty1toty2diff;
            tgoffset += tgoffsetplus;
            writepointer += writepointerplus;
            #if defined(DUPLICATE_FLOOR_PIX) && \
                    DUPLICATE_FLOOR_PIX >= 1
            int extratarget = rowoffset + extradups - 1;
            while (likely(rowoffset <= extratarget &&
                    rowoffset <= maxrowoffset)) {
                assert(tgoffset - tgoffsetplus >= 0);
                if (tghasalpha)
                    tgpixels[tgoffset + 3] = 255;
                memcpy(writepointer,
                    writepointer - writepointerplus - 3,
                    3);
                writepointer += 3;
                if (tghasalpha) *writepointer = 255;
                writepointer += writepointerplus;
                rowoffset++;
                tx1totx2diff_mult_rowoffset += tx1totx2diff;
                ty1toty2diff_mult_rowoffset += ty1toty2diff;
                tgoffset += tgoffsetplus;
            }
            #endif
        }
        row += rowoffset;
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
    const int32_t angle = cam->obj->angle;
    const int32_t vangle = cam->vangle;
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
            if (!_roomcam_ViewplaneXYToXYOnPlane_NoRecalc(
                    cam, xoffset, h - 1, geometry_z,
                    &close_world_x, &close_world_y
                    ))
                return 0;
            close_screen_offset = h - 1;
        } else {
            assert(geometry_z > cam->obj->z);
            if (!_roomcam_ViewplaneXYToXYOnPlane_NoRecalc(
                    cam, xoffset, 0, geometry_z,
                    &close_world_x, &close_world_y
                    ))
                return 0;
            close_screen_offset = 0;
        }
    } else {
        close_screen_offset = imin(h, imax(-1,
            _roomcam_XYZToViewplaneY_NoRecalc(
                cam, close_world_x, close_world_y,
                geometry_z * DRAW_CORNER_COORD_UPSCALE,
                DRAW_CORNER_COORD_UPSCALE
            )));
        close_world_x /= DRAW_CORNER_COORD_UPSCALE;
        close_world_y /= DRAW_CORNER_COORD_UPSCALE;
    }
    // Compute "farther" geometry point on screen:
    int64_t far_world_x = awaytocam_ix;
    int64_t far_world_y = awaytocam_iy;
    int32_t far_screen_offset = imin(h, imax(-1,
        _roomcam_XYZToViewplaneY_NoRecalc(
            cam, far_world_x, far_world_y,
            geometry_z * DRAW_CORNER_COORD_UPSCALE,
            DRAW_CORNER_COORD_UPSCALE
        )));
    far_world_x /= DRAW_CORNER_COORD_UPSCALE;
    far_world_y /= DRAW_CORNER_COORD_UPSCALE;
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
        int xoffset, int clamp,
        int64_t *topworldz, int64_t *bottomworldz,
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
    if (unlikely(clamp && (
            screentopatwall < geometry_floor_z ||
            screentopatwall - screenheightatwall >
            geometry_floor_z + geometry_height)))
        return 0;
    if (unlikely(screenheightatwall == 0))
        return 0;
    int64_t world_to_pixel_scalar = (
        ((int64_t)h * 100000L) / screenheightatwall
    );
    int64_t _topworldz = geometry_floor_z + geometry_height;
    assert(world_to_pixel_scalar >= 0);
    int64_t topoff_screen = (
        ((screentopatwall - _topworldz) *
            world_to_pixel_scalar) / 100000L
    );
    if (topoff_screen < 0 && clamp) {
        topoff_screen = 0;
        assert(screentopatwall < _topworldz);
        _topworldz = (_topworldz - (
            _topworldz - screentopatwall));
    }
    int64_t bottomoff_world = (screentopatwall - (
        geometry_floor_z
    ));
    int64_t bottomoff_screen = (
        (bottomoff_world * world_to_pixel_scalar) / 100000L
    );
    assert(bottomoff_world >= 0);
    if (bottomoff_screen >= h && clamp) {
        bottomoff_screen = h - 1;
        bottomoff_world = screentopatwall - screenheightatwall;
    }
    int64_t _bottomworldz = (_topworldz -
        ((bottomoff_screen - topoff_screen) * 100000L)
        / world_to_pixel_scalar
    );
    if (unlikely(topoff_screen > bottomoff_screen))
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
    if (unlikely(clamp &&
            (topoff_screen >= h || bottomoff_screen < 0)))
        return 0;  // off-screen.
    assert(!clamp || topoff_screen >= 0);
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
            *tx1 = (repeat_units_x - 1 -
                (((-reference_x) - 1) % repeat_units_x)) *
                TEX_COORD_SCALER/ repeat_units_x;
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
            *tx1 = (repeat_units_x - 1 -
                (((-reference_x) - 1) % repeat_units_x)) *
                TEX_COORD_SCALER / repeat_units_x;
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
        _wtop = relevant_wall_segment_top - _wtop;
    } else if (r->wall[wallid].wall_tex.
            tex_stickyside == ROOM_DIR_DOWN) {
        _wtop = relevant_wall_segment_bottom - _wtop;
    }
    int64_t reference_z = (
        (_wtop + (int64_t)texinfo->
            tex_gamestate_scrolly + (int64_t)texinfo->
            tex_shifty)
    );
    if (reference_z > 0) {
        *ty1 = (reference_z % repeat_units_y) *
            TEX_COORD_SCALER / repeat_units_y;
    } else {
        *ty1 = ((repeat_units_y - 1 - (
            ((-reference_z) - 1) % repeat_units_y)) *
            TEX_COORD_SCALER / repeat_units_y
        );
    }
    int64_t reference_zheight = (
        wsliceheight
    );
    if (reference_zheight < 0) reference_zheight = 0;
    *ty2 = *ty1 - (
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
        int32_t *result, uint8_t *resultbehindcam
        ) {
    if (w <= 0 || h <= 0)
        return 0;
    if (!roomcam_CalculateViewPlane(cam, w, h)) {
        return 0;
    }
    *result = _roomcam_XYToViewplaneX_NoRecalc(
        cam, px, py, resultbehindcam
    );
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
    if (cr < 0) cr = 0;
    if (cg < 0) cg = 0;
    if (cb < 0) cb = 0;
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
        assert(ty2 <= ty1);
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
        int sourcey_multiplied_w_copylen = (
            sourcey * srf->w * srccopylen
        );
        int k = screentop;
        assert(screenbottom >= screentop);
        assert(screenbottom <= h);
        assert(screentop >= 0);
        if (screenbottom == h)
            screenbottom--;
        const int slicepixellen = (
            (screenbottom - screentop) > 0 ?
            (screenbottom - screentop) : 1
        );
        const int32_t ty1toty2diff = (ty2 - ty1);
        const int writeoffsetplus = (
            targetw * rendertargetcopylen
        ) - 3;
        const int srfw = srf->w;
        int rowoffset_mult_ty1toty2diff = 0;
        const int maxrowoffset = (screenbottom - k);
        const int pastmaxrowoffset_mult = (
            ((maxrowoffset + 1) * ty1toty2diff));
        assert(TEX_COORD_SCALER > 0 &&
            !math_isnpot(TEX_COORD_SCALER));
        const int32_t texcoord_modulo_mask = (
            math_one_bits_from_right(
                math_count_bits_until_zeros(TEX_COORD_SCALER) - 1
            )
        );
        uint8_t *targetwriteptr = &tgpixels[
            (x + (y + k) * targetw) * rendertargetcopylen
        ];
        const int tghasalpha = rendertarget->hasalpha;
        while (likely(rowoffset_mult_ty1toty2diff !=
                pastmaxrowoffset_mult)) {
            ty = ((int32_t)(
                ty1 + (rowoffset_mult_ty1toty2diff
                    / slicepixellen)
            )) & texcoord_modulo_mask;
            // Remember, we're using the sideways tex.
            const int sourcex = (
                (srfw * (TEX_COORD_SCALER - 1 - ty))
                / TEX_COORD_SCALER
            ) * srccopylen;
            const int srcoffset = (
                sourcex + sourcey_multiplied_w_copylen
            );
            assert(srcoffset >= 0 &&
                srcoffset < srf->w * srf->h * srccopylen);
            *targetwriteptr = math_pixcliptop(
                srcpixels[srcoffset + 0] * cr /
                LIGHT_COLOR_SCALAR);
            targetwriteptr++;
            *targetwriteptr = math_pixcliptop(
                srcpixels[srcoffset + 1] * cg /
                LIGHT_COLOR_SCALAR);
            targetwriteptr++;
            *targetwriteptr = math_pixcliptop(
                srcpixels[srcoffset + 2] * cb /
                LIGHT_COLOR_SCALAR);
            targetwriteptr++;
            if (tghasalpha)
                *targetwriteptr = 255;
            targetwriteptr += writeoffsetplus;
            rowoffset_mult_ty1toty2diff += ty1toty2diff;
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
    int overdraw_for_inside = 0;
    room *r = NULL;
    if (geom->type == DRAWGEOM_ROOM) {
        r = geom->r;
        overdraw_for_inside = 1;
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

    // Render upfacing (floor for room, top for additive):
    int prev_valuesset = 0;
    int64_t prev_topwx, prev_topwy, prev_bottomwx, prev_bottomwy;
    int32_t prev_topoffset, prev_bottomoffset;
    int prev_onscreen;
    int32_t z = xcol;
    while (likely(z <= endcol)) {
        // Check over which segment to interpolate the perspective:
        int max_safe_next_cols = -1;
        if (!GetFloorCeilingSafeInterpolationColumns(
                cam, r, rcache, z, -1, &max_safe_next_cols
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
        int endcolslicefail = 0;
        int onscreen2 = 0;
        while (!roomcam_FloorCeilingSliceHeight(
                cam, geom,
                h, innerendcol, 1, &onscreen2,
                &top2wx, &top2wy, &bottom2wx, &bottom2wy,
                &top2offset, &bottom2offset
                )) {
            if (innerendcol > z + 1) {
                innerendcol--;
            } else {
                endcolslicefail = 1;
                break;
            }
        }
        if (endcolslicefail) {
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

        // Calculate dynamic light colors to interpolate between:
        const int lightsamplevaluerange = 1000;
        int32_t dynlightstarttop_r = 0;
        int32_t dynlightstarttop_g = 0;
        int32_t dynlightstarttop_b = 0;
        roomcam_DynamicLightAtXY(
            rcache, r, top1wx, top1wy, r->floor_z, -1,
            1, 0, lightsamplevaluerange,
            &dynlightstarttop_r, &dynlightstarttop_g,
            &dynlightstarttop_b
        );
        int32_t dynlightendtop_r = 0;
        int32_t dynlightendtop_g = 0;
        int32_t dynlightendtop_b = 0;
        roomcam_DynamicLightAtXY(
            rcache, r, top2wx, top2wy, r->floor_z, -1,
            1, 0, lightsamplevaluerange,
            &dynlightendtop_r, &dynlightendtop_g,
            &dynlightendtop_b
        );
        int32_t dynlightstartbottom_r = 0;
        int32_t dynlightstartbottom_g = 0;
        int32_t dynlightstartbottom_b = 0;
        roomcam_DynamicLightAtXY(
            rcache, r, bottom1wx, bottom1wy, r->floor_z, -1,
            1, 0, lightsamplevaluerange,
            &dynlightstartbottom_r, &dynlightstartbottom_g,
            &dynlightstartbottom_b
        );
        int32_t dynlightendbottom_r = 0;
        int32_t dynlightendbottom_g = 0;
        int32_t dynlightendbottom_b = 0;
        roomcam_DynamicLightAtXY(
            rcache, r, bottom2wx, bottom2wy, r->floor_z, -1,
            1, 0, lightsamplevaluerange,
            &dynlightendbottom_r, &dynlightendbottom_g,
            &dynlightendbottom_b
        );

        // Render surface by interpolating over above info:
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
            if (overdraw_for_inside)
                topoffset = imax(0, topoffset - 2);
            if (bottomoffset >= topoffset) {
                int64_t dyntopr = (
                    dynlightstarttop_r + ((dynlightendtop_r -
                    dynlightstarttop_r) * scalar) / scalarrange
                ) * LIGHT_COLOR_SCALAR / lightsamplevaluerange;
                int64_t dyntopg = (
                    dynlightstarttop_g + ((dynlightendtop_g -
                    dynlightstarttop_g) * scalar) / scalarrange
                ) * LIGHT_COLOR_SCALAR / lightsamplevaluerange;
                int64_t dyntopb = (
                    dynlightstarttop_b + ((dynlightendtop_b -
                    dynlightstarttop_b) * scalar) / scalarrange
                ) * LIGHT_COLOR_SCALAR / lightsamplevaluerange;
                int64_t dynbottomr = (
                    dynlightstartbottom_r + ((dynlightendbottom_r -
                    dynlightstartbottom_r) * scalar) / scalarrange
                ) * LIGHT_COLOR_SCALAR / lightsamplevaluerange;
                int64_t dynbottomg = (
                    dynlightstartbottom_g + ((dynlightendbottom_g -
                    dynlightstartbottom_g) * scalar) / scalarrange
                ) * LIGHT_COLOR_SCALAR / lightsamplevaluerange;
                int64_t dynbottomb = (
                    dynlightstartbottom_b + ((dynlightendbottom_b -
                    dynlightstartbottom_b) * scalar) / scalarrange
                ) * LIGHT_COLOR_SCALAR / lightsamplevaluerange;
                int cr = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                    r->sector_light_r + dyntopr * 2));
                int cg = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                    r->sector_light_g + dyntopg * 2));
                int cb = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                    r->sector_light_b + dyntopb * 2));
                int c2r = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                    r->sector_light_r + dynbottomr * 2));
                int c2g = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                    r->sector_light_g + dynbottomg * 2));
                int c2b = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                    r->sector_light_b + dynbottomb * 2));
                assert(
                    math_veclen2di(
                        topwx - cam->obj->x, topwy - cam->obj->y
                    ) >= math_veclen2di(
                        bottomwx - cam->obj->x, bottomwy - cam->obj->y
                    ) || llabs(bottomoffset == topoffset) <= 2
                );
                roomcam_DrawFloorCeilingSlice(
                    rendertarget, cam, r, geom, 1, z,
                    topwx, topwy, r->floor_z,
                    bottomwx, bottomwy, r->floor_z,
                    texinfo, topoffset, bottomoffset,
                    canvasx, canvasy, h, cr, cg, cb,
                    c2r, c2g, c2b
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

    // Render downfacing (ceiling for rooms, top for additive):
    prev_valuesset = 0;
    z = xcol;
    while (likely(z <= endcol)) {
        // Check over which segment to interpolate the perspective:
        int max_safe_next_cols = -1;
        if (!GetFloorCeilingSafeInterpolationColumns(
                cam, r, rcache, z, -1, &max_safe_next_cols
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

        // Calculate dynamic light colors to interpolate between:
        const int lightsamplevaluerange = 1000;
        int32_t dynlightstarttop_r = 0;
        int32_t dynlightstarttop_g = 0;
        int32_t dynlightstarttop_b = 0;
        int64_t ceil_z = r->floor_z + r->height;
        roomcam_DynamicLightAtXY(
            rcache, r, top1wx, top1wy, ceil_z, -1,
            0, 1, lightsamplevaluerange,
            &dynlightstarttop_r, &dynlightstarttop_g,
            &dynlightstarttop_b
        );
        int32_t dynlightendtop_r = 0;
        int32_t dynlightendtop_g = 0;
        int32_t dynlightendtop_b = 0;
        roomcam_DynamicLightAtXY(
            rcache, r, top2wx, top2wy, ceil_z, -1,
            0, 1, lightsamplevaluerange,
            &dynlightendtop_r, &dynlightendtop_g,
            &dynlightendtop_b
        );
        int32_t dynlightstartbottom_r = 0;
        int32_t dynlightstartbottom_g = 0;
        int32_t dynlightstartbottom_b = 0;
        roomcam_DynamicLightAtXY(
            rcache, r, bottom1wx, bottom1wy, ceil_z, -1,
            0, 1, lightsamplevaluerange,
            &dynlightstartbottom_r, &dynlightstartbottom_g,
            &dynlightstartbottom_b
        );
        int32_t dynlightendbottom_r = 0;
        int32_t dynlightendbottom_g = 0;
        int32_t dynlightendbottom_b = 0;
        roomcam_DynamicLightAtXY(
            rcache, r, bottom2wx, bottom2wy, ceil_z, -1,
            0, 1, lightsamplevaluerange,
            &dynlightendbottom_r, &dynlightendbottom_g,
            &dynlightendbottom_b
        );

        // Render surface by interpolating over above info:
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
            if (overdraw_for_inside)
                bottomoffset = imin(h, bottomoffset + 2);
            if (bottomoffset >= topoffset) {
                int64_t dyntopr = (
                    dynlightstarttop_r + ((dynlightendtop_r -
                    dynlightstarttop_r) * scalar) / scalarrange
                ) * LIGHT_COLOR_SCALAR / lightsamplevaluerange;
                int64_t dyntopg = (
                    dynlightstarttop_g + ((dynlightendtop_g -
                    dynlightstarttop_g) * scalar) / scalarrange
                ) * LIGHT_COLOR_SCALAR / lightsamplevaluerange;
                int64_t dyntopb = (
                    dynlightstarttop_b + ((dynlightendtop_b -
                    dynlightstarttop_b) * scalar) / scalarrange
                ) * LIGHT_COLOR_SCALAR / lightsamplevaluerange;
                int64_t dynbottomr = (
                    dynlightstartbottom_r + ((dynlightendbottom_r -
                    dynlightstartbottom_r) * scalar) / scalarrange
                ) * LIGHT_COLOR_SCALAR / lightsamplevaluerange;
                int64_t dynbottomg = (
                    dynlightstartbottom_g + ((dynlightendbottom_g -
                    dynlightstartbottom_g) * scalar) / scalarrange
                ) * LIGHT_COLOR_SCALAR / lightsamplevaluerange;
                int64_t dynbottomb = (
                    dynlightstartbottom_b + ((dynlightendbottom_b -
                    dynlightstartbottom_b) * scalar) / scalarrange
                ) * LIGHT_COLOR_SCALAR / lightsamplevaluerange;
                int cr = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                    r->sector_light_r + dyntopr * 2));
                int cg = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                    r->sector_light_g + dyntopg * 2));
                int cb = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                    r->sector_light_b + dyntopb * 2));
                int c2r = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                    r->sector_light_r + dynbottomr * 2));
                int c2g = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                    r->sector_light_g + dynbottomg * 2));
                int c2b = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                    r->sector_light_b + dynbottomb * 2));
                /*assert(
                    math_veclen2di(
                        topwx - cam->obj->x, topwy - cam->obj->y
                    ) <= math_veclen2di(
                        bottomwx - cam->obj->x, bottomwy - cam->obj->y
                    ) || llabs(bottomoffset - topoffset) <= 2
                );*/
                roomcam_DrawFloorCeilingSlice(
                    rendertarget, cam, r, geom, 0, z,
                    topwx, topwy, r->floor_z + r->height,
                    bottomwx, bottomwy, r->floor_z + r->height,
                    texinfo, topoffset, bottomoffset,
                    canvasx, canvasy, h, cr, cg, cb,
                    c2r, c2g, c2b
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
    int wallno = -1;
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
    roomrendercache_SetCullInfo(  // require culling data
        rcache, r, cam, cam->cache->cachedw, cam->cache->cachedh
    );
    if (endcol < rcache->cullinfo.corners_minscreenxcol ||
            xcol > rcache->cullinfo.corners_maxscreenxcol)
        return 1;
    if (xcol < rcache->cullinfo.corners_minscreenxcol)
        xcol = rcache->cullinfo.corners_minscreenxcol;
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
                cam, r, rcache, xcol, start_wallno,
                &max_safe_next_segment
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

        // Calculate dynamic light colors to interpolate between:
        const int lightsamplevaluerange = 1000;
        int32_t dynlightstart_r = 0;
        int32_t dynlightstart_g = 0;
        int32_t dynlightstart_b = 0;
        roomcam_DynamicLightAtXY(
            rcache, r, start_hitx, start_hity, 0, start_wallno,
            0, 0, lightsamplevaluerange,
            &dynlightstart_r, &dynlightstart_g,
            &dynlightstart_b
        );
        int32_t dynlightend_r = 0;
        int32_t dynlightend_g = 0;
        int32_t dynlightend_b = 0;
        roomcam_DynamicLightAtXY(
            rcache, r, end_hitx, end_hity, 0, start_wallno,
            0, 0, lightsamplevaluerange,
            &dynlightend_r, &dynlightend_g,
            &dynlightend_b
        );

        // For better interpolation, also get the
        // screen top/bottom for our start and end:
        int interpolatescreensize = 1;
        int32_t starttop, startbottom;
        int64_t _ignore1, _ignore2;
        if (unlikely(!roomcam_WallSliceHeight(
                cam, wall_floor_z, wall_height,
                h, start_hitx, start_hity,
                xcol, 0, &_ignore1, &_ignore2,
                &starttop, &startbottom
                ))) {
            // Fully off-screen, cant interpolate.
            interpolatescreensize = 0;
        }
        int32_t endtop, endbottom;
        if (unlikely(!roomcam_WallSliceHeight(
                cam, wall_floor_z, wall_height,
                h, end_hitx, end_hity,
                imin(xcol + max_render_ahead,
                cam->cache->cachedw - 1), 0,
                &_ignore1, &_ignore2,
                &endtop, &endbottom
                ))) {
            // Fully off-screen, cant interpolate.
            interpolatescreensize = 0;
        }

        // Do actual render:
        int z = xcol;
        while (likely(z < xcol + max_render_ahead)) {
            assert(z <= endcol);
            int64_t ix = (
                start_hitx + ((end_hitx - start_hitx) *
                (z - xcol)) / imax(1, max_render_ahead)
            );
            int64_t iy = (
                start_hity + ((end_hity - start_hity) *
                (z - xcol)) / imax(1, max_render_ahead)
            );
            int64_t dynr = (
                dynlightstart_r + ((dynlightend_r -
                dynlightstart_r) * (z - xcol)) /
                imax(1, max_render_ahead)
            ) * LIGHT_COLOR_SCALAR / lightsamplevaluerange;
            int64_t dyng = (
                dynlightstart_g + ((dynlightend_g -
                dynlightstart_g) * (z - xcol)) /
                imax(1, max_render_ahead)
            ) * LIGHT_COLOR_SCALAR / lightsamplevaluerange;
            int64_t dynb = (
                dynlightstart_b + ((dynlightend_b -
                dynlightstart_b) * (z - xcol)) /
                imax(1, max_render_ahead)
            ) * LIGHT_COLOR_SCALAR / lightsamplevaluerange;

            // Calculate above wall slice height:
            int32_t top, bottom;
            int64_t topworldz, bottomworldz;
            if (unlikely(!roomcam_WallSliceHeight(
                    cam, wall_floor_z, wall_height,
                    h, ix, iy,
                    z, 1, &topworldz, &bottomworldz,
                    &top, &bottom
                    ))) {
                // Fully off-screen.
                z++;
                continue;
            }
            // Override screen top/bottom with better interpolation:
            if (interpolatescreensize) {
                top = imax(0, imin(h - 1,
                    ((endtop - starttop) * (z - xcol)) /
                    imax(1, max_render_ahead) + starttop));
                bottom = imax(0, imin(h - 1,
                    ((endbottom - startbottom) * (z - xcol)) /
                    imax(1, max_render_ahead) + startbottom));
            }

            // Calculate the color for this slice:
            int cr = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                r->sector_light_r + dynr * 2));
            int cg = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                r->sector_light_g + dyng * 2));
            int cb = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                r->sector_light_b + dynb * 2));

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
            #if defined(DUPLICATE_WALL_PIX) && \
                    DUPLICATE_WALL_PIX >= 1
            int extra_z = z + DUPLICATE_WALL_PIX;
            while (likely(z < extra_z &&
                    z < xcol + max_render_ahead)) {
                int row = top + canvasy;
                const int maxrow = (
                    (bottom + canvasy) < (canvasy + h) ?
                    (bottom + canvasy) :
                    (canvasy + h - 1)
                );
                const int copylen = (rendertarget->hasalpha ? 4 : 3);
                const int xbyteoffsetnew = copylen * (canvasx + z);
                const int xbyteoffsetold = copylen * (canvasx + (z - 1));
                int ybyteoffset = (
                    copylen * rendertarget->w * (top + canvasy)
                );
                const int ybyteshift = copylen * rendertarget->w;
                const int ybyteposttarget = (
                    copylen * rendertarget->w * (bottom + 1 + canvasy)
                );
                while (row <= maxrow) {
                    if (rendertarget->hasalpha)
                        rendertarget->pixels[
                            xbyteoffsetnew + ybyteoffset + 3
                        ] = 255;
                    memcpy(&rendertarget->pixels[
                        xbyteoffsetnew + ybyteoffset
                    ], &rendertarget->pixels[
                        xbyteoffsetold + ybyteoffset
                    ], 3);
                    row++;
                    ybyteoffset += ybyteshift;
                }
                stats->base_geometry_slices_rendered++;
                z++;
            }
            #endif
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
    const int batchwidth = (imax(MIN_WALL_BATCH_LEN,
        w / WALL_BATCH_DIVIDER));

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
            cam, r->corner_x[wallno], r->corner_y[wallno],
            NULL
        );  // This assumes rooms are CCW.
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

    /*graphics_DrawRectangle(  // for debug purposes
        1, 0, 0, 1, x, y, w, h
    );*/

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


