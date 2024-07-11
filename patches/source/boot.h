#include <gctypes.h>

#define STUB_ADDR 0x80001800

typedef struct {
    u32 max_addr;
    void *entrypoint;
} dol_info_t;

void load_stub();
dol_info_t load_dol(char *path, bool flash);

void *load_apploader();
void prepare_game_lowmem(char *boot_path);
void chainload_boot_game(char *boot_path, bool passthrough);

void run(register void* entry_point);
