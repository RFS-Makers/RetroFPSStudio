// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// See LICENSE.md for details.

#ifndef RFS2_SCRIPTCOREEVENTS_H_
#define RFS2_SCRIPTCOREEVENTS_H_


#include <lua.h>


int scriptcoreevents_Process(lua_State *l);

void scriptcoreevents_SetRelativeMouse(int relative);

void scriptcoreevents_AddFunctions(lua_State *l);

#endif  // RFS2_SCRIPTCOREEVENTS_H_
