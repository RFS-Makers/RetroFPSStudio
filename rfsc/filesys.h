// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_FILESYS_H_
#define RFS2_FILESYS_H_

#include "compileconfig.h"

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#define h64filehandle HANDLE
#define H64_NOFILE (INVALID_HANDLE_VALUE)
#else
typedef int h64filehandle;
#define H64_NOFILE ((int)0)
#endif

enum {
    FS32_ERR_SUCCESS = 0,
    FS32_ERR_NOPERMISSION = -1,
    FS32_ERR_TARGETNOTADIRECTORY = -2,
    FS32_ERR_TARGETNOTAFILE = -3,
    FS32_ERR_NOSUCHTARGET = -4,
    FS32_ERR_OUTOFMEMORY = -5,
    FS32_ERR_TARGETALREADYEXISTS = -6,
    FS32_ERR_INVALIDNAME = -7,
    FS32_ERR_OUTOFFDS = -8,
    FS32_ERR_PARENTSDONTEXIST = -9,
    FS32_ERR_DIRISBUSY = -10,
    FS32_ERR_NONEMPTYDIRECTORY = -11,
    FS32_ERR_SYMLINKSWEREEXCLUDED = -12,
    FS32_ERR_IOERROR = -13,
    FS32_ERR_UNSUPPORTEDPLATFORM = -14,
    FS32_ERR_OTHERERROR = -15
};

const char *filesys_DocumentsFolder();

int filesys_TargetExists(const char *path, int *result);

int filesys_IsDirectory(const char *path, int *result);

int filesys_RemoveFolderRecursively(
    const char *path, int *error
);

int filesys_RemoveFileOrEmptyDir(const char *path, int *error);

char *filesys_AppDataSubFolder(
    const char *appname
);

int filesys_IsSymlink(const char *path, int *result);

void filesys_FreeFolderList(char **list);

int filesys_ListFolder(
    const char *path, char ***contents,
    int returnFullPath, int *error
);

int filesys_ListFolderEx(
    const char *path, char ***contents,
    int returnFullPath, int allowsymlink, int *error
);

int filesys_GetComponentCount(const char *path);

void filesys_RequestFilesystemAccess();

int filesys_CreateDirectory(const char *path);

char *filesys_GetOwnExecutable();

int filesys_LaunchExecutable(
    const char *path, int argc, const char **argv
);

char *filesys_ParentdirOfItem(const char *path);

char *filesys_Join(const char *path1, const char *path2);

int filesys_IsAbsolutePath(const char *path);

char *filesys_ToAbsolutePath(const char *path);

char *filesys_Basename(const char *path);

char *filesys_Dirname(const char *path);

int filesys_GetSize(const char *path, uint64_t *size, int *innererr);

char *filesys_Normalize(const char *path);

char *filesys_GetCurrentDirectory();

char *filesys_GetRealPath(const char *s);

char *filesys_TurnIntoPathRelativeTo(
    const char *path, const char *makerelativetopath
);

int filesys_PathCompare(const char *p1, const char *p2);

int filesys_FolderContainsPath(
    const char *folder_path, const char *check_path,
    int *result
);

FILE *filesys_OpenFromPath(
    const char *path, const char *mode, int *err
);


enum {
    _WIN32_OPEN_LINK_ITSELF = 0x1,
    _WIN32_OPEN_DIR = 0x2,
    OPEN_ONLY_IF_NOT_LINK = 0x4
};

h64filehandle filesys_OpenFromPathAsOSHandleEx(
    const char *path, const char *mode, int flags, int *err
);

int filesys_IsObviouslyInvalidPath(const char *p);

ATTR_UNUSED static FILE *_dupfhandle(FILE *f, const char* mode) {
    int fd = -1;
    int fd2 = -1;
    #if defined(_WIN32) || defined(_WIN64)
    fd = _fileno(f);
    fd2 = _dup(fd);
    #else
    fd = fileno(f);
    fd2 = dup(fd);
    #endif
    if (fd2 < 0)
        return NULL;
    FILE *f2 = fdopen(fd2, mode);
    if (!f2) {
        #if defined(_WIN32) || defined(_WIN64)
        _close(fd2);
        #else
        close(fd2);
        #endif
        return NULL;
    }
    return f2;
}

int filesys_SetExecBit(const char *p);

FILE *filesys_OpenOwnExecutable();

FILE *filesys_TempFile(
    int subfolder, int folderonly,
    const char *prefix, const char *suffix,
    char **folder_path, char **path
);

char *filesys_FindLibrary(const char *name);

#endif  // RFS2_FILESYS_H_
