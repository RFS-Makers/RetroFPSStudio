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

//#define DEBUG_VFS
//#define DEBUG_3DRENDERER
//#define DEBUG_3DRENDERER_EXTRA
//#define DEBUG_TEXTURES
//#define DEBUG_RENDERER
//#define DEBUG_TOUCH
//#define DEBUG_DOWNLOADS
//#define DEBUG_FS
//#define DEBUG_AUDIODECODE
#define DEBUG_MIDIPARSER
#define DEBUG_MIDIPARSER_EXTRA
#define HAVE_SDL

#endif  // RFS2_COMPILECONFIG_H_
