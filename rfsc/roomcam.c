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
#include "gamma.h"
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


int32_t texcoord_modulo_mask = 0;

static __attribute__((constructor)) void _init_modulomask() {
    assert(TEX_COORD_SCALER > 2 &&
           !math_isnpot(TEX_COORD_SCALER));
    texcoord_modulo_mask = math_one_bits_from_right(
        math_count_bits_until_zeros(TEX_COORD_SCALER) - 1
    );
    assert(LIGHT_COLOR_SCALAR > 2 &&
           !math_isnpot(LIGHT_COLOR_SCALAR));
}


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
    rc->cache->cachedgamma = -1;
    rc->cache->stats.fps_ts = datetime_Ticks();
    rc->cache->cachedangle = -99999;
    rc->cache->cachedvangle = -99999;
    rc->fovf = 70.0;
    rc->gamma = 8;
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
        mix_r += round((double)strength_scalar * linfo->r /
            (double)LIGHT_COLOR_SCALAR);
        mix_g += round((double)strength_scalar * linfo->g /
            (double)LIGHT_COLOR_SCALAR);
        mix_b += round((double)strength_scalar * linfo->b /
            (double)LIGHT_COLOR_SCALAR);
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


void roomcam_CalculateGamma(roomcam *cam) {
    if (cam->gamma == cam->cache->cachedgamma)
        return;
    gamma_GenerateMap(cam->gamma, cam->cache->gammamap);
    cam->cache->cachedgamma = cam->gamma;
}


int roomcam_CalculateViewPlane(roomcam *cam, int w, int h) {
    // Override mathematically unsafe values:
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;

    // Calculate viewplane:
    int force_recalculate_vertivecs = 0;
    int force_recalculate_rotated_vecs = 0;
    cam->fov = math_fixanglei(cam->fov);
    cam->obj->angle = math_fixanglei(cam->obj->angle);
    cam->vangle = math_fixanglei(cam->vangle);
    if (cam->cache->cachedfov != cam->fov ||
            cam->cache->cachedw != w ||
            cam->cache->cachedh != h) {
        #if defined(DEBUG_3DRENDERER)
        printf("rfsc/roomcam.c: debug: "
            "recalculating FOV and horizontal ray vectors\n");
        #endif
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
        cam->cache->planeheight = plane_world_height;
        cam->cache->cachedfovh = fov_h;
        force_recalculate_vertivecs = 1;
    }
    if (force_recalculate_vertivecs ||
            cam->cache->cachedvangle != cam->vangle) {
        #if defined(DEBUG_3DRENDERER) && \
            defined(DEBUG_3DRENDERER_EXTRA)
        printf("rfsc/roomcam.c: debug: "
            "recalculating vertical ray vectors\n");
        #endif
        const int64_t plane_height = cam->cache->planeheight;
        const int64_t plane_distance = cam->cache->planedist;
        int64_t _vertishift_x = plane_distance;
        int64_t _vertishift_z = 0;
        math_rotate2di(&_vertishift_x, &_vertishift_z, -cam->vangle);
        int scalar = ((int64_t)1000LL * _vertishift_x) /
            plane_distance;
        const int64_t vertishift_z = (_vertishift_z * scalar) /
            (int64_t)1000LL;
        cam->cache->planezshift = vertishift_z;
        int i = 0;
        while (i < h) {
            cam->cache->vertivecs_z[i] = (
                plane_height / 2 -
                plane_height * i / imax(1, h - 1)
            ) + vertishift_z;
            cam->cache->vertivecs_x[i] = plane_distance;
            i++;
        }
        int32_t fov_v = abs(math_fixanglei(math_angle2di(
            cam->cache->vertivecs_x[0],
            -cam->cache->vertivecs_z[0]
        ) - math_angle2di(
            cam->cache->vertivecs_x[h - 1],
            -cam->cache->vertivecs_z[h - 1]
        )));
        cam->cache->cachedfovv = fov_v;
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
    #if defined(DEBUG_3DRENDERER) && defined(DEBUG_3DRENDERER_EXTRA)
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
                defined(DEBUG_3DRENDERER_EXTRA))
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
        #if defined(DEBUG_3DRENDERER) && defined(DEBUG_3DRENDERER_EXTRA)
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
                    geomtex tex = {0};
                    tex.type = GEOMTEX_ROOM;
                    tex.roomtex = abovetexinfo;
                    roomcam_DrawWall(
                        cam, &geom, 1, x, y, col, endcol,
                        ignorewall, &tex,
                        target->floor_z + target->height,
                        (r->height + r->floor_z) -
                        (target->floor_z + target->height)
                    );
                }
                if (draw_below) {  // Below portal drawing!
                    geomtex tex = {0};
                    tex.type = GEOMTEX_ROOM;
                    tex.roomtex = belowtexinfo;
                    roomcam_DrawWall(
                        cam, &geom, 0, x, y, col, endcol,
                        ignorewall, &tex,
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
            geomtex tex = {0};
            tex.type = GEOMTEX_ROOM;
            tex.roomtex = texinfo;
            roomcam_DrawWall(
                cam, &geom, 0, x, y, col, endcol,
                ignorewall, &tex,
                r->floor_z, r->height
            );
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
    #if defined(DEBUG_3DRENDERER) && defined(DEBUG_3DRENDERER_EXTRA)
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
    roomcam_CalculateGamma(cam);
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


