// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(HAVE_SDL)
#include <SDL2/SDL.h>
#endif
#include <unistd.h>

#include "math2d.h"
#include "rfssurf.h"


#ifndef NDEBUG
static int _assert_pix256(int v) {
    assert(v >= 0 && v < 256);
    return v;
}
#else
#define _assert_pix256(x) x
#endif


static int sanitize_clipping(int *intgx, int *intgy,
        int *inclipx, int *inclipy, int *inclipw, int *incliph,
        rfssurf *source, rfssurf *target) {
    int tgx = *intgx;
    int tgy = *intgy;
    int clipx = *inclipx;
    int clipy = *inclipy;
    int clipw = *inclipw;
    int cliph = *incliph;
    if (clipx < 0) {
        clipw += clipx;
        clipx = 0;
    }
    if (clipy < 0) {
        cliph += clipy;
        clipy = 0;
    }
    if (tgx < 0) {
        clipx += -tgx;
        clipw -= -tgx;
        tgx = 0;
    }
    if (tgy < 0) {
        clipy += -tgy;
        cliph -= -tgy;
        tgy = 0;
    }
    if (tgx + clipw > target->w)
        clipw = target->w - tgx;
    if (tgy + cliph > target->h)
        cliph = target->h - tgy;
    if (clipx + clipw > source->w)
        clipw = (source->w - clipx);
    if (clipy + cliph > source->h)
        cliph = (source->h - clipy);
    if (clipw <= 0 || cliph <= 0)
        return 0;
    *intgx = tgx;
    *intgy = tgy;
    *inclipx = clipx;
    *inclipy = clipy;
    *inclipw = clipw;
    *incliph = cliph;
    return 1;
}


static int sanitize_clipping_scaled(int *intgx, int *intgy,
        int *inclipx, int *inclipy, int *inclipw, int *incliph,
        rfssurf *source, rfssurf *target,
        double scalex, double scaley) {
    int tgx = *intgx;
    int tgy = *intgy;
    int clipx = *inclipx;
    int clipy = *inclipy;
    int clipw = *inclipw;
    int cliph = *incliph;
    if (clipx < 0) {
        clipw += clipx;
        clipx = 0;
    }
    if (clipy < 0) {
        cliph += clipy;
        clipy = 0;
    }
    if (tgx < 0) {
        clipx += round((double)-tgx / scalex);
        clipw -= round((double)-tgx / scalex);
        tgx = 0;
    }
    if (tgy < 0) {
        clipy += round((double)-tgy / scaley);
        cliph -= round((double)-tgy / scaley);
        tgy = 0;
    }
    if (tgx + floor(clipw * scalex) > target->w)
        clipw = ceil((double)(target->w - tgx) / scalex);
    if (tgy + floor(cliph * scaley) > target->h)
        cliph = ceil((double)(target->h - tgy) / scaley);
    if (clipx + clipw > source->w)
        clipw = (source->w - clipx);
    if (clipy + cliph > source->h)
        cliph = (source->h - clipy);
    if (clipw <= 0 || cliph <= 0)
        return 0;
    *intgx = tgx;
    *intgy = tgy;
    *inclipx = clipx;
    *inclipy = clipy;
    *inclipw = clipw;
    *incliph = cliph;
    return 1;
}


rfssurf *rfssurf_Duplicate(rfssurf *surf) {
    rfssurf *newsrf = rfssurf_Create(
        surf->w, surf->h, surf->hasalpha
    );
    if (!newsrf)
        return NULL;
    if (surf->w > 0 && surf->h > 0) {
        if (surf->hasalpha) {
            memcpy(newsrf->pixels, surf->pixels,
                4 * newsrf->w * newsrf->h);
        } else {
            memcpy(newsrf->pixels, surf->pixels,
                3 * newsrf->w * newsrf->h);
        }
    }
    return newsrf;
}


rfssurf *rfssurf_DuplicateNoAlpha(rfssurf *surf) {
    rfssurf *newsrf = rfssurf_Create(
        surf->w, surf->h, 0
    );
    if (!newsrf)
        return NULL;
    if (surf->hasalpha) {
        int i = 0;
        const int maxi = newsrf->w * newsrf->h;
        int offsetnew = 0;
        int offsetold = 0;
        while (i < maxi) {
            memcpy(
                &newsrf->pixels[offsetnew],
                &surf->pixels[offsetold], 3);
            offsetnew += 3;
            offsetold += 4;
            i++;
        }
    } else {
        memcpy(newsrf->pixels, surf->pixels,
            3 * newsrf->w * newsrf->h);
    }
    return newsrf;
}


HOTSPOT void rfssurf_Rect(rfssurf *target,
        int x, int y, int w, int h,
        double r, double g, double b, double a) {
    if (x < 0) {
        w += -x;
        x = 0;
    }
    if (y < 0) {
        h += -y;
        y = 0;
    }
    if (x + w > target->w) {
        w = (target->w) - x;
    }
    if (y + h > target->h) {
        h = (target->h) - y;
    }
    if (w <= 0 || h <= 0 || a <= 0.0001)
        return;
    if (a >= 0.999) {
        const uint8_t copyval[4] = {
            fmax(0, fmin(255, round(r * 255))),
            fmax(0, fmin(255, round(g * 255))),
            fmax(0, fmin(255, round(b * 255))),
            255
        };
        const int copylen = (target->hasalpha ? 4 : 3);
        const int maxy = y + h;
        int cy = y;
        const int maxx = x + w;
        int cx = x;
        assert(cy < maxy && cx < maxx);
        while (cy < maxy) {
            int targetoffset = (cy * target->w + x) * (
                copylen
            );
            cx = x;
            while (cx < maxx) {
                memcpy(
                    &target->pixels[targetoffset],
                    copyval, copylen
                );
                cx++;
                targetoffset += copylen;
            }
            cy++;
        }
    } else {
        const int ir = fmax(0, fmin(255, round(r * 255)));
        const int ig = fmax(0, fmin(255, round(g * 255)));
        const int ib = fmax(0, fmin(255, round(b * 255)));
        const int INT_COLOR_SCALAR = 1024;
        const int alphar = (
            fmax(0, fmin(INT_COLOR_SCALAR, round(a
                * INT_COLOR_SCALAR)))
        );
        const int reverse_alphar = (INT_COLOR_SCALAR - alphar);
        const int copylen = (target->hasalpha ? 4 : 3);
        const int maxy = y + h;
        int cy = y;
        const int maxx = x + w;
        int cx = x;
        assert(cy < maxy && cx < maxx);
        while (cy < maxy) {
            int targetoffset = (cy * target->w + x) * (
                copylen
            );
            assert(alphar + reverse_alphar == INT_COLOR_SCALAR);
            assert(alphar >= 0 && reverse_alphar >= 0);
            assert(ir >= 0 && ig >= 0 && ib >= 0);
            cx = x;
            while (cx < maxx) {
                target->pixels[targetoffset + 0] = _assert_pix256(
                    ((int)target->pixels[targetoffset + 0] *
                        reverse_alphar / INT_COLOR_SCALAR) +
                    (ir * alphar / INT_COLOR_SCALAR)
                );
                target->pixels[targetoffset + 1] = _assert_pix256(
                    ((int)target->pixels[targetoffset + 1] *
                        reverse_alphar / INT_COLOR_SCALAR) +
                    (ig * alphar / INT_COLOR_SCALAR)
                );
                target->pixels[targetoffset + 2] = _assert_pix256(
                    ((int)target->pixels[targetoffset + 2] *
                        reverse_alphar / INT_COLOR_SCALAR) +
                    (ib * alphar / INT_COLOR_SCALAR)
                );
                if (likely(target->hasalpha)) {
                    int a = target->pixels[targetoffset + 3] *
                        INT_COLOR_SCALAR;
                    a = (INT_COLOR_SCALAR * 255 - a) * reverse_alphar /
                        INT_COLOR_SCALAR;
                    a = (INT_COLOR_SCALAR * 255 - a);
                    target->pixels[targetoffset + 3] = (
                        _assert_pix256(a / INT_COLOR_SCALAR)
                    );
                }
                cx++;
                targetoffset += copylen;
            }
            cy++;
        }
    }
}


HOTSPOT void rfssurf_BlitSimple(
        rfssurf *restrict target, rfssurf *restrict source,
        int tgx, int tgy, int clipx, int clipy, int clipw, int cliph
        ) {
    // The most simple blitter that supports neither overall blit alpha
    // nor coloring. It's also the fastest.

    // Some checks and clip to our render target:
    if (!sanitize_clipping(&tgx, &tgy, &clipx, &clipy, &clipw, &cliph,
            source, target))  // clip to target
        return;
    assert(clipx >= 0 && clipy >= 0 &&
        clipx < source->w && clipy < source->h);
    assert(clipw > 0 && cliph > 0);
    assert(clipx + clipw <= source->w &&
        clipy + cliph <= source->h);
    assert(tgx >= 0 && tgy >= 0 &&
        tgx < target->w && tgy < target->h);
    const int targetstep = (target->hasalpha ? 4 : 3);
    const int sourcestep = (source->hasalpha ? 4 : 3);
    const int tghasalpha = target->hasalpha;
    const int sourcehasalpha = source->hasalpha;
    const int INT_COLOR_SCALAR = 1024;
    const int clipalpha = floor(1 * INT_COLOR_SCALAR / 255);
    assert(clipalpha > 0);
    const int maxy = tgy + cliph;
    int y = tgy;
    const int maxx = tgx + clipw;
    int x = tgx;
    assert(y < maxy && x < maxx);
    while (y < maxy) {
        int targetoffset = (y * target->w + tgx) * (
            targetstep
        );
        int sourceoffset = (
            (y - tgy + clipy) * source->w + clipx
        ) * sourcestep;
        uint8_t *readptr = &source->pixels[sourceoffset];
        uint8_t *writeptr = &target->pixels[targetoffset];
        uint8_t *writeptrend = (
            writeptr + (maxx - tgx) * targetstep
        );
        if (likely(sourcehasalpha && tghasalpha)) {
            // Source HAS ALPHA and target HAS ALPHA loop.
            while (writeptr != writeptrend) {
                // XXX: assumes little endian. format is BGR/ABGR
                int alphar;
                uint8_t salpha = *(readptr + 3);
                // See if we can skip zero pixels faster:
                if (likely(salpha == 0)) {
                    while (likely(writeptr != writeptrend &&
                            *(readptr + 3) == 0
                            // ^ salpha is NOT updated.
                            )) {
                        readptr += 4;
                        writeptr += 4;
                        continue;
                    }
                    continue;
                }
                alphar = (((int)salpha
                    * INT_COLOR_SCALAR) / 255);
                //assert(alphar <= INT_COLOR_SCALAR);

                // Regular heavy duty blit:
                int reverse_alphar = 0;
                if (likely(alphar == INT_COLOR_SCALAR)) {
                    while (1) {
                        if (unlikely(writeptr == writeptrend))
                            break;
                        salpha = *(readptr + 3);
                        if (unlikely(salpha != 255))
                            break;
                        memcpy(
                            writeptr, readptr, 4
                        );
                        writeptr += 4;
                        readptr += 4;
                        continue;
                    }
                    continue;
                } else {
                    reverse_alphar = (INT_COLOR_SCALAR - alphar);
                    assert(alphar + reverse_alphar == INT_COLOR_SCALAR);
                    // Blend in rgb now:
                    *writeptr = _assert_pix256(
                        ((int)(*writeptr) *
                            reverse_alphar / INT_COLOR_SCALAR) +
                        ((int)(*readptr) *
                            alphar / INT_COLOR_SCALAR)
                    );
                    writeptr++;
                    readptr++;
                    *writeptr = _assert_pix256(
                        ((int)(*writeptr) *
                            reverse_alphar / INT_COLOR_SCALAR) +
                        ((int)(*readptr) *
                            alphar / INT_COLOR_SCALAR)
                    );
                    writeptr++;
                    readptr++;
                    *writeptr = _assert_pix256(
                        ((int)(*writeptr) *
                            reverse_alphar / INT_COLOR_SCALAR) +
                        ((int)(*readptr) *
                            alphar / INT_COLOR_SCALAR)
                    );
                    writeptr++;
                    readptr++;
                    int a = (*writeptr) *
                        INT_COLOR_SCALAR;
                    a = (INT_COLOR_SCALAR * 255 - a) * reverse_alphar /
                        INT_COLOR_SCALAR;
                    a = (INT_COLOR_SCALAR * 255 - a);
                    *writeptr = _assert_pix256(
                        a / INT_COLOR_SCALAR
                    );
                    writeptr++;
                }
                readptr++;
            }
        } else if (likely(sourcehasalpha && !tghasalpha)) {
            // Source HAS ALPHA and target with NO ALPHA loop.
            while (writeptr != writeptrend) {
                // XXX: assumes little endian. format is BGR/ABGR
                int alphar;
                uint8_t salpha = *(readptr + 3);
                // See if we can skip zero pixels faster:
                if (likely(salpha == 0)) {
                    while (likely(writeptr != writeptrend &&
                            *(readptr + 3) == 0
                            // ^ salpha is NOT updated.
                            )) {
                        readptr += 4;
                        writeptr += 3;
                        continue;
                    }
                    continue;
                }
                alphar = (((int)salpha
                    * INT_COLOR_SCALAR) / 255);
                //assert(alphar <= INT_COLOR_SCALAR);

                // Regular heavy duty blit:
                int reverse_alphar = 0;
                if (likely(alphar == INT_COLOR_SCALAR)) {
                    while (1) {
                        if (unlikely(writeptr == writeptrend))
                            break;
                        salpha = *(readptr + 3);
                        if (unlikely(salpha != 255))
                            break;
                        memcpy(
                            writeptr, readptr, 3
                        );
                        writeptr += 3;
                        readptr += 4;
                        continue;
                    }
                    continue;
                } else {
                    reverse_alphar = (INT_COLOR_SCALAR - alphar);
                    assert(alphar + reverse_alphar == INT_COLOR_SCALAR);
                    // Blend in rgb now:
                    *writeptr = _assert_pix256(
                        ((int)(*writeptr) *
                            reverse_alphar / INT_COLOR_SCALAR) +
                        ((int)(*readptr) *
                            alphar / INT_COLOR_SCALAR)
                    );
                    writeptr++;
                    readptr++;
                    *writeptr = _assert_pix256(
                        ((int)(*writeptr) *
                            reverse_alphar / INT_COLOR_SCALAR) +
                        ((int)(*readptr) *
                            alphar / INT_COLOR_SCALAR)
                    );
                    writeptr++;
                    readptr++;
                    *writeptr = _assert_pix256(
                        ((int)(*writeptr) *
                            reverse_alphar / INT_COLOR_SCALAR) +
                        ((int)(*readptr) *
                            alphar / INT_COLOR_SCALAR)
                    );
                    writeptr++;
                    readptr++;
                }
                readptr++;
            }
       } else {
            // Source with NO ALPHA loop.
            if (likely(tghasalpha)) {
                while (writeptr != writeptrend) {
                    // XXX: assumes little endian. format is BGR/ABGR
                    assert(writeptr < writeptrend);
                    memcpy(
                        writeptr, readptr, 3
                    );
                    writeptr += 3;
                    *writeptr = 255;
                    writeptr++;
                    readptr += 3;
                }
            } else {
                memcpy(
                    writeptr, readptr, writeptrend - writeptr
                );
            }
        }
        y++;
    }
}


rfssurf *rfssurf_Create(int w, int h, int alpha) {
    rfssurf *r = malloc(sizeof(*r));
    if (!r)
        return NULL;
    memset(r, 0, sizeof(*r));
    r->w = w;
    r->h = h;
    r->hasalpha = (alpha != 0);
    r->pixels = malloc(
        (w > 0 && h > 0) ? w * h * (alpha ? 4 : 3) : 1
    );
    if (!r->pixels) {
        free(r);
        return NULL;
    }
    if (w > 0 && h > 0)
        memset(r->pixels, 0, w * h * (alpha ? 4 : 3));
    return r;
}


void rfssurf_Free(rfssurf *surf) {
    if (!surf)
        return;
    free(surf->pixels);
    #if defined(HAVE_SDL)
    if (surf->sdlsrf)
        SDL_FreeSurface(surf->sdlsrf);
    #endif
    free(surf);
}


HOTSPOT void rfssurf_BlitColor(
        rfssurf *restrict target, rfssurf *restrict source,
        int tgx, int tgy, int clipx, int clipy, int clipw, int cliph,
        double r, double g, double b, double a
        ) {
    // Simplified fast blitter that has no idea of scaling at all.
    // It can do coloring though.

    // Basic fall-through check and preparing things:
    if (fabs(r - 1.0) < 0.0001 &&
            fabs(r - 1.0) < 0.0001 &&
            fabs(r - 1.0) < 0.0001 &&
            fabs(fmin(1, a) - 1.0) < 0.0001) {
        return rfssurf_BlitSimple(target, source,
            tgx, tgy, clipx, clipy, clipw, cliph);
    }
    const int INT_COLOR_SCALAR = 1024;
    int intrfull = round(fmin(2 * INT_COLOR_SCALAR,
        fmax(0, r * (double)INT_COLOR_SCALAR)));
    int intgfull = round(fmin(2 * INT_COLOR_SCALAR,
        fmax(0, g * (double)INT_COLOR_SCALAR)));
    int intbfull = round(fmin(2 * INT_COLOR_SCALAR,
        fmax(0, b * (double)INT_COLOR_SCALAR)));
    int inta = round(fmin(INT_COLOR_SCALAR,
        fmax(0, a * (double)INT_COLOR_SCALAR)));
    int intrwhite = (intrfull - INT_COLOR_SCALAR);
    if (intrwhite < 0) intrwhite = 0;
    int intgwhite = (intgfull - INT_COLOR_SCALAR);
    if (intgwhite < 0) intgwhite = 0;
    int intbwhite = (intbfull - INT_COLOR_SCALAR);
    if (intbwhite < 0) intbwhite = 0;
    int intr = intrfull;
    if (intr > INT_COLOR_SCALAR) intr = INT_COLOR_SCALAR - intrwhite;
    int intg = intgfull;
    if (intg > INT_COLOR_SCALAR) intg = INT_COLOR_SCALAR - intgwhite;
    int intb = intbfull;
    if (intb > INT_COLOR_SCALAR) intb = INT_COLOR_SCALAR - intbwhite;
    if (!sanitize_clipping(&tgx, &tgy, &clipx, &clipy, &clipw, &cliph,
            source, target))  // clip to target
        return;
    const int clipalpha = floor(1 * INT_COLOR_SCALAR / 255);
    assert(clipalpha > 0);
    const int sourcestep = (source->hasalpha ? 4 : 3);
    const int targetstep = (target->hasalpha ? 4 : 3);
    const int maxy = tgy + cliph;
    int y = tgy;
    const int maxx = tgx + clipw;
    int x = tgx;
    assert(y < maxy && x < maxx);
    while (y < maxy) {
        int targetoffset = (y * target->w + tgx) * (
            targetstep
        );
        int sourceoffset = (
            (y - tgy + clipy) * source->w + clipx
        ) * sourcestep;
        uint8_t *writeptr = &target->pixels[targetoffset];
        uint8_t *writeptrend = (
            writeptr + (maxx - tgx) * targetstep
        );
        uint8_t *readptr = &source->pixels[sourceoffset];
        const int readptrextrastep = sourcestep - 3;
        assert(inta >= 0 && intr >= 0 && intg >= 0 && intb >= 0);
        assert(intrwhite >= 0 && intgwhite >= 0 && intbwhite >= 0);
        while (writeptr != writeptrend) {
            // XXX: assumes little endian. format is BGR/ABGR
            int alphar;
            if (likely(source->hasalpha)) {
                // Check if we can skip invisible pixels faster:
                uint8_t salpha = *(readptr + 3);
                if (likely(salpha == 0)) {
                    while (likely(writeptr != writeptrend &&
                            *(readptr + 3) == 0
                            // ^ salpha is NOT updated
                            )) {
                        readptr += sourcestep;
                        writeptr += targetstep;
                        continue;
                    }
                    continue;
                }
                alphar = (inta *
                    ((int)salpha
                        * INT_COLOR_SCALAR / 255)) / INT_COLOR_SCALAR;
                assert(alphar <= INT_COLOR_SCALAR);
            } else {
                alphar = inta;
            }
            // Regular heavy duty path:
            int reverse_alphar = (INT_COLOR_SCALAR - alphar);
            *writeptr = _assert_pix256(
                (int)(*writeptr) *
                    reverse_alphar / INT_COLOR_SCALAR +
                ((int)(*readptr) *
                    intr / INT_COLOR_SCALAR +
                    255 *
                    intrwhite / INT_COLOR_SCALAR) * alphar /
                    INT_COLOR_SCALAR
            );
            writeptr++;
            readptr++;
            *writeptr = _assert_pix256(
                (int)(*writeptr) *
                    reverse_alphar / INT_COLOR_SCALAR +
                ((int)(*readptr) *
                    intg / INT_COLOR_SCALAR +
                    255 *
                    intgwhite / INT_COLOR_SCALAR) * alphar /
                    INT_COLOR_SCALAR
            );
            writeptr++;
            readptr++;
            *writeptr = _assert_pix256(
                (int)(*writeptr) *
                    reverse_alphar / INT_COLOR_SCALAR +
                ((int)(*readptr) *
                    intb / INT_COLOR_SCALAR +
                    255 *
                    intbwhite / INT_COLOR_SCALAR) * alphar /
                    INT_COLOR_SCALAR
            );
            writeptr++;
            readptr++;
            if (likely(target->hasalpha)) {
                int a = (*writeptr) *
                    INT_COLOR_SCALAR;
                a = (INT_COLOR_SCALAR * 255 - a) * reverse_alphar /
                    INT_COLOR_SCALAR;
                a = (INT_COLOR_SCALAR * 255 - a);
                *writeptr = _assert_pix256(
                    a / INT_COLOR_SCALAR
                );
                writeptr++;
            }
            readptr += readptrextrastep;
        }
        y++;
    }
}


HOTSPOT void rfssurf_BlitScaledUncolored(
        rfssurf *restrict target, rfssurf *restrict source,
        int tgx, int tgy, int clipx, int clipy,
        int clipw, int cliph,
        double scalex, double scaley, double a
        ) {
    // Specialized blitter that can scale, but it's
    // faster due to skipping color processing.

    // See if we can fall through to a simpler blitter:
    if (round(scalex * clipw) < 1 || round(scaley * cliph) < 1)
        return;
    if ((fabs(scalex - 1) < 0.0001f &&
            fabs(scaley - 1) < 0.0001f) ||
            ((int)round(scalex * clipw) == clipw &&
            (int)round(scaley * cliph) == cliph)) {
        return rfssurf_BlitSimple(target, source,
            tgx, tgy, clipx, clipy,
            clipw, cliph);
    }
    if (!sanitize_clipping_scaled(
            &tgx, &tgy, &clipx, &clipy, &clipw, &cliph,
            source, target, scalex, scaley))  // clip to target
        return;
    assert(clipx >= 0 && clipy >= 0 &&
        clipx < source->w && clipy < source->h);
    assert(clipw >= 0 && clipy >= 0);
    assert(tgx >= 0 && tgy >= 0 &&
        tgx < target->w && tgy < target->h);
    /*printf("sanitized clip scaled gave us: "
        "unscaled: sx/sy/sw/sh %d/%d/%d/%d tx/ty %d/%d, "
        "rendertarget: w/h %d/%d, "
        "scaled: swscaled/shscaled: %f,%f\n",
        clipx, clipy, clipw, cliph, tgx, tgy,
        target->w, target->h,
        (double)clipw * scalex, (double)cliph * scaley);*/
    const int INT_COLOR_SCALAR = 1024;
    int inta = round(fmin(INT_COLOR_SCALAR,
        fmax(0, a * (double)INT_COLOR_SCALAR)));
    const int alphaed = (
        inta != INT_COLOR_SCALAR
    );

    const int clipalpha = floor(1 * INT_COLOR_SCALAR / 255);
    assert(clipalpha > 0);
    const int maxy = imin(
        tgy + round(cliph * scaley), target->h);
    assert(maxy <= target->h);
    int y = tgy;
    const int maxx = imin(
        tgx + round(clipw * scalex), target->w);
    assert(maxx <= target->w);
    const int SCALE_SCALAR = 4096;
    const int targetstep = (target->hasalpha ? 4 : 3);
    const int sourcestep = (source->hasalpha ? 4 : 3);
    const int scaledivx = round(scalex * SCALE_SCALAR);
    const int scaledivy = round(scaley * SCALE_SCALAR);
    const int scaleintx = floor(scalex);
    const int scaleintx_minus1_times_tgstep = (
        (scaleintx - 1) * targetstep);
    const int sourcehasalpha = source->hasalpha;
    const int targethasalpha = target->hasalpha;
    const int tgalphaoffset = (target->hasalpha ? 3 : 0);
    if (y >= maxy || tgx >= maxx)
        return;
    while (y < maxy) {
        int targetoffset = (y * target->w + tgx) * targetstep;
        assert(targetoffset >= 0 && targetoffset <
            target->w * target->h * (target->hasalpha ? 4 : 3));
        int sourceoffset_start = (
            ((int)imin(clipy + cliph - 1,
                ((y - tgy) * SCALE_SCALAR / scaledivy) +
                clipy)) *
            source->w + clipx
        ) * sourcestep;
        assert(inta >= 0 && inta <= INT_COLOR_SCALAR);
        uint8_t *writeptr = &target->pixels[targetoffset];
        uint8_t *writeptrstart = writeptr;
        uint8_t *writeptrend = &target->pixels[targetoffset +
            (maxx - tgx) * targetstep];
        uint64_t sourcexstart_shift = (
            ((uint64_t)imin(clipw - 1,
            0 * SCALE_SCALAR / scaledivx) << 16));
        uint64_t sourcexend_shift = (
            ((uint64_t)(imin(clipw,
                (maxx - tgx) * SCALE_SCALAR /
                scaledivx)) << 16));
        const int64_t sourcexstep_shift = (
            (int64_t)(sourcexend_shift - sourcexstart_shift) /
            (int64_t)imax(1,
            (maxx - tgx) - 1  // Correct would be - 0,
                // but due to rounding issues it looks better to
                // shoot to the right side slightly "early" on some
                // uneven scaling factors (1.5x etc).
            )
        );
        int64_t sourcex_shift = (
            sourcexstart_shift - sourcexstep_shift
        );
        if (likely(sourcehasalpha && targethasalpha)) {
            // Source WITH ALPHA, target WITH ALPHA branch.
            while (writeptr != writeptrend) {
                // XXX: assumes little endian. format is BGR/ABGR
                sourcex_shift += sourcexstep_shift;
                int sourcex = imin(clipw - 1,
                    (int64_t)sourcex_shift >> (int64_t)16);
                int sourceoffset = (
                    sourceoffset_start + sourcex * 4
                );
                uint8_t *readptr = &source->pixels[sourceoffset];
                const int alphabyte = *(readptr + 3);
                if (likely(alphabyte == 0)) {
                    // Special case where the pixel is invisible:
                    // (We fast forward through upscale pixel size)
                    writeptr += 4;
                    if (likely(scaleintx > 1)) {
                        uint8_t *innerwriteptrend = (
                            writeptr +
                            scaleintx_minus1_times_tgstep
                        );
                        innerwriteptrend = (
                            innerwriteptrend < writeptrend ?
                            innerwriteptrend : writeptrend);
                        while (writeptr != innerwriteptrend) {
                            writeptr += 4;
                            sourcex_shift += sourcexstep_shift;
                        }
                    }
                    continue;
                } else if (likely(!alphaed &&
                        alphabyte == 255)) {
                    // Special case for opaque source pixels,
                    // batching upscaled ones:
                    memcpy(writeptr, readptr, 3);
                    *(writeptr + 3) = 255;
                    writeptr += 4;
                    if (likely(scaleintx > 1)) {
                        uint8_t *innerwriteptrend = (
                            writeptr +
                            scaleintx_minus1_times_tgstep
                        );
                        innerwriteptrend = (
                            innerwriteptrend < writeptrend ?
                            innerwriteptrend : writeptrend);
                        while (writeptr != innerwriteptrend) {
                            memcpy(writeptr, readptr, 3);
                            *(writeptr + 3) = 255;
                            writeptr += 4;
                            sourcex_shift += sourcexstep_shift;
                        }
                    }
                    continue;
                }
                // Regular heavy duty blit:
                int alphar = (inta *
                    ((int)alphabyte *
                        INT_COLOR_SCALAR / 255)) /
                        INT_COLOR_SCALAR;
                assert(alphar <= INT_COLOR_SCALAR);
                int reverse_alphar = (INT_COLOR_SCALAR - alphar);
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr)) * alphar /
                        INT_COLOR_SCALAR
                ));
                writeptr++;
                readptr++;
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr)) * alphar /
                        INT_COLOR_SCALAR
                ));
                writeptr++;
                readptr++;
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr)) * alphar /
                        INT_COLOR_SCALAR
                ));
                writeptr++;
                readptr++;
                int a = (int)(*writeptr) *
                    INT_COLOR_SCALAR;
                a = (INT_COLOR_SCALAR * 255 - a) *
                    reverse_alphar /
                    INT_COLOR_SCALAR;
                a = (INT_COLOR_SCALAR * 255 - a);
                *writeptr = _assert_pix256(
                    a / INT_COLOR_SCALAR
                );
                writeptr++;
            }
        } else if (likely(sourcehasalpha && !targethasalpha)) {
            // Source WITH ALPHA, target with NO ALPHA branch.
            while (writeptr != writeptrend) {
                // XXX: assumes little endian. format is BGR/ABGR
                sourcex_shift += sourcexstep_shift;
                int sourcex = imin(clipw - 1,
                    (int64_t)sourcex_shift >> (int64_t)16);
                int sourceoffset = (
                    sourceoffset_start + sourcex * 4
                );
                uint8_t *readptr = &source->pixels[sourceoffset];
                const int alphabyte = *(readptr + 3);
                if (likely(alphabyte == 0)) {
                    // Special case where the pixel is invisible:
                    // (We fast forward through upscale pixel size)
                    writeptr += 3;
                    if (likely(scaleintx > 1)) {
                        uint8_t *innerwriteptrend = (
                            writeptr +
                            scaleintx_minus1_times_tgstep
                        );
                        innerwriteptrend = (
                            innerwriteptrend < writeptrend ?
                            innerwriteptrend : writeptrend);
                        while (writeptr != innerwriteptrend) {
                            writeptr += 3;
                            sourcex_shift += sourcexstep_shift;
                        }
                    }
                    continue;
                } else if (likely(!alphaed &&
                        alphabyte == 255)) {
                    // Special case for opaque source pixels,
                    // batching upscaled ones:
                    memcpy(writeptr, readptr, 3);
                    writeptr += 3;
                    if (likely(scaleintx > 1)) {
                        uint8_t *innerwriteptrend = (
                            writeptr +
                            scaleintx_minus1_times_tgstep
                        );
                        innerwriteptrend = (
                            innerwriteptrend < writeptrend ?
                            innerwriteptrend : writeptrend);
                        while (writeptr != innerwriteptrend) {
                            memcpy(writeptr, readptr, 3);
                            writeptr += 3;
                            sourcex_shift += sourcexstep_shift;
                        }
                    }
                    continue;
                }
                // Regular heavy duty blit:
                int alphar = (inta *
                    ((int)alphabyte *
                        INT_COLOR_SCALAR / 255)) /
                        INT_COLOR_SCALAR;
                assert(alphar <= INT_COLOR_SCALAR);
                int reverse_alphar = (INT_COLOR_SCALAR - alphar);
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr)) * alphar /
                        INT_COLOR_SCALAR
                ));
                writeptr++;
                readptr++;
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr)) * alphar /
                        INT_COLOR_SCALAR
                ));
                writeptr++;
                readptr++;
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr)) * alphar /
                        INT_COLOR_SCALAR
                ));
                writeptr++;
                readptr++;
            }
        } else {
            // Source WITHOUT alpha loop.
            while (writeptr != writeptrend) {
                // XXX: assumes little endian. format is BGR/ABGR
                sourcex_shift += sourcexstep_shift;
                int sourcex = imin(clipw - 1,
                    (int64_t)sourcex_shift >> (int64_t)16);
                int sourceoffset = (
                    sourceoffset_start + sourcex * 3
                );
                uint8_t *readptr = &source->pixels[sourceoffset];
                int alphar = inta;
                if (likely(!alphaed)) {
                    // Special case: don't need to touch
                    // color channels for pixels -> faster.
                    *(writeptr + tgalphaoffset) = 255;
                    memcpy(writeptr, readptr, 3);
                    writeptr += targetstep;
                    if (likely(scaleintx > 1)) {
                        uint8_t *innerwriteptrend = (
                            writeptr +
                            scaleintx_minus1_times_tgstep
                        );
                        innerwriteptrend = (
                            innerwriteptrend < writeptrend ?
                            innerwriteptrend : writeptrend);
                        while (writeptr != innerwriteptrend) {
                            *(writeptr + tgalphaoffset) = 255;
                            memcpy(writeptr, readptr, 3);
                            writeptr += targetstep;
                            sourcex_shift += sourcexstep_shift;
                        }
                    }
                    continue;
                }
                // Regular heavy duty blitter:
                int reverse_alphar = (INT_COLOR_SCALAR - alphar);
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr)) * alphar /
                        INT_COLOR_SCALAR));
                writeptr++;
                readptr++;
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr)) * alphar /
                        INT_COLOR_SCALAR));
                writeptr++;
                readptr++;
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr)) * alphar /
                        INT_COLOR_SCALAR));
                writeptr++;
                readptr++;
                if (likely(target->hasalpha)) {
                    int a = (int)(*writeptr) *
                        INT_COLOR_SCALAR;
                    a = (INT_COLOR_SCALAR * 255 - a) *
                        reverse_alphar /
                        INT_COLOR_SCALAR;
                    a = (INT_COLOR_SCALAR * 255 - a);
                    *writeptr = _assert_pix256(
                        a / INT_COLOR_SCALAR
                    );
                    writeptr++;
                }
            }
        }
        y++;
    }
}


HOTSPOT void rfssurf_BlitScaledIntOpaque(
        rfssurf *restrict target, rfssurf *restrict source,
        int tgx, int tgy, int clipx, int clipy, int clipw, int cliph,
        int scalex, int scaley,
        double r, double g, double b
        ) {
    // Specialized blitter that optimizes the special case where
    // the sprite is scaled up by an exact int factor, and
    // with 100% overall blit alpha.

    // Check various base conditions and fall-throughs:
    if (scalex < 1 || scaley < 1)
        return;
    if (scalex == 1 && scaley == 1) {
        return rfssurf_BlitColor(target, source,
            tgx, tgy, clipx, clipy,
            clipw, cliph, r, g, b, 1);
    }
    if (!sanitize_clipping_scaled(
            &tgx, &tgy, &clipx, &clipy, &clipw, &cliph,
            source, target, scalex, scaley))  // clip to target
        return;
    assert(clipx >= 0 && clipy >= 0 &&
        clipx < source->w && clipy < source->h);
    assert(clipw >= 0 && clipy >= 0);
    assert(tgx >= 0 && tgy >= 0 &&
        tgx < target->w && tgy < target->h);
    /*printf("sanitized clip scaled gave us: "
        "unscaled: sx/sy/sw/sh %d/%d/%d/%d tx/ty %d/%d, "
        "rendertarget: w/h %d/%d, "
        "scaled: swscaled/shscaled: %f,%f\n",
        clipx, clipy, clipw, cliph, tgx, tgy,
        target->w, target->h,
        (double)clipw * scalex, (double)cliph * scaley);*/
    const int INT_COLOR_SCALAR = 1024;
    int intrfull = round(fmin(2 * INT_COLOR_SCALAR,
        fmax(0, r * (double)INT_COLOR_SCALAR)));
    int intgfull = round(fmin(2 * INT_COLOR_SCALAR,
        fmax(0, g * (double)INT_COLOR_SCALAR)));
    int intbfull = round(fmin(2 * INT_COLOR_SCALAR,
        fmax(0, b * (double)INT_COLOR_SCALAR)));
    int intrwhite = (intrfull - INT_COLOR_SCALAR);
    if (intrwhite < 0) intrwhite = 0;
    int intgwhite = (intgfull - INT_COLOR_SCALAR);
    if (intgwhite < 0) intgwhite = 0;
    int intbwhite = (intbfull - INT_COLOR_SCALAR);
    if (intbwhite < 0) intbwhite = 0;
    int intr = intrfull;
    if (intr > INT_COLOR_SCALAR) intr = INT_COLOR_SCALAR - intrwhite;
    int intg = intgfull;
    if (intg > INT_COLOR_SCALAR) intg = INT_COLOR_SCALAR - intgwhite;
    int intb = intbfull;
    if (intb > INT_COLOR_SCALAR) intb = INT_COLOR_SCALAR - intbwhite;
    const int colored = (
        intrfull != INT_COLOR_SCALAR ||
        intgfull != INT_COLOR_SCALAR ||
        intbfull != INT_COLOR_SCALAR
    );

    const int clipalpha = floor(1 * INT_COLOR_SCALAR / 255);
    assert(clipalpha > 0);
    const int maxy = imin(
        tgy + round(cliph * scaley), target->h);
    assert(maxy <= target->h);
    int y = tgy;
    const int maxx = imin(
        tgx + round(clipw * scalex), target->w);
    assert(maxx <= target->w);
    const int SCALE_SCALAR = 4096;
    const int targetstep = (target->hasalpha ? 4 : 3);
    const int sourcestep = (source->hasalpha ? 4 : 3);
    const int scaleintx_tgstep = (
        scalex * targetstep);
    const int sourcehasalpha = source->hasalpha;
    const int targethasalpha = target->hasalpha;
    const int tgalphaoffset = (target->hasalpha ? 3 : 0);
    if (y >= maxy || tgx >= maxx)
        return;
    while (y < maxy) {
        int targetoffset = (y * target->w + tgx) * targetstep;
        assert(targetoffset >= 0 && targetoffset <
            target->w * target->h * (target->hasalpha ? 4 : 3));
        int sourceoffset_start = (
            ((int)imin(clipy + cliph - 1,
                ((y - tgy) / scaley) + clipy)) *
            source->w + clipx
        ) * sourcestep;
        assert(intr >= 0 && intg >= 0 && intb >= 0);
        assert(intrwhite >= 0 && intgwhite >= 0 && intbwhite >= 0);
        uint8_t *writeptr = &target->pixels[targetoffset];
        uint8_t *writeptrstart = writeptr;
        uint8_t *writeptrend = &target->pixels[targetoffset +
            (maxx - tgx) * targetstep];
        int sourceoffset = sourceoffset_start - sourcestep;
        if (sourcehasalpha) {
            // Branch where source HAS ALPHA.
            const int skipforwardbytes = scalex * targetstep;
            uint8_t *readptr = &source->pixels[sourceoffset];
            while (writeptr != writeptrend) {
                // XXX: assumes little endian. format is BGR/ABGR
                readptr += 4;
                const int alphabyte = (int)*(readptr + 3);
                if (likely(alphabyte == 0)) {
                    // Special case where the pixel is invisible:
                    // (handle that faster)
                    writeptr += skipforwardbytes;
                    if (writeptr > writeptrend)
                        break;
                    continue;
                } else if (likely(!colored &&
                        alphabyte == 255)) {
                    // Special case for opaque source pixels,
                    // batching it if upscaling:
                    while (1) {
                        uint8_t *innerwriteptrend = (
                            writeptr + scaleintx_tgstep
                        );
                        innerwriteptrend = (
                            innerwriteptrend < writeptrend ?
                            innerwriteptrend : writeptrend);
                        while (likely(writeptr !=
                                innerwriteptrend)) {
                            *(writeptr + tgalphaoffset) = 255;
                            memcpy(writeptr, readptr, 3);
                            writeptr += targetstep;
                        }
                        if (unlikely(writeptr == writeptrend ||
                                *(readptr + 4 + 3) != 255)) {
                            break;
                        }
                        readptr += 4;
                    }
                    continue;
                }
                int alphar = (((int)alphabyte *
                    INT_COLOR_SCALAR / 255));
                assert(alphar <= INT_COLOR_SCALAR);
                int reverse_alphar = (INT_COLOR_SCALAR - alphar);
                int sourcecolor_r = (((int)(*readptr) *
                    intr / INT_COLOR_SCALAR +
                    255 *
                    intrwhite / INT_COLOR_SCALAR) *
                    alphar / INT_COLOR_SCALAR);
                readptr++;
                int sourcecolor_g = (((int)(*readptr) *
                    intg / INT_COLOR_SCALAR +
                    255 *
                    intgwhite / INT_COLOR_SCALAR) *
                    alphar / INT_COLOR_SCALAR);
                readptr++;
                int sourcecolor_b = (((int)(*readptr) *
                    intb / INT_COLOR_SCALAR +
                    255 *
                    intbwhite / INT_COLOR_SCALAR) *
                    alphar / INT_COLOR_SCALAR);
                readptr -= 2;
                int i = 0;
                while (i < scalex) {
                    *writeptr = _assert_pix256((
                        (int)(*writeptr) *
                            reverse_alphar / INT_COLOR_SCALAR +
                        sourcecolor_r));
                    writeptr++;
                    *writeptr = _assert_pix256((
                        (int)(*writeptr) *
                            reverse_alphar / INT_COLOR_SCALAR +
                        sourcecolor_g));
                    writeptr++;
                    *writeptr = _assert_pix256((
                        (int)(*writeptr) *
                            reverse_alphar / INT_COLOR_SCALAR +
                        sourcecolor_b));
                    writeptr++;
                    if (likely(targethasalpha)) {
                        int a = (int)(*writeptr) *
                            INT_COLOR_SCALAR;
                        a = (INT_COLOR_SCALAR * 255 - a) *
                            reverse_alphar /
                            INT_COLOR_SCALAR;
                        a = (INT_COLOR_SCALAR * 255 - a);
                        *writeptr = _assert_pix256(
                            a / INT_COLOR_SCALAR
                        );
                        writeptr++;
                    }
                    i++;
                }
            }
        } else {
            // Branch where source has NO ALPHA.
            while (writeptr != writeptrend) {
                // XXX: assumes little endian. format is BGR/ABGR
                sourceoffset += 3;
                uint8_t *readptr = &source->pixels[sourceoffset];
                const int alphabyte = 255;
                if (likely(!colored)) {
                    // If the blit isn't colored, we can fast-forward
                    // everything since we don't need any blending:
                    uint8_t *innerwriteptrend = (
                        writeptr + scaleintx_tgstep
                    );
                    innerwriteptrend = (
                        innerwriteptrend < writeptrend ?
                        innerwriteptrend : writeptrend);
                    assert(scalex > 0);
                    while (likely(writeptr != innerwriteptrend)) {
                        *(writeptr + tgalphaoffset) = 255;
                        memcpy(writeptr, readptr, 3);
                        writeptr += targetstep;
                    }
                    continue;
                }
                int alphar = INT_COLOR_SCALAR;
                int reverse_alphar = 0;
                int i = 0;
                while (i < scalex) {
                    *(writeptr + tgalphaoffset) = 255;
                    *writeptr = _assert_pix256((
                        ((int)(*readptr) *
                            intr / INT_COLOR_SCALAR +
                            255 *
                            intrwhite / INT_COLOR_SCALAR)));
                    writeptr++;
                    readptr++;
                    *writeptr = _assert_pix256((
                        ((int)(*readptr) *
                            intg / INT_COLOR_SCALAR +
                            255 *
                            intgwhite / INT_COLOR_SCALAR)));
                    writeptr++;
                    readptr++;
                    *writeptr = _assert_pix256((
                        ((int)(*readptr) *
                            intb / INT_COLOR_SCALAR +
                            255 *
                            intbwhite / INT_COLOR_SCALAR)));
                    writeptr++;
                    readptr -= 2;
                    i++;
                }
            }
       }
       y++;
    }
}


HOTSPOT void rfssurf_BlitScaledBeyond2X(
        rfssurf *restrict target, rfssurf *restrict source,
        int tgx, int tgy, int clipx, int clipy, int clipw, int cliph,
        double scalex, double scaley,
        double r, double g, double b, double a
        ) {
    // Almost the fully generalized blitter, except that
    // the blit must be scaling up 2x or higher on all axes.
    // This allows skipping one branch for higher speed: while
    // that may seem ridiculous to near-duplicate the function
    // for, colored/alpha sprites with >2x scale are THE common
    // disaster case for ingame sprites.
    // So we desperately want to optimize this all we can.

    // Base sanity checks:
    assert(scalex >= 2 && scaley >= 2);
    if (!sanitize_clipping_scaled(
            &tgx, &tgy, &clipx, &clipy, &clipw, &cliph,
            source, target, scalex, scaley))  // clip to target
        return;
    assert(clipx >= 0 && clipy >= 0 &&
        clipx < source->w && clipy < source->h);
    assert(clipw >= 0 && clipy >= 0);
    assert(tgx >= 0 && tgy >= 0 &&
        tgx < target->w && tgy < target->h);
    /*printf("sanitized clip scaled gave us: "
        "unscaled: sx/sy/sw/sh %d/%d/%d/%d tx/ty %d/%d, "
        "rendertarget: w/h %d/%d, "
        "scaled: swscaled/shscaled: %f,%f\n",
        clipx, clipy, clipw, cliph, tgx, tgy,
        target->w, target->h,
        (double)clipw * scalex, (double)cliph * scaley);*/
    const int INT_COLOR_SCALAR = 1024;
    assert(INT_COLOR_SCALAR >= 2 &&
        !math_isnpot(INT_COLOR_SCALAR));
    int intrfull = round(fmin(2 * INT_COLOR_SCALAR,
        fmax(0, r * (double)INT_COLOR_SCALAR)));
    int intgfull = round(fmin(2 * INT_COLOR_SCALAR,
        fmax(0, g * (double)INT_COLOR_SCALAR)));
    int intbfull = round(fmin(2 * INT_COLOR_SCALAR,
        fmax(0, b * (double)INT_COLOR_SCALAR)));
    int inta = round(fmin(INT_COLOR_SCALAR,
        fmax(0, a * (double)INT_COLOR_SCALAR)));
    int intrwhite = (intrfull - INT_COLOR_SCALAR);
    if (intrwhite < 0) intrwhite = 0;
    int intgwhite = (intgfull - INT_COLOR_SCALAR);
    if (intgwhite < 0) intgwhite = 0;
    int intbwhite = (intbfull - INT_COLOR_SCALAR);
    if (intbwhite < 0) intbwhite = 0;
    int intr = intrfull;
    if (intr > INT_COLOR_SCALAR)
        intr = INT_COLOR_SCALAR - intrwhite;
    int intg = intgfull;
    if (intg > INT_COLOR_SCALAR)
        intg = INT_COLOR_SCALAR - intgwhite;
    int intb = intbfull;
    if (intb > INT_COLOR_SCALAR)
        intb = INT_COLOR_SCALAR - intbwhite;

    const int clipalpha = floor(1 * INT_COLOR_SCALAR / 255);
    assert(clipalpha > 0);
    const int maxy = imin(
        tgy + round(cliph * scaley), target->h);
    assert(maxy <= target->h);
    int y = tgy;
    const int maxx = imin(
        tgx + round(clipw * scalex), target->w);
    assert(maxx <= target->w);
    const int SCALE_SCALAR = 4096;
    const int scaledivx = round(scalex * SCALE_SCALAR);
    const int scaledivy = round(scaley * SCALE_SCALAR);
    const int scaleintx = floor(scalex);
    assert(scaleintx >= 2);
    const int targetstep = (target->hasalpha ? 4 : 3);
    const int sourcestep = (source->hasalpha ? 4 : 3);
    const int scaleintx_times_tgstep = (
        scaleintx * targetstep);
    const int scaleintx_minus1_times_tgstep = (
        (scaleintx - 1) * targetstep);
    const int sourcehasalpha = source->hasalpha;
    const int targethasalpha = target->hasalpha;
    const int tgalphaoffset = (target->hasalpha ? 3 : 0);
    if (y >= maxy || tgx >= maxx)
        return;
    while (y < maxy) {
        int targetoffset = (
            (y * target->w + tgx) * targetstep);
        assert(targetoffset >= 0 && targetoffset <
            target->w * target->h *
            (target->hasalpha ? 4 : 3));
        int sourceoffset_start = (
            ((int)imin(clipy + cliph - 1,
                ((y - tgy) * SCALE_SCALAR / scaledivy) +
                clipy)) *
            source->w + clipx
        ) * sourcestep;
        assert(inta >= 0 && intr >= 0 && intg >= 0 && intb >= 0);
        assert(inta <= INT_COLOR_SCALAR);
        assert(intrwhite >= 0 && intgwhite >= 0 &&
               intbwhite >= 0);
        uint8_t *writeptr = &target->pixels[targetoffset];
        uint8_t *writeptrstart = writeptr;
        uint8_t *writeptrend = &target->pixels[targetoffset +
            (maxx - tgx) * targetstep];
        uint64_t sourcexstart_shift = (
            ((uint64_t)imin(clipw - 1,
            0 * SCALE_SCALAR / scaledivx) << 16));
        uint64_t sourcexend_shift = (
            ((uint64_t)(imin(clipw,
                (maxx - tgx) * SCALE_SCALAR /
                scaledivx)) << 16));
        const int64_t sourcexstep_shift = (
            (int64_t)(sourcexend_shift - sourcexstart_shift) /
            (int64_t)imax(1,
            (maxx - tgx) - 1  // Correct would be - 0,
                // but due to rounding issues it looks better to
                // shoot to the right side slightly "early" on some
                // uneven scaling factors (1.5x etc).
            ));
        int64_t sourcex_shift = (
            sourcexstart_shift - sourcexstep_shift
        );
        if (likely(sourcehasalpha &&
                targethasalpha)) {
            // Source WITH ALPHA, target WITH ALPHA branch.
            while (writeptr != writeptrend) {
                // XXX: assumes little endian. format is BGR/ABGR
                sourcex_shift += sourcexstep_shift;
                int sourcex = imin(clipw - 1,
                    (int64_t)sourcex_shift >> (int64_t)16);
                int sourceoffset = (
                    sourceoffset_start + sourcex * 4
                );
                uint8_t *readptr = &source->pixels[sourceoffset];
                const int alphabyte = *(readptr + 3);
                if (likely(alphabyte == 0)) {
                    // Special case where the pixel is invisible:
                    // (We fast forward through upscale pixel size)
                    writeptr += 4;
                    uint8_t *innerwriteptrend = (
                        writeptr +
                        scaleintx_minus1_times_tgstep
                    );
                    innerwriteptrend = (
                        innerwriteptrend < writeptrend ?
                        innerwriteptrend : writeptrend);
                    while (writeptr != innerwriteptrend) {
                        writeptr += 4;
                        sourcex_shift += sourcexstep_shift;
                    }
                    continue;
                }
                // NOTE: no special case for alpha=255, since
                // we got guaranteed color tint or we would have
                // gone into rfssurf_BlitScaledUncolored instead.

                // Regular heavy duty blitting:
                int alphar = (inta * ((int)alphabyte *
                        INT_COLOR_SCALAR / 255)) /
                        INT_COLOR_SCALAR;
                assert(alphar <= INT_COLOR_SCALAR);
                int reverse_alphar = (INT_COLOR_SCALAR - alphar);
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr) *
                        intr / INT_COLOR_SCALAR +
                        255 *
                        intrwhite / INT_COLOR_SCALAR) *
                        alphar / INT_COLOR_SCALAR));
                writeptr++;
                readptr++;
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr) *
                        intg / INT_COLOR_SCALAR +
                        255 *
                        intgwhite / INT_COLOR_SCALAR) *
                        alphar / INT_COLOR_SCALAR));
                writeptr++;
                readptr++;
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr) *
                        intb / INT_COLOR_SCALAR +
                        255 *
                        intbwhite / INT_COLOR_SCALAR) *
                        alphar / INT_COLOR_SCALAR));
                writeptr++;
                readptr++;
                int a = (int)(*writeptr) *
                    INT_COLOR_SCALAR;
                a = (INT_COLOR_SCALAR * 255 - a) *
                    reverse_alphar /
                    INT_COLOR_SCALAR;
                a = (INT_COLOR_SCALAR * 255 - a);
                *writeptr = _assert_pix256(
                    a / INT_COLOR_SCALAR
                );
                writeptr++;
            }
        } else if (likely(sourcehasalpha &&
                !targethasalpha)) {
            // Source WITH ALPHA, target with NO ALPHA branch.
            while (writeptr != writeptrend) {
                // XXX: assumes little endian. format is BGR/ABGR
                sourcex_shift += sourcexstep_shift;
                int sourcex = imin(clipw - 1,
                    (int64_t)sourcex_shift >> (int64_t)16);
                int sourceoffset = (
                    sourceoffset_start + sourcex * 4
                );
                uint8_t *readptr = &source->pixels[sourceoffset];
                const int alphabyte = *(readptr + 3);
                if (likely(alphabyte == 0)) {
                    // Special case where the pixel is invisible:
                    // (We fast forward through upscale pixel size)
                    writeptr += 3;
                    uint8_t *innerwriteptrend = (
                        writeptr +
                        scaleintx_minus1_times_tgstep
                    );
                    innerwriteptrend = (
                        innerwriteptrend < writeptrend ?
                        innerwriteptrend : writeptrend);
                    while (writeptr != innerwriteptrend) {
                        writeptr += 3;
                        sourcex_shift += sourcexstep_shift;
                    }
                    continue;
                }
                // NOTE: as explained in loop above, no 255 case.
                // Regular heavy duty blit:
                int alphar = (inta * ((int)alphabyte *
                        INT_COLOR_SCALAR / 255)) /
                        INT_COLOR_SCALAR;
                assert(alphar <= INT_COLOR_SCALAR);
                int reverse_alphar = (INT_COLOR_SCALAR - alphar);
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr) *
                        intr / INT_COLOR_SCALAR +
                        255 *
                        intrwhite / INT_COLOR_SCALAR) *
                        alphar / INT_COLOR_SCALAR
                ));
                writeptr++;
                readptr++;
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr) *
                        intg / INT_COLOR_SCALAR +
                        255 *
                        intgwhite / INT_COLOR_SCALAR) *
                        alphar / INT_COLOR_SCALAR
                ));
                writeptr++;
                readptr++;
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr) *
                        intb / INT_COLOR_SCALAR +
                        255 *
                        intbwhite / INT_COLOR_SCALAR) *
                        alphar / INT_COLOR_SCALAR
                ));
                writeptr++;
                readptr++;
            }
        } else {
            // Source WITHOUT alpha loop.
            while (writeptr != writeptrend) {
                // XXX: assumes little endian. format is BGR/ABGR
                sourcex_shift += sourcexstep_shift;
                int sourcex = imin(clipw - 1,
                    (int64_t)sourcex_shift >> (int64_t)16);
                int sourceoffset = (
                    sourceoffset_start + sourcex * 3
                );
                uint8_t *readptr = &source->pixels[sourceoffset];
                int alphar = inta;
                // Note: since source has no alpha and we always
                // have tinting here, there are no good fast paths.

                // Regular heavy duty blit:
                int reverse_alphar = (INT_COLOR_SCALAR - alphar);
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr) *
                        intr / INT_COLOR_SCALAR +
                        255 *
                        intrwhite / INT_COLOR_SCALAR) *
                        alphar / INT_COLOR_SCALAR
                ));
                writeptr++;
                readptr++;
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr) *
                        intg / INT_COLOR_SCALAR +
                        255 *
                        intgwhite / INT_COLOR_SCALAR) *
                        alphar / INT_COLOR_SCALAR
                ));
                writeptr++;
                readptr++;
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr) *
                        intb / INT_COLOR_SCALAR +
                        255 *
                        intbwhite / INT_COLOR_SCALAR) *
                        alphar / INT_COLOR_SCALAR
                ));
                writeptr++;
                readptr++;
                if (likely(target->hasalpha)) {
                    int a = (int)(*writeptr) *
                        INT_COLOR_SCALAR;
                    a = (INT_COLOR_SCALAR * 255 - a) *
                        reverse_alphar /
                        INT_COLOR_SCALAR;
                    a = (INT_COLOR_SCALAR * 255 - a);
                    *writeptr = _assert_pix256(
                        a / INT_COLOR_SCALAR
                    );
                    writeptr++;
                }
            }
        }
        y++;
    }
}


HOTSPOT void rfssurf_BlitScaled(
        rfssurf *restrict target, rfssurf *restrict source,
        int tgx, int tgy, int clipx, int clipy, int clipw, int cliph,
        double scalex, double scaley,
        double r, double g, double b, double a
        ) {
    // Most generalized, but slowest blitter that can handle
    // scaling, coloring, alpha, and everything.

    // Base checks and fall-through to simpler blitters:
    if (round(scalex * clipw) < 1 ||
            round(scaley * cliph) < 1)
        return;
    if ((fabs(scalex - 1) < 0.0001f &&
            fabs(scaley - 1) < 0.0001f) ||
            ((int)round(scalex * clipw) == clipw &&
            (int)round(scaley * cliph) == cliph)) {
        return rfssurf_BlitColor(target, source,
            tgx, tgy, clipx, clipy,
            clipw, cliph, r, g, b, a);
    }
    if (fabs(round(scalex) - scalex) < 0.001f &&
            fabs(round(scaley) - scaley) < 0.001f &&
            fabs(a - 1) < 0.01f) {
        return rfssurf_BlitScaledIntOpaque(
            target, source,
            tgx, tgy, clipx, clipy, clipw, cliph,
            scalex, scaley, r, g, b
        );
    }
    if (round(r * 255) == 255 && round(g * 255) == 255 &&
            round(b * 255) == 255) {
        return rfssurf_BlitScaledUncolored(
            target, source,
            tgx, tgy, clipx, clipy, clipw, cliph,
            scalex, scaley, a);
    }
    if (scalex >= 2 && scaley >= 2) {
        return rfssurf_BlitScaledBeyond2X(
            target, source,
            tgx, tgy, clipx, clipy, clipw, cliph,
            scalex, scaley, r, g, b, a);
    }
    if (!sanitize_clipping_scaled(
            &tgx, &tgy, &clipx, &clipy, &clipw, &cliph,
            source, target, scalex, scaley))  // clip to target
        return;
    assert(clipx >= 0 && clipy >= 0 &&
        clipx < source->w && clipy < source->h);
    assert(clipw >= 0 && clipy >= 0);
    assert(tgx >= 0 && tgy >= 0 &&
        tgx < target->w && tgy < target->h);
    /*printf("sanitized clip scaled gave us: "
        "unscaled: sx/sy/sw/sh %d/%d/%d/%d tx/ty %d/%d, "
        "rendertarget: w/h %d/%d, "
        "scaled: swscaled/shscaled: %f,%f\n",
        clipx, clipy, clipw, cliph, tgx, tgy,
        target->w, target->h,
        (double)clipw * scalex, (double)cliph * scaley);*/
    const int INT_COLOR_SCALAR = 1024;
    assert(INT_COLOR_SCALAR >= 2 &&
        !math_isnpot(INT_COLOR_SCALAR));
    int intrfull = round(fmin(2 * INT_COLOR_SCALAR,
        fmax(0, r * (double)INT_COLOR_SCALAR)));
    int intgfull = round(fmin(2 * INT_COLOR_SCALAR,
        fmax(0, g * (double)INT_COLOR_SCALAR)));
    int intbfull = round(fmin(2 * INT_COLOR_SCALAR,
        fmax(0, b * (double)INT_COLOR_SCALAR)));
    int inta = round(fmin(INT_COLOR_SCALAR,
        fmax(0, a * (double)INT_COLOR_SCALAR)));
    int intrwhite = (intrfull - INT_COLOR_SCALAR);
    if (intrwhite < 0) intrwhite = 0;
    int intgwhite = (intgfull - INT_COLOR_SCALAR);
    if (intgwhite < 0) intgwhite = 0;
    int intbwhite = (intbfull - INT_COLOR_SCALAR);
    if (intbwhite < 0) intbwhite = 0;
    int intr = intrfull;
    if (intr > INT_COLOR_SCALAR)
        intr = INT_COLOR_SCALAR - intrwhite;
    int intg = intgfull;
    if (intg > INT_COLOR_SCALAR)
        intg = INT_COLOR_SCALAR - intgwhite;
    int intb = intbfull;
    if (intb > INT_COLOR_SCALAR)
        intb = INT_COLOR_SCALAR - intbwhite;

    const int clipalpha = floor(1 * INT_COLOR_SCALAR / 255);
    assert(clipalpha > 0);
    const int maxy = imin(
        tgy + round(cliph * scaley), target->h);
    assert(maxy <= target->h);
    int y = tgy;
    const int maxx = imin(
        tgx + round(clipw * scalex), target->w);
    assert(maxx <= target->w);
    const int SCALE_SCALAR = 4096;
    const int scaledivx = round(scalex * SCALE_SCALAR);
    const int scaledivy = round(scaley * SCALE_SCALAR);
    const int scaleintx = floor(scalex);
    const int targetstep = (target->hasalpha ? 4 : 3);
    const int sourcestep = (source->hasalpha ? 4 : 3);
    const int scaleintx_times_tgstep = (
        scaleintx * targetstep);
    const int scaleintx_minus1_times_tgstep = (
        (scaleintx - 1) * targetstep);
    const int sourcehasalpha = source->hasalpha;
    const int targethasalpha = target->hasalpha;
    const int tgalphaoffset = (target->hasalpha ? 3 : 0);
    if (y >= maxy || tgx >= maxx)
        return;
    while (y < maxy) {
        int targetoffset = (
            (y * target->w + tgx) * targetstep);
        assert(targetoffset >= 0 && targetoffset <
            target->w * target->h *
            (target->hasalpha ? 4 : 3));
        int sourceoffset_start = (
            ((int)imin(clipy + cliph - 1,
                ((y - tgy) * SCALE_SCALAR / scaledivy) +
                clipy)) *
            source->w + clipx
        ) * sourcestep;
        assert(inta >= 0 && intr >= 0 && intg >= 0 && intb >= 0);
        assert(inta <= INT_COLOR_SCALAR);
        assert(intrwhite >= 0 && intgwhite >= 0 &&
               intbwhite >= 0);
        uint8_t *writeptr = &target->pixels[targetoffset];
        uint8_t *writeptrstart = writeptr;
        uint8_t *writeptrend = &target->pixels[targetoffset +
            (maxx - tgx) * targetstep];
        uint64_t sourcexstart_shift = (
            ((uint64_t)imin(clipw - 1,
            0 * SCALE_SCALAR / scaledivx) << 16));
        uint64_t sourcexend_shift = (
            ((uint64_t)(imin(clipw,
                (maxx - tgx) * SCALE_SCALAR /
                scaledivx)) << 16));
        const int64_t sourcexstep_shift = (
            (int64_t)(sourcexend_shift - sourcexstart_shift) /
            (int64_t)imax(1,
            (maxx - tgx) - 1  // Correct would be - 0,
                // but due to rounding issues it looks better to
                // shoot to the right side slightly "early" on some
                // uneven scaling factors (1.5x etc).
            )
        );
        int64_t sourcex_shift = (
            sourcexstart_shift - sourcexstep_shift
        );
        if (likely(sourcehasalpha &&
                targethasalpha)) {
            // Source WITH ALPHA, target WITH ALPHA branch.
            #pragma GCC unroll 0
            while (writeptr != writeptrend) {
                // XXX: assumes little endian. format is BGR/ABGR
                sourcex_shift += sourcexstep_shift;
                int sourcex = imin(clipw - 1,
                    (int64_t)sourcex_shift >> (int64_t)16);
                int sourceoffset = (
                    sourceoffset_start + sourcex * 4
                );
                uint8_t *readptr = &source->pixels[sourceoffset];
                const int alphabyte = *(readptr + 3);
                if (likely(alphabyte == 0)) {
                    // Special case where the pixel is invisible:
                    // (We fast forward through upscale pixel size)
                    writeptr += 4;
                    if (likely(scaleintx > 1)) {
                        uint8_t *innerwriteptrend = (
                            writeptr +
                            scaleintx_minus1_times_tgstep
                        );
                        innerwriteptrend = (
                            innerwriteptrend < writeptrend ?
                            innerwriteptrend : writeptrend);
                        #pragma GCC unroll 2
                        while (writeptr != innerwriteptrend) {
                            writeptr += 4;
                            sourcex_shift += sourcexstep_shift;
                        }
                    }
                    continue;
                }
                // NOTE: no special case for alpha=255, since
                // we got guaranteed color tint or we would have
                // gone into rfssurf_BlitScaledUncolored instead.

                // Regular heavy duty blitting:
                int alphar = (inta * ((int)alphabyte *
                        INT_COLOR_SCALAR / 255)) /
                        INT_COLOR_SCALAR;
                assert(alphar <= INT_COLOR_SCALAR);
                int reverse_alphar = (INT_COLOR_SCALAR - alphar);
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr) *
                        intr / INT_COLOR_SCALAR +
                        255 *
                        intrwhite / INT_COLOR_SCALAR) *
                        alphar / INT_COLOR_SCALAR));
                writeptr++;
                readptr++;
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr) *
                        intg / INT_COLOR_SCALAR +
                        255 *
                        intgwhite / INT_COLOR_SCALAR) *
                        alphar / INT_COLOR_SCALAR));
                writeptr++;
                readptr++;
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr) *
                        intb / INT_COLOR_SCALAR +
                        255 *
                        intbwhite / INT_COLOR_SCALAR) *
                        alphar / INT_COLOR_SCALAR));
                writeptr++;
                readptr++;
                int a = (int)(*writeptr) *
                    INT_COLOR_SCALAR;
                a = (INT_COLOR_SCALAR * 255 - a) *
                    reverse_alphar /
                    INT_COLOR_SCALAR;
                a = (INT_COLOR_SCALAR * 255 - a);
                *writeptr = _assert_pix256(
                    a / INT_COLOR_SCALAR
                );
                writeptr++;
            }
        } else if (likely(sourcehasalpha &&
                !targethasalpha)) {
            // Source WITH ALPHA, target with NO ALPHA branch.
            #pragma GCC unroll 0
            while (writeptr != writeptrend) {
                // XXX: assumes little endian. format is BGR/ABGR
                sourcex_shift += sourcexstep_shift;
                int sourcex = imin(clipw - 1,
                    (int64_t)sourcex_shift >> (int64_t)16);
                int sourceoffset = (
                    sourceoffset_start + sourcex * 4
                );
                uint8_t *readptr = &source->pixels[sourceoffset];
                const int alphabyte = *(readptr + 3);
                if (likely(alphabyte == 0)) {
                    // Special case where the pixel is invisible:
                    // (We fast forward through upscale pixel size)
                    writeptr += 3;
                    if (likely(scaleintx > 1)) {
                        uint8_t *innerwriteptrend = (
                            writeptr +
                            scaleintx_minus1_times_tgstep
                        );
                        innerwriteptrend = (
                            innerwriteptrend < writeptrend ?
                            innerwriteptrend : writeptrend);
                        #pragma GCC unroll 2
                        while (writeptr != innerwriteptrend) {
                            writeptr += 3;
                            sourcex_shift += sourcexstep_shift;
                        }
                    }
                    continue;
                }
                // NOTE: as explained in loop above, no 255 case.
                // Regular heavy duty blit:
                int alphar = (inta * ((int)alphabyte *
                        INT_COLOR_SCALAR / 255)) /
                        INT_COLOR_SCALAR;
                assert(alphar <= INT_COLOR_SCALAR);
                int reverse_alphar = (INT_COLOR_SCALAR - alphar);
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr) *
                        intr / INT_COLOR_SCALAR +
                        255 *
                        intrwhite / INT_COLOR_SCALAR) *
                        alphar / INT_COLOR_SCALAR
                ));
                writeptr++;
                readptr++;
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr) *
                        intg / INT_COLOR_SCALAR +
                        255 *
                        intgwhite / INT_COLOR_SCALAR) *
                        alphar / INT_COLOR_SCALAR
                ));
                writeptr++;
                readptr++;
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr) *
                        intb / INT_COLOR_SCALAR +
                        255 *
                        intbwhite / INT_COLOR_SCALAR) *
                        alphar / INT_COLOR_SCALAR
                ));
                writeptr++;
                readptr++;
            }
        } else {
            // Source WITHOUT alpha loop.
            #pragma GCC unroll 0
            while (writeptr != writeptrend) {
                // XXX: assumes little endian. format is BGR/ABGR
                sourcex_shift += sourcexstep_shift;
                int sourcex = imin(clipw - 1,
                    (int64_t)sourcex_shift >> (int64_t)16);
                int sourceoffset = (
                    sourceoffset_start + sourcex * 3
                );
                uint8_t *readptr = &source->pixels[sourceoffset];
                int alphar = inta;
                // Note: since source has no alpha and we always
                // have tinting here, there are no good fast paths.

                // Regular heavy duty blit:
                int reverse_alphar = (INT_COLOR_SCALAR - alphar);
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr) *
                        intr / INT_COLOR_SCALAR +
                        255 *
                        intrwhite / INT_COLOR_SCALAR) *
                        alphar / INT_COLOR_SCALAR
                ));
                writeptr++;
                readptr++;
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr) *
                        intg / INT_COLOR_SCALAR +
                        255 *
                        intgwhite / INT_COLOR_SCALAR) *
                        alphar / INT_COLOR_SCALAR
                ));
                writeptr++;
                readptr++;
                *writeptr = _assert_pix256((
                    (int)(*writeptr) *
                        reverse_alphar / INT_COLOR_SCALAR +
                    ((int)(*readptr) *
                        intb / INT_COLOR_SCALAR +
                        255 *
                        intbwhite / INT_COLOR_SCALAR) *
                        alphar / INT_COLOR_SCALAR
                ));
                writeptr++;
                readptr++;
                if (likely(target->hasalpha)) {
                    int a = (int)(*writeptr) *
                        INT_COLOR_SCALAR;
                    a = (INT_COLOR_SCALAR * 255 - a) *
                        reverse_alphar /
                        INT_COLOR_SCALAR;
                    a = (INT_COLOR_SCALAR * 255 - a);
                    *writeptr = _assert_pix256(
                        a / INT_COLOR_SCALAR
                    );
                    writeptr++;
                }
            }
        }
        y++;
    }
}


void rfssurf_Clear(rfssurf *surf) {
    if (surf->w <= 0 || surf->h <= 0)
        return;
    memset(
        surf->pixels, 0,
        surf->w * surf->h * (surf->hasalpha ? 4 : 3)
    );
}


#if defined(HAVE_SDL)
HOTSPOT SDL_Surface *rfssurf_AsSrf(
        rfssurf *srf, int withalpha) {
    if (srf->sdlsrf && (srf->sdlsrf->w != srf->w ||
            srf->sdlsrf->h != srf->h ||
            srf->sdlsrf_hasalpha != withalpha)) {
        SDL_FreeSurface(srf->sdlsrf);
        srf->sdlsrf = NULL;
    }
    if (!srf->sdlsrf) {
        srf->sdlsrf = SDL_CreateRGBSurfaceWithFormat(
            0, srf->w, srf->h, 32, (
                withalpha ? SDL_PIXELFORMAT_ABGR8888 :
                SDL_PIXELFORMAT_XBGR8888
            )
        );
        srf->sdlsrf_hasalpha = withalpha;
    }
    if (!srf->sdlsrf)
        return NULL;
    SDL_LockSurface(srf->sdlsrf);
    if (unlikely(srf->sdlsrf->pitch == 4 * srf->w &&
            srf->sdlsrf_hasalpha && srf->hasalpha)) {
        // Super fast path (copies entire surface in one)
        memcpy(srf->sdlsrf->pixels, srf->pixels,
               4 * srf->w * srf->h);
        SDL_UnlockSurface(srf->sdlsrf);
        return srf->sdlsrf;
    }
    char *p = (char *)srf->sdlsrf->pixels;
    const int width = srf->w;
    const int height = srf->h;
    const int sourcestepsize = (srf->hasalpha ? 4 : 3);
    const int srfhasalpha = srf->hasalpha;
    const uint8_t *srfpixels = srf->pixels;
    const int sdlsrfpitch = srf->sdlsrf->pitch;
    int sourceoffset = 0;
    #pragma GCC unroll 0
    for (int y = 0; y < height; y++) {
        int x = 0;
        char *pnow = p;
        char *maxp = p + (4 * width);
        if (likely(srfhasalpha && withalpha)) {
            // Fast path (copies entire row in one):
            memcpy(pnow, srfpixels,
                4 * width);
            srfpixels += sourcestepsize * width;
            continue;
        }
        while (likely(pnow != maxp)) {
            // Slow path:
            memcpy(pnow, srfpixels, 3);
            // XXX: SDL2's XBGR still has this unused alpha byte:
            pnow += 3;
            pnow[3] = 255;
            pnow++;
            srfpixels += sourcestepsize;
        }
        p += sdlsrfpitch;
    }
    SDL_UnlockSurface(srf->sdlsrf);
    return srf->sdlsrf;
}


SDL_Texture *rfssurf_AsTex_Update(
        SDL_Renderer *r, rfssurf *srf, int withalpha,
        SDL_Texture *updateto
        ) {
    SDL_Texture *t = (
        updateto ? updateto : SDL_CreateTexture(
        r, (withalpha ? SDL_PIXELFORMAT_RGBA8888 :
        SDL_PIXELFORMAT_RGB24), SDL_TEXTUREACCESS_STREAMING,
        srf->w, srf->h));
    if (!t) {
        return NULL;
    }
    int pitch = 0;
    uint8_t *tpixels = NULL;
    if (SDL_LockTexture(t, NULL,
            (void **)&tpixels, &pitch) < 0) {
        if (t != updateto)
            SDL_DestroyTexture(t);
        return NULL;
    }
    uint8_t *readptr = srf->pixels;
    int readstep = (srf->hasalpha ? 4 : 3);
    int writestep = (withalpha ? 4 : 3);
    uint8_t *readptrend = &srf->pixels[
        srf->w * srf->h * readstep
    ];
    if (withalpha == srf->hasalpha) {
        const int rowlen = (srf->w * readstep);
        if (pitch == rowlen) {
            memcpy(tpixels, readptr, rowlen * srf->h);
        } else {
            while (readptr != readptrend) {
                memcpy(tpixels, readptr, rowlen);
                tpixels += pitch;
                readptr += rowlen;
            }
        }
    } else {
        while (readptr != readptrend) {
            memcpy(tpixels, readptr, 3);
            if (withalpha) tpixels[3] = 255;
            tpixels += writestep;
            readptr += readstep;
        }
    }
    SDL_UnlockTexture(t);
    return t;
}


SDL_Texture *rfssurf_AsTex(
        SDL_Renderer *r, rfssurf *srf, int withalpha
        ) {
    return rfssurf_AsTex_Update(r, srf, withalpha, NULL);
}
#endif

