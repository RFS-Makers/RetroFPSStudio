// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_HTTP_H_
#define RFS2_HTTP_H_

#include "compileconfig.h"

#include <stdint.h>
#include <stdlib.h>


typedef struct httpdownload httpdownload;


httpdownload *http_NewDownload(
    const char *url, int64_t max_bytes, int retries
);

int http_IsDefinitelyInsecureUrl(const char *url);

int64_t http_DownloadedByteCount(httpdownload *hp);

int http_IsDownloadDone(httpdownload *hp);

char *http_GetDownloadResult(httpdownload *hp);

void http_FreeDownload(httpdownload *hp);

char *http_GetDownloadResultAsBytes(httpdownload *hp, int64_t *size);

char *http_GetDownloadMD5(httpdownload *hp);

int http_EnsureLibsLoaded();

int http_IsDownloadFailure(httpdownload *hp);

#endif  // RFS2_HTTP_H_
