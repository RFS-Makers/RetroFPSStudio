// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_MAIN_H_
#define RFS2_MAIN_H_

#include "compileconfig.h"


extern _Atomic volatile int main_gotsigintsigterm;

int main_ConsoleWasSpawned();

int main_IsEditorExecutable();

int main_EnsureConsole();

#endif  // RFS2_MAIN_H_
