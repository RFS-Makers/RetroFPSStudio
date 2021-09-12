// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_SDLKEYBOARD_H_
#define RFS2_SDLKEYBOARD_H_

#include "compileconfig.h"

#include <stdint.h>

#ifdef HAVE_SDL
int sdlkeyboard_IsKeyPressed(const char *name);
#endif  // HAVE_SDL

#endif  // RFS2_SDLKEYBOARD_H_
