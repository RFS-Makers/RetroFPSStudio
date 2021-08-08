// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

// This file includes miniz to pull it into an object file.
// It will also make it compatible with our VFS, instead
// of direct disk access.


#define _FILE_OFFSET_BITS 64
#ifndef __USE_LARGEFILE64
#define __USE_LARGEFILE64 1
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#define _LARGEFILE_SOURCE
#if defined(_WIN32) || defined(_WIN64)
#define fseek64 _fseeki64
#define ftell64 _ftelli64
#else
#define fseek64 fseeko64
#define ftell64 ftello64
#endif

#include "compileconfig.h"

#include <stdio.h>
#include <stdlib.h>


#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "miniz/miniz.c"
