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
#include "sdl/sdlkeyboard.h"

int sdlkeyboard_IsKeyPressed(const char *name) {
    uint64_t keysym = 0;
    if (!ConvertStrToSDLKey(name, &keysym))
        return 0; 
    SDL_Scancode scancode = SDL_GetScancodeFromKey(
        (SDL_Keycode)keysym
    );
    const uint8_t *state = SDL_GetKeyboardState(NULL);
    return (state[scancode] != 0);
}

#endif  // HAVE_SDL
