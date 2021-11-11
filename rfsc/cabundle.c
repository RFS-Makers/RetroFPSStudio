// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "filesys.h"
#include "threading.h"
#include "vfs.h"

#define CA_BUNDLE_VFS_PATH "ca-bundle.crt"


static char *_ca_bundle_folder_path = NULL;
static char *_ca_bundle_file_path = NULL;
static mutex *_ca_bundle_mutex = NULL;


static __attribute__((constructor)) void _createcurlmutex() {
    if (!_ca_bundle_mutex)
        _ca_bundle_mutex = mutex_Create();
}


const char *cabundle_GetPath() {
    if (!_ca_bundle_mutex)
        _ca_bundle_mutex = mutex_Create();
    if (!_ca_bundle_mutex)
        return NULL;

    mutex_Lock(_ca_bundle_mutex);
    if (_ca_bundle_file_path) {
        mutex_Release(_ca_bundle_mutex);
        return _ca_bundle_file_path;
    }

    #ifdef DEBUG_DOWNLOADS
    fprintf(stderr,
        "rfsc/cabundle.c: debug: cabundle_GetPath() is "
        "now writing out bundle to disk...\n");
    #endif

    char *folder_path = NULL;
    FILE *f = filesys_TempFile(
        1, 1, "rfs2-cabundle-", "",
        &folder_path, NULL
    );
    assert(f == NULL);  // Since we requested a folder ONLY.
    if (!folder_path) {
        #ifdef DEBUG_DOWNLOADS
        fprintf(stderr,
            "rfsc/cabundle.c: error: cabundle_GetPath() "
            "got NULL folder from filesys_TempFile()\n");
        #endif
        mutex_Release(_ca_bundle_mutex);
        return NULL;
    }
    #ifdef DEBUG_DOWNLOADS
    fprintf(stderr,
        "rfsc/cabundle.c: debug: cabundle_GetPath() "
        "will extract to folder: %s\n", folder_path);
    #endif

    char *bytes = NULL;
    char *file_path = filesys_Join(folder_path, "ca-bundle.crt");
    if (!file_path) {
        haderror: ;
        #ifdef DEBUG_DOWNLOADS
        fprintf(stderr,
            "rfsc/cabundle.c: debug: cabundle_GetPath() "
            "extraction failure, disk I/O error or "
            "out of memory\n");
        #endif
        free(folder_path);
        free(file_path);
        free(bytes);
        int _err = 0;
        filesys_RemoveFolderRecursively(folder_path, &_err); 
        mutex_Release(_ca_bundle_mutex);
        return NULL;
    }

    int _exists = 0;
    if (!vfs_Exists(CA_BUNDLE_VFS_PATH, &_exists,
            VFSFLAG_NO_REALDISK_ACCESS) || !_exists)
        goto haderror;
    uint64_t byteslen = 0;
    if (!vfs_Size(CA_BUNDLE_VFS_PATH, &byteslen,
            VFSFLAG_NO_REALDISK_ACCESS) || byteslen == 0)
        goto haderror;
    bytes = malloc(byteslen);
    if (!bytes)
        goto haderror;
    if (!vfs_GetBytes(CA_BUNDLE_VFS_PATH, 0,
            byteslen, bytes,
            VFSFLAG_NO_REALDISK_ACCESS))
        goto haderror;

    int _err = 0;
    f = filesys_OpenFromPath(file_path, "wb", &_err);
    if (!f)
        goto haderror;
    if (fwrite(bytes, 1, byteslen, f) != byteslen) {
        fclose(f);
        goto haderror;
    }
    fclose(f);
    free(bytes);
    bytes = NULL;

    _ca_bundle_folder_path = folder_path;
    _ca_bundle_file_path = file_path;
    #ifdef DEBUG_DOWNLOADS
    fprintf(stderr,
        "rfsc/cabundle.c: debug: cabundle_GetPath() "
        "finished writing bundle to %s\n",
        _ca_bundle_file_path);
    #endif

    mutex_Release(_ca_bundle_mutex);
    return _ca_bundle_file_path;
}
