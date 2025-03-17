#include <memory.h>

#include "flippy_sync.h"
#include "reloc.h"
#include "attr.h"

#include "dolphin_os.h"
#include "video.h"

#include "time.h"
#include "picolibc.h"
#include "boot.h"
#include "dol.h"

#include "games.h"

#include "dolphin_dvd.h"
#include "gc_dvd.h"

#define SYS_VIDEO_NTSC   0
#define SYS_VIDEO_PAL    1
#define SYS_VIDEO_MPAL   2

#define TB_BUS_CLOCK     162000000u
#define TB_CORE_CLOCK    486000000u

void load_stub() {
    custom_OSReport("Loading stub...\n");
    dvd_custom_open_flash("/stub.bin", FILE_ENTRY_TYPE_FILE, 0);
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
static dol_info_t load_dol(uint64_t offset, uint8_t fd) {
    DOLHEADER *hdr = &dol_hdr;
    dvd_read(hdr, sizeof(DOLHEADER), offset, fd);

    // Clear BSS
    // TODO: check if this overlaps with IPL?
    if (hdr->bssAddress && hdr->bssLength) {
        custom_OSReport("Clearing BSS %08x - %08x...\n", hdr->bssAddress, hdr->bssAddress + hdr->bssLength);
        memset((void*)hdr->bssAddress, 0, hdr->bssLength);
    }

    // Inspect text sections to see if what we found lies in here
    for (int i = 0; i < MAXTEXTSECTION; i++) {
        if (hdr->textAddress[i] && hdr->textLength[i]) {
            dvd_read_data((void*)hdr->textAddress[i], hdr->textLength[i], offset + hdr->textOffset[i], fd);
        }
    }

    // Inspect data sections (shouldn't really need to unless someone was sneaky..)
    for (int i = 0; i < MAXDATASECTION; i++) {
        if (hdr->dataAddress[i] && hdr->dataLength[i]) {
            dvd_read_data((void*)hdr->dataAddress[i], hdr->dataLength[i], offset + hdr->dataOffset[i], fd);
        }
    }

    custom_OSReport("Copy done...\n");

    void *entrypoint = (void*)hdr->entryPoint;
    u32 dol_max = DOLMax(hdr);

    dol_info_t info = {dol_max, entrypoint};
    return info;
}


dol_info_t load_dol_file(char *path, bool flash) {
    uint8_t flags = IPC_FILE_FLAG_DISABLECACHE | IPC_FILE_FLAG_DISABLEFASTSEEK | IPC_FILE_FLAG_DISABLESPEEDEMU;
    if (flash) {
        dvd_custom_open_flash(path, FILE_ENTRY_TYPE_FILE, flags);
    } else {
        dvd_custom_open(path, FILE_ENTRY_TYPE_FILE, flags);
    }

    file_status_t *file_status = dvd_custom_status();
    if (file_status == NULL || file_status->result != 0) {
        custom_OSReport("Failed to open file %s\n", path);
        return (dol_info_t){0, 0};
    }

    dol_info_t info = load_dol(0, file_status->fd);
    dvd_custom_close(file_status->fd);

    return info;
}

void chainload_swiss_game(char *game_path, bool passthrough) {
    dol_info_t info = load_dol_file("/swiss-gc.dol", true);

    char *argz = (void*)info.max_addr + 32;
    int argz_len = 0;

    char autoload_arg[256];
    if (passthrough) {
        strcpy(autoload_arg, "Autoload=dvd:/*.gcm");
    } else {
        strcpy(autoload_arg, "Autoload=fldrv:");
        strcat(autoload_arg, game_path);
    }

    const char *arg_list[] = {
        "swiss-novideo.dol", // this causes Swiss to black the screen
        autoload_arg,
        "AutoBoot=Yes",
        "IGRType=Reboot",
        "BS2Boot=No",
        "Prefer Clean Boot=No",
        NULL
    };

    int arg_count = 0;
    while(arg_list[arg_count] != NULL) {
        const char *arg = arg_list[arg_count];
        int arg_len = strlen(arg) + 1;
        memcpy(argz + argz_len, arg, arg_len);
        argz_len += arg_len;
        arg_count++;
    }

    struct __argv *args = (void*)(info.entrypoint + 8);
    args->argvMagic = ARGV_MAGIC;
    args->commandLine = argz;
    args->length = argz_len;

    DCFlushRange(argz, argz_len);
    DCFlushRange(args, sizeof(struct __argv));

    run(info.entrypoint);
}

const char uart_bin[416] = { // Linked to 0x817ffe00
	0x3d, 0x20, 0xcc, 0x00, 0x3c, 0xe0, 0x90, 0x00, 0x39, 0x29, 0x68,
	0x00, 0x39, 0x00, 0x00, 0x19, 0x81, 0x49, 0x00, 0x14, 0x71, 0x4a,
	0x04, 0x05, 0x61, 0x4a, 0x00, 0xd0, 0x91, 0x49, 0x00, 0x14, 0x90,
	0xe9, 0x00, 0x24, 0x91, 0x09, 0x00, 0x20, 0x81, 0x49, 0x00, 0x20,
	0x71, 0x4a, 0x00, 0x01, 0x40, 0x82, 0xff, 0xf8, 0x81, 0x09, 0x00,
	0x24, 0x81, 0x49, 0x00, 0x14, 0x55, 0x08, 0x84, 0x3e, 0x2f, 0x88,
	0x04, 0x70, 0x71, 0x4a, 0x04, 0x05, 0x91, 0x49, 0x00, 0x14, 0x40,
	0x9e, 0x01, 0x04, 0x2c, 0x04, 0x00, 0x00, 0x38, 0xe0, 0x00, 0x00,
	0x39, 0x00, 0x00, 0x00, 0x39, 0x40, 0x00, 0x01, 0x3d, 0x60, 0xc0,
	0x00, 0x38, 0xa0, 0x00, 0x09, 0x38, 0xc0, 0x00, 0x19, 0x40, 0x81,
	0x00, 0x80, 0x71, 0x4a, 0x00, 0x01, 0x40, 0x82, 0x00, 0xb4, 0x81,
	0x49, 0x00, 0x14, 0x71, 0x4a, 0x04, 0x05, 0x61, 0x4a, 0x00, 0xd0,
	0x91, 0x49, 0x00, 0x14, 0x7d, 0x43, 0x40, 0xae, 0x55, 0x4a, 0xa0,
	0x16, 0x65, 0x4a, 0xb0, 0x00, 0x91, 0x49, 0x00, 0x24, 0x90, 0xc9,
	0x00, 0x20, 0x81, 0x49, 0x00, 0x20, 0x71, 0x4a, 0x00, 0x01, 0x40,
	0x82, 0xff, 0xf8, 0x80, 0x09, 0x00, 0x24, 0x81, 0x49, 0x00, 0x14,
	0x74, 0x00, 0x04, 0x00, 0x4f, 0x80, 0x00, 0x00, 0x71, 0x4a, 0x04,
	0x05, 0x91, 0x49, 0x00, 0x14, 0x41, 0x9e, 0x00, 0x50, 0x39, 0x08,
	0x00, 0x01, 0x7d, 0x0a, 0x3e, 0x70, 0x7d, 0x4a, 0x01, 0x94, 0x7c,
	0x04, 0x40, 0x00, 0x55, 0x4a, 0x38, 0x30, 0x7d, 0x4a, 0x40, 0x50,
	0x7d, 0x4a, 0x3a, 0x78, 0x7d, 0x4a, 0x00, 0x34, 0x55, 0x4a, 0xd9,
	0x7e, 0x41, 0x81, 0xff, 0x88, 0x38, 0x60, 0x00, 0x00, 0x4e, 0x80,
	0x00, 0x20, 0x80, 0x09, 0x00, 0x24, 0x81, 0x49, 0x00, 0x14, 0x74,
	0x00, 0x04, 0x00, 0x4f, 0x80, 0x00, 0x00, 0x71, 0x4a, 0x04, 0x05,
	0x91, 0x49, 0x00, 0x14, 0x40, 0xbe, 0xff, 0x6c, 0x7c, 0x04, 0x40,
	0x00, 0x7d, 0x07, 0x3e, 0x70, 0x7c, 0xe7, 0x01, 0x94, 0x54, 0xe7,
	0x38, 0x30, 0x7c, 0xe7, 0x40, 0x50, 0x40, 0xa1, 0xff, 0xc8, 0x81,
	0x49, 0x00, 0x14, 0x71, 0x4a, 0x04, 0x05, 0x61, 0x4a, 0x00, 0xd0,
	0x91, 0x49, 0x00, 0x14, 0x91, 0x69, 0x00, 0x24, 0x90, 0xa9, 0x00,
	0x20, 0x81, 0x49, 0x00, 0x20, 0x71, 0x4a, 0x00, 0x01, 0x40, 0x82,
	0xff, 0xf8, 0x4b, 0xff, 0xff, 0xa8, 0x38, 0x60, 0xff, 0xff, 0x4e,
	0x80, 0x00, 0x20, 0x60, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x01, 0x7a, 0x52,
	0x00, 0x04, 0x7c, 0x41, 0x01, 0x1b, 0x0c, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x10, 0x00, 0x00, 0x00, 0x18, 0xff, 0xff, 0xfe, 0x84, 0x00,
	0x00, 0x01, 0x58, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00,
	0x60, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00,
	0x00, 0x60, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00
};

const int uart_bin_length = 416;

const char GXPeekARGBPatch_bin[0x1c] = { // PIC
    0x3c, 0xc0, 0xc8, 0x00,    // 	lis		%r6, 0xC800
    0x50, 0x66, 0x15, 0x3a,    // 	insrwi	%r6, %r3, 10, 20
    0x50, 0x86, 0x62, 0xa6,    // 	insrwi	%r6, %r4, 10, 10
    0x7c, 0x00, 0x04, 0xac,    // 	sync
    0x80, 0x06, 0x00, 0x00,    // 	lwz		%r0, 0 (%r6)
    0x90, 0x05, 0x00, 0x00,    // 	stw		%r0, 0 (%r5)
    0x4e, 0x80, 0x00, 0x20,    // 	blr
};

const int GXPeekARGBPatch_length = 0x1c;

const char GXPokeARGBPatch_bin[0x18] = { // PIC
    0x3c, 0xc0, 0xc8, 0x00,    // 	lis		%r6, 0xC800
    0x50, 0x66, 0x15, 0x3a,    // 	insrwi	%r6, %r3, 10, 20
    0x50, 0x86, 0x62, 0xa6,    // 	insrwi	%r6, %r4, 10, 10
    0x7c, 0x00, 0x04, 0xac,    // 	sync
    0x90, 0xa6, 0x00, 0x00,    // 	stw		%r5, 0 (%r6)
    0x4e, 0x80, 0x00, 0x20,    // 	blr
};

const int GXPokeARGBPatch_length = 0x18;

extern const void _patches_end;

void chainload_boot_game(gm_file_entry_t *boot_entry, bool passthrough) {
    u32 patchSize = 0x200; // setup patching space
    u32 topAddr = 0x81800000 - patchSize;

    memset((void*)0x80000000, 0, 0x100);
    memset((void*)0x80000000, 0 ,0x100);

    // lowmem
    *(vu32*)(0x80000028) = 0x01800000;
	*(vu32*)(0x8000002C) = 0x00000001;
	*(vu32*)(0x800000CC) = SYS_VIDEO_NTSC; // overwrite with tv_mode from BI2
	*(vu32*)(0x800000D0) = 0x01000000;
	*(vu32*)(0x800000E8) = 0x81800000 - topAddr;
	*(vu32*)(0x800000EC) = topAddr;
	*(vu32*)(0x800000F0) = 0x01800000;
	*(vu32*)(0x800000F8) = TB_BUS_CLOCK;
	*(vu32*)(0x800000FC) = TB_CORE_CLOCK;

    u32 dol_offset = 0;
    u32 fst_offset = 0;
	u32 fst_size = 0;
	u32 max_fst_size = 0;

    if (!passthrough) {
        // open file
        dvd_custom_open(boot_entry->path, FILE_ENTRY_TYPE_FILE, 0);
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
            dvd_custom_open(boot_entry->second->path, FILE_ENTRY_TYPE_FILE, 0);
            file_status_t *file_status = dvd_custom_status();
            if (file_status == NULL || file_status->result != 0) {
                custom_OSReport("Failed to open file %s\n", boot_entry->second->path);
                return;
            }
            second_fd = file_status->fd;
        }
        dvd_set_default_fd(first_fd, second_fd);

        // Read the game header to 0x80000000
        dvd_read((void*)0x80000000, 0x20, 0, 0);

        dol_offset = boot_entry->extra.dvd_dol_offset;
        fst_offset = boot_entry->extra.dvd_fst_offset;
        fst_size = boot_entry->extra.dvd_fst_size;
        max_fst_size = boot_entry->extra.dvd_max_fst_size;
    } else {
        __attribute__((aligned(32))) static DiskHeader header;
        dvd_read(&header, sizeof(DiskHeader), 0, 0); //Read in the disc header

        dol_offset = header.DOLOffset;
        fst_offset = header.FSTOffset;
        fst_size = header.FSTSize;
        max_fst_size = header.MaxFSTSize;
    }

    // print all offsets and sizes
    custom_OSReport("DOL Offset: %08x\n", dol_offset);
    custom_OSReport("FST Offset: %08x\n", fst_offset);
    custom_OSReport("FST Size: %08x\n", fst_size);
    custom_OSReport("Max FST Size: %08x\n", max_fst_size);

    // Read FST to top of Main Memory (round to 32 byte boundary)
    u32 fstAddr = (topAddr-max_fst_size)&~31;
    custom_OSReport("READING: FST Address: %08x\n", fstAddr);
    u32 fstSize = (fst_size+31)&~31;
    int ret = dvd_read((void*)fstAddr, fstSize, fst_offset, 0);
    if (ret != 0) {
        custom_OSReport("Failed to read FST!\n");
        return;
    }

    // Copy bi2.bin (Disk Header Information) to just under the FST
    u32 bi2Addr = (fstAddr-0x2000)&~31;
    ret = dvd_read((void*)bi2Addr, 0x2000, 0x440, 0);
    if (ret != 0) {
        custom_OSReport("Failed to read BI2!\n");
        return;
    }

    custom_OSReport("BI2 Address: %08x\n", bi2Addr);

    // Patch bi2.bin
    DiskHeaderInformation *bi2 = (DiskHeaderInformation*)bi2Addr;
    bi2->SimMemSize = 0x1800000;

    // // test only
    // bi2->DebugFlag = 1;

    *(vu32*)(0x80000034) = fstAddr;       // Arena Hi
    *(vu32*)(0x80000038) = fstAddr;       // FST Location in ram
    *(vu32*)(0x8000003C) = max_fst_size;  // FST Max Length
    *(vu32*)(0x800000F4) = bi2Addr;

    custom_OSReport("BI2: %08x\n", bi2);
    custom_OSReport("Country: %x\n", bi2->RegionCode);

    // game id
    struct dolphin_lowmem *lowmem = (struct dolphin_lowmem*)0x80000000;
    custom_OSReport("Game ID: %c%c%c%c\n", lowmem->b_disk_info.game_code[0], lowmem->b_disk_info.game_code[1], lowmem->b_disk_info.game_code[2], lowmem->b_disk_info.game_code[3]);

    if (bi2->RegionCode == COUNTRY_EUR) {
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

    if (dol_offset == 0) {
        dol_offset = get_dol_iso9660(0);
        if (dol_offset) {
            // memset((void*)0x80000000, 0, 0x20000);
            // DCFlushRange((void*)0x80000000, 0x20000);
            // ICInvalidateRange((void*)0x80000000, 0x20000);

            // custom_OSReport("Clearing range: %08x - %08x\n", (u32)(&_patches_end), 0x81800000);
            // memset((void*)&_patches_end, 0, (0x81800000 - (u32)(&_patches_end)));

            // lowmem
            *(u32*)0x8000001c = 0x00000000;
            *(u32*)0x80000020 = 0x0d15ea5e;
            *(u32*)0x80000028 = 0x01800000;
            *(u32*)0x80000024 = 0x00000001;
            *(u32*)0x8000002c = 0x00000003;

            // sys vars
            *(u32*)0x800000d0 = 0x01000000;
            *(u32*)0x800000ec = 0x81800000;
            *(u32*)0x800000f0 = 0x01800000;
            *(u32*)0x800000f4 = 0x00000000;
        }
    }

    dol_info_t info = load_dol(dol_offset, 0);
    custom_OSReport("Booting... (%08x)\n", (u32)info.entrypoint);

    // memcpy((void*)topAddr, uart_bin, uart_bin_length);
    // *(vu32*)0x80166aa0 = 0x49699361; // bl 0x817ffe00
    // DCFlushRange((void*)topAddr, uart_bin_length);
    // DCFlushRange((void*)0x80166aa0, 4);
    // ICInvalidateRange((void*)topAddr, uart_bin_length);
    // ICBlockInvalidate((void*)0x80166aa0);

    // memcpy((void*)0x8019013c, GXPeekARGBPatch_bin, GXPeekARGBPatch_length);
    // DCFlushRange((void*)0x8019013c, GXPeekARGBPatch_length);
    // ICInvalidateRange((void*)0x8019013c, GXPeekARGBPatch_length);

    // memcpy((void*)0x80190160, GXPokeARGBPatch_bin, GXPokeARGBPatch_length);
    // DCFlushRange((void*)0x80190160, GXPokeARGBPatch_length);
    // ICInvalidateRange((void*)0x80190160, GXPokeARGBPatch_length);

    // custom_OSReport("Running game...\n");
    // udelay(1 * 1000 * 1000);
    // while(1);

    run(info.entrypoint);
    __builtin_unreachable();
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
