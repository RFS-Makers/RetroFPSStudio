// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#ifdef HAVE_SDL

#include <math.h>
#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sdl/sdlkey_to_str.h"


typedef struct sdlkey_to_str {
    char *str;
    int strlen;
    uint64_t sdlkey;
} sdlkey_to_str;

typedef struct sdlkey_to_str_bucket {
    sdlkey_to_str *key;
    int keycount;
} sdlkey_to_str_bucket;

const int sdlkey_to_str_slot_count = 128;
sdlkey_to_str_bucket *sdlkey_to_str_slot = NULL;
sdlkey_to_str_bucket *str_to_sdlkey_slot = NULL;


static void _add_sdlkey_entry_ex(
        SDL_Keycode sym, const char *str,
        int addreverse
        ) {
    if (!sdlkey_to_str_slot) {
        sdlkey_to_str_slot = malloc(
            sizeof(*sdlkey_to_str_slot) *
            sdlkey_to_str_slot_count
        );
        if (!sdlkey_to_str_slot) {
            oom: ;
            fprintf(stderr,
                "rfsc/sdl/sdlkey_to_str.c: error: "
                "out of memory assembling table");
            _exit(1);
            return;
        }
        memset(sdlkey_to_str_slot, 0,
            sizeof(*sdlkey_to_str_slot) *
            sdlkey_to_str_slot_count);
    }
    if (!str_to_sdlkey_slot) {
        str_to_sdlkey_slot = malloc(
            sizeof(*str_to_sdlkey_slot) *
            sdlkey_to_str_slot_count
        );
        if (!str_to_sdlkey_slot)
            goto oom;
        memset(str_to_sdlkey_slot, 0,
            sizeof(*str_to_sdlkey_slot) *
            sdlkey_to_str_slot_count);
    }
    int slot = (sym % sdlkey_to_str_slot_count);
    int lenstr = strlen(str);
    int slotreverse = (
        (int)(((uint8_t*)str)[0]) + (lenstr > 1 ?
        (int)(((uint8_t*)str)[1]) : (int)0) + lenstr
    ) % sdlkey_to_str_slot_count;

    int kcount = sdlkey_to_str_slot[slot].keycount;
    sdlkey_to_str_slot[slot].key = realloc(
        sdlkey_to_str_slot[slot].key,
        sizeof(*sdlkey_to_str_slot[slot].key) *
        (kcount + 1)
    );
    if (!sdlkey_to_str_slot[slot].key) goto oom;
    sdlkey_to_str_slot[slot].key[kcount].sdlkey = sym;
    sdlkey_to_str_slot[slot].key[kcount].str = strdup(str);
    if (!sdlkey_to_str_slot[slot].key[kcount].str)
        goto oom;
    sdlkey_to_str_slot[slot].key[kcount].strlen = strlen(
        sdlkey_to_str_slot[slot].key[kcount].str
    );
    sdlkey_to_str_slot[slot].keycount++;

    if (addreverse) {
        kcount = str_to_sdlkey_slot[slotreverse].keycount;
        str_to_sdlkey_slot[slotreverse].key = realloc(
            str_to_sdlkey_slot[slotreverse].key,
            sizeof(*str_to_sdlkey_slot[slotreverse].key) *
            (kcount + 1)
        );
        if (!str_to_sdlkey_slot[slotreverse].key) goto oom;
        str_to_sdlkey_slot[slotreverse].key[kcount].sdlkey = sym;
        str_to_sdlkey_slot[slotreverse].key[kcount].str = strdup(str);
        if (!str_to_sdlkey_slot[slotreverse].key[kcount].str)
            goto oom;
        str_to_sdlkey_slot[slotreverse].key[kcount].strlen = strlen(
            str_to_sdlkey_slot[slotreverse].key[kcount].str
        );
        str_to_sdlkey_slot[slot].keycount++;
    }
}


static void _add_sdlkey_entry(
        SDL_Keycode sym, const char *str
        ) {
    _add_sdlkey_entry_ex(sym, str, 1);
}


static __attribute__((constructor)) void _buildsdlkeys() {
    char buf[12];

    uint64_t i = SDLK_0;
    while (i < (uint64_t)SDLK_9) {
        buf[0] = '0' + (i - SDLK_0);
        buf[1] = '\0';
        _add_sdlkey_entry(i, buf);
        i++;
    }
    i = SDLK_a;
    while (i < (uint64_t)SDLK_z) {
        buf[0] = 'a' + (i - SDLK_a);
        buf[1] = '\0';
        _add_sdlkey_entry(i, buf);
        i++;
    }
    _add_sdlkey_entry(SDLK_F1, "f1");
    _add_sdlkey_entry(SDLK_F2, "f2");
    _add_sdlkey_entry(SDLK_F3, "f3");
    _add_sdlkey_entry(SDLK_F4, "f4");
    _add_sdlkey_entry(SDLK_F5, "f5");
    _add_sdlkey_entry(SDLK_F6, "f6");
    _add_sdlkey_entry(SDLK_F7, "f7");
    _add_sdlkey_entry(SDLK_F8, "f8");
    _add_sdlkey_entry(SDLK_F9, "f9");
    _add_sdlkey_entry(SDLK_F10, "f10");
    _add_sdlkey_entry(SDLK_F11, "f11");
    _add_sdlkey_entry(SDLK_F12, "f12");
    _add_sdlkey_entry(SDLK_UP, "up");
    _add_sdlkey_entry(SDLK_DOWN, "down");
    _add_sdlkey_entry(SDLK_LEFT, "left");
    _add_sdlkey_entry(SDLK_RIGHT, "right");
    _add_sdlkey_entry(SDLK_SPACE, "space");
    _add_sdlkey_entry(SDLK_RSHIFT, "rightshift");
    _add_sdlkey_entry(SDLK_LSHIFT, "leftshift");
    _add_sdlkey_entry(SDLK_RCTRL, "rightcontrol");
    _add_sdlkey_entry(SDLK_LCTRL, "leftcontrol");
    _add_sdlkey_entry(SDLK_ESCAPE, "escape");
    _add_sdlkey_entry_ex(SDLK_AC_BACK, "escape", 0);
    _add_sdlkey_entry(SDLK_BACKSPACE, "backspace");
    _add_sdlkey_entry(SDLK_QUOTE, "'");
    _add_sdlkey_entry(SDLK_COMMA, ",");
    _add_sdlkey_entry(SDLK_PERIOD, ".");
    _add_sdlkey_entry(SDLK_PAGEDOWN, "pagedown");
    _add_sdlkey_entry(SDLK_PAGEUP, "pageup");
    _add_sdlkey_entry(SDLK_LALT, "lalt");
    _add_sdlkey_entry(SDLK_RALT, "ralt");
    _add_sdlkey_entry(SDLK_TAB, "tab");
    _add_sdlkey_entry(SDLK_PAUSE, "pause");
    _add_sdlkey_entry(SDLK_RETURN, "return");
}


int ConvertSDLKeyToStr(uint64_t sdlkeysym, char *result) {
    int slot = (sdlkeysym % sdlkey_to_str_slot_count);
    int kcount = sdlkey_to_str_slot[slot].keycount;
    int i = 0;
    while (i < kcount) {
        if (sdlkey_to_str_slot[slot].key[i].sdlkey ==   
                (uint64_t)sdlkeysym) {
            memcpy(result,
                sdlkey_to_str_slot[slot].key[i].str,
                strlen(sdlkey_to_str_slot[slot].key[i].str) + 1);
            return 1;
        }
        i++;
    }
    return 0;
}


int ConvertStrToSDLKey(const char *str, uint64_t *result) {
    int lenstr = strlen(str);
    int slotr = (
        (int)(((uint8_t*)str)[0]) + (lenstr > 1 ?
        (int)(((uint8_t*)str)[1]) : (int)0) + lenstr
    ) % sdlkey_to_str_slot_count;
    int kcount = str_to_sdlkey_slot[slotr].keycount;
    int i = 0;
    while (i < kcount) {
        if (str_to_sdlkey_slot[slotr].key[i].strlen == lenstr &&
                memcmp(str_to_sdlkey_slot[slotr].key[i].str,
                str, lenstr) == 0) {
            *result = sdlkey_to_str_slot[slotr].key[i].sdlkey;
            return 1;
        }
        i++;
    }
    return 0;
}

#endif  // HAVE_SDL
