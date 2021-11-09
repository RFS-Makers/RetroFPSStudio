
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
#include "scriptcore.h"
#include "settings.h"
#include "vfs.h"
#include "widechar.h"


int main_IsEditorExecutable() {
    return 1;
}

int main(int argc, char **argv) {
    #if defined(DEBUG_VFS) && !defined(NDEBUG)
    printf("rfsc/main.c: debug: calling vfs_Init()\n");
    #endif
    vfs_Init(argv[0]);

    #if defined(HAVE_SDL)
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
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|
                 SDL_INIT_EVENTS|SDL_INIT_TIMER) != 0) {
        int recovered = 0;
        if (!strstr(SDL_GetError(), "No available video device")) {
            SDL_SetHintWithPriority(
                "SDL_VIDEODRIVER", "dummy", SDL_HINT_OVERRIDE
            );
            if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|
                    SDL_INIT_EVENTS|SDL_INIT_TIMER) == 0)
                recovered = 1;
        }
        if (!recovered) {
            fprintf(stderr, "rfsc/main.c: error: SDL "
                    "initialization failed: %s\n", SDL_GetError());
            return 1;
        }
    }
    // Initialize non-essentials:
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) == 0)
        SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    SDL_InitSubSystem(SDL_INIT_HAPTIC);
    #endif

    #if defined(DEBUG_VFS) && !defined(NDEBUG)
    printf("rfsc/main.c: debug: calling vfs_Init()\n");
    #endif

    return scriptcore_Run(argc, argv);
}


#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
static int str_is_spaces(const char *s) {
    if (!*s)
        return 0;
    while (*s) {
        if (*s == ' ') {
            s++;
            continue;
        }
        return 0;
    }
    return 1;
}


int WINAPI WinMain(
        ATTR_UNUSED HINSTANCE hInst, ATTR_UNUSED HINSTANCE hPrev,
        ATTR_UNUSED LPSTR szCmdLine, ATTR_UNUSED int sw) {
    int argc = 0;
    char **argv = malloc(sizeof(*argv));
    if (!argv)
        return 1;
    char *execfullpath = filesys_GetOwnExecutable();
    char *execname = NULL;
    if (execfullpath) {
        char *execname = filesys_Basename(execfullpath);
        free(execfullpath);
        execfullpath = NULL;
    }
    if (execname) {
        argv[0] = strdup(execname);
    } else {
        argv[0] = strdup("RFS2.exe");
    }
    if (!argv[0])
        return 1;
    argc = 1;

    int _winSplitCount = 0;
    LPWSTR *_winSplitList = NULL;
    _winSplitList = CommandLineToArgvW(
        GetCommandLineW(), &_winSplitCount
    );
    if (!_winSplitList) {
        oom: ;
        if (_winSplitList) LocalFree(_winSplitList);
        int i = 0;
        while (i < argc) {
            free(argv[i]);
            i++;
        }
        free(argv);
        fprintf(
            stderr, "rfsc/main.c: error: "
            "arg alloc or convert failure"
        );
        return -1;
    }
    if (_winSplitCount > 1) {
        char **argv_new = realloc(
            argv, sizeof(*argv) * (argc + _winSplitCount - 1)
        );
        if (!argv_new)
            goto oom;
        argv = argv_new;
        memset(&argv[argc], 0, sizeof(*argv) * (_winSplitCount - 1));
        argc += _winSplitCount - 1;
        int k = 1;
        while (k < _winSplitCount) {
            assert(sizeof(wchar_t) == sizeof(uint16_t));
            char *argbuf = NULL;
            argbuf = AS_U8_FROM_U16(
                _winSplitList[k]
            );
            if (!argbuf)
                goto oom;
            assert(k < argc);
            argv[k] = argbuf;
            k++;
        }
    }
    LocalFree(_winSplitList);
    _winSplitList = NULL;
    SDL_SetMainReady();
    int result = SDL_main(argc, argv);
    int k = 0;
    while (k < argc) {
        free(argv[k]);
        k++;
    }
    free(argv);
    return result;
}
#endif

