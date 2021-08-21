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
        clipw = ceil((target->w - tgx) / scalex);
    if (tgy + floor(cliph * scaley) > target->h)
        cliph = ceil((target->h - tgy) / scaley);
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
                target->pixels[targetoffset + 0] = math_pixcliptop(
                    ((int)target->pixels[targetoffset + 0] *
                        reverse_alphar / INT_COLOR_SCALAR) +
                    (ir * alphar / INT_COLOR_SCALAR)
                );
                target->pixels[targetoffset + 1] = math_pixcliptop(
                    ((int)target->pixels[targetoffset + 1] *
                        reverse_alphar / INT_COLOR_SCALAR) +
                    (ig * alphar / INT_COLOR_SCALAR)
                );
                target->pixels[targetoffset + 2] = math_pixcliptop(
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
                    target->pixels[targetoffset + 3] = math_pixcliptop(
                        a / INT_COLOR_SCALAR
                    );
                }
                cx++;
                targetoffset += copylen;
            }
            cy++;
        }
    }
}

HOTSPOT void rfssurf_BlitSimple(rfssurf *target, rfssurf *source,
        int tgx, int tgy, int clipx, int clipy, int clipw, int cliph
        ) {
    if (!sanitize_clipping(&tgx, &tgy, &clipx, &clipy, &clipw, &cliph,
            source, target))
        return;
    const int targetstep = (target->hasalpha ? 4 : 3);
    const int sourcestep = (source->hasalpha ? 4 : 3);
    const int tghasalpha = target->hasalpha;
    const int sourcehasalpha = source->hasalpha;
    const int INT_COLOR_SCALAR = 1024;
    const int clipalpha = floor(1 * INT_COLOR_SCALAR / 255);
    assert(clipalpha > 0);
    const int alphaindex = (target->hasalpha ? 3 : 0);
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
        x = tgx;
        while (x < maxx) {
            // XXX: assumes little endian. format is BGR/ABGR
            int alphar;
            if (likely(sourcehasalpha)) {
                if (likely(source->pixels[sourceoffset + 3] == 0)) {
                    while (likely(source->pixels[sourceoffset + 3] == 0 &&
                            x < maxx)) {
                        x++;
                        sourceoffset += sourcestep;
                        targetoffset += targetstep;
                        continue;
                    }
                    continue;
                }
                alphar = (((int)source->pixels[sourceoffset + 3]
                    * INT_COLOR_SCALAR) / 255);
                assert(alphar <= INT_COLOR_SCALAR);
            } else {
                alphar = INT_COLOR_SCALAR;
            }
            int reverse_alphar = 0;
            if (likely(alphar >= INT_COLOR_SCALAR)) {
                while (likely(source->pixels[sourceoffset + 3] == 255 &&
                        x < maxx)) {
                    target->pixels[targetoffset + alphaindex] = 255;
                    memcpy(
                        &target->pixels[targetoffset],
                        &source->pixels[sourceoffset], 3
                    );
                    x++;
                    targetoffset += targetstep;
                    sourceoffset += sourcestep;
                    continue;
                }
                continue;
            } else {
                reverse_alphar = (INT_COLOR_SCALAR - alphar);
                assert(alphar + reverse_alphar == INT_COLOR_SCALAR);
                if (tghasalpha) {
                    // ^ possibly faster to branch for rgb-only surfaces,
                    // since math_pixcliptop also branches anyway.
                    int a = target->pixels[targetoffset + alphaindex] *
                        INT_COLOR_SCALAR;
                    a = (INT_COLOR_SCALAR * 255 - a) * reverse_alphar /
                        INT_COLOR_SCALAR;
                    a = (INT_COLOR_SCALAR * 255 - a);
                    target->pixels[targetoffset + alphaindex] = math_pixcliptop(
                        a / INT_COLOR_SCALAR
                    );
                }
                // Blend in rgb now:
                target->pixels[targetoffset + 0] = math_pixcliptop(
                    ((int)target->pixels[targetoffset + 0] *
                        reverse_alphar / INT_COLOR_SCALAR) +
                    ((int)source->pixels[sourceoffset + 0] *
                        alphar / INT_COLOR_SCALAR)
                );
                target->pixels[targetoffset + 1] = math_pixcliptop(
                    ((int)target->pixels[targetoffset + 1] *
                        reverse_alphar / INT_COLOR_SCALAR) +
                    ((int)source->pixels[sourceoffset + 1] *
                        alphar / INT_COLOR_SCALAR)
                );
                target->pixels[targetoffset + 2] = math_pixcliptop(
                    ((int)target->pixels[targetoffset + 2] *
                        reverse_alphar / INT_COLOR_SCALAR) +
                    ((int)source->pixels[sourceoffset + 2] *
                        alphar / INT_COLOR_SCALAR)
                );
            }
            if (likely(target->hasalpha)) {
                int a = target->pixels[targetoffset + 3] *
                    INT_COLOR_SCALAR;
                a = (INT_COLOR_SCALAR * 255 - a) * reverse_alphar /
                    INT_COLOR_SCALAR;
                a = (INT_COLOR_SCALAR * 255 - a);
                target->pixels[targetoffset + 3] = math_pixclip(
                    a / INT_COLOR_SCALAR
                );
            }
            x++;
            targetoffset += targetstep;
            sourceoffset += sourcestep;
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


HOTSPOT void rfssurf_BlitColor(rfssurf *target, rfssurf *source,
        int tgx, int tgy, int clipx, int clipy, int clipw, int cliph,
        double r, double g, double b, double a
        ) {
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
            source, target))
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
        x = tgx;
        assert(inta >= 0 && intr >= 0 && intg >= 0 && intb >= 0);
        assert(intrwhite >= 0 && intgwhite >= 0 && intbwhite >= 0);
        while (x < maxx) {
            // XXX: assumes little endian. format is BGR/ABGR
            int alphar;
            if (likely(source->hasalpha)) {
                if (likely(source->pixels[sourceoffset + 3] == 0)) {
                    while (likely(source->pixels[
                            sourceoffset + 3] == 0 &&
                            x < maxx)) {
                        x++;
                        sourceoffset += sourcestep;
                        targetoffset += targetstep;
                        continue;
                    }
                    continue;
                }
                alphar = (inta *
                    ((int)source->pixels[sourceoffset + 3]
                        * INT_COLOR_SCALAR / 255)) / INT_COLOR_SCALAR;
                assert(alphar <= INT_COLOR_SCALAR);
            } else {
                alphar = inta;
            }
            int reverse_alphar = (INT_COLOR_SCALAR - alphar);
            target->pixels[targetoffset + 0] = math_pixcliptop((
                (int)target->pixels[targetoffset + 0] *
                    reverse_alphar / INT_COLOR_SCALAR +
                ((int)source->pixels[sourceoffset + 0] *
                    intr / INT_COLOR_SCALAR +
                    255 *
                    intrwhite / INT_COLOR_SCALAR) * alphar /
                    INT_COLOR_SCALAR
            ));
            target->pixels[targetoffset + 1] = math_pixcliptop((
                (int)target->pixels[targetoffset + 1] *
                    reverse_alphar / INT_COLOR_SCALAR +
                ((int)source->pixels[sourceoffset + 1] *
                    intg / INT_COLOR_SCALAR +
                    255 *
                    intgwhite / INT_COLOR_SCALAR) * alphar /
                    INT_COLOR_SCALAR
            ));
            target->pixels[targetoffset + 2] = math_pixcliptop((
                (int)target->pixels[targetoffset + 2] *
                    reverse_alphar / INT_COLOR_SCALAR +
                ((int)source->pixels[sourceoffset + 2] *
                    intb / INT_COLOR_SCALAR +
                    255 *
                    intbwhite / INT_COLOR_SCALAR) * alphar /
                    INT_COLOR_SCALAR
            ));
            if (likely(target->hasalpha)) {
                int a = target->pixels[targetoffset + 3] *
                    INT_COLOR_SCALAR;
                a = (INT_COLOR_SCALAR * 255 - a) * reverse_alphar /
                    INT_COLOR_SCALAR;
                a = (INT_COLOR_SCALAR * 255 - a);
                target->pixels[targetoffset + 3] = math_pixcliptop(
                    a / INT_COLOR_SCALAR
                );
            }
            x++;
            targetoffset += targetstep;
            sourceoffset += sourcestep;
        }
        y++;
    }
}

HOTSPOT void rfssurf_BlitScaled(
        rfssurf *target, rfssurf *source,
        int tgx, int tgy, int clipx, int clipy, int clipw, int cliph,
        double scalex, double scaley,
        double r, double g, double b, double a
        ) {
    if ((fabs(scalex - 1) < 0.0001f &&
            fabs(scaley - 1) < 0.0001f) ||
            ((int)round(scalex * clipw) == clipw &&
            (int)round(scaley * cliph) == cliph)) {
        return rfssurf_BlitColor(target, source,
            tgx, tgy, clipx, clipy,
            clipw, cliph, r, g, b, a);
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
    if (!sanitize_clipping_scaled(
            &tgx, &tgy, &clipx, &clipy, &clipw, &cliph,
            source, target, scalex, scaley))
        return;
    const int clipalpha = floor(1 * INT_COLOR_SCALAR / 255);
    assert(clipalpha > 0);
    const int maxy = tgy + cliph * scaley;
    int y = tgy;
    const int maxx = tgx + clipw * scalex;
    int x = tgx;
    const int scaledivx = scalex * INT_COLOR_SCALAR;
    const int scaledivy = scaley * INT_COLOR_SCALAR;
    const int targetstep = (target->hasalpha ? 4 : 3);
    const int sourcestep = (source->hasalpha ? 4 : 3);
    const int sourcehasalpha = source->hasalpha;
    const int targethasalpha = target->hasalpha;
    if (y >= maxy || x >= maxx)
        return;
    while (y < maxy) {
        int targetoffset = (y * target->w + tgx) * (
            target->hasalpha ? 4 : 3
        );
        int sourceoffset_start = (
            ((int)imin(clipy + cliph - 1,
                ((y - tgy) * INT_COLOR_SCALAR / scaledivy) +
                clipy)) *
            source->w + clipx
        ) * sourcestep;
        assert(inta >= 0 && intr >= 0 && intg >= 0 && intb >= 0);
        assert(inta <= INT_COLOR_SCALAR);
        assert(intrwhite >= 0 && intgwhite >= 0 && intbwhite >= 0);
        x = tgx;
        while (x < maxx) {
            // XXX: assumes little endian. format is BGR/ABGR
            int sourceoffset = (
                sourceoffset_start +
                ((int)imin(clipw - 1, ((x - tgx) *
                    INT_COLOR_SCALAR / scaledivx))) *
                    sourcestep
            );
            int alphar;
            if (likely(sourcehasalpha)) {
                if (likely(source->pixels[sourceoffset + 3] == 0)) {
                    x++;
                    targetoffset += targetstep;
                    continue;
                }
                alphar = (inta *
                    ((int)source->pixels[sourceoffset + 3]
                        * INT_COLOR_SCALAR / 255)) / INT_COLOR_SCALAR;
                assert(alphar <= INT_COLOR_SCALAR);
            } else {
                alphar = inta;
            }
            int reverse_alphar = (INT_COLOR_SCALAR - alphar);
            target->pixels[targetoffset + 0] = math_pixcliptop((
                (int)target->pixels[targetoffset + 0] *
                    reverse_alphar / INT_COLOR_SCALAR +
                ((int)source->pixels[sourceoffset + 0] *
                    intr / INT_COLOR_SCALAR +
                    255 *
                    intrwhite / INT_COLOR_SCALAR) * alphar /
                    INT_COLOR_SCALAR
            ));
            target->pixels[targetoffset + 1] = math_pixcliptop((
                (int)target->pixels[targetoffset + 1] *
                    reverse_alphar / INT_COLOR_SCALAR +
                ((int)source->pixels[sourceoffset + 1] *
                    intg / INT_COLOR_SCALAR +
                    255 *
                    intgwhite / INT_COLOR_SCALAR) * alphar /
                    INT_COLOR_SCALAR
            ));
            target->pixels[targetoffset + 2] = math_pixcliptop((
                (int)target->pixels[targetoffset + 2] *
                    reverse_alphar / INT_COLOR_SCALAR +
                ((int)source->pixels[sourceoffset + 2] *
                    intb / INT_COLOR_SCALAR +
                    255 *
                    intbwhite / INT_COLOR_SCALAR) * alphar /
                    INT_COLOR_SCALAR
            ));
            if (likely(target->hasalpha)) {
                int a = target->pixels[targetoffset + 3] *
                    INT_COLOR_SCALAR;
                a = (INT_COLOR_SCALAR * 255 - a) * reverse_alphar /
                    INT_COLOR_SCALAR;
                a = (INT_COLOR_SCALAR * 255 - a);
                target->pixels[targetoffset + 3] = math_pixcliptop(
                    a / INT_COLOR_SCALAR
                );
            }
            x++;
            targetoffset += targetstep;
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
    const int stepsize = 4;  // SDL's XBGR also is 4 bytes
    const int sourcestepsize = (srf->hasalpha ? 4 : 3);
    int sourceoffset = 0;
    int y = 0;
    while (y < height) {
        int x = 0;
        char *pnow = p;
        char *maxp = p + (stepsize * width);
        if (likely(srf->hasalpha)) {
            // Fast path (copies entire row in one):
            memcpy(
                pnow, srf->pixels + sourceoffset,
                4 * width
            );
            sourceoffset += sourcestepsize * width;
            continue;
        }
        while (pnow < maxp) {
            // Slow path:
            memcpy(
                pnow, srf->pixels + sourceoffset, 3
            );
            // XXX: SDL2's XBGR still has this unused alpha byte:
            pnow[3] = 255;
            pnow += stepsize;
            sourceoffset += sourcestepsize;
        }
        p += srf->sdlsrf->pitch;
        y++;
    }
    SDL_UnlockSurface(srf->sdlsrf);
    return srf->sdlsrf;
}

SDL_Texture *rfssurf_AsTex(
        SDL_Renderer *r, rfssurf *srf, int withalpha
        ) {
    SDL_Surface *sdlsrf = rfssurf_AsSrf(srf, withalpha);
    SDL_Texture *t = SDL_CreateTextureFromSurface(
        r, sdlsrf
    );
    return t;
}
#endif

