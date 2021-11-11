// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "crypto.h"
#include "md5.h"
#include "openssl/sha.h"
#include "sha512crypt/sha512crypt.h"
#include "threading.h"


void crypto_hashmd5(
        const char *data, uint64_t datalen,
        char *out_hash) {
    size_t plen = datalen;
    const char *p = data;
    NOTOSSL_MD5_CTX c = {0};
    NOTOSSL_MD5_Init(&c);
    NOTOSSL_MD5_Update(&c, p, plen);
    char hashdata[16];
    NOTOSSL_MD5_Final((uint8_t *)hashdata, &c);
    char dighex[16 * 2 + 1];
    int i = 0;
    while (i < 16) {
        char c[3];
        snprintf(c, 3, "%x", hashdata[i]);
        dighex[i * 2] = tolower(c[0]);
        dighex[i * 2 + 1] = tolower(c[1]);
        i++;
    }
    dighex[16 * 2] = '\0';
    memcpy(out_hash, dighex, 33);
}


static mutex *_sha512cryptjob_mutex = NULL;
typedef struct sha512cryptjob {
    char *key, *salt, *result;
    int64_t iterations;
    _Atomic volatile int done;
    thread *t;
} sha512cryptjob;


static __attribute__((constructor)) void _sha512cryptjob_mutex_Make() {
    if (!_sha512cryptjob_mutex)
        _sha512cryptjob_mutex = mutex_Create();
}


void _crypto_sha512cryptjob_do(void* userdata) {
    sha512cryptjob *job = (sha512cryptjob *)userdata;
    char *r = crypto_sha512crypt(
        job->key, job->salt, job->iterations);
    mutex_Lock(_sha512cryptjob_mutex);
    job->result = r;
    job->done = 1;
    mutex_Release(_sha512cryptjob_mutex);
}


sha512cryptjob *crypto_sha512crypt_SpawnThreaded(
        const char *key, const char *salt,
        int64_t iterations
        ) {
    if (!_sha512cryptjob_mutex)
        _sha512cryptjob_mutex = mutex_Create();
    if (!_sha512cryptjob_mutex)
        return NULL;

    mutex_Lock(_sha512cryptjob_mutex);
    sha512cryptjob *job = malloc(sizeof(*job));
    if (!job) {
        mutex_Release(_sha512cryptjob_mutex);
        return NULL;
    }
    memset(job, 0, sizeof(*job));
    job->key = strdup(key);
    job->salt = strdup(salt);
    job->iterations = iterations;
    if (!job->key || !job->salt) {
        errorquit: ;
        free(job->key);
        free(job->salt);
        free(job);
        mutex_Release(_sha512cryptjob_mutex);
        return NULL;
    }
    job->t = thread_SpawnWithPriority(
        THREAD_PRIO_LOW, &_crypto_sha512cryptjob_do, job);
    if (!job->t)
        goto errorquit;
    mutex_Release(_sha512cryptjob_mutex);
    return job;
}


int crypto_sha512crypt_CheckThreadedDone(
        sha512cryptjob *job
        ) {
    if (!job)
        return 0;

    mutex_Lock(_sha512cryptjob_mutex);
    if (job->done) {
        mutex_Release(_sha512cryptjob_mutex);
        return 1;
    }
    mutex_Release(_sha512cryptjob_mutex);
    return 0;
}


char *crypto_sha512crypt_GetThreadedResultAndFree(
        sha512cryptjob *job
        ) {
    mutex_Lock(_sha512cryptjob_mutex);
    #ifndef NDEBUG
    assert(job && job->done);
    #else
    if (!job->done) {
        mutex_Release(_sha512cryptjob_mutex);
        return NULL;
    }
    #endif
    char *result = job->result;
    thread_Join(job->t);
    free(job->key);
    free(job->salt);
    free(job);
    mutex_Release(_sha512cryptjob_mutex);
    return result;
}


char *crypto_sha512crypt(
        const char *key, const char *salt,
        int64_t iterations
        ) {
    if (!key || !salt)
        return NULL;

    int64_t rounds = 5000;
    if (iterations > 0) rounds = iterations;
    char rounds_str[64];
    snprintf(rounds_str, sizeof(rounds_str) - 1,
        "$6$rounds=%" PRId64 "$", rounds);
    char *saltnullterminated_rounds = malloc(
        strlen(rounds_str) + strlen(salt) + 1);
    if (!saltnullterminated_rounds) {
        free(saltnullterminated_rounds);
        return NULL;
    }
    memcpy(saltnullterminated_rounds,
           rounds_str, strlen(rounds_str));
    memcpy(saltnullterminated_rounds + strlen(rounds_str),
           salt, strlen(salt));
    saltnullterminated_rounds[
        strlen(rounds_str) + strlen(salt)] = '\0';
    char *result = sha512_crypt(
        key, saltnullterminated_rounds);
    free(saltnullterminated_rounds);
    if (!result) {
        return NULL;
    }
    int dollars_seen = 0;
    int i = 0;
    while (i < (int)strlen(result)) {
        if (result[i] == '$') {
            dollars_seen++;
            if (dollars_seen == 4) {
                memmove(result, result + i + 1,
                        strlen(result) - (i + 1) + 1);
                break;
            }
        }
        i++;
    }
    return result;
}


void crypto_hashsha512(
        const char *data, uint64_t datalen,
        char *out_hash
        ) {
    size_t plen = datalen;
    const char *p = data;
    uint8_t dig[SHA512_DIGEST_LENGTH];
    SHA512((void *)p, plen, dig);
    char dighex[SHA512_DIGEST_LENGTH * 2 + 1];
    int i = 0;
    while (i < SHA512_DIGEST_LENGTH) {
        char c[3];
        snprintf(c, 3, "%x", dig[i]);
        dighex[i * 2] = tolower(c[0]);
        dighex[i * 2 + 1] = tolower(c[1]);
        i++;
    }
    dighex[SHA512_DIGEST_LENGTH * 2] = '\0';
    memcpy(out_hash, dighex, SHA512_DIGEST_LENGTH * 2 + 1);
}

