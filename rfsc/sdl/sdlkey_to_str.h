// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_SDLKEY_TO_STR_H_
#define RFS2_SDLKEY_TO_STR_H_

#include "compileconfig.h"

#include <stdint.h>

#ifdef HAVE_SDL
int ConvertSDLKeyToStr(uint64_t sdlkeysym, char *result);

int ConvertStrToSDLKey(const char *str, uint64_t *result);
#endif  // HAVE_SDL

#endif  // RFS2_SDLKEY_TO_STR_H_
