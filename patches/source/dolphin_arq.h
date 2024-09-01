#include <gctypes.h>
#include "decomp_ar.h"

void dolphin_ARAMInit();
void dolphin_ARQPostRequest(ARQRequest *task, u32 owner, u32 type, u32 priority, u32 source, u32 dest, u32 length, ARQCallback callback);
