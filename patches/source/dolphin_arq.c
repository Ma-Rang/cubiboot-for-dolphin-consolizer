#include "dolphin_arq.h"
#include "dolphin_os.h"

#include "reloc.h"
#include "attr.h"

#include "ipl.h"

void dolphin_ARAMInit() {
    ipl_revision_t rev = get_ipl_revision();
    if (rev == IPL_NTSC_10_001 || rev == IPL_NTSC_10_002) {
        OSReport("ARAMInit: NTSC 1.0\n");
        ARInit(NULL, 0);
        ARQInit();
    }
}

__attribute_reloc__ void (*ARQPostRequest)(ARQRequest *task, u32 owner, u32 type, u32 priority, u32 source, u32 dest, u32 length, ARQCallback callback);

#define ARQ_NONE 0
#define ARQ_ORIG 1
#define ARQ_DECOMP 2

int arq_switcher = 0;
void dolphin_ARQPostRequest(ARQRequest *task, u32 owner, u32 type, u32 priority, u32 source, u32 dest, u32 length, ARQCallback callback) {
    if (arq_switcher == ARQ_NONE) {
        ipl_revision_t rev = get_ipl_revision();
        if (rev == IPL_NTSC_10_001 || rev == IPL_NTSC_10_002) {
            OSReport("ARQ: NTSC 1.0\n");
            arq_switcher = ARQ_DECOMP;
        } else {
            arq_switcher = ARQ_ORIG;
        }
    }

    if (arq_switcher == ARQ_ORIG) {
        ARQPostRequest(task, owner, type, priority, source, dest, length, callback);
    } else if (arq_switcher == ARQ_DECOMP) {
        decomp_ARQPostRequest(task, owner, type, priority, source, dest, length, callback);
    } else {
        OSReport("ARQPostRequest: Invalid ARQ switcher\n");
    }

    return;
}
