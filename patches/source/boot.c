#include <memory.h>

#include "flippy_sync.h"
#include "reloc.h"

#include "dolphin_os.h"
#include "video.h"

#include "picolibc.h"
#include "boot.h"
#include "dol.h"

#include "games.h"

void load_stub() {
    custom_OSReport("Loading stub...\n");
    dvd_custom_open_flash("/stub.bin", FILE_ENTRY_TYPES_FILE, 0);
    file_status_t *file_status = dvd_custom_status();
    if (file_status == NULL || file_status->result != 0) {
        custom_OSReport("Failed to open stub\n");
        return;
    }

    u32 file_size = (u32)__builtin_bswap64(*(u64*)(&file_status->fsize));
    file_size += 31;
    file_size &= 0xffffffe0;

    dvd_read((void*)STUB_ADDR, file_size, 0, file_status->fd);
    ICInvalidateRange((void*)STUB_ADDR, file_size);

    dvd_custom_close(file_status->fd);
    return;
}

__attribute__((aligned(32))) static DOLHEADER dol_hdr;
dol_info_t load_dol(char *path, bool flash) {
    uint8_t flags = IPC_FILE_FLAG_DISABLECACHE | IPC_FILE_FLAG_DISABLEFASTSEEK | IPC_FILE_FLAG_DISABLESPEEDEMU;
    if (flash) {
        dvd_custom_open_flash(path, FILE_ENTRY_TYPES_FILE, flags);
    } else {
        dvd_custom_open(path, FILE_ENTRY_TYPES_FILE, flags);
    }

    file_status_t *file_status = dvd_custom_status();
    if (file_status == NULL || file_status->result != 0) {
        custom_OSReport("Failed to open file %s\n", path);
        return (dol_info_t){0, 0};
    }

    DOLHEADER *hdr = &dol_hdr;
    dvd_read(hdr, sizeof(DOLHEADER), 0, file_status->fd);

    // Inspect text sections to see if what we found lies in here
    for (int i = 0; i < MAXTEXTSECTION; i++) {
        if (hdr->textAddress[i] && hdr->textLength[i]) {
            dvd_read((void*)hdr->textAddress[i], hdr->textLength[i], hdr->textOffset[i], file_status->fd);
        }
    }

    // Inspect data sections (shouldn't really need to unless someone was sneaky..)
    for (int i = 0; i < MAXDATASECTION; i++) {
        if (hdr->dataAddress[i] && hdr->dataLength[i]) {
            dvd_read((void*)hdr->dataAddress[i], hdr->dataLength[i], hdr->dataOffset[i], file_status->fd);
        }
    }

    custom_OSReport("Copy done...\n");
    dvd_custom_close(file_status->fd);

    // Clear BSS
    // TODO: check if this overlaps with IPL
    memset((void*)hdr->bssAddress, 0, hdr->bssLength);
    void *entrypoint = (void*)hdr->entryPoint;
    u32 dol_max = DOLMax(hdr);

    dol_info_t info = {dol_max, entrypoint};
    return info;
}

void prepare_game_lowmem(char *boot_path) {
    // open file
    dvd_custom_open(boot_path, FILE_ENTRY_TYPES_FILE, 0);
    file_status_t *file_status = dvd_custom_status();
    if (file_status == NULL || file_status->result != 0) {
        custom_OSReport("Failed to open file %s\n", boot_path);
        return;
    }

    struct dolphin_lowmem *lowmem = (struct dolphin_lowmem*)0x80000000;
    dvd_read(&lowmem->b_disk_info, 0x20, 0, file_status->fd);
    dvd_custom_close(file_status->fd);
}

void chainload_boot_game(gm_file_entry_t *boot_entry, bool passthrough) {
    // read boot info into lowmem
    struct dolphin_lowmem *lowmem = (struct dolphin_lowmem*)0x80000000;
    lowmem->a_boot_magic = 0x0D15EA5E;
    lowmem->a_version = 0x00000001;
    lowmem->b_physical_memory_size = 0x01800000;

    if (!passthrough) {
        // open file
        dvd_custom_open(boot_entry->path, FILE_ENTRY_TYPES_FILE, 0);
        file_status_t *file_status = dvd_custom_status();
        if (file_status == NULL || file_status->result != 0) {
            custom_OSReport("Failed to open file %s\n", boot_entry->path);
            return;
        }

        uint32_t first_fd = file_status->fd;
        uint32_t second_fd = 0;
        custom_OSReport("Boot entry: %s\n", boot_entry->path);
        if (boot_entry->second != NULL) {
            custom_OSReport("Second entry: %s (valid: %d, %p)\n", boot_entry->second->path, boot_entry->second->second == boot_entry, boot_entry->second);
            dvd_custom_open(boot_entry->second->path, FILE_ENTRY_TYPES_FILE, 0);
            file_status_t *file_status = dvd_custom_status();
            if (file_status == NULL || file_status->result != 0) {
                custom_OSReport("Failed to open file %s\n", boot_entry->second->path);
                return;
            }
            second_fd = file_status->fd;
        }
        dvd_set_default_fd(first_fd, second_fd);
    }

    dvd_read(&lowmem->b_disk_info, 0x20, 0, 0);
    void *entrypoint = load_apploader();

    struct gcm_disk_header_info *bi2 = lowmem->a_bi2;

    custom_OSReport("BI2: %08x\n", bi2);
    custom_OSReport("Country: %x\n", bi2->country_code);

    // game id
    custom_OSReport("Game ID: %c%c%c%c\n", lowmem->b_disk_info.game_code[0], lowmem->b_disk_info.game_code[1], lowmem->b_disk_info.game_code[2], lowmem->b_disk_info.game_code[3]);

    if (bi2->country_code == COUNTRY_EUR) {
        custom_OSReport("PAL game detected\n");
        ogc__VIInit(VI_TVMODE_PAL_INT);

        // set video mode PAL
        u32 mode = rmode->viTVMode >> 2;
        if (mode == VI_MPAL) {
            lowmem->tv_mode = 5;
        } else {
            lowmem->tv_mode = 1;
        }
    } else {
        custom_OSReport("NTSC game detected\n");
        ogc__VIInit(VI_TVMODE_NTSC_INT);

        lowmem->tv_mode = 0;
    }

    custom_OSReport("booting... (%08x)\n", (u32)entrypoint);

    // TODO: copy a DCZeroRange routine to 0x81200000 instead (like sidestep)

    // run the game
    run(entrypoint);
    __builtin_unreachable();
}

__attribute__((aligned(32))) static u8 apploader_buf[0x20];
void* load_apploader() {
    // variables
    int err;
    void *buffer = &apploader_buf[0];
    void (*app_init)(void (*report)(const char *fmt, ...));
    int (*app_main)(void **dst, int *size, int *offset);
    void *(*app_final)();
    void (*app_entry)(void(**init)(void (*report)(const char *fmt, ...)), int (**main)(void**,int*,int*), void *(**final)());

    // // disable interrupts
    // u32 msr;
    // msr = GetMSR();
    // msr &= ~0x8000;
    // msr |= 0x2002;
    // SetMSR(msr);

    // start disc drive & read apploader
    err = dvd_read(buffer,0x20,0x2440, 0);
    if (err) {
        custom_OSReport("Could not load apploader header\n");
        while(1);
    }
    err = dvd_read((void*)0x81200000,((*(unsigned long*)((u32)buffer+0x14)) + 31) &~31,0x2460, 0);
    if (err) {
        custom_OSReport("Could not load apploader data\n");
        while(1);
    }

    // run apploader
    app_entry = (void (*)(void(**)(void(*)(const char*,...)),int(**)(void**,int*,int*),void*(**)()))(*(unsigned long*)((u32)buffer + 0x10));
    app_entry(&app_init,&app_main,&app_final);
    app_init((void(*)(const char*,...))&custom_OSReport);
    for (;;)
    {
        void *dst = 0;
        int len = 0,offset = 0;
        int res = app_main(&dst,&len,&offset);
		custom_OSReport("res = %d\n", res);
        if (!res) break;
        err = dvd_read(dst,len,offset,0);
        if (err) {
            custom_OSReport("Apploader read failed\n");
            while(1);
        }
        DCFlushRange(dst,len);
    }
	custom_OSReport("GOT ENTRY\n");

    void* entrypoint = app_final();
    custom_OSReport("THIS ENTRY, %p\n", entrypoint);
    return entrypoint;
}

void run(register void* entry_point) {
    // ICFlashInvalidate
    asm("mfhid0	4");
    asm("ori 4, 4, 0x0800");
    asm("mthid0	4");
    // hwsync
    asm("sync");
    asm("isync");
    // boot
    asm("mtlr %0" : : "r" (entry_point));
    asm("blr");
}
