#include "dolphin_os.h"
#include "reloc.h"

typedef void OSThread;

BOOL (*OSRestoreInterrupts)(BOOL) = (void*)0x8135bc98;
OSThread* (*SelectThread)(BOOL) = (void*)0x8135f5a4;

BOOL (*OSCreateThread)(void *thread, OSThreadStartFunction func, void* param, void* stack, u32 stackSize, s32 priority, u16 attr) = (void*)0x8135f7d4;
s32 (*OSResumeThread)(void *thread) = (void*)0x8135fb94;

// from https://github.com/projectPiki/pikmin2/blob/de4b757bcb49e10b6d6822e6198be93c01890dcf/src/Dolphin/os/OSThread.c#L336-L343
void OSYieldThread() {
    BOOL enabled = OSDisableInterrupts();
    SelectThread(TRUE);
    OSRestoreInterrupts(enabled);
}

BOOL dolphin_OSCreateThread(void *thread, OSThreadStartFunction func, void* param, void* stack, u32 stackSize, s32 priority, u16 attr) {
    return OSCreateThread(thread, func, param, stack, stackSize, priority, attr);
}
s32 dolphin_OSResumeThread(void *thread) {
    return OSResumeThread(thread);
}
