// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_SCRIPTCORE_H_
#define RFS2_SCRIPTCORE_H_

#include <lua.h>
#include <stdint.h>

#define OBJREF_TEXTURE 1
#define OBJREF_AUDIODEVICE 2
#define OBJREF_PLAYINGSOUND 3
#define OBJREF_FILE 4
#define OBJREF_ARCHIVE 5
#define OBJREF_DOWNLOAD 6
#define OBJREF_ROOMOBJ 7
#define OBJREF_ROOMLAYER 8
#define OBJREF_FONT 9
#define OBJREF_PRELOADEDSOUND 10

#define OBJREFMAGIC 4862490

typedef struct scriptobjref {
    int magic;
    int type;
    int64_t value;
    int64_t value2;
} scriptobjref;


char *_prefix__file__(
    const char *contents, const char *filepath
);

int scriptcore_Run();

lua_State *scriptcore_MainState();

#endif  // RFS2_SCRIPTCORE_H_
