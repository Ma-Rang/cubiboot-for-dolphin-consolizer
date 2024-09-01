#if 0
#include <gctypes.h>

#include "../../cubeboot/include/bnr.h"

typedef struct {
    bool is_dol;
    bool is_gcm;
    bool is_valid;
    u32 fst_size;
    u32 fst_offset;
    u32 dvd_bnr_size;
    u32 dvd_bnr_offset;
    // u32 mem_bnr_offset;
    // u32 aram_bnr_offset;
    char disc_name[64];
    char iso_path[128];

    // ARQRequest aram_req;
} game_backing_entry_t;

typedef struct {
    // BNR banner;
    u8 pixelData[BNR_PIXELDATA_LEN];
    int backing_index;
    s8 in_use;
    // ????
    u8 game_id[6];
    // u8 padding[96 - (4 * 2) - 6]; // pad to 8kb
} game_asset_t;

extern int game_backing_count;
extern OSMutex *game_enum_mutex;
// extern game_backing_entry_t *game_backing_list[2000];

void start_file_enum();
void start_main_loop();

bool is_dol_slot(int slot_num);

game_asset_t *claim_game_asset();
void free_game_asset(int backing_index);
game_asset_t *get_game_asset(int backing_index);

const char *get_game_path(int backing_index);
#endif
