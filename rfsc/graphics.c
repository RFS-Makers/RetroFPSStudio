// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <inttypes.h>
#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "filesys.h"
#include "graphics.h"
#include "hash.h"
#include "math2d.h"
#include "outputwindow.h"
#include "rfssurf.h"
#include "settings.h"
#include "vfs.h"


struct renderscissors {
    int x, y, w, h;
};

static struct renderscissors effectivescissors = {0};
static int effectivescissors_set = 0;
static int32_t _last_tex_id = 0;
static hashmap *texture_by_name_cache = NULL;
static hashmap *texture_by_id_cache = NULL;
static void _texture_RemoveGlobalTexture(rfs2tex *tex);
static int texturelist_count = 0;
static rfs2tex **texturelist_entry = NULL;
static rfs2tex *_current_rt = NULL;
static int _current_rt_is_void = 0;


static void _texture_RemoveGlobalTexture(rfs2tex *tex) {
    if (!tex) return;

    int i = 0;
    while (i < texturelist_count) {
        if (texturelist_entry[i] == tex) {
            if (i + 1 < texturelist_count)
                memmove(
                    &texturelist_entry[i],
                    &texturelist_entry[i + 1],
                    sizeof(*texturelist_entry) * (
                        texturelist_count - i - 1
                    )
                );
            texturelist_count--;
            continue;
        }
        i++;
    }
    free(tex->diskpath);
    if (tex->srf)
        rfssurf_Free(tex->srf);
    if (tex->srfalpha)
        rfssurf_Free(tex->srfalpha);
    if (tex->_internal_srfsideways)
        rfssurf_Free(tex->_internal_srfsideways);
    free(tex);
}

rfs2tex *graphics_GetTexById(int32_t id) {
    uintptr_t hashptrval = 0;
    if (texture_by_id_cache == NULL)
        return NULL;
    if (!hash_BytesMapGet(
            texture_by_id_cache,
            (char *)&id, sizeof(id),
            (uint64_t *)&hashptrval))
        return NULL;
    return (rfs2tex *)(void*)hashptrval;
}


int _texture_SetPixData(
        rfs2tex *tex, unsigned char *data,
        int width, int height
        ) {
    if (!tex || tex->isrendertarget ||
            !data || width < 0 || height < 0) {
        return 0;
    }

    // Load data into surface to convert later:
    rfssurf *srforig = rfssurf_Create(
        width, height, 1
    );
    if (!srforig) {
        return 0;
    }
    memcpy(srforig->pixels, data, width * height * 4);

    // Create converted surfaces:
    rfssurf *srf = rfssurf_DuplicateNoAlpha(srforig);
    rfssurf *srfalpha = srforig;
    srforig = NULL;
    if (!srf || !srfalpha) {
        if (srf)
            rfssurf_Free(srf);
        if (srfalpha)
            rfssurf_Free(srfalpha);
        if (srforig)
            rfssurf_Free(srforig);
        return 0;
    }

    // Store as new global texture:
    tex->w = srf->w;
    tex->h = srf->h;
    tex->srf = srf;
    tex->srfalpha = srfalpha;
    return 1;
}


rfs2tex *graphics_CreateTarget(int w, int h) {
    if (w < 0 || h < 0) return NULL;
    rfs2tex *tex = NULL;
    {
        tex = malloc(sizeof(*tex));
        if (!tex) {
            return NULL;
        }
        memset(tex, 0, sizeof(*tex));
        tex->id = -1;
        tex->w = w;
        tex->h = h;
        tex->isrendertarget = 1;
        tex->srfalpha = rfssurf_Create(tex->w, tex->h, 1);
        if (!tex->srfalpha) {
            free(tex->diskpath);
            free(tex);
            return NULL;
        }
        assert(tex->srfalpha->w == tex->w);
        assert(tex->srfalpha->h == tex->h);
        _last_tex_id++;
        tex->id = _last_tex_id;
        #ifdef DEBUG_RENDERER
        fprintf(stderr,
            "rfsc/graphics.c: debug: graphics_ResizeTarget() "
            "-> creating target id=%" PRId64 " to w=%d,h=%d\n",
            (int64_t)tex->id, tex->w, tex->h);
        #endif
        rfs2tex **texturelist_entry_new = realloc(
            texturelist_entry,
            sizeof(*texturelist_entry) * (texturelist_count + 1)
        );
        if (!texturelist_entry_new) {
            free(tex->diskpath);
            rfssurf_Free(tex->srfalpha);
            free(tex);
            return NULL;
        }
        texturelist_entry_new[texturelist_count] = tex;
        texturelist_entry = texturelist_entry_new;
        texturelist_count++;
    }
    if (!texture_by_id_cache) {
        texture_by_id_cache = hash_NewBytesMap(128);
        if (!texture_by_id_cache) {
            _texture_RemoveGlobalTexture(tex);
            return NULL;
        }
    }
    if (!hash_BytesMapSet(
            texture_by_id_cache,
            (char *)&tex->id, sizeof(tex->id),
            (uint64_t)(uintptr_t)tex)) {
        _texture_RemoveGlobalTexture(tex);
        return NULL;
    }
    return tex;
}


rfs2tex *graphics_GetWriteCopy(rfs2tex *tex) {
    return graphics_GetWriteCopyDifferentSize(
        tex, tex->w, tex->h);
}


rfs2tex *graphics_GetWriteCopyDifferentSize(
        rfs2tex *orig_tex, int w, int h
        ) {
    if (orig_tex->isrendertarget || w < 0 || h < 0)
        return NULL;
    rfs2tex *t = malloc(sizeof(*t));
    if (!t)
        return NULL;
    memset(t, 0, sizeof(*t));
    t->srfalpha = rfssurf_Create(w, h, 1);
    t->srf = rfssurf_Create(w, h, 0);
    if (!t->srfalpha || !t->srf) {
        rfssurf_Free(t->srfalpha);
        rfssurf_Free(t->srf);
        free(t);
        return NULL;
    }
    t->iswritecopy = 1;
    if (orig_tex->diskpath) {
        t->diskpath = strdup(orig_tex->diskpath);
        if (!t->diskpath) {
            rfssurf_Free(t->srfalpha);
            rfssurf_Free(t->srf);
            free(t);
            return NULL;
        }
    }
    {
        rfs2tex **texturelist_entry_new = realloc(
            texturelist_entry,
            sizeof(*texturelist_entry) * (texturelist_count + 1)
        );
        if (!texturelist_entry_new) {
            free(t->diskpath);
            rfssurf_Free(t->srf);
            rfssurf_Free(t->srfalpha);
            free(t);
            return NULL;
        }
        texturelist_entry_new[texturelist_count] = t;
        texturelist_entry = texturelist_entry_new;
        texturelist_count++;
    }
    _last_tex_id++;
    t->id = _last_tex_id;
    if (!hash_BytesMapSet(
            texture_by_id_cache,
            (char *)&t->id, sizeof(t->id),
            (uint64_t)(uintptr_t)t)) {
        _texture_RemoveGlobalTexture(t);
        return NULL;
    }
    t->w = w;
    t->h = h;
    rfssurf_BlitSimple(t->srf, orig_tex->srf,
        0, 0, 0, 0, imin(w, orig_tex->w),
        imin(h, orig_tex->h)
    );
    rfssurf_BlitSimple(t->srfalpha, orig_tex->srfalpha,
        0, 0, 0, 0, imin(w, orig_tex->w),
        imin(h, orig_tex->h)
    );

    return t;
}


rfssurf *graphics_GetTexSideways(rfs2tex *tex) {
    if (!tex)
        return NULL;
    if (tex->_internal_srfsideways)
        return tex->_internal_srfsideways;
    #ifdef DEBUG_TEXTURES
    fprintf(stderr,
        "rfsc/graphics.c: debug: graphics_GetTexSideways() "
        "-> turning texture id=%" PRId64 "\n",
        (int64_t)tex->id);
    #endif
    assert(tex->srf != NULL || tex->isrendertarget ||
        tex->iswritecopy);
    rfssurf *src = (
        tex->srf ? tex->srf : tex->srfalpha
    );
    tex->_internal_srfsideways = rfssurf_Create(
        src->h, src->w, 0
    );
    const int targetpixellen = (
        tex->_internal_srfsideways->hasalpha ? 4 : 3
    );
    const int srcpixellen = (
        src->hasalpha ? 4 : 3
    );
    int y = 0;
    while (y < src->w) {
        int x = 0;
        while (x < src->h) {
            assert(x < tex->_internal_srfsideways->w);
            assert(y < tex->_internal_srfsideways->h);
            assert(y >= 0 && y < src->w && src->w - y - 1 < src->w);
            int srcoffset = (
                ((src->w - y - 1) + x * src->w) * srcpixellen
            );
            assert(srcoffset >= 0 && srcoffset <
                src->w * src->h * srcpixellen);
            int targetoffset = (
                (x + y * tex->_internal_srfsideways->w) *
                    targetpixellen
            );
            assert(targetoffset <
                tex->_internal_srfsideways->w *
                tex->_internal_srfsideways->h * targetpixellen);
            memcpy(&tex->_internal_srfsideways->pixels[targetoffset],
                &src->pixels[srcoffset],
                3);
            if (targetpixellen == 4)
                tex->_internal_srfsideways->pixels[targetoffset + 3] =
                    255;
            x++;
        }
        y++;
    }
    return tex->_internal_srfsideways;
}

char *graphics_FixTexturePath(const char *path) {
    char *p = filesys_Normalize(path);
    if (!p)
        return NULL;
    int result = 0;
    if (!vfs_Exists(p, &result, 0)) {
        free(p);
        return NULL;
    }

    char *joined_1 = malloc(strlen(p) + strlen(".png") + 1);
    char *joined_2 = malloc(strlen(p) + strlen(".jpg") + 1);
    char *joined_3 = malloc(strlen(p) + strlen(".bmp") + 1);
    if (!joined_1 || !joined_2 || !joined_3) {
        abort:
        free(p);
        free(joined_1);
        free(joined_2);
        free(joined_3);
        return NULL;
    }
    memcpy(joined_1, p, strlen(p) + 1);
    strcat(joined_1, ".png");
    memcpy(joined_2, p, strlen(p) + 1);
    strcat(joined_2, ".jpg");
    memcpy(joined_3, p, strlen(p) + 1);
    strcat(joined_3, ".bmp");
    result = 0;
    if (!vfs_Exists(joined_1, &result, 0))
        goto abort;
    if (result) {
        free(p); free(joined_2); free(joined_3);
        return joined_1;
    }
    result = 0;
    if (!vfs_Exists(joined_2, &result, 0))
        goto abort;
    if (result) {
        free(p); free(joined_1); free(joined_3);
        return joined_2;
    }
    result = 0;
    if (!vfs_Exists(joined_3, &result, 0))
        goto abort;
    if (result) {
        free(p); free(joined_1); free(joined_2);
        return joined_3;
    }
    free(joined_1); free(joined_2); free(joined_3);
    return p;
}

rfs2tex *graphics_LoadTexEx(const char *path, int usecache) {
    if (usecache) {
        char *p = graphics_FixTexturePath(path);
        if (!p)
            return NULL;

        if (!texture_by_name_cache) {
            texture_by_name_cache = hash_NewBytesMap(128);
            if (!texture_by_name_cache) {
                free(p);
                return NULL;
            }
        }
        if (!texture_by_id_cache) {
            texture_by_id_cache = hash_NewBytesMap(128);
            if (!texture_by_id_cache) {
                free(p);
                return NULL;
            }
        }
        uintptr_t hashptrval = 0;
        if (!hash_BytesMapGet(
                texture_by_name_cache, p, strlen(p),
                (uint64_t *)&hashptrval)) {
            #if defined(DEBUG_TEXTURES)
            fprintf(stderr,
                "rfsc/graphics.c: debug: graphics_LoadTex(\"%s\") "
                    "-> adding new: %s\n",
                path, p);
            #endif
            rfs2tex *t = graphics_LoadTexEx(
                path, 0
            );
            if (!t) {
                free(p);
                return NULL;
            }
            if (!hash_BytesMapSet(
                    texture_by_name_cache, p, strlen(p),
                    (uint64_t)(uintptr_t)t)) {
                free(p);
                _texture_RemoveGlobalTexture(t);
                return NULL;
            }
            if (!hash_BytesMapSet(
                    texture_by_id_cache,
                    (char *)&t->id, sizeof(t->id),
                    (uint64_t)(uintptr_t)t)) {
                hash_BytesMapUnset(
                    texture_by_name_cache, p, strlen(p)
                );
                free(p);
                _texture_RemoveGlobalTexture(t);
                return NULL;
            }
            free(p);
            return t;
        }
        free(p);
        return (rfs2tex *)(void *)hashptrval;
    }

    rfs2tex *tex = NULL;
    {
        char *p = graphics_FixTexturePath(path);
        if (!p)
            return NULL;
        tex = malloc(sizeof(*tex));
        if (!tex) {
            free(p);
            return NULL;
        }
        memset(tex, 0, sizeof(*tex));
        tex->id = -1;
        tex->diskpath = p;
        rfs2tex **texturelist_entry_new = realloc(
            texturelist_entry,
            sizeof(*texturelist_entry) * (texturelist_count + 1)
        );
        if (!texturelist_entry_new) {
            free(tex->diskpath);
            free(tex);
            return NULL;
        }
        texturelist_entry_new[texturelist_count] = tex;
        texturelist_entry = texturelist_entry_new;
        texturelist_count++;
    }

    // Load raw image data with stb_image:
    int w = 0;
    int h = 0;
    int n = 0;
    char *imgencodedfile = NULL;
    int32_t size = 0;
    VFSFILE *f = vfs_fopen(tex->diskpath, "rb", 0);
    if (!f) {
        _texture_RemoveGlobalTexture(tex);
        return NULL;
    }
    while (f && !vfs_feof(f)) {
        char buf[512];
        int result = vfs_fread(buf, 1, sizeof(buf), f);
        if (result <= 0) {
            if (vfs_feof(f))
                break;
            if (imgencodedfile)
                free(imgencodedfile);
            vfs_fclose(f);
            _texture_RemoveGlobalTexture(tex);
            return NULL;
        }
        char *imgencodedfilenew = realloc(
            imgencodedfile, size + result
        );
        if (!imgencodedfilenew) {
            if (imgencodedfile)
                free(imgencodedfile);
            vfs_fclose(f);
            _texture_RemoveGlobalTexture(tex);
            return NULL;
        }
        imgencodedfile = imgencodedfilenew;
        memcpy(imgencodedfilenew + size, buf, result);
        size += result;
    }
    if (f)
        vfs_fclose(f);
    unsigned char *data = stbi_load_from_memory(
        (unsigned char *)imgencodedfile, size, &w, &h, &n, 4
    );
    free(imgencodedfile);
    if (!data) {
        _texture_RemoveGlobalTexture(tex);
        return NULL;
    }

    // Create texture:
    int result = _texture_SetPixData(
        tex, data, w, h
    );
    stbi_image_free(data);
    if (!tex) {
        _texture_RemoveGlobalTexture(tex);
    }

    // Set id:
    _last_tex_id++;
    tex->id = _last_tex_id;

    return tex;
}

rfs2tex *graphics_LoadTex(const char *path) {
    return graphics_LoadTexEx(path, 1);
}

int graphics_ResizeTarget(rfs2tex *tex, int w, int h) {
    if (!tex || w < 0 || h < 0 || !tex->isrendertarget)
        return 1;
    if (tex->w == w && tex->h == h)
        return 1;
    if (tex->srfalpha)
        rfssurf_Free(tex->srfalpha);
    tex->srfalpha = NULL;
    if (w != 0 && h != 0) {
        rfssurf *newsrf = rfssurf_Create(
            w, h, 1
        );
        if (!newsrf)
            return 0;
        if (tex->srfalpha)
            rfssurf_Free(tex->srfalpha);
        tex->srfalpha = newsrf;
        tex->w = w;
        tex->h = h;
    } else {
        if (tex->srfalpha)
            rfssurf_Free(tex->srfalpha);
        tex->srfalpha = NULL;
    }
    #ifdef DEBUG_RENDERER
    fprintf(stderr,
        "rfsc/graphics.c: debug: graphics_ResizeTarget() "
        "-> resized target id=%" PRId64 " to w=%d,h=%d\n",
        (int64_t)tex->id, tex->w, tex->h);
    #endif
    if (w == 0 || h == 0) {
        tex->w = w;
        tex->h = h;
        return 1;
    }
    assert(tex->srfalpha);
    tex->w = w;
    tex->h = h;
    return 1;
}

int graphics_SetTarget(rfs2tex *tex) {
    if (!tex) {
        _current_rt = NULL;
        _current_rt_is_void = 0;
        return 1;
    }
    if (!tex->isrendertarget) {
        #ifdef DEBUG_RENDERER
        fprintf(stderr,
            "rfsc/graphics.c: debug: graphics_SetTarget() "
            "-> no renderer!\n");
        #endif
        return 0;
    }
    if (tex->isrendertarget && (tex->w == 0 || tex->h == 0)) {
        _current_rt = tex;
        _current_rt_is_void = 1;
        return 1;
    }
    _current_rt = tex;
    _current_rt_is_void = 0;
    return 1;
}

rfs2tex *graphics_GetTarget() {
    return _current_rt;
}

int graphics_ClearTarget() {
    if (_current_rt_is_void)
        return 1;
    if (_current_rt) {
        assert(_current_rt->srfalpha != NULL);
        rfssurf_Clear(_current_rt->srfalpha);
    } else {
        rfssurf *surf = outputwindow_GetSurface();
        if (!surf)
            return 0;
        rfssurf_Clear(surf);
    }
    return 1;
}

int graphics_PresentTarget() {
    if (_current_rt_is_void)
        return 1;
    if (_current_rt != NULL)
        return 1;
    return outputwindow_PresentSurface();
}

int graphics_DrawLine(
        double r, double g, double b, double a,
        int x1, int y1, int x2, int y2, int thickness
        ) {
    return 1;
}

int graphics_DrawRectangle(
        double r, double g, double b, double a,
        int x, int y, int w, int h
        ) {
    if (_current_rt_is_void)
        return 1;
    rfssurf *target = NULL;
    if (_current_rt)
        target = _current_rt->srfalpha;
    else
        target = outputwindow_GetSurface();
    if (target == NULL)
        return 0;
    rfssurf_Rect(target, x, y, w, h, r, g, b, a);
    return 1;
}

int graphics_DrawTex(rfs2tex *tex, int withalpha,
        int cropx, int cropy, int cropw, int croph,
        double r, double g, double b, double a,
        int x, int y, double scalex, double scaley
        ) {
    if (!tex)
        return 0;
    rfssurf *winsrf = outputwindow_GetSurface();
    if (!winsrf && _current_rt == NULL) {
        #ifdef DEBUG_RENDERER
        fprintf(stderr,
            "rfsc/graphics.c: debug: graphics_DrawTex -> "
            "got no window surface\n");
        #endif
        return 0;
    }
    if (tex->w == 0 || tex->h == 0)
        return 1;
    if (_current_rt_is_void)
        return 1;
    if (cropx >= tex->w || cropy >= tex->h ||
            cropx + cropw <= 0 || cropy + croph <= 0 ||
            cropw <= 0 || croph <= 0 || a * 255.0 < 1.0)
        return 1;
    if (cropx < 0) {
        cropw += -cropx;
        cropx = 0;
    }
    if (cropy < 0) {
        croph += -cropy;
        cropy = 0;
    }
    if (cropw + cropx > tex->w) cropw = tex->w - cropx;
    if (croph + cropy > tex->h) croph = tex->h - cropy;

    int draw_w = (cropw * scalex);
    int draw_h = (croph * scaley);

    if (cropw < 1 || croph < 1 || draw_w < 1 || draw_h < 1)
        return 1;

    rfssurf *target = (_current_rt ? _current_rt->srfalpha : winsrf);
    rfssurf *source = (withalpha ? tex->srfalpha : tex->srf);
    rfssurf_BlitScaled(
        target, source,
        x, y, cropx, cropy, cropw, croph, scalex, scaley,
        r, g, b, a
    );
    return 1;
}


static struct renderscissors *scissorsstack = NULL;
static int scissorscount = 0;

void _applyscissorstack(int *_x, int *_y, int *_w, int *_h) {
    int x = *_x;
    int y = *_y;
    int w = *_w;
    int h = *_h;
    int i = scissorscount - 1;
    while (i >= 0) {
        if (scissorsstack[scissorscount].x > x) {
            int d = (scissorsstack[scissorscount].x - x);
            x += d;
            w -= d;
            if (w < 0)
                w = 0;
        }
        if (scissorsstack[scissorscount].y > y) {
            int d = (scissorsstack[scissorscount].y - y);
            y += d;
            h -= d;
            if (h < 0)
                w = 0;
        }
        if (scissorsstack[scissorscount].x +
                scissorsstack[scissorscount].w < x + w) {
            int d = (x + w) - (scissorsstack[scissorscount].x + 
                scissorsstack[scissorscount].w);
            w -= d;
            if (w < 0)
                w = 0;
        }
        if (scissorsstack[scissorscount].y + 
                scissorsstack[scissorscount].h < y + h) {
            int d = (y + h) - (scissorsstack[scissorscount].y +
                scissorsstack[scissorscount].h);
            h -= d;
            if (h < 0)
                h = 0;
        }
        i--;
    }
    *_x = x;
    *_y = y;
    *_w = w;
    *_h = h;
}
 
int graphics_PushRenderScissors(int x, int y, int w, int h) {
    if (w < 0)
        w = 0;
    if (h < 0)
        h = 0;
    struct renderscissors *newscissorsstack = realloc(
        scissorsstack, sizeof(*scissorsstack) * (scissorscount + 1)
    );
    if (!newscissorsstack)
        return 0;
    scissorsstack = newscissorsstack;
    scissorsstack[scissorscount].x = x;
    scissorsstack[scissorscount].y = y;
    scissorsstack[scissorscount].w = w;
    scissorsstack[scissorscount].h = h;
    _applyscissorstack(&x, &y, &w, &h);
    effectivescissors_set = 1;
    effectivescissors.x = x;
    effectivescissors.y = y;
    effectivescissors.w = w;
    effectivescissors.h = h;
    scissorscount++;
    return 1;
}

void graphics_PopRenderScissors() {
    if (scissorscount > 1) {
        int x, y, w, h;
        x = scissorsstack[scissorscount - 2].x;
        y = scissorsstack[scissorscount - 2].y;
        w = scissorsstack[scissorscount - 2].w;
        h = scissorsstack[scissorscount - 2].h;
        _applyscissorstack(
            &x, &y, &w, &h
        );
        effectivescissors.x = x;
        effectivescissors.y = y;
        effectivescissors.w = w;
        effectivescissors.h = h;
        scissorscount--;
        return;
    }
    effectivescissors_set = 0;
    scissorscount--;
    return;
}

int graphics_DeleteTex(rfs2tex *tex) {
    if (!tex || (!tex->isrendertarget && !tex->iswritecopy))
        return 0;
    if (tex->isrendertarget && _current_rt == tex)
        return 0;
    _texture_RemoveGlobalTexture(tex);
    return 1;
}

rfssurf *graphics_GetTargetSrf() {
    rfssurf *target = NULL;
    if (_current_rt)
        target = _current_rt->srfalpha;
    else
        target = outputwindow_GetSurface();
    return target;
}
