#include <string.h>

#include "picolibc.h"
#include "time.h"

#include "reloc.h"
#include "paths.h"
#include "config.h"

#include "gc_dvd.h"
#include "dolphin_os.h"
#include "dolphin_dvd.h"
#include "dolphin_arq.h"

#include "grid.h"
#include "filebrowser.h"

#include "../../cubeboot/include/bnr.h"

#define SD_TARGET_PATH "/"
#define DEFAULT_THREAD_PRIO 0x10

// default
// favorites
// alphabetical (optionally show letter?)

typedef struct {
    bool is_gcm;
    bool is_valid;
    u32 fst_offset;
    u32 fst_size;
    u32 dvd_bnr_offset;
    u32 mem_bnr_offset;
    u32 aram_bnr_offset;
    char disc_name[64];
    char iso_path[128];
    ARQRequest aram_req;
} game_backing_entry_t;

int cmp_iso_path(const void* ptr_a, const void* ptr_b){
    const game_backing_entry_t *obj_a = *(game_backing_entry_t**)ptr_a;
    const game_backing_entry_t *obj_b = *(game_backing_entry_t**)ptr_b;
    return strcmp(obj_a->iso_path, obj_b->iso_path);
}

int game_backing_count = 0;
game_backing_entry_t raw_game_backing_list[1000];
game_backing_entry_t *sorted_raw_game_backing_list[1000];
game_backing_entry_t *game_backing_list[1000];

// bnr buffers for each game

static game_asset_t (*game_assets)[384] = (void*)0x80400000; // needs 3mb usable

// draw variables
int number_of_lines = 0;

// start code
void weird_panic() {
    OSReport("PANIC: unknown\n");
    while(1);
}

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
static u8 *fst = NULL;
static u8 __attribute__((aligned(32))) fst_buf[0x80000];
static DiskHeader __attribute__((aligned(32))) header;
static BNR __attribute__((aligned(32))) game_loading_banner;

void __DVDFSInit_threaded(game_backing_entry_t *backing) {
    u32 size = OSRoundUp32B(header.FSTSize);
    fst = &fst_buf[0];
    
    for (u32 i = 0; size > 0; i++) {
        if (size > 0x4000) {
            dvd_threaded_read(fst + (i * 0x4000), 0x4000, (header.FSTOffset + (0x4000 * i)));
            size -= 0x4000;
        } else {
            dvd_threaded_read(fst + (i * 0x4000), size, (header.FSTOffset + (0x4000 * i)));
            size = 0;
        }
    }

    entry_table = (FSTEntry*)fst;
    total_entries = entry_table[0].len;
    string_table = (char*)&(entry_table[total_entries]);

    OSReport("FST contains %u\n", total_entries);
    OSYieldThread();

    FSTEntry* p = entry_table;
    for (u32 i = 1; i < total_entries; ++i) { //Start @ 1 to skip FST header
        u32 string_offset = GET_OFFSET(p[i].offset);
        char *string = (char*)((u32)string_table + string_offset);
        OSReport("FST (0x%08x) entry: %s\n", FILE_POSITION(i), string);
        if (entry_table[i].filetype == T_FILE && strcmp(string, "opening.bnr") == 0) {
            OSReport("FST (0x%08x) entry: %s\n", FILE_POSITION(i), string);
            backing->dvd_bnr_offset = FILE_POSITION(i);
            break;
        }
    }
    OSYieldThread();
}

// a function that will allocate a new entry in the asset list
game_asset_t *claim_game_asset() {
    for (int i = 0; i < 384; i++) {
        game_asset_t *asset = &(*game_assets)[i];
        if (!asset->in_use) {
            asset->in_use = 1;
            return asset;
        }
    }
    return NULL;
}

// a function that will free an entry in the asset list
void free_game_asset(int backing_index) {
    game_asset_t *asset = get_game_asset(backing_index);
    if (asset != NULL) {
        asset->in_use = 0;
    }
}

// a function that will get the current asset based on the backing index
game_asset_t *get_game_asset(int backing_index) {
    for (int i = 0; i < 384; i++) {
        game_asset_t *asset = &(*game_assets)[i];
        if (asset->backing_index == backing_index) {
            return asset;
        }
    }
    return NULL;
}

const char *get_game_path(int backing_index) {
    return game_backing_list[backing_index]->iso_path;
}

static int early_file_enum() {
    u64 start_time = gettime();
    // dvd_custom_open(IPC_DEVICE_SD, SD_TARGET_PATH, FILE_ENTRY_TYPE_DIR, 0); // reset readdir

    // // TODO: fallback to WiFi
    // file_status_t status;
    // dvd_custom_status(IPC_DEVICE_SD, &status);
    // if (status.result != 0) {
    //     OSReport("PANIC: SD Card could not be opened\n");
    //     while(1);
    // }

    file_entry_t ent;
    int current_ent_index = 0;
    int ret = 0;
    while(1) {
        ret = dvd_custom_readdir(IPC_DEVICE_SD, &ent);
        if (ret != 0) weird_panic();
        if (ent.name[0] == 0)
            break;
        if (ent.type != 0)
            continue;
        if (strcmp(ent.name, "boot.iso") == 0)
            continue;

        char *ext = FileSuffix(ent.name);
        if (strcmp(ext, ".iso") != 0)
            continue;

#ifdef PRINT_READDIR_NAMES
        OSReport("READDIR ent(%u): %s [len=%d]\n", ent.type, ent.name, strlen(ent.name));
#endif

        game_backing_entry_t *game_backing = &raw_game_backing_list[current_ent_index];
        strcpy(game_backing->iso_path, SD_TARGET_PATH);
        strcat(game_backing->iso_path, ent.name);
        game_backing->disc_name[0] = '\0'; // zero name string

        // ret = dvd_custom_open(IPC_DEVICE_SD, game_backing->iso_path, FILE_ENTRY_TYPE_FILE, IPC_FILE_FLAG_DISABLECACHE | IPC_FILE_FLAG_DISABLEFASTSEEK);
        // OSReport("OPEN ret: %d\n", ret);

        sorted_raw_game_backing_list[current_ent_index] = game_backing;
        current_ent_index++;
    }

    f32 runtime = (f32)diff_usec(start_time, gettime()) / 1000.0;
    OSReport("File enum completed! took=%f (%d)\n", runtime, game_backing_count);

    start_time = gettime();
    qsort(sorted_raw_game_backing_list, current_ent_index, sizeof(game_backing_entry_t*), &cmp_iso_path);

    runtime = (f32)diff_usec(start_time, gettime()) / 1000.0;
    OSReport("Sort took=%f\n", runtime);

    return current_ent_index;
}

void *file_enum_worker(void* param) {
    OSReport("FIRST File enum:\n");
    int number_of_entries = early_file_enum();
    number_of_lines = (number_of_entries + 7) >> 3;
    OSReport("Current lines = %d\n", number_of_lines);

    memset((void*)0x80400000, 0, 0x300000); // clear assets

    // test only
    number_of_lines = 20;
    grid_setup_func();

    OSReport("SECOND File enum:\n");
    u64 start_time = gettime();
    int current_ent_index = 0;
    for (int i = 0; i < number_of_entries; i++) {
        OSReport("game backing: %d\n", i);
        game_backing_entry_t *game_backing = sorted_raw_game_backing_list[i];
        int ret = dvd_threaded_custom_open(IPC_DEVICE_SD, game_backing->iso_path, FILE_ENTRY_TYPE_FILE, IPC_FILE_FLAG_DISABLECACHE | IPC_FILE_FLAG_DISABLEFASTSEEK);
        if (ret != 0) {
            weird_panic();
        }
        OSYieldThread();

        OSReport("BEFORE THREAD READ, %p\n", &header);
        dvd_threaded_read(&header, sizeof(DiskHeader), 0); //Read in the disc header
        OSYieldThread();

        if (header.DVDMagicWord != 0xC2339F3D) {
            //We'll do some error handling for this later
            OSReport("ERROR ERROR! MAGIC IS BAD, %08x\n", header.DVDMagicWord);
            OSReport("skipping bad ISO: %s\n", game_backing->iso_path);
            continue;
        } else {
            game_backing->is_gcm = true;
        }

        strncpy(game_backing->disc_name, header.GameName, 64);

        char game_id[8];
        memcpy(game_id, &header, 6);
        game_id[6] = '\x00';
        OSReport("GAME loaded: %s\n", game_id);

        game_backing->fst_offset = header.FSTOffset;
        game_backing->fst_size = header.FSTSize;
        OSYieldThread();

        OSReport("FSTSize = 0x%08x\n", game_backing->fst_size);
        __DVDFSInit_threaded(game_backing);
        if (game_backing->dvd_bnr_offset == 0) {
            OSReport("No opening.bnr found, skipping\n");
            continue;
        }

        OSReport("Reading opening.bnr\n");
        BNR *banner_buffer = &game_loading_banner;
        if (current_ent_index < 384) {
            game_asset_t *asset = &(*game_assets)[current_ent_index];
            OSReport("Claiming asset %d (@%p)\n", current_ent_index, asset);
            asset->backing_index = current_ent_index;
            asset->in_use = 1;

            banner_buffer = (void*)&asset->banner;
        }

        dvd_threaded_read(banner_buffer, sizeof(BNR), game_backing->dvd_bnr_offset); //Read banner file
        OSYieldThread();

        // print the 4 bytes of the magic
        OSReport("BNR magic: %c%c%c%c\n", banner_buffer->magic[0], banner_buffer->magic[1], banner_buffer->magic[2], banner_buffer->magic[3]);

        // u32 bnr_magic = *(u32*)banner_buffer->magic[0];
        // if (bnr_magic != 0x424e5231 && bnr_magic != 0x424e5232) {
        //     OSReport("Invalid banner magic, skipping\n");
        //     continue;
        // }

        if (banner_buffer->desc[0].fullGameName[0] == 0) {
            strncpy(banner_buffer->desc[0].fullGameName, game_backing->disc_name, 63);
            banner_buffer->desc[0].fullGameName[63] = '\0';
        }

        if (banner_buffer->desc[0].description[0] == 0) {
            strncpy(banner_buffer->desc[0].description, game_backing->iso_path, 127);
            banner_buffer->desc[0].description[127] = '\0';
        }

        OSReport("Copying banner to ARAM\n");
        aram_copy(&game_backing->aram_req, banner_buffer, 0x0200000 + (i * sizeof(BNR)), sizeof(BNR));

        // register game backing
        game_backing_list[current_ent_index] = game_backing;

        current_ent_index++;
        game_backing_count = current_ent_index; // make entry active
    }

    f32 runtime = (f32)diff_usec(start_time, gettime()) / 1000.0;
    OSReport("Second file enum completed! took=%f (%d)\n", runtime, game_backing_count);

    OSReport("[DONE] - KILL DOLPHIN NOW\n");
    return NULL;
}

typedef void* (*OSThreadStartFunction)(void*);
BOOL (*OSCreateThread)(void *thread, OSThreadStartFunction func, void* param, void* stack, u32 stackSize, s32 priority, u16 attr) = (void*)0x8135f7d4;
s32 (*OSResumeThread)(void *thread) = (void*)0x8135fb94;

// match https://github.com/projectPiki/pikmin2/blob/snakecrowstate-work/include/Dolphin/OS/OSThread.h#L55-L74
static u8 thread_obj[0x310];
static u8 thread_stack[32 * 1024];
void start_file_enum() {
    u32 thread_stack_size = sizeof(thread_stack);
    void *thread_stack_top = thread_stack + thread_stack_size;
    s32 thread_priority = DEFAULT_THREAD_PRIO + 3;

    OSCreateThread(&thread_obj[0], file_enum_worker, 0, thread_stack_top, thread_stack_size, thread_priority, 0);
    OSResumeThread(&thread_obj[0]);
}
