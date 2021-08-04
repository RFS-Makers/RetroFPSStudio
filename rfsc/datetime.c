// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

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


uint64_t datetime_TicksWithSuspendJump() {
    #if defined(_WIN32) || defined(_WIN64)
    return GetTickCount64();
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
