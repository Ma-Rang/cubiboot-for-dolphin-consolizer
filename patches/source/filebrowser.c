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
    char iso_name[64];
    char iso_path[128];
    ARQRequest aram_req;
} game_backing_entry_t;

int cmp_iso_path(const void* ptr_a, const void* ptr_b){
    const game_backing_entry_t *obj_a = *(game_backing_entry_t**)ptr_a;
    const game_backing_entry_t *obj_b = *(game_backing_entry_t**)ptr_b;
    return strcmp(obj_a->iso_path, obj_b->iso_path);
}

int game_backing_count;
game_backing_entry_t raw_game_backing_list[1000];
game_backing_entry_t *game_backing_list[1000];

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
        if (entry_table[i].filetype == T_FILE && strcmp(string, "opening.bnr") == 0) {
            OSReport("FST (0x%08x) entry: %s\n", FILE_POSITION(i), string);
            backing->dvd_bnr_offset = FILE_POSITION(i);
            break;
        }
    }
    OSYieldThread();
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
        game_backing->iso_name[0] = '\0'; // zero name string

        // ret = dvd_custom_open(IPC_DEVICE_SD, game_backing->iso_path, FILE_ENTRY_TYPE_FILE, IPC_FILE_FLAG_DISABLECACHE | IPC_FILE_FLAG_DISABLEFASTSEEK);
        // OSReport("OPEN ret: %d\n", ret);

        game_backing_list[current_ent_index] = game_backing;
        current_ent_index++;
    }
    game_backing_count = current_ent_index;

    f32 runtime = (f32)diff_usec(start_time, gettime()) / 1000.0;
    OSReport("File enum completed! took=%f (%d)\n", runtime, game_backing_count);

    start_time = gettime();
    qsort(game_backing_list, game_backing_count, sizeof(game_backing_entry_t*), &cmp_iso_path);

    runtime = (f32)diff_usec(start_time, gettime()) / 1000.0;
    OSReport("Sort took=%f\n", runtime);

    return game_backing_count;
}

void *file_enum_worker(void* param) {
    OSReport("FIRST File enum:\n");
    int number_of_entries = early_file_enum();
    number_of_lines = (number_of_entries + 7) >> 3;
    OSReport("Current lines = %d\n", number_of_lines);

    // test only
    number_of_lines = 20;
    grid_setup_func();

    OSReport("SECOND File enum:\n");
    u64 start_time = gettime();
    for (int i = 0; i < game_backing_count; i++) {
        OSReport("game backing: %d\n", i);
        game_backing_entry_t *game_backing = game_backing_list[i];
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

        char game_id[8];
        memcpy(game_id, &header, 6);
        game_id[6] = '\x00';
        OSReport("GAME loaded: %s\n", game_id);

        game_backing->fst_offset = header.FSTOffset;
        game_backing->fst_size = header.FSTSize;
        OSYieldThread();

        OSReport("FSTSize = 0x%08x\n", game_backing->fst_size);
        __DVDFSInit_threaded(game_backing);

        OSReport("Reading opening.bnr\n");
        dvd_threaded_read(&game_loading_banner, sizeof(BNR), game_backing->dvd_bnr_offset); //Read banner file
        OSYieldThread();

        OSReport("Copying banner to ARAM\n");
        aram_copy(&game_loading_banner, 0x0200000 + (i * sizeof(BNR)), sizeof(BNR));
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
