#include <gctypes.h>
#include "config.h"

// gcbool
typedef unsigned int BOOL;

typedef void* (*OSThreadStartFunction)(void*);

void OSYieldThread();
BOOL dolphin_OSCreateThread(void *thread, OSThreadStartFunction func, void* param, void* stack, u32 stackSize, s32 priority, u16 attr);
s32 dolphin_OSResumeThread(void *thread);
