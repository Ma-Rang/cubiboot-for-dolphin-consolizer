#include <gctypes.h>

// offsets for __DSPRegs[i]
#define DSP_MAILBOX_IN_HI  (0)
#define DSP_MAILBOX_IN_LO  (1)
#define DSP_MAILBOX_OUT_HI (2)
#define DSP_MAILBOX_OUT_LO (3)
#define DSP_CONTROL_STATUS (5)

#define DSP_ARAM_SIZE        (9)
#define DSP_ARAM_MODE        (11)
#define DSP_ARAM_REFRESH     (13)
#define DSP_ARAM_DMA_MM_HI   (16) // Main mem address
#define DSP_ARAM_DMA_MM_LO   (17)
#define DSP_ARAM_DMA_ARAM_HI (18) // ARAM address
#define DSP_ARAM_DMA_ARAM_LO (19)
#define DSP_ARAM_DMA_SIZE_HI (20) // DMA buffer size
#define DSP_ARAM_DMA_SIZE_LO (21)

#define DSP_DMA_START_HI    (24) // DMA start address
#define DSP_DMA_START_LO    (25)
#define DSP_DMA_CONTROL_LEN (27)
#define DSP_DMA_BYTES_LEFT  (29)

#define DSP_DMA_START_FLAG (0x8000) // set to start DSP

// Digital Signal Processor registers (for audio mixing).
extern vu16 DSP[32];

#define __DSPRegs DSP
