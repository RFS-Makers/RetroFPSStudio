// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details


#include "compileconfig.h"

#include <assert.h>
#if defined(HAVE_SDL)
#include <SDL2/SDL.h>
#endif
#include <stdio.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#include "filesys.h"
#include "main.h"
#include "scriptcore.h"
#include "settings.h"
#include "vfs.h"
#include "widechar.h"


static _Atomic volatile int _sdlinitdone = 0;

int sdlinit_Do() {
    if (_sdlinitdone)
        return 1;
    #if defined(HAVE_SDL)
    #if defined(DEBUG_STARTUP)
    printf("rfsc/sdlinit.c: debug: overriding some SDL2 hints\n");
    #endif
    if (main_IsEditorExecutable()) {
        SDL_SetHintWithPriority(
            "SDL_APP_NAME", "Retro FPS Studio",
            SDL_HINT_OVERRIDE
        );
        SDL_SetHintWithPriority(  // older SDL2 versions
            "SDL_HINT_AUDIO_DEVICE_APP_NAME",
            "Retro FPS Studio",
            SDL_HINT_OVERRIDE
        );
        SDL_SetHintWithPriority(
            "SDL_SCREENSAVER_INHIBIT_ACTIVITY_NAME",
            "Using Retro FPS Studio",
            SDL_HINT_OVERRIDE
        );
    } else {
        SDL_SetHintWithPriority(
            "SDL_APP_NAME", "RFS Game",
            SDL_HINT_OVERRIDE
        );
        SDL_SetHintWithPriority(  // older SDL2 versions
            "SDL_HINT_AUDIO_DEVICE_APP_NAME",
            "Retro FPS Studio",
            SDL_HINT_OVERRIDE
        );
        SDL_SetHintWithPriority(
            "SDL_SCREENSAVER_INHIBIT_ACTIVITY_NAME",
            "Playing an RFS game",
            SDL_HINT_OVERRIDE
        );
    }
    SDL_SetHintWithPriority(
        "SDL_MOUSE_FOCUS_CLICKTHROUGH", "1",
        SDL_HINT_OVERRIDE
    );
    SDL_SetHintWithPriority(
        "SDL_RENDER_SCALE_QUALITY",
        (global_smooth_textures ? "2" : "0"), SDL_HINT_OVERRIDE
    );
    SDL_SetHintWithPriority(
        "SDL_MOUSE_TOUCH_EVENTS", "0", SDL_HINT_OVERRIDE
    );
    SDL_SetHintWithPriority(
        "SDL_TOUCH_MOUSE_EVENTS", "1", SDL_HINT_OVERRIDE
    );
    SDL_SetHintWithPriority(
        "SDL_TIMER_RESOLUTION", "0", SDL_HINT_OVERRIDE
    );
    SDL_SetHintWithPriority(
        "SDL_MOUSE_FOCUS_CLICKTHROUGH", "1", SDL_HINT_OVERRIDE
    );

    // Initialize essentials:
    #if defined(DEBUG_STARTUP)
    printf("rfsc/sdlinit.c: debug: initial SDL_Init() call now\n");
    #endif
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|
                 SDL_INIT_EVENTS|SDL_INIT_TIMER) != 0) {
        #if defined(DEBUG_STARTUP)
        printf("rfsc/sdlinit.c: debug: SDL_Init() failed: %s\n",
            SDL_GetError());
        #endif
        int recovered = 0;
        if (!strstr(SDL_GetError(), "No available video device")) {
            #if defined(DEBUG_STARTUP)
            printf("rfsc/sdlinit.c: debug: repeated SDL_Init() "
                "with dummy video device\n");
            #endif
            SDL_SetHintWithPriority(
                "SDL_VIDEODRIVER", "dummy", SDL_HINT_OVERRIDE
            );
            if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|
                    SDL_INIT_EVENTS|SDL_INIT_TIMER) == 0)
                recovered = 1;
        }
        if (!recovered) {
            fprintf(stderr, "rfsc/sdlinit.c: error: SDL "
                    "initialization failed: %s\n", SDL_GetError());
            return 0;
        }
    }
    // Initialize non-essentials:
    #if defined(DEBUG_STARTUP)
    printf("rfsc/sdlinit.c: debug: SDL_InitSubSystem() on gamepads "
        "and haptics\n");
    #endif
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) == 0)
        SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    SDL_InitSubSystem(SDL_INIT_HAPTIC);
    #endif

    #if defined(DEBUG_STARTUP)
    printf("rfsc/sdlinit.c: debug: all done.\n");
    #endif

    _sdlinitdone = 1;
    return 1;
}


int sdlinit_WasDone() {
    return _sdlinitdone;
}
