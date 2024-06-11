#include "dolphin_dvd.h"

#define assert(...)
// #include <assert.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <gcutil.h>
#include <ogc/cache.h>
#include <ogc/irq.h>
#include <ogc/lwp.h>
#include <ogc/system.h>

#include "reloc.h"
#include "flippy_sync.h"

#define DVD_FS_DUMP 1

#define GET_OFFSET(o) ((u32)((o[0] << 16) | (o[1] << 8) | o[2]))

#define ENTRY_IS_DIR(i) (entry_table[i].filetype == T_DIR)
#define FILE_POSITION(i) (entry_table[i].addr)
#define FILE_LENGTH(i) (entry_table[i].len)

// from https://github.com/doldecomp/melee/blob/62e7b6af39da9ac19288ac7fbb454ad04d342658/src/dolphin/os/os.h#L16
#define OSRoundUp32B(x) (((u32) (x) + 32 - 1) & ~(32 - 1))

// mostly from https://github.com/PsiLupan/FRAY/blob/7fd533b0aa1274a2bd01e060b1547386b30c3f26/src/ogcext/dvd.c#L229
static u32 total_entries = 0; //r13_441C
static char* string_table = NULL; //r13_4420
static FSTEntry* entry_table = NULL; //r13_4424
// static u32* start_memory = NULL; //r13_4428

// static s8* fst = NULL;

// static __attribute__((aligned(32))) DiskHeader header;
// DiskHeader *__DVDFSInit(void)
// {
//     // dvdcmdblk cmdblk = { 0 };

//     // memset(&cmdblk, 0, sizeof(dvdcmdblk));
//     memset(&header, 0, sizeof(DiskHeader));

//     start_memory = (u32*)(SYS_BASE_CACHED);

//     OSReport("BEFORE BAD READ, %p\n", &header);
//     dvd_read(&header, sizeof(DiskHeader), 0, 0); //Read in the disc header

//     if (header.DVDMagicWord != 0xC2339F3D) {
//         //We'll do some error handling for this later
//         OSReport("ERROR ERROR! MAGIC IS BAD, %08x\n", header.DVDMagicWord);
//         // TODO: this should prompt a failure later on
//         return NULL;
//     }

//     OSReport("FSTOffset = 0x%08x\n", header.FSTOffset);
//     OSReport("FSTSize = 0x%08x\n", header.FSTSize);

//     // TODO: use apploader based FST from lowmem
//     // static struct dolphin_lowmem *lowmem = (struct dolphin_lowmem *)0x80000000;
// #if 0
//     if (fst != NULL) {
//         free(fst);
//     }

//     u32 size = OSRoundUp32B(header.FSTSize);
//     fst = memalign(32, size);
//     start_memory[14] = header.FSTOffset;
//     start_memory[15] = header.FSTSize;

//     DCFlushRange((void*)0x80000000, 0x40);

//     // memset(&cmdblk, 0, sizeof(dvdcmdblk));
//     for (u32 i = 0; size > 0; i++) {
//         if (size > 0x4000) {
//             dvd_read(fst + (i * 0x4000), 0x4000, (header.FSTOffset + (0x4000 * i)));
//             size -= 0x4000;
//         } else {
//             dvd_read(fst + (i * 0x4000), size, (header.FSTOffset + (0x4000 * i)));
//             size = 0;
//         }
//     }

//     entry_table = (FSTEntry*)fst;
//     total_entries = entry_table[0].len;
//     string_table = (char*)&(entry_table[total_entries]);

//     OSReport("FST contains %u\n", total_entries);

// #ifdef DVD_FS_DUMP
//     FSTEntry* p = entry_table;
//     for (u32 i = 1; i < total_entries; ++i) { //Start @ 1 to skip FST header
//         u32 string_offset = GET_OFFSET(p[i].offset);
//         char *string = (char*)((u32)string_table + string_offset);
//         char type = entry_table[i].filetype == T_FILE ? 'F' : 'D';
//         OSReport("[%c] (0x%08x) entry: %s\n", type, FILE_POSITION(i), string);
//     }
// #endif

// #endif

//     return &header;
// }

BOOL DVDFastOpen(s32 entrynum, dvdfileinfo* fileinfo)
{
    assert(entrynum > 0 && entrynum < total_entries);
    assert(fileinfo != NULL);

    u8 filetype = entry_table[entrynum].filetype;
    if (filetype == T_FILE) {
        fileinfo->addr = entry_table[entrynum].addr;
        fileinfo->len = entry_table[entrynum].len;
        fileinfo->cb = (dvdcallback)NULL;
        fileinfo->block.state = DVD_STATE_END;
        return TRUE;
    }
    return FALSE;
}

// adapted from https://github.com/zeldaret/tp/blob/be1764e06009998f9a280b1d1a26f1e4b9133309/libs/dolphin/dvd/dvdfs.c#L226
BOOL DVDOpen(const char* fileName, dvdfileinfo* fileInfo) {
    s32 entry = DVDConvertPathToEntrynum((char*)fileName);

    if (0 > entry) {
        OSReport("Warning: DVDOpen(): file '%s' was not found under %d.\n", fileName, entry);
        return FALSE;
    }

    if (ENTRY_IS_DIR(entry)) {
        return FALSE;
    }

    fileInfo->addr = FILE_POSITION(entry);
    fileInfo->len = FILE_LENGTH(entry);
    fileInfo->cb = (dvdcallback)NULL;
    fileInfo->block.state = DVD_STATE_END;

    return TRUE;
}

s32 DVDConvertFilenameToEntrynum(const char* filename)
{
    FSTEntry* p = entry_table;
    for (u32 i = 1; i < total_entries; ++i) { //Start @ 1 to skip FST header
        u32 string_offset = GET_OFFSET(p[i].offset);
        char* string = (char*)((u32)string_table + string_offset);
        if (strcmp(string, filename) == 0) {
            if (entry_table[i].filetype == T_FILE) {
                return (s32)i;
            }
        }
    }
    return -1;
}

s32 DVDConvertPathToEntrynum(char* filepath)
{
    assert(filepath != NULL);

    char* file = NULL;

    char* dir = strtok(filepath, "/");
    while (dir != NULL) {
        file = dir;
        dir = strtok(NULL, "/");
    }
    return DVDConvertFilenameToEntrynum(file);
}
