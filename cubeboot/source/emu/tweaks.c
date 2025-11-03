#include "tweaks.h"

#include <ogc/gx.h>
#include <string.h>
#include "../flippy_sync.h"

#ifndef IPL_CODE
#include <gccore.h>
#include "../descrambler.h"
#include "../ipl.h"
#include "../boot/ssaram.h"
#include "../crc32.h"
#include "../sd.h"
#else
#include "../gc_dvd.h"
#include "../time.h"
#endif


#ifdef IPL_CODE
static bool found_swiss = false;

void emu_update_boot() {
    dvd_custom_open_flash("/swiss-gc.dol", FILE_ENTRY_TYPE_FILE, 0);
    file_status_t* status = dvd_custom_status();
    found_swiss = (status != NULL && status->result == 0);
    dvd_custom_close(status->fd);
}

bool emu_can_boot(gm_file_type_t type) {
    switch (type) {
        case GM_FILE_TYPE_GAME:
        case GM_FILE_TYPE_PROGRAM:
            return found_swiss;
        default:
            return true;
    }
}

extern void draw_info_box(u16 width, u16 height, u16 center_x, u16 center_y, u8 alpha, GXColor* top_color, GXColor* bottom_color);
extern void draw_text(char* s, s16 size, u16 x, u16 y, GXColor* color);

void emu_draw_boot_error(gm_file_type_t type, u8 ui_alpha) {
    GXColor white = {0xFF, 0xFF, 0xFF, ui_alpha};
    GXColor error_top = {0xd4, 0x2c, 0x2c, 0xc8};
    GXColor error_bottom = {0x8b, 0x00, 0x50, 0xb4}; 

    draw_info_box(0x1A00, 0x7E0, 0x1230, 0x6F0, ui_alpha, &error_top, &error_bottom);
    draw_text("File not found: swiss-gc.dol", 24, 170, 100 - 20, &white);
    draw_text("Download swiss, rename to swiss-gc.dol", 18, 170, 155 - 20, &white);
    draw_text("and place it on the SD card", 18, 170, 190 - 20, &white);
}

bool emu_has_dvd() {
    dvd_reset();
    udelay(10 * 1000);

    if (dvd_read_id() != 0 || dvd_get_error() != 0) {
        dvd_stop_motor();
        return false;
    }

    return true;
}


#define BNR_CACHE_SIZE 1024

typedef struct {
    u8 game_id[6];
    u32 aram_offset;
    bool valid;
} bnr_cache_entry_t;

static bnr_cache_entry_t bnr_cache[BNR_CACHE_SIZE] = {0};
static u32 bnr_cache_next_index = 0;

bool bnr_cache_get(u8 game_id[6], u32* aram_offset) {
    if (!game_id || !aram_offset) {
        return false;
    }
    
    for (int i = 0; i < BNR_CACHE_SIZE; i++) {
        if (bnr_cache[i].valid && memcmp(bnr_cache[i].game_id, game_id, 6) == 0) {
            *aram_offset = bnr_cache[i].aram_offset;
            return true;
        }
    }
    
    return false;
}

void bnr_cache_put(u8 game_id[6], u32 aram_offset) {
    if (!game_id) {
        return;
    }
    
    for (int i = 0; i < BNR_CACHE_SIZE; i++) {
        if (bnr_cache[i].valid && memcmp(bnr_cache[i].game_id, game_id, 6) == 0) {
            bnr_cache[i].aram_offset = aram_offset;
            return;
        }
    }
    
    bnr_cache[bnr_cache_next_index].valid = true;
    memcpy(bnr_cache[bnr_cache_next_index].game_id, game_id, 6);
    bnr_cache[bnr_cache_next_index].aram_offset = aram_offset;
    
    bnr_cache_next_index = (bnr_cache_next_index + 1) % BNR_CACHE_SIZE;
}

#else

#define IPL_ROM_FONT_SJIS	0x1AFF00
#define DECRYPT_START		0x100

#define IPL_SIZE 0x200000
#define BS2_START_OFFSET 0x800
#define BS2_CODE_OFFSET (BS2_START_OFFSET + 0x20)
#define BS2_BASE_ADDR 0x81300000

extern void __SYS_ReadROM(void* buf, u32 len, u32 offset);

static u32 bs2_size = IPL_SIZE - BS2_CODE_OFFSET;
static u8 *bs2 = (u8*)(BS2_BASE_ADDR);

void ensure_ipl_loaded(uint8_t* bios_buffer) {
    ipl_metadata_t* metadata = (void*)0x81500000 - sizeof(ipl_metadata_t);
    if (metadata->magic == 0xC0DE)
        return;

    memset(metadata, 0, sizeof(ipl_metadata_t));

    const char* bios_path = "/ipl.bin";
    if (get_file_size(bios_path) != IPL_SIZE || load_file_buffer(bios_path, bios_buffer)) {
        __SYS_ReadROM(bios_buffer, IPL_SIZE, 0);
    }
    Descrambler(bios_buffer + DECRYPT_START, IPL_ROM_FONT_SJIS - DECRYPT_START);
    memcpy(bs2, bios_buffer + BS2_CODE_OFFSET, bs2_size);

    metadata->magic = 0xC0DE;
    
    u32 code_end_addr = 0;
    if (*(u32*)0x81300310 == 0x81300000)
        code_end_addr = *(u32*)0x81300324;
    else
        code_end_addr = *(u32*)0x8130039c;
    
    u32 code_size = code_end_addr - 0x81300000;
    if (code_size > IPL_SIZE)
        code_size = 0;
    metadata->code_size = code_end_addr - 0x81300000;
}

void apply_additional_patches() {
    extern s8 bios_index;
    if (bios_index < 0)
        return;

    // disable exi probe for sd slot
    const char* dev = emu_get_device(); 
    if (strcmp(dev, "sda") == 0) {
        u32 probe_card_0[] = { 0x8131b1c4, 0x8131b8f0, 0x8131bc88, 0x8131bca0, 0x8131c29c, 0x8131b81c, 0x8131c3dc };
        *(u32*)probe_card_0[bios_index] = 0x38600000; // li r3, 0
    } else if (strcmp(dev, "sdb") == 0) {
        u32 probe_card_1[] = { 0x8131b274, 0x8131b9a0, 0x8131bd38, 0x8131bd50, 0x8131c34c, 0x8131b8cc, 0x8131c48c };
        *(u32*)probe_card_1[bios_index] = 0x38600000; // li r3, 0
    } else if (strcmp(dev, "sdc") == 0) {
        u32 init_ad16_addr[] = { 0x81335f54, 0x8135b9b4, 0x8136572c, 0x81365890, 0x8135ef94, 0x8135b8d4, 0x81368c08 };
        *(u32*)init_ad16_addr[bios_index] = 0x4e800020; // blr
    }
}

#endif
