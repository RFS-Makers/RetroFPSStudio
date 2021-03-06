// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#ifndef WINDOWS
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64) || defined(__WIN32__)
#define WINDOWS
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#if defined __MINGW_H
#define _WIN32_IE 0x0400
#endif
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sched.h>
#endif
#if defined(__linux__) || defined(linux) || defined(__linux)
#include <linux/sched.h>
#endif


#include <assert.h>
#include <stdlib.h>
#include <string.h>
#ifdef WINDOWS
#include <process.h>
#include <windows.h>
#else
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#endif

#include "secrandom.h"
#include "threading.h"


typedef struct mutex {
#ifdef WINDOWS
    HANDLE m;
#else
    pthread_mutex_t m;
#endif
} mutex;


typedef struct semaphore {
#ifdef WINDOWS
    HANDLE s;
#else
#if defined(__APPLE__) || defined(__OSX__)
    sem_t *s;
    char *sname;
#else
    sem_t s;
#endif
#endif
} semaphore;


typedef struct threadinfo {
#ifdef WINDOWS
    HANDLE t;
#else
    pthread_t t;
#endif
} thread;


#if defined(__APPLE__) || defined(__OSX__)
uint64_t _last_sem_id = 0;
mutex *synchronize_sem_id = NULL;
uint64_t uniquesemidpart = 0;
__attribute__((constructor)) void __init_semId_synchronize() {
    synchronize_sem_id = mutex_Create();
    secrandom_GetBytes(
        (char*)&uniquesemidpart, sizeof(uniquesemidpart)
    );
}
#endif


semaphore* semaphore_Create(int value) {
    semaphore* s = malloc(sizeof(*s));
    if (!s) {
        return NULL;
    }
    memset(s, 0, sizeof(*s));
#ifdef WINDOWS
    s->s = CreateSemaphore(NULL, value, INT_MAX, NULL);
    if (!s->s) {
        free(s);
        return NULL;
    }
#elif defined(__APPLE__) || defined(__OSX__)
    mutex_Lock(synchronize_sem_id);
    _last_sem_id++;
    uint64_t semid = _last_sem_id;
    mutex_Release(synchronize_sem_id);
    char sem_name[128];
    snprintf(
        sem_name, sizeof(sem_name) - 1,
        "/rfssemaphore%" PRIu64 "id%" PRIu64,
        uniquesemidpart, semid
    );
    s->sname = strdup(sem_name);
    if (!s->sname) {
        free(s);
        return NULL;
    }
    s->s = sem_open(sem_name, O_CREAT, 0600, value);
    if (s->s == SEM_FAILED) {
        free(s->sname);
        free(s);
        return NULL;
    }
#else
    if (sem_init(&s->s, 0, value) != 0) {
        free(s);
        return NULL;
    }
#endif
    return s;
}


void semaphore_Wait(semaphore* s) {
#ifdef WINDOWS
    WaitForSingleObject(s->s, INFINITE);
#else
#if defined(__APPLE__) || defined(__OSX__)
    sem_wait(s->s);
#else
    sem_wait(&s->s);
#endif
#endif
}


void semaphore_Post(semaphore* s) {
#ifdef WINDOWS
    ReleaseSemaphore(s->s, 1, NULL);
#else
#if defined(__APPLE__) || defined(__OSX__)
    sem_post(s->s);
#else
    sem_post(&s->s);
#endif
#endif
}


void semaphore_Destroy(semaphore* s) {
    if (!s) {
        return;
    }
#ifdef WINDOWS
    CloseHandle(s->s);
#else
#if defined(__APPLE__) || defined(__OSX__)
    sem_unlink(s->sname);
    free(s->sname);
#else
    sem_destroy(&s->s);
#endif
#endif
    free(s);
}


mutex* mutex_Create() {
    mutex* m = malloc(sizeof(*m));
    if (!m) {
        return NULL;
    }
    memset(m, 0, sizeof(*m));
#ifdef WINDOWS
    m->m = CreateMutex(NULL, FALSE, NULL);
    if (!m->m) {
        free(m);
        return NULL;
    }
#else
    pthread_mutexattr_t blub;
    pthread_mutexattr_init(&blub);
#ifndef NDEBUG
    pthread_mutexattr_settype(&blub, PTHREAD_MUTEX_ERRORCHECK);
#else
    pthread_mutexattr_settype(&blub, PTHREAD_MUTEX_NORMAL);
#endif
    while (pthread_mutex_init(&m->m, &blub) != 0) {
        if (errno != EAGAIN) {
            free(m);
            return NULL;
        }
    }
    pthread_mutexattr_destroy(&blub);
#endif
    return m;
}


void mutex_Destroy(mutex* m) {
    if (!m) {
        return;
    }
#ifdef WINDOWS
    CloseHandle(m->m);
#else
    while (pthread_mutex_destroy(&m->m) != 0) {
        if (errno != EBUSY) {
            break;
        }
    }
#endif
    free(m);
}


void mutex_Lock(mutex* m) {
#ifdef WINDOWS
    WaitForSingleObject(m->m, INFINITE);
#else
#ifndef NDEBUG
    assert(pthread_mutex_lock(&m->m) == 0);
#else
    pthread_mutex_lock(&m->m);
#endif
#endif
}

int mutex_TryLock(mutex* m) {
#ifdef WINDOWS
    if (WaitForSingleObject(m->m, 0) == WAIT_OBJECT_0) {
        return 1;
    }
    return 0;
#else
    if (pthread_mutex_trylock(&m->m) != 0) {
        return 0;
    }
    return 1;
#endif
}


int mutex_TryLockWithTimeout(mutex* m, int32_t timeoutms) {
    if (timeoutms <= 0)
        return mutex_TryLock(m);
#ifdef WINDOWS
    if (WaitForSingleObject(m->m, (DWORD)timeoutms) == WAIT_OBJECT_0) {
        return 1;
    }
    return 0;
#else
    struct timespec msspec;
    clock_gettime(CLOCK_REALTIME, &msspec);
    msspec.tv_nsec += (int32_t)1000 * timeoutms;
    if (pthread_mutex_timedlock(&m->m, &msspec) != 0) {
        return 0;
    }
    return 1;
#endif
}


void mutex_Release(mutex* m) {
#ifdef WINDOWS
    ReleaseMutex(m->m);
#else
#ifndef NDEBUG
    //int i = pthread_mutex_unlock(&m->m);
    //printf("return value: %d\n", i);
    //assert(i == 0);
    assert(pthread_mutex_unlock(&m->m) == 0);
#else
    pthread_mutex_unlock(&m->m);
#endif
#endif
}


void thread_Detach(thread* t) {
#ifdef WINDOWS
    CloseHandle(t->t);
#else
    pthread_detach(t->t);
#endif
    free(t);
}


struct spawninfo {
    void (*func)(void* userdata);
    void* userdata;
};


#ifdef WINDOWS
static unsigned __stdcall spawnthread(void* data) {
#else
static void* spawnthread(void* data) {
#endif
    struct spawninfo* sinfo = data;
    sinfo->func(sinfo->userdata);
    free(sinfo);
#ifdef WINDOWS
    return 0;
#else
    return NULL;
#endif
}


thread *thread_Spawn(
        void (*func)(void *userdata),
        void *userdata
        ) {
    return thread_SpawnWithPriority(
        THREAD_PRIO_NORMAL, func, userdata
    );
}


thread *thread_SpawnWithPriority(
        int priority,
        void (*func)(void* userdata), void *userdata
        ) {
    struct spawninfo* sinfo = malloc(sizeof(*sinfo));
    if (!sinfo)
        return NULL;
    memset(sinfo, 0, sizeof(*sinfo));
    sinfo->func = func;
    sinfo->userdata = userdata;
    thread *t = malloc(sizeof(*t));
    if (!t) {
        free(sinfo);
        return NULL;
    }
    memset(t, 0, sizeof(*t));
#ifdef WINDOWS
    HANDLE h = (HANDLE)_beginthreadex(NULL, 0, spawnthread, sinfo, 0, NULL);
    t->t = h;
#else
    while (pthread_create(&t->t, NULL, spawnthread, sinfo) != 0) {
        assert(errno == EAGAIN);
    }
    #if defined(__linux__) || defined(__LINUX__)
    if (priority == 0) {
        struct sched_param param;
        memset(&param, 0, sizeof(param));
        param.sched_priority = sched_get_priority_min(SCHED_BATCH);
        pthread_setschedparam(t->t, SCHED_BATCH, &param);
    }
    if (priority == 2) {
        struct sched_param param;
        int policy;
        pthread_getschedparam(t->t, &policy, &param);
        param.sched_priority = sched_get_priority_max(policy);
        pthread_setschedparam(t->t, policy, &param);
    }
    #endif
#endif
    return t;
}


#ifndef WINDOWS
static pthread_t mainThread;
#else
static DWORD mainThread;
#endif


__attribute__((constructor)) void thread_MarkAsMainThread(void) {
    // mark current thread as main thread
#ifndef WINDOWS
    mainThread = pthread_self();
#else
    mainThread = GetCurrentThreadId();
#endif
}


int thread_InMainThread() {
#ifndef WINDOWS
    return (pthread_self() == mainThread);
#else
    return (GetCurrentThreadId() == mainThread);
#endif
}


void thread_Join(thread *t) {
#ifdef WINDOWS
    WaitForMultipleObjects(1, &t->t, TRUE, INFINITE);
    CloseHandle(t->t);
#else
    pthread_join(t->t, NULL);
#endif
    free(t);
}
