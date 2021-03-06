// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#include "threading.h"

mutex *_winticksmutex = NULL;
uint64_t lastwinticks = 0;
uint64_t winticksoffset = 0;

static __attribute__((constructor)) void _create_winticksmutex() {
    if (!_winticksmutex)
        _winticksmutex = mutex_Create();
}


uint64_t datetime_TicksWithSuspendJump() {
    #if defined(_WIN32) || defined(_WIN64)
    if (!_winticksmutex)
        _winticksmutex = mutex_Create();
    if (!_winticksmutex)
        return 0;
    mutex_Lock(_winticksmutex);
    uint64_t newticks = GetTickCount() + winticksoffset;
    if (newticks < lastwinticks) {
        winticksoffset += (lastwinticks - newticks);
        mutex_Release(_winticksmutex);
        return lastwinticks;
    }
    lastwinticks = newticks;
    mutex_Release(_winticksmutex);
    return newticks;
    #else
    struct timespec spec;
    clock_gettime(CLOCK_BOOTTIME, &spec);
    uint64_t ms1 = ((uint64_t)spec.tv_sec) * 1000ULL;
    uint64_t ms2 = ((uint64_t)spec.tv_nsec) / 1000000ULL;
    return ms1 + ms2;
    #endif
}


mutex *_nosuspendticksmutex = NULL;
int64_t _nosuspendlastticks = 0;
int64_t _nosuspendoffset = 0;

static __attribute__((constructor)) void _create_nosuspendmutex() {
    if (!_nosuspendticksmutex)
        _nosuspendticksmutex = mutex_Create();
}

uint64_t datetime_Ticks() {
    if (!_nosuspendticksmutex) {
        _nosuspendticksmutex = mutex_Create();
    if (!_nosuspendticksmutex)
        return 0;
    }
    int64_t ticks = datetime_TicksWithSuspendJump();
    mutex_Lock(_nosuspendticksmutex);
    ticks += _nosuspendoffset;
    if (ticks > _nosuspendlastticks + 1000L) {
        _nosuspendoffset -= (ticks - _nosuspendlastticks - 10L);
        ticks = _nosuspendlastticks + 10L;
    } else {
        _nosuspendlastticks = ticks;
    }
    mutex_Release(_nosuspendticksmutex);
    return ticks;
}


void datetime_Sleep(uint64_t sleepms) {
    if (sleepms <= 0)
        return;
    #if defined(_WIN32) || defined(_WIN64)
    Sleep(sleepms);
    #else
    #if _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    ts.tv_sec = sleepms / 1000LL;
    ts.tv_nsec = (sleepms % 1000LL) * 1000000LL;
    nanosleep(&ts, NULL);
    #else
    if (sleepms >= 1000LL) {
        sleep(sleepms / 1000LL);
        sleepms = sleepms % 1000LL;
    }
    usleep(1000LL * sleepms);
    #endif
    #endif
}
