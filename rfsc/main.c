// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
#if !defined(_WIN32) && !defined(_WIN64)
#include <signal.h>
#endif
#include <stdio.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#include "filesys.h"
#include "scriptcore.h"
#include "sdl/sdlinit.h"
#include "settings.h"
#include "vfs.h"
#include "widechar.h"


int main_consolewasspawned = 0;
_Atomic volatile int main_gotsigintsigterm = 0;
#if !defined(_WIN32) && !defined(_WIN64)
void sigint_handler(int signo) {
    assert(signo == SIGINT);
    main_gotsigintsigterm = 1;
}


void sigterm_handler(int signo) {
    assert(signo == SIGTERM);
    main_gotsigintsigterm = 1;
}


int main_EnsureConsole() {
    return 1;
}


int main_ConsoleWasSpawned() {return 0;}
#else
static int main_havewinconsole = 0;
static int main_triedconsolealloc = 0;

int main_EnsureConsole() {
    if (main_havewinconsole)
        return 1;
    if (main_triedconsolealloc)
        return 0;
    main_triedconsolealloc();
    if (AllocConsole()) {
        main_consolewasspawned = 1;
        main_havewinconsole = 1;
    }
    return main_havewinconsole;
}


int main_ConsoleWasSpawned() {
    return main_consolewasspawned;
}
#endif


int main_IsEditorExecutable() {
    return 1;
}


int main(int argc, char **argv) {
    #if defined(DEBUG_STARTUP)
    printf("rfsc/main.c: debug: entering main(), let's go!\n");
    #endif
    #if !defined(_WIN32) && !defined(_WIN64)
    #if defined(DEBUG_STARTUP)
    printf("rfsc/main.c: debug: installing signal handlers...\n");
    #endif
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigterm_handler);
    #endif
    #if (defined(DEBUG_VFS) || defined(DEBUG_STARTUP))
    printf("rfsc/main.c: debug: calling vfs_Init()\n");
    #endif
    vfs_Init(argv[0]);

    #if defined(DEBUG_STARTUP)
    printf("rfsc/main.c: debug: calling scriptcore_Run() now\n");
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
    if (AttachConsole(ATTACH_PARENT_PROCESS))
        main_havewinconsole = 1;
    #if defined(DEBUG_STARTUP)
    printf("rfsc/main.c: debug: WinMain() entered, "
        "doing argument reformatting...\n");
    #endif
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

