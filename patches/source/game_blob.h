// Dolphin Launcher — Embedded game list blob format.
// Copyright 2024-2026, licensed under GPL-2.0-or-later.
//
// The Helper embeds the game list as a DOL data section at GAME_BLOB_ADDR.
// The GC launcher reads it directly from memory — no USB Gecko transfer needed.
// USB Gecko is only used for the LAUNCH command back to Helper.

#pragma once

#include <gctypes.h>
#include "bnr.h"
#include "icon.h"

// Load address for the game blob DOL section.
// Must match the value in dol_patcher.py.
#define GAME_BLOB_ADDR   0x81200000
#define GAME_BLOB_MAGIC  0x444C474C  // "DLGL" (Dolphin Launcher Game List)

// Per-game entry within the blob (packed, big-endian).
typedef struct {
    u8   game_id[6];
    u8   disc_num;
    u8   disc_ver;
    u8   bnr_type;                // 0 = BNR1, 1 = BNR2
    u8   has_save_icon;           // 1 if save_icon_pixels contains valid icon(s)
    u8   save_icon_frame_count;   // number of animation frames (1-8), 0 if no icon
    u8   padding1;                // alignment
    u16  save_icon_speed;         // GCI iconSpeed field (2 bits per frame timing)
    u8   padding2[2];             // alignment to 16
    char game_name[32];           // BNRDesc.gameName (short)
    char full_game_name[64];      // BNRDesc.fullGameName
    char description[128];        // BNRDesc.description
    u8   banner_pixels[BNR_PIXELDATA_LEN];  // 96x32 RGB5A3 = 6144 bytes
    u8   save_icon_pixels[ICON_PIXELDATA_LEN * ICON_MAX_FRAMES]; // 8 frames, 32x32 RGB5A3 = 16384 bytes
    u8   padding3[16];            // pad to 32-byte multiple for GX texture alignment
} game_blob_entry_t;              // 22784 bytes (32 * 712)

// Blob header at GAME_BLOB_ADDR.
typedef struct {
    u32 magic;               // GAME_BLOB_MAGIC
    u32 entry_count;         // number of game_blob_entry_t that follow
    u32 entry_size;          // sizeof(game_blob_entry_t) — for forward compat
    u32 reserved;            // padding to 16 bytes
} game_blob_hdr_t;

// Entries follow immediately after the header:
//   game_blob_hdr_t hdr;
//   game_blob_entry_t entries[hdr.entry_count];
