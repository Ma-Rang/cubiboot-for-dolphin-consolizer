#include "decomp_os.h"
#include "attr.h"

__attribute_reloc__ void (*OSClearContext)(OSContext* context);
__attribute_reloc__ void (*OSSetCurrentContext)(OSContext* context);

__attribute_reloc__ __OSInterruptHandler (*__OSSetInterruptHandler)(__OSInterrupt interrupt, __OSInterruptHandler handler);
__attribute_reloc__ OSInterruptMask (*__OSUnmaskInterrupts)(OSInterruptMask mask);
