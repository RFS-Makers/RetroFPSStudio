
#ifndef RFS2_WIDECHAR_H_
#define RFS2_WIDECHAR_H_

#include <stdint.h>

char *AS_U8_FROM_U16(const uint16_t *s);

uint16_t *AS_U16(const char *s);

int get_utf8_codepoint(
    const unsigned char *p, int size,
    uint64_t *out, int *cpbyteslen
);

int utf8_char_len(const unsigned char *p);

int is_valid_utf8_char(
    const unsigned char *p, int size
);

#endif  // RFS2_WIDECHAR_H_
