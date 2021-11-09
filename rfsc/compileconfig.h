// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_COMPILECONFIG_H_
#define RFS2_COMPILECONFIG_H_


#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define HOTSPOT __attribute__ ((hot))
#define ATTR_UNUSED __attribute__((unused))

#if defined(__x86_64__) || defined(__x86_64) || defined(_M_AMD64)
#define ARCH_X64
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ARCH_ARM64
#elif defined(__X86__) || defined(_M_IX86) || \
            defined(_X86_) || defined(__i686__)
#define ARCH_X86
#elif defined(__arm__) || defined(_M_ARM)
#define ARCH_ARMV7
#endif

#if defined(_WIN32) || defined(_WIN64)
#define _WIN32_WINNT 0x0502
#endif

//#define DEBUG_VFS
//#define DEBUG_3DRENDERER
//#define DEBUG_3DRENDERER_EXTRA
//#define DEBUG_TEXTURES
//#define DEBUG_RENDERER
//#define DEBUG_TOUCH
//#define DEBUG_DOWNLOADS
//#define DEBUG_FS
//#define DEBUG_AUDIODECODE
#define DEBUG_MIDISONGLIFETIME
//#define DEBUG_MIDIPARSER
//#define DEBUG_MIDIPARSER_EXTRA
#define DEBUG_MIDIPLAYBACK
//#define DEBUG_MIDIPLAYBACK_EXTRA
//#define DEBUG_MIDIPLAYBACK_BUFFERS

#define NOMINIAUDIO
#define HAVE_SDL
#define HAVE_TSF_COPY
#define USE_CURL
#define USE_CURL_STATIC

#endif  // RFS2_COMPILECONFIG_H_
