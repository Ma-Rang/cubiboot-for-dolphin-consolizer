#include <gctypes.h>

// from https://github.com/zeldaret/tp/blob/e1147cf047a525242178bd0ec39c8ed88a2633f1/include/dolphin/arq.h#L12-L37
typedef void (*ARQCallback)(u32 request_address);

typedef enum _ARamType {
    ARAM_DIR_MRAM_TO_ARAM,
    ARAM_DIR_ARAM_TO_MRAM,
} ARamType;

typedef enum _ArqPriotity {
    ARQ_PRIORITY_LOW,
    ARQ_PRIORITY_HIGH,
} ArqPriotity;

typedef struct ARQRequest {
    struct ARQRequest* next;
    u32 owner;
    u32 type;
    u32 priority;
    u32 source;
    u32 destination;
    u32 length;
    ARQCallback callback;
} ARQRequest;

void aram_copy(ARQRequest *task, void *src, u32 dst, u32 size);
