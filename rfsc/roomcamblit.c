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
#include "roomcamblit.h"
#include "roomcamcache.h"
#include "roomcamcull.h"
#include "roomcamrenderstatistics.h"
#include "roomdefs.h"
#include "roomlayer.h"
#include "roomobject.h"
#include "roomobject_block.h"
#include "roomrendercache.h"
#include "rfssurf.h"


extern int32_t texcoord_modulo_mask;  // from roomcam.c

int _roomcam_ViewplaneXYToXYOnPlane_NoRecalc(
    roomcam *cam,
    int viewx, int viewy,
    int64_t plane_z,
    int64_t *px, int64_t *py
);  // from roomcam.c


int32_t _roomcam_XYZToViewplaneY_NoRecalc(
    roomcam *cam,
    int64_t px, int64_t py, int64_t pz,
    int coordsextrascaler
);  // from roomcam.c


int32_t _roomcam_XYToViewplaneX_NoRecalc(
    roomcam *cam, int64_t px, int64_t py,
    uint8_t *isbehindcam
);  // from roomam.c


#ifndef NDEBUG
static int _assert_pix256(int v) {
    assert(v >= 0 && v < 256);
    return v;
}
#else
#define _assert_pix256(x) x
#endif


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
    const uint8_t *gammat = cam->cache->gammamap;
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
        const int cr1to2diff = (cr2 - cr);
        const int cg1to2diff = (cg2 - cg);
        const int cb1to2diff = (cb2 - cb);
        const int slicepixellen = (endrow - row) + 1;
        const int fullslicelen = (screenbottom - screentop + 1);
        assert(fullslicelen >= 1);
        uint8_t *writepointer = (
            tgpixels + (x + xoffset +
            (y + row) * targetw) * rendertargetcopylen
        );
        const int writepointerplus = (
            targetw * rendertargetcopylen - 3
        );
        const int writepointerplus_plus3 = (
            writepointerplus + 3);
        int rednonwhite = 0;
        int greennonwhite = 0;
        int bluenonwhite = 0;
        int redwhite = 0;
        int greenwhite = 0;
        int bluewhite = 0;
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
        const int alphapixoffset = (tghasalpha ? 3 : 0);

        int32_t ty1_positive = topty;
        int32_t ty2_positive = bottomty;
        if (ty1_positive < 0) {
            int32_t coordstoadd = (-ty1_positive) /
                TEX_COORD_SCALER + 1;
            ty1_positive += coordstoadd * TEX_COORD_SCALER;
            ty2_positive += coordstoadd * TEX_COORD_SCALER;
        }
        if (ty2_positive < 0) {
            int32_t coordstoadd = (-ty2_positive) /
                TEX_COORD_SCALER + 1;
            ty1_positive += coordstoadd * TEX_COORD_SCALER;
            ty2_positive += coordstoadd * TEX_COORD_SCALER;
        }
        int steps = imax(1, (maxrowoffset - rowoffset) + 1);
        const uint32_t ty1_shift = ty1_positive << 5;
        const uint32_t ty2_shift = ty2_positive << 5;
        const int32_t tystep_shift = (
            (int32_t)ty2_shift - (int32_t)ty1_shift) / steps;
        int32_t tx1_positive = toptx;
        int32_t tx2_positive = bottomtx;
        if (tx1_positive < 0) {
            int32_t coordstoadd = (-tx1_positive) /
                TEX_COORD_SCALER + 1;
            tx1_positive += coordstoadd * TEX_COORD_SCALER;
            tx2_positive += coordstoadd * TEX_COORD_SCALER;
        }
        if (tx2_positive < 0) {
            int32_t coordstoadd = (-tx2_positive) /
                TEX_COORD_SCALER + 1;
            tx1_positive += coordstoadd * TEX_COORD_SCALER;
            tx2_positive += coordstoadd * TEX_COORD_SCALER;
        }
        const uint32_t tx1_shift = tx1_positive << 5;
        const uint32_t tx2_shift = tx2_positive << 5;
        const int32_t txstep_shift = (
            (int32_t)tx2_shift - (int32_t)tx1_shift) / steps;
        uint32_t tx_nomod_shift = tx1_shift - txstep_shift;
        uint32_t ty_nomod_shift = ty1_shift - tystep_shift;
        #if !defined(FIXED_ROOMTEX_SIZE) || \
                FIXED_ROOMTEX_SIZE <= 2 || \
                !defined(FIXED_ROOMTEX_SIZE_2) || \
                FIXED_ROOMTEX_SIZE_2 <= 2
        #error "need fixed tex sizes"
        #endif
        if (srf->w == FIXED_ROOMTEX_SIZE) {
            // Divisor to get from TEX_COORD_SCALER to tex size:
            assert(TEX_COORD_SCALER >= FIXED_ROOMTEX_SIZE);
            assert(!math_isnpot(FIXED_ROOMTEX_SIZE));
            const int coorddiv = (TEX_COORD_SCALER / FIXED_ROOMTEX_SIZE);
            assert(coorddiv == 1 || coorddiv == 2 ||
                   !math_isnpot(coorddiv));
            assert(TEX_COORD_SCALER / coorddiv == FIXED_ROOMTEX_SIZE);

            while (likely(rowoffset <= maxrowoffset)) {
                // Update light/color levels for next batch:
                #ifndef NDEBUG
                assert(updatecolorcounter %
                       colorupdateinterval == 0);
                #endif
                const int row = startrow + rowoffset;
                const int screentoprowoffset = row - screentop;
                int fullred = cr + (cr1to2diff *
                    screentoprowoffset) / fullslicelen;
                int fullgreen = cg + (cg1to2diff *
                    screentoprowoffset) / fullslicelen;
                int fullblue = cb + (cb1to2diff *
                    screentoprowoffset) / fullslicelen;
                rednonwhite = imin(fullred, LIGHT_COLOR_SCALAR);
                greennonwhite = imin(fullgreen, LIGHT_COLOR_SCALAR);
                bluenonwhite = imin(fullblue, LIGHT_COLOR_SCALAR);
                int redwhite = imin(imax(0, fullred -
                    LIGHT_COLOR_SCALAR), LIGHT_COLOR_SCALAR);
                rednonwhite = imin(rednonwhite,
                    LIGHT_COLOR_SCALAR - redwhite);
                int greenwhite = imin(imax(0, fullgreen -
                    LIGHT_COLOR_SCALAR), LIGHT_COLOR_SCALAR);
                greennonwhite = imin(greennonwhite,
                    LIGHT_COLOR_SCALAR - greenwhite);
                int bluewhite = imin(imax(0, fullblue -
                    LIGHT_COLOR_SCALAR), LIGHT_COLOR_SCALAR);
                bluenonwhite = imin(bluenonwhite,
                    LIGHT_COLOR_SCALAR - bluewhite);
                assert(fullred >= cr || fullred >= cr2);

                // See how far we can blit with no light update:
                const int next_to = imin(maxrowoffset,
                    rowoffset + (colorupdateinterval - 1));
                // Blit until we need next light update:
                #if defined(DUPLICATE_FLOOR_PIX) && \
                            DUPLICATE_FLOOR_PIX >= 1
                const int loopiter = 1 + extradups;
                #else
                const int loopiter = 1;
                #endif
                #pragma GCC ivdep
                for (; rowoffset <= next_to; rowoffset += loopiter) {
                    tx_nomod_shift += txstep_shift;
                    int64_t tx = (tx_nomod_shift >> 5) & texcoord_modulo_mask;
                    ty_nomod_shift += tystep_shift;
                    int64_t ty = (ty_nomod_shift >> 5) & texcoord_modulo_mask;
                    // Here, we're using the non-sideways regular tex.
                    const int srcoffset = (
                        (tx / coorddiv) * srccopylen
                    ) + (
                        (ty / coorddiv) * srccopylen * FIXED_ROOMTEX_SIZE
                    );
                    const uint8_t *readptr = &srcpixels[srcoffset];
                    assert(srcoffset >= 0 &&
                        srcoffset < srfw * srfh * srccopylen);
                    *(writepointer + alphapixoffset) = 255;
                    *writepointer = gammat[_assert_pix256(
                        (*readptr) * rednonwhite /
                        LIGHT_COLOR_SCALAR +
                        255 * redwhite /
                        LIGHT_COLOR_SCALAR)];
                    readptr++;
                    writepointer++;
                    *writepointer = gammat[_assert_pix256(
                        (*readptr) * greennonwhite /
                        LIGHT_COLOR_SCALAR +
                        255 * greenwhite /
                        LIGHT_COLOR_SCALAR)];
                    readptr++;
                    writepointer++;
                    *writepointer = gammat[_assert_pix256(
                        (*readptr) * bluenonwhite /
                        LIGHT_COLOR_SCALAR +
                        255 * bluewhite /
                        LIGHT_COLOR_SCALAR)];
                    readptr++;
                    writepointer++;
                    #ifndef NDEBUG
                    updatecolorcounter++;
                    #endif
                    writepointer += writepointerplus;
                    #if defined(DUPLICATE_FLOOR_PIX) && \
                            DUPLICATE_FLOOR_PIX >= 1
                    int innerrowoffset = rowoffset + 1;
                    const int extratarget_past = imin(
                        innerrowoffset + extradups - 1, next_to) + 1;
                    while (likely(innerrowoffset != extratarget_past)) {
                        *(writepointer + alphapixoffset) = 255;
                        memcpy(writepointer,
                            writepointer - writepointerplus - 3,
                            3);
                        writepointer += writepointerplus_plus3;
                        #ifndef NDEBUG
                        updatecolorcounter++;
                        #endif
                        innerrowoffset++;
                        tx_nomod_shift += txstep_shift;
                        ty_nomod_shift += tystep_shift;
                    }
                    #endif
                }
                if (rowoffset > next_to) rowoffset = next_to + 1;
            }
        } else {
            // Divisor to get from TEX_COORD_SCALER_2 to tex size:
            assert(srf->w == FIXED_ROOMTEX_SIZE_2);
            assert(TEX_COORD_SCALER >= FIXED_ROOMTEX_SIZE_2);
            assert(!math_isnpot(FIXED_ROOMTEX_SIZE_2));
            const int coorddiv = (TEX_COORD_SCALER / FIXED_ROOMTEX_SIZE_2);
            assert(coorddiv == 1 || coorddiv == 2 ||
                   !math_isnpot(coorddiv));
            assert(TEX_COORD_SCALER / coorddiv == FIXED_ROOMTEX_SIZE_2);

            while (likely(rowoffset <= maxrowoffset)) {
                // Update light/color levels for next batch:
                #ifndef NDEBUG
                assert(updatecolorcounter %
                       colorupdateinterval == 0);
                #endif
                const int row = startrow + rowoffset;
                const int screentoprowoffset = row - screentop;
                int fullred = cr + (cr1to2diff *
                    screentoprowoffset) / fullslicelen;
                int fullgreen = cg + (cg1to2diff *
                    screentoprowoffset) / fullslicelen;
                int fullblue = cb + (cb1to2diff *
                    screentoprowoffset) / fullslicelen;
                rednonwhite = imin(fullred, LIGHT_COLOR_SCALAR);
                greennonwhite = imin(fullgreen, LIGHT_COLOR_SCALAR);
                bluenonwhite = imin(fullblue, LIGHT_COLOR_SCALAR);
                int redwhite = imin(imax(0, fullred -
                    LIGHT_COLOR_SCALAR), LIGHT_COLOR_SCALAR);
                rednonwhite = imin(rednonwhite,
                    LIGHT_COLOR_SCALAR - redwhite);
                int greenwhite = imin(imax(0, fullgreen -
                    LIGHT_COLOR_SCALAR), LIGHT_COLOR_SCALAR);
                greennonwhite = imin(greennonwhite,
                    LIGHT_COLOR_SCALAR - greenwhite);
                int bluewhite = imin(imax(0, fullblue -
                    LIGHT_COLOR_SCALAR), LIGHT_COLOR_SCALAR);
                bluenonwhite = imin(bluenonwhite,
                    LIGHT_COLOR_SCALAR - bluewhite);
                assert(fullred >= cr || fullred >= cr2);

                // See how far we can blit with no light update:
                const int next_to = imin(maxrowoffset,
                    rowoffset + (colorupdateinterval - 1));
                // Blit until we need next light update:
                #if defined(DUPLICATE_FLOOR_PIX) && \
                            DUPLICATE_FLOOR_PIX >= 1
                const int loopiter = 1 + extradups;
                #else
                const int loopiter = 1;
                #endif
                #pragma GCC ivdep
                for (; rowoffset <= next_to; rowoffset += loopiter) {
                    tx_nomod_shift += txstep_shift;
                    int64_t tx = (tx_nomod_shift >> 5) & texcoord_modulo_mask;
                    ty_nomod_shift += tystep_shift;
                    int64_t ty = (ty_nomod_shift >> 5) & texcoord_modulo_mask;
                    // Here, we're using the non-sideways regular tex.
                    const int srcoffset = (
                        (tx / coorddiv) * srccopylen
                    ) + (
                        (ty / coorddiv) * srccopylen * FIXED_ROOMTEX_SIZE_2
                    );
                    const uint8_t *readptr = &srcpixels[srcoffset];
                    assert(srcoffset >= 0 &&
                        srcoffset < srfw * srfh * srccopylen);
                    *(writepointer + alphapixoffset) = 255;
                    *writepointer = gammat[_assert_pix256(
                        (*readptr) * rednonwhite /
                        LIGHT_COLOR_SCALAR +
                        255 * redwhite /
                        LIGHT_COLOR_SCALAR)];
                    readptr++;
                    writepointer++;
                    *writepointer = gammat[_assert_pix256(
                        (*readptr) * greennonwhite /
                        LIGHT_COLOR_SCALAR +
                        255 * greenwhite /
                        LIGHT_COLOR_SCALAR)];
                    readptr++;
                    writepointer++;
                    *writepointer = gammat[_assert_pix256(
                        (*readptr) * bluenonwhite /
                        LIGHT_COLOR_SCALAR +
                        255 * bluewhite /
                        LIGHT_COLOR_SCALAR)];
                    readptr++;
                    writepointer++;
                    #ifndef NDEBUG
                    updatecolorcounter++;
                    #endif
                    writepointer += writepointerplus;
                    #if defined(DUPLICATE_FLOOR_PIX) && \
                            DUPLICATE_FLOOR_PIX >= 1
                    int innerrowoffset = rowoffset + 1;
                    const int extratarget_past = imin(
                        innerrowoffset + extradups - 1, next_to) + 1;
                    while (likely(innerrowoffset != extratarget_past)) {
                        *(writepointer + alphapixoffset) = 255;
                        memcpy(writepointer,
                            writepointer - writepointerplus - 3,
                            3);
                        writepointer += writepointerplus_plus3;
                        #ifndef NDEBUG
                        updatecolorcounter++;
                        #endif
                        innerrowoffset++;
                        tx_nomod_shift += txstep_shift;
                        ty_nomod_shift += tystep_shift;
                    }
                    #endif
                }
                if (rowoffset > next_to) rowoffset = next_to + 1;
            }
        }
        row += rowoffset;
        assert(row == endrow + 1);
        past_world_x = target_world_x;
        past_world_y = target_world_y;
    }
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
    int32_t far_screen_offset = (
        _roomcam_XYZToViewplaneY_NoRecalc(
            cam, far_world_x, far_world_y,
            geometry_z * DRAW_CORNER_COORD_UPSCALE,
            DRAW_CORNER_COORD_UPSCALE
        ));
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
    const int64_t planecoldistscalar = (
        (dist * 100000LL) / planecoldist
    );
    const int64_t screenheightatwall = (
        (cam->cache->planeh * planecoldistscalar) / 100000LL
    );
    const int64_t zshiftatwall = (
        (cam->cache->planezshift * planecoldistscalar) /
        100000LL
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
    ) + zshiftatwall;
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
    int64_t _topworldz = (
        geometry_floor_z + geometry_height
    );
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
    if (unlikely(bottomoff_world < 0))
        return 0;  // means floor z is above screentop.
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


void roomcam_WallCubeMappingX(
        drawgeom *geom, int wallno,
        int64_t wx, int64_t wy,
        int64_t wx2, int64_t wy2,
        geomtex *texinfo,
        int64_t *tx1, int64_t *tx2
        ) {
    assert(geom->type == DRAWGEOM_ROOM ||
        geom->type == DRAWGEOM_BLOCK);
    int64_t wallstart_x, wallstart_y;
    int64_t wallend_x, wallend_y;
    int64_t wallnormal_x, wallnormal_y;
    if (geom->type == DRAWGEOM_ROOM) {
        int wallnext = wallno + 1;
        if (wallnext >= geom->r->corners)
            wallnext = 0;
        wallstart_x = geom->r->corner_x[wallno];
        wallstart_y = geom->r->corner_y[wallno];
        wallend_x = geom->r->corner_x[wallnext];
        wallend_y = geom->r->corner_y[wallnext];
        wallnormal_x = geom->r->wall[wallno].normal_x;
        wallnormal_y = geom->r->wall[wallno].normal_y;
    } else {
        int wallnext = wallno + 1;
        if (wallnext >= geom->bl->corners)
            wallnext = 0;
        wallstart_x = geom->bl->corner_x[wallno];
        wallstart_y = geom->bl->corner_y[wallno];
        wallend_x = geom->bl->corner_x[wallnext];
        wallend_y = geom->bl->corner_y[wallnext];
        wallnormal_x = geom->bl->normal_x[wallno];
        wallnormal_y = geom->bl->normal_y[wallno];
    }
    assert(texinfo->type == GEOMTEX_ROOM ||
        texinfo->type == GEOMTEX_BLOCK);
    int64_t tex_scalex = (
        texinfo->type == GEOMTEX_ROOM ?
        texinfo->roomtex->tex_scaleintx :
        texinfo->blocktex->tex_scaleintx
    );
    int64_t tex_scaley = (
        texinfo->type == GEOMTEX_ROOM ?
        texinfo->roomtex->tex_scaleinty :
        texinfo->blocktex->tex_scaleinty
    );
    int64_t tex_shiftx = (
        texinfo->type == GEOMTEX_ROOM ?
        ((int64_t)texinfo->roomtex->tex_shiftx +
         (int64_t)texinfo->roomtex->tex_gamestate_scrollx):
        0
    );
    int64_t tex_shifty = (
        texinfo->type == GEOMTEX_ROOM ?
        ((int64_t)texinfo->roomtex->tex_shifty +
         (int64_t)texinfo->roomtex->tex_gamestate_scrolly):
        0
    );
    int64_t repeat_units_x = (TEX_REPEAT_UNITS * (
        ((int64_t)tex_scalex) / TEX_FULLSCALE_INT
    ));
    if (repeat_units_x < 1) repeat_units_x = 1;
    if (llabs(wallstart_x - wallend_x) >
            llabs(wallstart_y - wallend_y)) {
        if (wallnormal_y < 0) {
            wx = -wx;
            wx2 = -wx2;
        }
        int64_t reference_x = (
            (wx + tex_shiftx)
        );
        if (reference_x > 0) {
            *tx1 = (reference_x % repeat_units_x) *
                TEX_COORD_SCALER / repeat_units_x;
        } else {
            *tx1 = (repeat_units_x - 1 -
                (((-reference_x) - 1) % repeat_units_x)) *
                TEX_COORD_SCALER/ repeat_units_x;
        }
        int64_t progress_x = (
            wx2 - wx
        );
        *tx2 = *tx1 + (progress_x *
            TEX_COORD_SCALER) / repeat_units_x;
    } else {
        if (wallnormal_x > 0) {
            wy = -wy;
            wy2 = -wy2;
        }
        int64_t reference_x = (
            (wy + tex_shifty)
        );
        if (reference_x > 0) {
            *tx1 = (reference_x % repeat_units_x) *
                TEX_COORD_SCALER / repeat_units_x;
        } else {
            *tx1 = (repeat_units_x - 1 -
                (((-reference_x) - 1) % repeat_units_x)) *
                TEX_COORD_SCALER / repeat_units_x;
        }
        int64_t progress_x = (
            wy2 - wy
        );
        *tx2 = *tx1 + (progress_x *
            TEX_COORD_SCALER) / repeat_units_x;
    }
}


HOTSPOT void roomcam_WallCubeMappingY(
        drawgeom *geom, int wallid,
        int aboveportal, geomtex *texinfo,
        int64_t wtop, int64_t wsliceheight,
        int64_t *ty1, int64_t *ty2
        ) {
    assert(geom->type == DRAWGEOM_ROOM ||
        geom->type == DRAWGEOM_BLOCK);
    assert(texinfo->type == GEOMTEX_ROOM ||
        texinfo->type == GEOMTEX_BLOCK);
    int64_t tex_scaley = (
        texinfo->type == GEOMTEX_ROOM ?
        texinfo->roomtex->tex_scaleinty :
        texinfo->blocktex->tex_scaleinty
    );
    int64_t tex_shifty = (
        texinfo->type == GEOMTEX_ROOM ?
        ((int64_t)texinfo->roomtex->tex_shifty +
         (int64_t)texinfo->roomtex->tex_gamestate_scrolly):
        0
    );
    int tex_stickyside = (
        texinfo->type == DRAWGEOM_ROOM ?
        texinfo->roomtex->tex_stickyside : -1);
    int64_t repeat_units_y = (TEX_REPEAT_UNITS * (
        ((int64_t)tex_scaley) / TEX_FULLSCALE_INT
    ));
    if (repeat_units_y < 1) repeat_units_y = 1;
    int64_t relevant_wall_segment_top = -1;
    if (geom->type == DRAWGEOM_ROOM) {
        relevant_wall_segment_top = (geom->r->floor_z +
            geom->r->height);
        if (!aboveportal && geom->r->wall[wallid].has_portal) {
            relevant_wall_segment_top = (
                geom->r->wall[wallid].portal_targetroom->floor_z
            );
            if (relevant_wall_segment_top < geom->r->floor_z)
                relevant_wall_segment_top = geom->r->floor_z;
        }
    } else {
        assert(geom->type == DRAWGEOM_BLOCK);
        relevant_wall_segment_top = (
            geom->bl->bottom_z + geom->bl->height
        );
    }
    int64_t relevant_wall_segment_bottom = -1;
    if (geom->type == DRAWGEOM_ROOM) {
        relevant_wall_segment_bottom = (geom->r->floor_z);
        if (aboveportal && geom->r->wall[wallid].has_portal) {
            relevant_wall_segment_bottom = (
                geom->r->wall[wallid].portal_targetroom->floor_z +
                    geom->r->wall[wallid].portal_targetroom->height
            );
            if (relevant_wall_segment_bottom > geom->r->floor_z +
                    geom->r->height)
                relevant_wall_segment_bottom = geom->r->floor_z +
                    geom->r->height;
        }
    } else {
        assert(geom->type == DRAWGEOM_BLOCK);
        relevant_wall_segment_bottom = (
            geom->bl->bottom_z
        );
    }
    int64_t _wtop = wtop;
    if (tex_stickyside == ROOM_DIR_UP) {
        _wtop = relevant_wall_segment_top - _wtop;
    } else if (tex_stickyside == ROOM_DIR_DOWN) {
        _wtop = relevant_wall_segment_bottom - _wtop;
    }
    int64_t reference_z = (_wtop + tex_shifty);
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


HOTSPOT static int roomcam_DrawWallSlice(
        rfssurf *rendertarget, const uint8_t *gammat,
        geomtex *texinfo, int64_t tx, int64_t ty1,
        int64_t ty2,
        int screentop, int screenbottom,
        int64_t topworldz, int64_t bottomworldz,
        int x, int y, int h, int cr, int cg, int cb
        ) {
    if (screentop >= h)
        return 1;
    assert(texinfo->type == GEOMTEX_ROOM ||
        texinfo->type == GEOMTEX_BLOCK);
    assert(cr >= 0);
    assert(cg >= 0);
    assert(cb >= 0);
    int crnonwhite = imin(cr, LIGHT_COLOR_SCALAR);
    int cgnonwhite = imin(cg, LIGHT_COLOR_SCALAR);
    int cbnonwhite = imin(cb, LIGHT_COLOR_SCALAR);
    int crwhite = imin(imax(0, cr - LIGHT_COLOR_SCALAR),
        LIGHT_COLOR_SCALAR);
    crnonwhite = imin(crnonwhite, LIGHT_COLOR_SCALAR - crwhite);
    int cgwhite = imin(imax(0, cg - LIGHT_COLOR_SCALAR),
        LIGHT_COLOR_SCALAR);
    cgnonwhite = imin(cgnonwhite, LIGHT_COLOR_SCALAR - cgwhite);
    int cbwhite = imin(imax(0, cb - LIGHT_COLOR_SCALAR),
        LIGHT_COLOR_SCALAR);
    cbnonwhite = imin(cbnonwhite, LIGHT_COLOR_SCALAR - cbwhite);
    assert(crwhite + crnonwhite <= LIGHT_COLOR_SCALAR);
    assert(cgwhite + cgnonwhite <= LIGHT_COLOR_SCALAR);
    assert(cbwhite + cbnonwhite <= LIGHT_COLOR_SCALAR);
    rfs2tex *t = (texinfo->type == GEOMTEX_ROOM ?
        roomlayer_GetTexOfRef(texinfo->roomtex->tex) :
        roomobj_GetTexOfRef(texinfo->blocktex->tex)
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
        assert(bottomworldz <= topworldz);
        if (unlikely(screentop < 0)) {
            int64_t z_scalar = (
                (topworldz - bottomworldz) * 1024 /
                (screenbottom - screentop));
            assert(z_scalar >= 0);
            int64_t ty_scalar = (
                (ty2 - ty1) * 1024 /
                (screenbottom - screentop));
            topworldz -= (-screentop * z_scalar) / 1024;
            if (topworldz < bottomworldz)
                return 1;
            ty1 += (-screentop * ty_scalar) / 1024;
            screentop = 0;
            if (screentop > screenbottom || screentop >= h)
                return 1;
        }
        if (unlikely(screenbottom > h)) {
            int64_t z_scalar = (
                (topworldz - bottomworldz) * 1024 /
                (screenbottom - screentop));
            int64_t ty_scalar = (
                (ty2 - ty1) * 1024 /
                (screenbottom - screentop));
            assert(z_scalar >= 0);
            bottomworldz += ((screenbottom - h) * z_scalar) / 1024;
            if (topworldz < bottomworldz)
                return 1;
            ty2 -= ((screenbottom - h) * ty_scalar) / 1024;
            screenbottom = h;
            if (screentop > screenbottom)
                return 1;
        }
        assert(bottomworldz <= topworldz);
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
        assert(tx >= 0 && tx < TEX_COORD_SCALER);
        int sourcey = srf->h - 1 - (
            (srf->h * tx) / TEX_COORD_SCALER
        );
        if (sourcey < 0) sourcey = 0;
        if (sourcey >= srf->h) sourcey = srf->h - 1;
        int sourcey_multiplied_w_copylen = (
            sourcey * srf->w * srccopylen
        );
        assert(screenbottom >= screentop);
        assert(screenbottom <= h);
        assert(screentop >= 0);
        if (screenbottom == h) {
            screenbottom--;
            if (screentop > screenbottom)
                return 1;
        }
        assert(screentop < h);
        const int writeoffsetplus = (
            targetw * rendertargetcopylen
        ) - (rendertarget->hasalpha ? 3 : 2);
        const int srfw = srf->w;
        uint8_t *targetwriteptr = &tgpixels[
            (x + (y + screentop) * targetw) * rendertargetcopylen
        ];
        uint8_t *targetwriteptrend = (
            targetwriteptr + ((screenbottom - screentop) + 1) *
            rendertargetcopylen * targetw
        );
        const int tghasalpha = rendertarget->hasalpha;
        // Prepare positive-only, reversed texture coordinates:
        int32_t ty1_positive = ty1;
        int32_t ty2_positive = ty2;
        {
            // Reverse, since our texture look-up needs it reversed:
            ty1_positive *= -1;
            ty2_positive *= -1;
        }
        if (ty1_positive < 0) {
            int32_t coordstoadd = (-ty1_positive) /
                TEX_COORD_SCALER + 1;
            ty1_positive += coordstoadd * TEX_COORD_SCALER;
            ty2_positive += coordstoadd * TEX_COORD_SCALER;
        }
        if (ty2_positive < 0) {
            int32_t coordstoadd = (-ty2_positive) /
                TEX_COORD_SCALER + 1;
            ty1_positive += coordstoadd * TEX_COORD_SCALER;
            ty2_positive += coordstoadd * TEX_COORD_SCALER;
        }
        // See how much texture coordinates step, shifted up for
        // higher step precision:
        int steps = imax(1, screenbottom - screentop);
        const uint32_t ty1_shift = ty1_positive << 5;
        const uint32_t ty2_shift = ty2_positive << 5;
        const int32_t tystep_shift = (
            (int32_t)ty2_shift - (int32_t)ty1_shift) / steps;
        uint32_t ty_nomod_shift = ty1_shift - tystep_shift;
        #if !defined(FIXED_ROOMTEX_SIZE) || \
                FIXED_ROOMTEX_SIZE <= 2 || \
                !defined(FIXED_ROOMTEX_SIZE_2) || \
                FIXED_ROOMTEX_SIZE_2 <= 2
        #error "need fixed tex sizes"
        #endif
        if (srf->w == FIXED_ROOMTEX_SIZE) {
            // Divisor to get from TEX_COORD_SCALER to tex size:
            assert(TEX_COORD_SCALER >= FIXED_ROOMTEX_SIZE);
            assert(!math_isnpot(FIXED_ROOMTEX_SIZE));
            const int coorddiv = (TEX_COORD_SCALER / FIXED_ROOMTEX_SIZE);
            assert(coorddiv == 1 || coorddiv == 2 ||
                   !math_isnpot(coorddiv));
            assert(TEX_COORD_SCALER / coorddiv == FIXED_ROOMTEX_SIZE);
            int ty;
            const int colorclippossible = (
                crwhite > 0 || cgwhite > 0 ||
                cbwhite > 0);
            if (likely(!tghasalpha)) {
                // Blit branch: tex size ONE, [ ] render target alpha
                while (likely(targetwriteptr !=
                        targetwriteptrend)) {
                    ty_nomod_shift += tystep_shift;
                    ty = (ty_nomod_shift >> 5) & texcoord_modulo_mask;
                    // Remember, we're using the sideways tex.
                    const int sourcex = (
                        (ty / coorddiv) * srccopylen
                    );
                    const int srcoffset = (
                        sourcex + sourcey_multiplied_w_copylen
                    );
                    uint8_t *readptr = (
                        (uint8_t *)&srcpixels[srcoffset]
                    );
                    assert(srcoffset >= 0 &&
                        srcoffset < srf->w * srf->h * srccopylen);
                    *targetwriteptr = gammat[_assert_pix256(
                        (*readptr) * crnonwhite /
                        LIGHT_COLOR_SCALAR +
                        255 * crwhite /
                        LIGHT_COLOR_SCALAR)];
                    targetwriteptr++;
                    readptr++;
                    *targetwriteptr = gammat[_assert_pix256(
                        (*readptr) * cgnonwhite /
                        LIGHT_COLOR_SCALAR +
                        255 * cgwhite /
                        LIGHT_COLOR_SCALAR)];
                    targetwriteptr++;
                    readptr++;
                    *targetwriteptr = gammat[_assert_pix256(
                        (*readptr) * cbnonwhite /
                        LIGHT_COLOR_SCALAR +
                        255 * cbwhite /
                        LIGHT_COLOR_SCALAR)];
                    targetwriteptr += writeoffsetplus;
                }
            } else if (unlikely(tghasalpha)) {
                // Blit branch: tex size ONE, [x] render target alpha
                while (likely(targetwriteptr !=
                        targetwriteptrend)) {
                    ty_nomod_shift += tystep_shift;
                    ty = (ty_nomod_shift >> 5) & texcoord_modulo_mask;
                    const int sourcex = (
                        (ty / coorddiv) * srccopylen
                    );
                    const int srcoffset = (
                        sourcex + sourcey_multiplied_w_copylen
                    );
                    uint8_t *readptr = (
                        (uint8_t *)&srcpixels[srcoffset]
                    );
                    assert(srcoffset >= 0 &&
                        srcoffset < srf->w * srf->h * srccopylen);
                    *targetwriteptr = gammat[_assert_pix256(
                        (*readptr) * crnonwhite /
                        LIGHT_COLOR_SCALAR +
                        255 * crwhite /
                        LIGHT_COLOR_SCALAR)];
                    targetwriteptr++;
                    readptr++;
                    *targetwriteptr = gammat[_assert_pix256(
                        (*readptr) * cgnonwhite /
                        LIGHT_COLOR_SCALAR +
                        255 * cgwhite /
                        LIGHT_COLOR_SCALAR)];
                    targetwriteptr++;
                    readptr++;
                    *targetwriteptr = gammat[_assert_pix256(
                        (*readptr) * cbnonwhite /
                        LIGHT_COLOR_SCALAR +
                        255 * cbwhite /
                        LIGHT_COLOR_SCALAR)];
                    targetwriteptr++;
                    *targetwriteptr = 255;
                    targetwriteptr += writeoffsetplus;
                }
            } else {
                assert(0);  // should not be hit
            }
        } else {
            // Divisor to get from TEX_COORD_SCALER_2 to tex size:
            assert(srf->w == FIXED_ROOMTEX_SIZE_2);
            assert(TEX_COORD_SCALER >= FIXED_ROOMTEX_SIZE_2);
            assert(!math_isnpot(FIXED_ROOMTEX_SIZE_2));
            const int coorddiv = (TEX_COORD_SCALER / FIXED_ROOMTEX_SIZE_2);
            assert(coorddiv == 1 || coorddiv == 2 ||
                   !math_isnpot(coorddiv));
            assert(TEX_COORD_SCALER / coorddiv == FIXED_ROOMTEX_SIZE_2);
            int ty;
            const int colorclippossible = (
                crwhite > 0 || cgwhite > 0 ||
                cbwhite > 0);
            if (likely(!tghasalpha)) {
                // Blit branch: tex size TWO, [ ] render target alpha
                while (likely(targetwriteptr !=
                        targetwriteptrend)) {
                    ty_nomod_shift += tystep_shift;
                    ty = (ty_nomod_shift >> 5) & texcoord_modulo_mask;
                    // Remember, we're using the sideways tex.
                    const int sourcex = (
                        (ty / coorddiv) * srccopylen
                    );
                    const int srcoffset = (
                        sourcex + sourcey_multiplied_w_copylen
                    );
                    uint8_t *readptr = (
                        (uint8_t *)&srcpixels[srcoffset]
                    );
                    assert(srcoffset >= 0 &&
                        srcoffset < srf->w * srf->h * srccopylen);
                    *targetwriteptr = gammat[_assert_pix256(
                        (*readptr) * crnonwhite /
                        LIGHT_COLOR_SCALAR +
                        255 * crwhite /
                        LIGHT_COLOR_SCALAR)];
                    targetwriteptr++;
                    readptr++;
                    *targetwriteptr = gammat[_assert_pix256(
                        (*readptr) * cgnonwhite /
                        LIGHT_COLOR_SCALAR +
                        255 * cgwhite /
                        LIGHT_COLOR_SCALAR)];
                    targetwriteptr++;
                    readptr++;
                    *targetwriteptr = gammat[_assert_pix256(
                        (*readptr) * cbnonwhite /
                        LIGHT_COLOR_SCALAR +
                        255 * cbwhite /
                        LIGHT_COLOR_SCALAR)];
                    targetwriteptr += writeoffsetplus;
                }
            } else if (unlikely(tghasalpha)) {
                // Blit branch: tex size TWO, [x] render target alpha
                while (likely(targetwriteptr !=
                        targetwriteptrend)) {
                    ty_nomod_shift += tystep_shift;
                    ty = (ty_nomod_shift >> 5) & texcoord_modulo_mask;
                    // Remember, we're using the sideways tex.
                    const int sourcex = (
                        (ty / coorddiv) * srccopylen
                    );
                    const int srcoffset = (
                        sourcex + sourcey_multiplied_w_copylen
                    );
                    uint8_t *readptr = (
                        (uint8_t *)&srcpixels[srcoffset]
                    );
                    assert(srcoffset >= 0 &&
                        srcoffset < srf->w * srf->h * srccopylen);
                    *targetwriteptr = gammat[_assert_pix256(
                        (*readptr) * crnonwhite /
                        LIGHT_COLOR_SCALAR +
                        255 * crwhite /
                        LIGHT_COLOR_SCALAR)];
                    targetwriteptr++;
                    readptr++;
                    *targetwriteptr = gammat[_assert_pix256(
                        (*readptr) * cgnonwhite /
                        LIGHT_COLOR_SCALAR +
                        255 * cgwhite /
                        LIGHT_COLOR_SCALAR)];
                    targetwriteptr++;
                    readptr++;
                    *targetwriteptr = gammat[_assert_pix256(
                        (*readptr) * cbnonwhite /
                        LIGHT_COLOR_SCALAR +
                        255 * cbwhite /
                        LIGHT_COLOR_SCALAR)];
                    targetwriteptr++;
                    *targetwriteptr = 255;
                    targetwriteptr += writeoffsetplus;
                }
            } else {
                assert(0);  // should not be hit
            }
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
            if (overdraw_for_inside &&
                    imax(0, imin(h - 1, bottomoffset)) !=
                    imax(0, imin(h - 1, topoffset)))
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
            if (overdraw_for_inside &&
                    imax(0, imin(h - 1, bottomoffset)) !=
                    imax(0, imin(h - 1, topoffset)))
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
        roomcam *cam, drawgeom *geom, int aboveportal,
        int canvasx, int canvasy,
        int xcol, int endcol,
        int originwall, geomtex *texinfo,
        int64_t wall_floor_z, int64_t wall_height
        ) {
    assert(texinfo->type == GEOMTEX_ROOM ||
        texinfo->type == GEOMTEX_BLOCK);
    assert(!aboveportal || geom->type == DRAWGEOM_ROOM);

    // First, extract some info:
    const int orig_xcol = xcol;
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
    if (rendertarget->w == 0 || rendertarget->h == 0)
        return 1;

    // Do the actual drawing:
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
                int64_t localx = start_hitx - cam->obj->x;
                int64_t localy = start_hity - cam->obj->y;
                int64_t camcenterdist = math_veclen2di(
                    localx, localy
                );
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
                int64_t localx = end_hitx - cam->obj->x;
                int64_t localy = end_hity - cam->obj->y;
                int64_t camcenterdist = math_veclen2di(
                    localx, localy
                );
                assert(end_wallno >= 0);
                break;
            }
        }
        if (end_wallno < 0) {
            xcol++;
            prev_hitset = 0;
            continue;
        }

        // Calculate horizontal tex coordinates:
        int64_t tx1, tx2;
        roomcam_WallCubeMappingX(
            geom, start_wallno, start_hitx, start_hity,
            end_hitx, end_hity, texinfo, &tx1, &tx2
        );
        int64_t tdefaulty1, tdefaulty2;
        roomcam_WallCubeMappingY(
            geom, start_wallno,
            aboveportal, texinfo,
            wall_floor_z + wall_height, wall_height,
            &tdefaulty1, &tdefaulty2
        );

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
            const int64_t perspmapscalar = 10000;
            const int64_t progress = (perspmapscalar *
                (int64_t)(z - xcol)) /
                (int64_t)imax(1, max_render_ahead);
            const int64_t invprogress = perspmapscalar - progress;
            int64_t tx = (
                tx1 + (((tx2 - tx1) *
                (z - xcol)) / imax(1, max_render_ahead)));
            assert(texcoord_modulo_mask != 0);
            tx = tx & texcoord_modulo_mask;
            assert(tx >= 0 && tx < TEX_COORD_SCALER);
            int64_t ix = (
                start_hitx + ((end_hitx - start_hitx) *
                (z - xcol)) / imax(1, max_render_ahead));
            int64_t iy = (
                start_hity + ((end_hity - start_hity) *
                (z - xcol)) / imax(1, max_render_ahead));
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
                    z, 0, &topworldz, &bottomworldz,
                    &top, &bottom
                    ))) {
                // Fully off-screen.
                z++;
                continue;
            }
            // Override screen top/bottom with better interpolation:
            if (interpolatescreensize) {
                top = (((endtop - starttop) * (z - xcol)) /
                    imax(1, max_render_ahead) + starttop);
                bottom = (((endbottom - startbottom) * (z - xcol)) /
                    imax(1, max_render_ahead) + startbottom);
            }

            int64_t ty1, ty2;
            if (likely(wall_floor_z + wall_height == wall_height)) {
                ty1 = tdefaulty1;
                ty2 = tdefaulty2;
            } else {
                roomcam_WallCubeMappingY(
                    geom, start_wallno,
                    aboveportal, texinfo,
                    topworldz, topworldz - bottomworldz,
                    &ty1, &ty2
                );
            }

            // Calculate the color for this slice:
            int cr = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                r->sector_light_r + dynr * 2));
            int cg = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                r->sector_light_g + dyng * 2));
            int cb = imax(0, imin(LIGHT_COLOR_SCALAR * 2,
                r->sector_light_b + dynb * 2));

            // Draw slice:
            assert(bottom >= top);
            assert(start_wallno != originwall);
            if (unlikely(!roomcam_DrawWallSlice(
                    rendertarget, cam->cache->gammamap,
                    texinfo, tx, ty1, ty2,
                    top, bottom, topworldz, bottomworldz,
                    canvasx + z, canvasy, h,
                    cr, cg, cb
                    )))
                return 0;
            stats->base_geometry_slices_rendered++;
            z++;
            #if defined(DUPLICATE_WALL_PIX) && \
                    DUPLICATE_WALL_PIX >= 1
            int extra_z = imin(z + DUPLICATE_WALL_PIX,
                xcol + max_render_ahead);
            if (likely(top < h && bottom > top)) {
                while (likely(z < extra_z)) {
                    int row = imax(0, top) + canvasy;
                    const int maxrow = (
                        (bottom + canvasy) < (canvasy + h) ?
                        (bottom + canvasy) :
                        (canvasy + h - 1)
                    );
                    const int copylen = (rendertarget->hasalpha ? 4 : 3);
                    const int xbyteoffsetnew = (
                        copylen * (canvasx + z));
                    const int xbyteoffsetold = (
                        copylen * (canvasx + (z - 1)));
                    int ybyteoffset = (
                        copylen * rendertarget->w * (imax(0, top) + canvasy)
                    );
                    const int ybyteposttarget = (
                        copylen * rendertarget->w * (imin(rendertarget->h - 1,
                            bottom + 1 + canvasy))
                    );
                    const int ybyteshift = copylen * rendertarget->w;
                    if (rendertarget->hasalpha) {
                        // With alpha:
                        while (ybyteoffset != ybyteposttarget) {
                            memcpy(&rendertarget->pixels[
                                xbyteoffsetnew + ybyteoffset
                            ], &rendertarget->pixels[
                                xbyteoffsetold + ybyteoffset
                            ], 4);
                            row++;
                            ybyteoffset += ybyteshift;
                        }
                    } else {
                        // Without alpha:
                        while (ybyteoffset != ybyteposttarget) {
                            memcpy(&rendertarget->pixels[
                                xbyteoffsetnew + ybyteoffset
                            ], &rendertarget->pixels[
                                xbyteoffsetold + ybyteoffset
                            ], 3);
                            row++;
                            ybyteoffset += ybyteshift;
                        }
                    }
                    stats->base_geometry_slices_rendered++;
                    z++;
                }
            }
            #endif
        }
        xcol = z;
    }
    assert(xcol == endcol + 1);
    return 1;
}

