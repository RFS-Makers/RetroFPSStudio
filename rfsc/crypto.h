// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_CRYPTO_H_
#define RFS2_CRYPTO_H_

#include "compileconfig.h"

#include <stdint.h>

typedef struct sha512cryptjob sha512cryptjob;


void crypto_hashmd5(
    const char *data, uint64_t datalen,
    char *out_hash  // must be 33 bytes (null terminator!!)
);

char *crypto_sha512crypt(
    const char *key, const char *salt,
    int64_t iterations  // set to 0 for default
);  // free() return value later! returns NULL on OOM

sha512cryptjob *crypto_sha512crypt_SpawnThreaded(
    const char *key, const char *salt,
    int64_t iterations
);

int crypto_sha512crypt_CheckThreadedDone(
    sha512cryptjob *job
);

char *crypto_sha512crypt_GetThreadedResultAndFree(
    sha512cryptjob *job
);  // free() return value later! returns NULL on OOM

void crypto_hashsha512(
    const char *data, uint64_t datalen,
    char *out_hash  // must be 129 bytes (null terminator!!)
);

#endif  // RFS2_CRYPTO_H_

