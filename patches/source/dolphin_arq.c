#include "dolphin_arq.h"
#include "reloc.h"
#include "attr.h"

// void aram_done(u32 request_address) {
//     OSReport("Copy Done %08x\n", request_address);
// }

// __attribute_data__ ARQRequest arq_request;
// void aram_copy(ARQRequest *task, void *src, u32 dst, u32 size) {
//         void (*ARQPostRequest)(ARQRequest *task, u32 owner, u32 type, u32 priority, u32 source, u32 dest, u32 length, ARQCallback callback) = (void*)0x8136ab80;
//         ARQPostRequest(&arq_request, 0, ARAM_DIR_MRAM_TO_ARAM, ARQ_PRIORITY_LOW, (u32)src, dst, size, &aram_done);
// }
