#include <gctypes.h>

#define CRC(x) x
#define SDA(x) x

typedef enum ipl_types {
    IPL_NTSC,
    IPL_PAL,
} ipl_types;

typedef enum ipl_version {
    IPL_NTSC_10,
    IPL_NTSC_11,
    IPL_NTSC_12_001,
    IPL_NTSC_12_101,
    IPL_PAL_10,
    IPL_PAL_11,
    IPL_PAL_12,
} ipl_version;

typedef struct {
    ipl_version version;
    ipl_types type;
    char *name;
    char *reloc_prefix;
    char *patch_suffix;
    u32 crc;
    u32 sda;
} bios_item;

extern bios_item *current_bios;

void load_ipl(bool is_running_dolphin);
u32 get_sda_address();
