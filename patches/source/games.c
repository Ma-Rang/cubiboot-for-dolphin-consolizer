

// Dolphin Launcher — Game list management.
// Receives game entries from Helper via USB Gecko, stores banners in ARAM,
// and manages the cube grid display.
//
// Original cubiboot loaded games from SD via Flippy ODE.
// This version receives pre-scanned game data from the Helper app.

#include <gctypes.h>

#include "pmalloc/pmalloc.h"
#include "picolibc.h"
#include "reloc.h"
#include "attr.h"
#include "os.h"

#include "dolphin_os.h"
#include "dolphin_arq.h"

#include "metaphrasis.h"

#include "games.h"
#include "grid.h"
#include "menu.h"
#include "time.h"

#include "gecko_link.h"
#include "game_blob.h"

#define PRELOAD_LINE_COUNT 2
#define ASSETS_PER_LINE 8
#define ASSETS_PER_PAGE (ASSETS_PER_LINE * DRAW_TOTAL_ROWS)
#define ASSETS_INITIAL_COUNT (ASSETS_PER_PAGE + (PRELOAD_LINE_COUNT * ASSETS_PER_LINE)) // assuming we start at the top
#define ASSET_BUFFER_COUNT 128

// Globals
int number_of_lines = 4;
int game_backing_count = 0;

static OSMutex game_enum_mutex_obj;
OSMutex *game_enum_mutex = &game_enum_mutex_obj;

char game_enum_path[128] = {0};
bool game_enum_running = false;

__attribute_data_lowmem__ static gm_file_entry_t *gm_entry_backing[2000];

static u32 gm_entry_count = 0;

gm_file_entry_t *gm_get_game_entry(int index) {
    if (index >= gm_entry_count) return NULL;
    return gm_entry_backing[index];
}

__attribute_aligned_data_lowmem__ static gm_icon_buf_t gm_icon_pool[ASSET_BUFFER_COUNT] = {};
__attribute_aligned_data_lowmem__ static gm_banner_buf_t gm_banner_pool[ASSET_BUFFER_COUNT] = {};

static inline gm_icon_buf_t *gm_get_icon_buf() {
    for (int i = 0; i < ASSET_BUFFER_COUNT; i++) {
        if (gm_icon_pool[i].used == 0) {
            OSReport("Allocating icon buffer\n");
            gm_icon_pool[i].used = 1;
            return &gm_icon_pool[i];
        }
    }

    return NULL;
}

static inline void gm_free_icon_buf(gm_icon_buf_t *buf) {
    if (buf == NULL) return;
    buf->used = 0;
}

static inline gm_banner_buf_t *gm_get_banner_buf() {
    for (int i = 0; i < ASSET_BUFFER_COUNT; i++) {
        if (gm_banner_pool[i].used == 0) {
            gm_banner_pool[i].used = 1;
            return &gm_banner_pool[i];
        }
    }

    return NULL;
}

static inline void gm_free_banner_buf(gm_banner_buf_t *buf) {
    if (buf == NULL) return;
    buf->used = 0;
}

// ARQ callbacks for ARAM <-> main RAM DMA transfers
static void arq_icon_callback_setup(u32 arq_request_ptr) {
    ARQRequest *req = (ARQRequest*)arq_request_ptr;
    gm_icon_t *icon = (gm_icon_t*)req;

    icon->state = GM_LOAD_STATE_LOADED;
    DCFlushRange(icon, sizeof(gm_icon_t));
}

static void arq_icon_callback_unload(u32 arq_request_ptr) {
    ARQRequest *req = (ARQRequest*)arq_request_ptr;
    gm_icon_t *icon = (gm_icon_t*)req;

    icon->state = GM_LOAD_STATE_UNLOADED;
    gm_free_icon_buf(icon->buf);
    DCFlushRange(icon, sizeof(gm_icon_t));
}

static void arq_icon_callback_load(u32 arq_request_ptr) {
    ARQRequest *req = (ARQRequest*)arq_request_ptr;
    gm_icon_t *icon = (gm_icon_t*)req;

    icon->state = GM_LOAD_STATE_LOADED;
    DCFlushRange(icon, sizeof(gm_icon_t));
}

static void arq_banner_callback_setup(u32 arq_request_ptr) {
    ARQRequest *req = (ARQRequest*)arq_request_ptr;
    gm_banner_t *banner = (gm_banner_t*)req;

    banner->state = GM_LOAD_STATE_LOADED;
    DCFlushRange(banner, sizeof(gm_banner_t));
}

static void arq_banner_callback_unload(u32 arq_request_ptr) {
    ARQRequest *req = (ARQRequest*)arq_request_ptr;
    gm_banner_t *banner = (gm_banner_t*)req;

    banner->state = GM_LOAD_STATE_UNLOADED;
    gm_free_banner_buf(banner->buf);
    DCFlushRange(banner, sizeof(gm_banner_t));
}

static void arq_banner_callback_load(u32 arq_request_ptr) {
    ARQRequest *req = (ARQRequest*)arq_request_ptr;
    gm_banner_t *banner = (gm_banner_t*)req;

    banner->state = GM_LOAD_STATE_LOADED;
    DCFlushRange(banner, sizeof(gm_banner_t));
}

// asset offload helpers
void gm_icon_setup(gm_icon_t *icon, u32 aram_offset) {
    OSReport("Setting up icon\n");
    icon->aram_offset = aram_offset;
    icon->state = GM_LOAD_STATE_SETUP;

    ARQRequest *req = &icon->req;
    u32 owner = make_type('I', 'X', 'X', 'S');
    u32 type = ARAM_DIR_MRAM_TO_ARAM;
    u32 priority = ARQ_PRIORITY_LOW;
    u32 source = (u32)icon->buf->data;
    u32 dest = aram_offset;
    u32 length = ICON_PIXELDATA_LEN;

    dolphin_ARQPostRequest(req, owner, type, priority, source, dest, length, &arq_icon_callback_setup);
}

void gm_icon_setup_unload(gm_icon_t *icon, u32 aram_offset) {
    icon->aram_offset = aram_offset;
    icon->state = GM_LOAD_STATE_UNLOADING;

    ARQRequest *req = &icon->req;
    u32 owner = make_type('I', 'C', 'O', 'U');
    u32 type = ARAM_DIR_MRAM_TO_ARAM;
    u32 priority = ARQ_PRIORITY_LOW;
    u32 source = (u32)icon->buf->data;
    u32 dest = aram_offset;
    u32 length = ICON_PIXELDATA_LEN;

    dolphin_ARQPostRequest(req, owner, type, priority, source, dest, length, &arq_icon_callback_unload);
}

void gm_icon_load(gm_icon_t *icon) {
    if (icon->state == GM_LOAD_STATE_SETUP) {
        OSReport("ERROR: banner is still in setup\n");
    }
    if (icon->state == GM_LOAD_STATE_UNLOADING) {
        OSReport("ERROR: banner is still in unload\n");
    }
    if (icon->state == GM_LOAD_STATE_NONE || icon->state == GM_LOAD_STATE_LOADED) return;
    icon->state = GM_LOAD_STATE_LOADING;

    gm_icon_buf_t *icon_ptr = gm_get_icon_buf();
    if (icon_ptr == NULL) {
        OSReport("ERROR: could not allocate memory\n");
        return;
    }
    icon->buf = icon_ptr;
    DCFlushRange(icon, sizeof(gm_icon_t));

    ARQRequest *req = &icon->req;
    u32 owner = make_type('I', 'C', 'O', 'L');
    u32 type = ARAM_DIR_ARAM_TO_MRAM;
    u32 priority = ARQ_PRIORITY_LOW;
    u32 source = icon->aram_offset;
    u32 dest = (u32)icon->buf->data;
    u32 length = ICON_PIXELDATA_LEN;

    dolphin_ARQPostRequest(req, owner, type, priority, source, dest, length, &arq_icon_callback_load);
}

void gm_icon_free(gm_icon_t *icon) {
    if (icon->state == GM_LOAD_STATE_NONE) return;

    if (icon->state == GM_LOAD_STATE_LOADING || icon->state == GM_LOAD_STATE_SETUP) {
        OSReport("ERROR: icon is still loading\n");
        icon->schedule_free = true;
        return;
    }

    icon->state = GM_LOAD_STATE_UNLOADING;
    gm_free_icon_buf(icon->buf);
    icon->buf = NULL;
    icon->state = GM_LOAD_STATE_UNLOADED;
}

void gm_banner_setup(gm_banner_t *banner, u32 aram_offset) {
    banner->aram_offset = aram_offset;
    banner->state = GM_LOAD_STATE_SETUP;

    ARQRequest *req = &banner->req;
    u32 owner = make_type('B', 'X', 'X', 'S');
    u32 type = ARAM_DIR_MRAM_TO_ARAM;
    u32 priority = ARQ_PRIORITY_LOW;
    u32 source = (u32)banner->buf->data;
    u32 dest = aram_offset;
    u32 length = BNR_PIXELDATA_LEN;

    dolphin_ARQPostRequest(req, owner, type, priority, source, dest, length, &arq_banner_callback_setup);
}

void gm_banner_setup_unload(gm_banner_t *banner, u32 aram_offset) {
    banner->aram_offset = aram_offset;
    banner->state = GM_LOAD_STATE_UNLOADING;

    ARQRequest *req = &banner->req;
    u32 owner = make_type('I', 'X', 'X', 'U');
    u32 type = ARAM_DIR_MRAM_TO_ARAM;
    u32 priority = ARQ_PRIORITY_LOW;
    u32 source = (u32)banner->buf->data;
    u32 dest = aram_offset;
    u32 length = BNR_PIXELDATA_LEN;

    dolphin_ARQPostRequest(req, owner, type, priority, source, dest, length, &arq_banner_callback_unload);
}

void gm_banner_load(gm_banner_t *banner) {
    if (banner->state == GM_LOAD_STATE_SETUP) {
        OSReport("ERROR: banner is still in setup\n");
    }
    if (banner->state == GM_LOAD_STATE_UNLOADING) {
        OSReport("ERROR: banner is still in unload\n");
    }
    if (banner->state == GM_LOAD_STATE_NONE || banner->state == GM_LOAD_STATE_LOADED) return;
    banner->state = GM_LOAD_STATE_LOADING;

    gm_banner_buf_t *banner_ptr = gm_get_banner_buf();
    if (banner_ptr == NULL) {
        OSReport("ERROR: could not allocate memory\n");
        return;
    }
    banner->buf = banner_ptr;
    DCFlushRange(banner, sizeof(gm_banner_t));

    ARQRequest *req = &banner->req;
    u32 owner = make_type('I', 'X', 'X', 'L');
    u32 type = ARAM_DIR_ARAM_TO_MRAM;
    u32 priority = ARQ_PRIORITY_LOW;
    u32 source = banner->aram_offset;
    u32 dest = (u32)banner->buf->data;
    u32 length = BNR_PIXELDATA_LEN;

    dolphin_ARQPostRequest(req, owner, type, priority, source, dest, length, &arq_banner_callback_load);
}

void gm_banner_free(gm_banner_t *banner) {
    if (banner->state == GM_LOAD_STATE_NONE || banner->state == GM_LOAD_STATE_UNLOADING) {
        if (banner->state == GM_LOAD_STATE_UNLOADING) OSReport("ERROR: banner is unloading??\n");
        return;
    }

    if (banner->state == GM_LOAD_STATE_LOADING || banner->state == GM_LOAD_STATE_SETUP) {
        OSReport("ERROR: banner is still loading\n");
        banner->schedule_free = true;
        return;
    }

    banner->state = GM_LOAD_STATE_UNLOADING;
    gm_free_banner_buf(banner->buf);
    banner->buf = NULL;
    banner->state = GM_LOAD_STATE_UNLOADED;
}

// HEAP
pmalloc_t pmblock;
pmalloc_t *pm = &pmblock;
__attribute_aligned_data_lowmem__ static u8 gm_heap_buffer[2 * 1024 * 1024];

// MACROS
#define gm_malloc(x) pmalloc_memalign(pm, x, 32);
#define gm_free(x) pmalloc_freealign(pm, x);

void gm_init_heap() {
    OSReport("Initializing heap [%x]\n", sizeof(gm_heap_buffer));

    // Initialise our pmalloc
	pmalloc_init(pm);
	pmalloc_addblock(pm, &gm_heap_buffer[0], sizeof(gm_heap_buffer));
}

// --- USB Gecko game receive callback ---
// State shared between the callback and gm_thread_worker
static u32 gecko_aram_offset;

static void gecko_add_game_cb(const gecko_game_entry_t *entry, int index) {
    if (gm_entry_count >= 2000) {
        OSReport("WARNING: max game entries reached\n");
        return;
    }

    bool force_unload = false;
    if (gm_entry_count - (top_line_num * ASSETS_PER_LINE) > ASSETS_INITIAL_COUNT) {
        force_unload = true;
    }

    OSReport("Gecko game %d: %.32s\n", index, entry->game_name);

    // Allocate a new backing entry
    gm_file_entry_t *backing = gm_malloc(sizeof(gm_file_entry_t));
    memset(backing, 0, sizeof(gm_file_entry_t));
    backing->type = GM_FILE_TYPE_GAME;

    // Copy game ID and disc info
    memcpy(backing->extra.game_id, entry->game_id, 6);
    backing->extra.disc_num = entry->disc_num;
    backing->extra.disc_ver = entry->disc_ver;
    backing->extra.dvd_bnr_type = entry->bnr_type;

    // Copy text fields into BNRDesc
    memcpy(backing->desc.gameName, entry->game_name, BNR_SHORT_TEXT_LEN);
    memcpy(backing->desc.fullGameName, entry->full_game_name, BNR_FULL_TEXT_LEN);
    memcpy(backing->desc.description, entry->description, BNR_DESC_LEN);

    // Copy banner pixel data into a banner buffer and DMA to ARAM
    gm_banner_buf_t *banner_ptr = gm_get_banner_buf();
    if (banner_ptr != NULL) {
        memcpy(&banner_ptr->data[0], entry->banner_pixels, BNR_PIXELDATA_LEN);
        DCFlushRange(&banner_ptr->data[0], BNR_PIXELDATA_LEN);

        backing->asset.banner.state = GM_LOAD_STATE_LOADING;
        backing->asset.banner.buf = banner_ptr;
        if (force_unload) {
            gm_banner_setup_unload(&backing->asset.banner, gecko_aram_offset);
        } else {
            gm_banner_setup(&backing->asset.banner, gecko_aram_offset);
        }
    }
    gecko_aram_offset += BNR_PIXELDATA_LEN;

    // No separate icon — use banner for cube face
    backing->asset.use_banner = true;
    gecko_aram_offset += ICON_PIXELDATA_LEN;

    // Add to backing array
    gm_entry_backing[gm_entry_count] = backing;
    gm_entry_count++;
    game_backing_count = gm_entry_count;
}

// --- Multi-disc linking ---
static void gm_link_multi_disc(void) {
    for (int i = 0; i < gm_entry_count; i++) {
        gm_file_entry_t *entry = gm_entry_backing[i];
        if (entry->type != GM_FILE_TYPE_GAME) continue;
        if (entry->extra.disc_num == 0) continue; // check Disc 2 only

        for (int j = 0; j < gm_entry_count; j++) {
            gm_file_entry_t *entry2 = gm_entry_backing[j];
            if (entry2->type != GM_FILE_TYPE_GAME) continue;
            if (entry2 == entry) continue;

            bool is_same_game = memcmp(entry->extra.game_id, entry2->extra.game_id, 6) == 0;
            bool is_different_disc = entry->extra.disc_num != entry2->extra.disc_num;
            if (is_same_game && is_different_disc) {
                OSReport("Found multi-disc [%d] <-> [%d]\n", i, j);
                entry->second = entry2;
                entry2->second = entry;
            }
        }
    }
}

// --- Line load/unload for scrolling ---

void gm_line_load(int line_num) {
    for (int i = 0; i < ASSETS_PER_LINE; i++) {
        int index = (line_num * ASSETS_PER_LINE) + i;
        if (index >= gm_entry_count) break;

        gm_file_entry_t *entry = gm_entry_backing[index];
        if (entry->type == GM_FILE_TYPE_GAME) {
            gm_icon_load(&entry->asset.icon);
            gm_banner_load(&entry->asset.banner);
            if (entry->asset.has_save_icon)
                gm_icon_load(&entry->asset.save_icon);
        } else {
            gm_icon_load(&entry->asset.icon);
        }
    }
}

void gm_line_free(int line_num) {
    for (int i = 0; i < ASSETS_PER_LINE; i++) {
        int index = (line_num * ASSETS_PER_LINE) + i;
        if (index >= gm_entry_count) break;

        gm_file_entry_t *entry = gm_entry_backing[index];
        if (entry->type == GM_FILE_TYPE_GAME) {
            gm_icon_free(&entry->asset.icon);
            gm_banner_free(&entry->asset.banner);
            if (entry->asset.has_save_icon)
                gm_icon_free(&entry->asset.save_icon);
        } else {
            gm_icon_free(&entry->asset.icon);
        }
    }
}

void gm_line_changed(int delta) {
    int new_line_num = top_line_num + delta;

    if (delta < 0) {
        int load_line = new_line_num - PRELOAD_LINE_COUNT + 1;
        if (load_line >= 0) {
            gm_line_load(load_line);
        }

        int unload_line = new_line_num + DRAW_TOTAL_ROWS + PRELOAD_LINE_COUNT;
        if (unload_line < number_of_lines) {
            gm_line_free(unload_line);
        }
    } else if (delta > 0) {
        int load_line = new_line_num + DRAW_TOTAL_ROWS + PRELOAD_LINE_COUNT - 1;
        if (load_line < number_of_lines) {
            gm_line_load(load_line);
        }

        int unload_line = new_line_num - PRELOAD_LINE_COUNT;
        if (unload_line >= 0) {
            gm_line_free(unload_line);
        }
    }
}

bool gm_can_move() {
    return true;
}

void gm_setup_grid(int line_count, bool initial) {
    number_of_lines = (line_count + 7) >> 3;
    if (number_of_lines < 4) {
        number_of_lines = 4;
    }

    if (initial) {
        grid_setup_func();
    }
}

// --- Load games from embedded blob (injected as DOL data section) ---

static int gm_load_from_blob(void) {
    const game_blob_hdr_t *hdr = (const game_blob_hdr_t *)GAME_BLOB_ADDR;

    // Invalidate cache so we see what the DOL loader placed in RAM
    DCInvalidateRange((void *)GAME_BLOB_ADDR, sizeof(game_blob_hdr_t));

    if (hdr->magic != GAME_BLOB_MAGIC) {
        OSReport("No game blob at %08x (magic=%08x)\n", GAME_BLOB_ADDR, hdr->magic);
        return -1;
    }

    u32 count = hdr->entry_count;
    u32 esize = hdr->entry_size;
    OSReport("Game blob: %d entries, entry_size=%d\n", count, esize);

    if (esize == 0 || count == 0) return 0;

    // Invalidate the full blob so we read fresh data
    DCInvalidateRange((void *)GAME_BLOB_ADDR,
                      sizeof(game_blob_hdr_t) + count * esize);

    const u8 *base = (const u8 *)(GAME_BLOB_ADDR + sizeof(game_blob_hdr_t));
    u32 aram_offset = (1 * 1024 * 1024); // 1MB mark

    for (u32 i = 0; i < count && i < 2000; i++) {
        const game_blob_entry_t *entry = (const game_blob_entry_t *)(base + i * esize);

        bool force_unload = false;
        if (gm_entry_count - (top_line_num * ASSETS_PER_LINE) > ASSETS_INITIAL_COUNT) {
            force_unload = true;
        }

        OSReport("Blob game %d: %.32s\n", i, entry->game_name);

        gm_file_entry_t *backing = gm_malloc(sizeof(gm_file_entry_t));
        memset(backing, 0, sizeof(gm_file_entry_t));
        backing->type = GM_FILE_TYPE_GAME;

        memcpy(backing->extra.game_id, entry->game_id, 6);
        backing->extra.disc_num = entry->disc_num;
        backing->extra.disc_ver = entry->disc_ver;
        backing->extra.dvd_bnr_type = entry->bnr_type;

        memcpy(backing->desc.gameName, entry->game_name, BNR_SHORT_TEXT_LEN);
        memcpy(backing->desc.fullGameName, entry->full_game_name, BNR_FULL_TEXT_LEN);
        memcpy(backing->desc.description, entry->description, BNR_DESC_LEN);

        // Banner pixel data — copy to buffer and DMA to ARAM
        gm_banner_buf_t *banner_ptr = gm_get_banner_buf();
        if (banner_ptr != NULL) {
            memcpy(&banner_ptr->data[0], entry->banner_pixels, BNR_PIXELDATA_LEN);
            DCFlushRange(&banner_ptr->data[0], BNR_PIXELDATA_LEN);

            backing->asset.banner.state = GM_LOAD_STATE_LOADING;
            backing->asset.banner.buf = banner_ptr;
            if (force_unload) {
                gm_banner_setup_unload(&backing->asset.banner, aram_offset);
            } else {
                gm_banner_setup(&backing->asset.banner, aram_offset);
            }
        }
        aram_offset += BNR_PIXELDATA_LEN;

        // Save icon (32x32 from memory card save, if present)
        if (entry->has_save_icon && entry->save_icon_frame_count > 0) {
            gm_icon_buf_t *icon_ptr = gm_get_icon_buf();
            if (icon_ptr != NULL) {
                // DMA frame 0 to ARAM for the existing load/unload system
                memcpy(&icon_ptr->data[0], entry->save_icon_pixels, ICON_PIXELDATA_LEN);
                DCFlushRange(&icon_ptr->data[0], ICON_PIXELDATA_LEN);

                backing->asset.save_icon.state = GM_LOAD_STATE_LOADING;
                backing->asset.save_icon.buf = icon_ptr;
                backing->asset.has_save_icon = true;
                backing->asset.save_icon_frame_count = entry->save_icon_frame_count;
                backing->asset.save_icon_speed = entry->save_icon_speed;
                // Store direct pointer to all frames in the blob (persists in RAM)
                backing->asset.save_icon_blob = entry->save_icon_pixels;
                if (force_unload) {
                    gm_icon_setup_unload(&backing->asset.save_icon, aram_offset);
                } else {
                    gm_icon_setup(&backing->asset.save_icon, aram_offset);
                }
            }
        }
        aram_offset += ICON_PIXELDATA_LEN;

        backing->asset.use_banner = true;
        aram_offset += ICON_PIXELDATA_LEN;

        gm_entry_backing[gm_entry_count] = backing;
        gm_entry_count++;
        game_backing_count = gm_entry_count;
    }

    return (int)count;
}

// --- Thread worker ---

void *gm_thread_worker(void* param) {
    (void)param;

    // Always probe USB Gecko — needed for LAUNCH command even when using blob
    OSReport("Probing USB Gecko...\n");
    if (!gecko_link_probe()) {
        OSReport("WARNING: USB Gecko not found (LAUNCH will fail)\n");
    } else {
        OSReport("USB Gecko found\n");
    }

    // Set up grid with initial placeholder
    gm_setup_grid(0, true);

    // Try embedded blob first (instant — data already in RAM)
    OSReport("Checking for embedded game blob...\n");
    u64 start_time = gettime();
    int game_count = gm_load_from_blob();
    f32 runtime = (f32)diff_usec(start_time, gettime()) / 1000.0;

    if (game_count >= 0) {
        OSReport("Loaded %d games from blob in %f ms\n", game_count, runtime);
    } else {
        // No blob found — Helper should have injected it into the DOL.
        // Show empty grid; don't try Gecko (game list is no longer sent that way).
        OSReport("ERROR: No game blob found. Was the DOL patched by Helper?\n");
        game_count = 0;
    }

    // Link multi-disc games
    gm_link_multi_disc();

    // Update grid with final count
    gm_setup_grid(gm_entry_count, false);

    game_enum_running = false;
    DCFlushRange((void*)OSRoundDown32B((u32)&game_enum_running), 4);

    return NULL;
}

void gm_init_thread() {
    OSInitMutex(game_enum_mutex);
}

static OSThread thread_obj;
static u8 thread_stack[32 * 1024];
void gm_start_thread(const char *target) {
    (void)target; // Not used — games come from USB Gecko, not a directory path

    if (game_enum_running) {
        OSReport("ERROR: game enum thread is already running\n");
        return;
    }

    OSReport("Starting game receive thread\n");

    game_enum_running = true;
    DCBlockStore((void*)OSRoundDown32B((u32)&game_enum_running));

    // Free any previous entries
    if (gm_entry_count > 0) {
        for (int i = 0; i < gm_entry_count; i++) {
            gm_file_entry_t *entry = gm_entry_backing[i];
            if (entry->type == GM_FILE_TYPE_GAME) {
                gm_icon_free(&entry->asset.icon);
                gm_banner_free(&entry->asset.banner);
                if (entry->asset.has_save_icon)
                    gm_icon_free(&entry->asset.save_icon);
            } else {
                gm_icon_free(&entry->asset.icon);
            }
            gm_free(entry);
        }
        gm_entry_count = 0;
    }

    number_of_lines = 0;
    DCBlockStore((void*)OSRoundDown32B((u32)&number_of_lines));

    game_backing_count = 0;
    DCBlockStore((void*)OSRoundDown32B((u32)&game_backing_count));

    // Start the thread
    u32 thread_stack_size = sizeof(thread_stack);
    void *thread_stack_top = thread_stack + thread_stack_size;
    s32 thread_priority = DEFAULT_THREAD_PRIO + 3;

    dolphin_OSCreateThread(&thread_obj, gm_thread_worker, NULL, thread_stack_top, thread_stack_size, thread_priority, 0);
    dolphin_OSResumeThread(&thread_obj);
}

void gm_deinit_thread() {
    if (game_enum_running) {
        OSReport("Stopping file enum\n");
        OSLockMutex(game_enum_mutex);
        OSReport("Waiting for thread to exit, %d\n", game_enum_running);
        OSJoinThread(&thread_obj, NULL);
        OSReport("File enum done\n");
        OSUnlockMutex(game_enum_mutex);
    }
}
