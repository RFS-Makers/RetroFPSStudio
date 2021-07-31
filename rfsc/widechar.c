
#include "compileconfig.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#include "widechar.h"

char *AS_U8_FROM_U16(const uint16_t *s) {
    #if defined(_WIN32) || defined(_WIN64)
    assert(sizeof(wchar_t) == sizeof(uint16_t));
    if (wcslen(s) == 0) {
        uint8_t *result = malloc(sizeof(*result));
        if (result) result[0] = '\0';
        return (char *)result;
    }
    int resultlen = WideCharToMultiByte(
        CP_UTF8, 0, s, (int)wcslen(s), NULL, 0,
        NULL, NULL
    );
    if (resultlen <= 0) {
        return NULL;
    }
    uint8_t *result = malloc((resultlen + 10) * sizeof(*result));
    int cvresult = WideCharToMultiByte(
        CP_UTF8, 0, s, (int)wcslen(s),
        (char *)result, resultlen + 10, NULL, NULL
    );
    if (cvresult <= 0 || cvresult > resultlen + 5) {
        free(result);
        return NULL;
    }
    result[cvresult] = '\0';
    return (char *)result;
    #else
    return NULL;
    #endif
}

uint16_t *AS_U16(const char *s) {
    #if defined(_WIN32) || defined(_WIN64)
    if (strlen(s) == 0) {
        uint16_t *result = malloc(sizeof(*result));
        if (result) result[0] = '\0';
        return result;
    }
    int resultlen = MultiByteToWideChar(
        CP_UTF8, 0, s, (int)strlen(s), NULL, 0
    );
    if (resultlen <= 0) {
        return NULL;
    }
    uint16_t *result = malloc((resultlen + 10) * sizeof(*result));
    int cvresult = MultiByteToWideChar(
        CP_UTF8, 0, s, (int)strlen(s),
        result, resultlen + 10
    );
    if (cvresult <= 0 || cvresult > resultlen + 5) {
        free(result);
        return NULL;
    }
    result[cvresult] = '\0';
    return result;
    #else
    return NULL;
    #endif
}

static int is_utf8_start(uint8_t c) {
    if ((int)(c & 0xE0) == (int)0xC0) {  // 110xxxxx
        return 1;
    } else if ((int)(c & 0xF0) == (int)0xE0) {  // 1110xxxx
        return 1;
    } else if ((int)(c & 0xF8) == (int)0xF0) {  // 11110xxx
        return 1;
    }
    return 0;
}

int utf8_char_len(const unsigned char *p) {
    if ((int)((*p) & 0xE0) == (int)0xC0)
        return 2;
    if ((int)((*p) & 0xF0) == (int)0xE0)
        return 3;
    if ((int)((*p) & 0xF8) == (int)0xF0)
        return 4;
    return 1;
}


int get_utf8_codepoint(
        const unsigned char *p, int size,
        uint64_t *out, int *cpbyteslen
        ) {
    if (size < 1)
        return 0;
    if (!is_utf8_start(*p)) {
        if (*p > 127)
            return 0;
        if (out) *out = (uint64_t)(*p);
        if (cpbyteslen) *cpbyteslen = 1;
        return 1;
    }
    uint8_t c = (*(uint8_t*)p);
    if ((int)(c & 0xE0) == (int)0xC0 && size >= 2) {  // p[0] == 110xxxxx
        if ((int)(*(p + 1) & 0xC0) != (int)0x80) { // p[1] != 10xxxxxx
            return 0;
        }
        if (size >= 3 &&
                (int)(*(p + 2) & 0xC0) == (int)0x80) { // p[2] == 10xxxxxx
            return 0;
        }
        uint64_t c = (   // 00011111 of first byte
            (uint64_t)(*p) & (uint64_t)0x1FULL
        ) << (uint64_t)6ULL;
        c += (  // 00111111 of second byte
            (uint64_t)(*(p + 1)) & (uint64_t)0x3FULL
        );
        if (c <= 127ULL)
            return 0;  // not valid to be encoded with two bytes.
        if (out) *out = c;
        if (cpbyteslen) *cpbyteslen = 2;
        return 1;
    }
    if ((int)(c & 0xF0) == (int)0xE0 && size >= 3) {  // p[0] == 1110xxxx
        if ((int)(*(p + 1) & 0xC0) != (int)0x80) { // p[1] != 10xxxxxx
            return 0;
        }
        if ((int)(*(p + 2) & 0xC0) != (int)0x80) { // p[2] != 10xxxxxx
            return 0;
        }
        if (size >= 4 &&
                (int)(*(p + 3) & 0xC0) == (int)0x80) { // p[3] == 10xxxxxx
            return 0;
        }
        uint64_t c = (   // 00001111 of first byte
            (uint64_t)(*p) & (uint64_t)0xFULL
        ) << (uint64_t)12ULL;
        c += (  // 00111111 of second byte
            (uint64_t)(*(p + 1)) & (uint64_t)0x3FULL
        ) << (uint64_t)6ULL;
        c += (  // 00111111 of third byte
            (uint64_t)(*(p + 2)) & (uint64_t)0x3FULL
        );
        if (c <= 0x7FFULL)
            return 0;  // not valid to be encoded with three bytes.
        if (c >= 0xD800ULL && c <= 0xDFFFULL) {
            // UTF-16 surrogate code points may not be used in UTF-8
            // (in part because we re-use them to store invalid bytes)
            return 0;
        }
        if (out) *out = c;
        if (cpbyteslen) *cpbyteslen = 3;
        return 1;
    }
    if ((int)(c & 0xF8) == (int)0xF0 && size >= 4) {  // p[0] == 11110xxx
        if ((int)(*(p + 1) & 0xC0) != (int)0x80) { // p[1] != 10xxxxxx
            return 0;
        }
        if ((int)(*(p + 2) & 0xC0) != (int)0x80) { // p[2] != 10xxxxxx
            return 0;
        }
        if ((int)(*(p + 3) & 0xC0) != (int)0x80) { // p[3] != 10xxxxxx
            return 0;
        }
        if (size >= 5 &&
                (int)(*(p + 4) & 0xC0) == (int)0x80) { // p[4] == 10xxxxxx
            return 0;
        }
        uint64_t c = (   // 00000111 of first byte
            (uint64_t)(*p) & (uint64_t)0x7ULL
        ) << (uint64_t)18ULL;
        c += (  // 00111111 of second byte
            (uint64_t)(*(p + 1)) & (uint64_t)0x3FULL
        ) << (uint64_t)12ULL;
        c += (  // 00111111 of third byte
            (uint64_t)(*(p + 2)) & (uint64_t)0x3FULL
        ) << (uint64_t)6ULL;
        c += (  // 00111111 of fourth byte
            (uint64_t)(*(p + 3)) & (uint64_t)0x3FULL
        );
        if (c <= 0xFFFFULL)
            return 0;  // not valid to be encoded with four bytes.
        if (out) *out = c;
        if (cpbyteslen) *cpbyteslen = 4;
        return 1;
    }
    return 0;
}

int is_valid_utf8_char(
        const unsigned char *p, int size
        ) {
    if (!get_utf8_codepoint(p, size, NULL, NULL))
        return 0;
    return 1;
}

