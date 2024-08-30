#include "dolphin_arq.h"
#include "dolphin_os.h"

#include "reloc.h"
#include "attr.h"

// TODO: find symbols for this

// ntsc 1.1
void (*ARQPostRequest)(ARQRequest *task, u32 owner, u32 type, u32 priority, u32 source, u32 dest, u32 length, ARQCallback callback) = (void*)0x8136ab80;

void dolphin_ARQPostRequest(ARQRequest *task, u32 owner, u32 type, u32 priority, u32 source, u32 dest, u32 length, ARQCallback callback) {
    ARQPostRequest(task, owner, type, priority, source, dest, length, callback);
}

