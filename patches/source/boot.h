#include <gctypes.h>

#include "games.h"

#define STUB_ADDR 0x80001800

typedef struct {
    u32 max_addr;
    void *entrypoint;
} dol_info_t;

void load_stub();
dol_info_t load_dol(char *path, bool flash);

void *load_apploader();
// void prepare_game_lowmem(char *boot_path);
void legacy_chainload_boot_game(gm_file_entry_t *boot_entry, bool passthrough);
void chainload_swiss_game(char *game_path, bool passthrough, bool enable_patches);

void run(register void* entry_point);
