#include "dolphin_os.h"
#include "reloc.h"
#include "attr.h"

__attribute_reloc__ OSThread* (*SelectThread)(BOOL);

__attribute_reloc__ BOOL (*OSCreateThread)(void *thread, OSThreadStartFunction func, void* param, void* stack, u32 stackSize, s32 priority, u16 attr);
__attribute_reloc__ s32 (*OSResumeThread)(void *thread);

__attribute_reloc__ OSThread* (*SetEffectivePriority)(OSThread* thread, s32 priority);
__attribute_reloc__ s32 (*__OSGetEffectivePriority)(OSThread* thread);

__attribute_reloc__ void (*OSSleepThread)(OSThreadQueue* queue);
__attribute_reloc__ void (*OSWakeupThread)(OSThreadQueue* queue);

OSThread* OS_CURRENT_THREAD;

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

// from https://github.com/zeldaret/tp/blob/a61e3491f7c46b698514af50464cf71ba76bd3a3/libs/dolphin/os/OSThread.c#L196
OSThread* OSGetCurrentThread(void) {
    return OS_CURRENT_THREAD;
}

void OSInitThreadQueue(OSThreadQueue* queue) {
    queue->tail = NULL;
    queue->head = NULL;
}

// NTSC11
// s32 (*OSDisableScheduler)(void) = (void*)0x8135f2c0;
// s32 (*OSEnableScheduler)(void) = (void*)0x8135f300;

void __OSPromoteThread(OSThread* thread, s32 priority) {
    do {
        if (thread->suspend_count > 0 || thread->effective_priority <= priority) {
            break;
        }

        thread = SetEffectivePriority(thread, priority);
    } while (thread != NULL);
}

// from https://github.com/zeldaret/tp/blob/fcf137a90218064c0d4218abadf934538ed43671/libs/dolphin/os/OSMutex.c
#define PushTail(queue, mutex, link)                                                               \
    do {                                                                                           \
        OSMutex* __prev;                                                                           \
                                                                                                   \
        __prev = (queue)->tail;                                                                    \
        if (__prev == NULL)                                                                        \
            (queue)->head = (mutex);                                                               \
        else                                                                                       \
            __prev->link.next = (mutex);                                                           \
        (mutex)->link.prev = __prev;                                                               \
        (mutex)->link.next = NULL;                                                                 \
        (queue)->tail = (mutex);                                                                   \
    } while (0)

#define PopHead(queue, mutex, link)                                                                \
    do {                                                                                           \
        OSMutex* __next;                                                                           \
                                                                                                   \
        (mutex) = (queue)->head;                                                                   \
        __next = (mutex)->link.next;                                                               \
        if (__next == NULL)                                                                        \
            (queue)->tail = NULL;                                                                  \
        else                                                                                       \
            __next->link.prev = NULL;                                                              \
        (queue)->head = __next;                                                                    \
    } while (0)

#define PopItem(queue, mutex, link)                                                                \
    do {                                                                                           \
        OSMutex* __next;                                                                           \
        OSMutex* __prev;                                                                           \
                                                                                                   \
        __next = (mutex)->link.next;                                                               \
        __prev = (mutex)->link.prev;                                                               \
                                                                                                   \
        if (__next == NULL)                                                                        \
            (queue)->tail = __prev;                                                                \
        else                                                                                       \
            __next->link.prev = __prev;                                                            \
                                                                                                   \
        if (__prev == NULL)                                                                        \
            (queue)->head = __next;                                                                \
        else                                                                                       \
            __prev->link.next = __next;                                                            \
    } while (0)

//
// Declarations:
//

void OSInitMutex(OSMutex* mutex) {
    OSInitThreadQueue(&mutex->queue);
    mutex->thread = 0;
    mutex->count = 0;
}

void OSLockMutex(OSMutex* mutex) {
    BOOL enabled = OSDisableInterrupts();
    OSThread* currentThread = OSGetCurrentThread();
    OSThread* ownerThread;

    while (TRUE) {
        ownerThread = ((OSMutex*)mutex)->thread;
        if (ownerThread == 0) {
            mutex->thread = currentThread;
            mutex->count++;
            PushTail(&currentThread->owned_mutexes, mutex, link);
            break;
        } else if (ownerThread == currentThread) {
            mutex->count++;
            break;
        } else {
            currentThread->mutex = mutex;
            __OSPromoteThread(mutex->thread, currentThread->effective_priority);
            OSSleepThread(&mutex->queue);
            currentThread->mutex = 0;
        }
    }
    OSRestoreInterrupts(enabled);
}

void OSUnlockMutex(OSMutex* mutex) {
    BOOL enabled = OSDisableInterrupts();
    OSThread* currentThread = OSGetCurrentThread();

    if (mutex->thread == currentThread && --mutex->count == 0) {
        PopItem(&currentThread->owned_mutexes, mutex, link);
        mutex->thread = NULL;
        if ((s32)currentThread->effective_priority < (s32)currentThread->base_priority) {
            currentThread->effective_priority = __OSGetEffectivePriority(currentThread);
        }

        OSWakeupThread(&mutex->queue);
    }
    OSRestoreInterrupts(enabled);
}

void __OSUnlockAllMutex(OSThread* thread) {
    OSMutex* mutex;

    while (thread->owned_mutexes.head) {
        PopHead(&thread->owned_mutexes, mutex, link);
        mutex->count = 0;
        mutex->thread = NULL;
        OSWakeupThread(&mutex->queue);
    }
}

BOOL OSTryLockMutex(OSMutex* mutex) {
    BOOL enabled = OSDisableInterrupts();
    OSThread* currentThread = OSGetCurrentThread();
    BOOL locked;
    if (mutex->thread == 0) {
        mutex->thread = currentThread;
        mutex->count++;
        PushTail(&currentThread->owned_mutexes, mutex, link);
        locked = TRUE;
    } else if (mutex->thread == currentThread) {
        mutex->count++;
        locked = TRUE;
    } else {
        locked = FALSE;
    }
    OSRestoreInterrupts(enabled);
    return locked;
}

// void OSInitCond(OSCond* cond) { OSInitThreadQueue(&cond->queue); }

// void OSWaitCond(OSCond* cond, OSMutex* mutex) {
//     BOOL enabled = OSDisableInterrupts();
//     OSThread* currentThread = OSGetCurrentThread();
//     s32 count;

//     if (mutex->thread == currentThread) {
//         count = mutex->count;
//         mutex->count = 0;
//         PopItem(&currentThread->owned_mutexes, mutex, link);
//         mutex->thread = NULL;

//         if (currentThread->effective_priority < (s32)currentThread->base_priority) {
//             currentThread->effective_priority = __OSGetEffectivePriority(currentThread);
//         }

//         OSDisableScheduler();
//         OSWakeupThread(&mutex->queue);
//         OSEnableScheduler();
//         OSSleepThread(&cond->queue);
//         OSLockMutex(mutex);
//         mutex->count = count;
//     }

//     OSRestoreInterrupts(enabled);
// }

// void OSSignalCond(OSCond* cond) {
//     OSWakeupThread(&cond->queue);
// }

// // from https://github.com/zeldaret/tp/blob/a61e3491f7c46b698514af50464cf71ba76bd3a3/libs/dolphin/os/OSMessage.c
// void OSInitMessageQueue(OSMessageQueue* mq, OSMessage* msgArray, s32 msgCount) {
//     OSInitThreadQueue(&mq->sending_queue);
//     OSInitThreadQueue(&mq->receiving_queue);
//     mq->message_array = msgArray;
//     mq->num_messages = msgCount;
//     mq->first_index = 0;
//     mq->num_used = 0;
// }

// BOOL OSSendMessage(OSMessageQueue* mq, OSMessage msg, s32 flags) {
//     BOOL enabled;
//     s32 lastIndex;

//     enabled = OSDisableInterrupts();

//     while (mq->num_messages <= mq->num_used) {
//         if (!(flags & OS_MESSAGE_BLOCK)) {
//             OSRestoreInterrupts(enabled);
//             return FALSE;
//         } else {
//             OSSleepThread(&mq->sending_queue);
//         }
//     }

//     lastIndex = (mq->first_index + mq->num_used) % mq->num_messages;
//     mq->message_array[lastIndex] = msg;
//     mq->num_used++;

//     OSWakeupThread(&mq->receiving_queue);

//     OSRestoreInterrupts(enabled);
//     return TRUE;
// }

// BOOL OSReceiveMessage(OSMessageQueue* mq, OSMessage* msg, s32 flags) {
//     BOOL enabled;

//     enabled = OSDisableInterrupts();

//     while (mq->num_used == 0) {
//         if (!(flags & OS_MESSAGE_BLOCK)) {
//             OSRestoreInterrupts(enabled);
//             return FALSE;
//         } else {
//             OSSleepThread(&mq->receiving_queue);
//         }
//     }

//     if (msg != NULL) {
//         *msg = mq->message_array[mq->first_index];
//     }
//     mq->first_index = (mq->first_index + 1) % mq->num_messages;
//     mq->num_used--;

//     OSWakeupThread(&mq->sending_queue);

//     OSRestoreInterrupts(enabled);
//     return TRUE;
// }
