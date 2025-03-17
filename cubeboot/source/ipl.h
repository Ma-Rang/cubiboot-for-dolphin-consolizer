#include <gctypes.h>

#define DIRTYCRC(x) x
#define CLEANCRC(x) x
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
    u16 magic;
    u8 revision;
    u8 padding;
    u32 blob_checksum;
    u32 code_size;
    u32 code_checksum;
} ipl_metadata_t;

typedef struct {
    ipl_version version;
    ipl_types type;
    char *name;
    char *reloc_prefix;
    char *patch_suffix;
    u32 clean_crc;
    u32 dirty_crc;
    u32 sda;
} bios_item_t;

extern bios_item_t *current_bios;

void load_ipl(bool is_running_dolphin);
u32 get_sda_address();
