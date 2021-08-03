
#ifndef OLDSCHOOLC_HTTP_H_
#define OLDSCHOOLC_HTTP_H_

#include <stdint.h>
#include <stdlib.h>


typedef struct httpdownload httpdownload;


httpdownload *http_NewDownload(
    const char *url, int64_t max_bytes, int retries
);

int64_t http_DownloadedByteCount(httpdownload *hp);

int http_IsDownloadDone(httpdownload *hp);

char *http_GetDownloadResult(httpdownload *hp);

void http_FreeDownload(httpdownload *hp);

char *http_GetDownloadResultAsBytes(httpdownload *hp, int64_t *size);

char *http_GetDownloadMD5(httpdownload *hp);

int http_EnsureLibsLoaded();

int http_IsDownloadFailure(httpdownload *hp);

#endif  // OLDSCHOOLC_HTTP_H_
