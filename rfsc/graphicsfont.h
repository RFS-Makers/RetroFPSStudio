// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_GRAPHICSFONT_H_
#define RFS2_GRAPHICSFONT_H_

#include "compileconfig.h"

#include <stdint.h>

typedef struct rfsfont rfsfont;


rfsfont *graphicsfont_Get(const char *path, char **errmsg);

int graphicsfont_GetLetterWidth(
    rfsfont *f, const char *p, int *byteslen
);

int graphicsfont_CalcWidth(
    rfsfont *f, const char *text,
    double pt_size, double letter_spacing
);

int graphicsfont_CalcHeight(
    rfsfont *f, const char *text, int width,
    double pt_size, double letter_spacing
);

int graphicsfont_Draw(
    rfsfont *f, const char *text, int width,
    int x, int y,
    double r, double g, double b, double a,
    double pt_size, double letter_spacing
);

char **graphicsfont_TextWrap(
    rfsfont *f, const char *text, int width,
    double pt_size, double letter_spacing
);

#endif  // RFS2_GRAPHICSFONT_H_

