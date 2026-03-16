// Dolphin Launcher — USB Gecko communication with Helper app.
// Copyright 2024-2026, licensed under GPL-2.0-or-later.
//
// Protocol: GC sends HELLO, Helper responds with game list + banners.
// GC sends LAUNCH when the user picks a game.

#pragma once

#include <gctypes.h>
#include <stdbool.h>
#include "bnr.h"
#include "icon.h"

// --- Protocol command bytes ---
// Helper -> GC
#define GECKO_CMD_GAME_LIST   0x01
#define GECKO_CMD_LIST_END    0x02

// GC -> Helper
#define GECKO_CMD_HELLO       0x10
#define GECKO_CMD_LAUNCH      0x13
#define GECKO_CMD_DBG         0xDB  // Debug log line (text, DEBUG builds only)

// HELLO type byte
#define GECKO_TYPE_GC         0x01

// --- Per-game entry sent by Helper ---
// All multi-byte integers are big-endian.
typedef struct {
    u8  game_id[6];
    u8  disc_num;
    u8  disc_ver;
    u8  bnr_type;                // 0 = BNR1, 1 = BNR2
    u8  has_save_icon;           // 1 if save_icon_pixels contains valid icon(s)
    u8  save_icon_frame_count;   // number of animation frames (1-8)
    u8  padding1;                // alignment
    u16 save_icon_speed;         // GCI iconSpeed field
    u8  padding2[2];             // alignment
    char game_name[32];          // BNRDesc.gameName (short)
    char full_game_name[64];     // BNRDesc.fullGameName
    char description[128];       // BNRDesc.description
    u8  banner_pixels[BNR_PIXELDATA_LEN]; // 96x32 RGB5A3 = 6144 bytes
    u8  save_icon_pixels[ICON_PIXELDATA_LEN * ICON_MAX_FRAMES]; // 8 frames, 32x32 RGB5A3
    u8  padding3[16];            // pad to 32-byte multiple for GX texture alignment
} gecko_game_entry_t;

// --- API ---

// Probe for USB Gecko on Slot B.  Returns true if present.
bool gecko_link_probe(void);

// Send HELLO to Helper.  Blocks until acknowledged.
// Returns true on success.
bool gecko_link_hello(void);

// Receive the game list from Helper.
// Calls add_game_cb for each entry.  Returns total game count, or -1 on error.
// The callback receives the entry and a sequential index (0-based).
typedef void (*gecko_add_game_fn)(const gecko_game_entry_t *entry, int index);
int gecko_link_receive_games(gecko_add_game_fn add_game_cb);

// Send LAUNCH command with the selected game index (0-based).
void gecko_link_launch(u16 game_index);

// Send a debug log line to the Helper (GECKO_CMD_DBG frame).
// Only meaningful when the Helper's GC Log tab is open.
void gecko_link_debug(const char *msg, u16 len);
