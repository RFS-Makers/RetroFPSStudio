// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFSC_LUAMEM_H_
#define RFSC_LUAMEM_H_


int luamem_EnsureFreePools(lua_State *l);

int luamem_EnsureCanAllocSize(lua_State *l, size_t bytes);

lua_State *luamem_NewMemManagedState();


#endif  // RFSC_LUAMEM_H_
