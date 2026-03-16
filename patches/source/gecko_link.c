// Dolphin Launcher — USB Gecko communication with Helper app.
// Copyright 2024-2026, licensed under GPL-2.0-or-later.
//
// Implements the protocol defined in gecko_link.h using low-level EXI
// register access to talk over USB Gecko (EXI Channel 1, Device 0).

#include <stdint.h>
#include <stdbool.h>
#include <gctypes.h>

#include "gecko_link.h"
#include "dolphin_os.h"

// --- Low-level EXI register access ---
// These duplicate the static functions in usbgecko.c because those are
// guarded by GECKO_PRINT_ENABLE and we need Gecko access unconditionally.

extern volatile u32 EXI[3][5];

#define GECKO_EXI_CHAN  1
#define GECKO_EXI_DEV   0
#define GECKO_EXI_SPEED 5  // EXI_SPEED_32MHZ

static void gecko_exi_select(void)
{
    EXI[GECKO_EXI_CHAN][0] = (EXI[GECKO_EXI_CHAN][0] & 0x405) |
                             ((1 << GECKO_EXI_DEV) << 7) |
                             (GECKO_EXI_SPEED << 4);
}

static void gecko_exi_deselect(void)
{
    EXI[GECKO_EXI_CHAN][0] &= 0x405;
}

static u32 gecko_exi_imm(u32 data, u32 len)
{
    EXI[GECKO_EXI_CHAN][4] = data;
    // EXI_READ_WRITE=2, start bit=1
    EXI[GECKO_EXI_CHAN][3] = ((len - 1) << 4) | (2 << 2) | 1;
    while (EXI[GECKO_EXI_CHAN][3] & 1)
        ;
    return EXI[GECKO_EXI_CHAN][4] >> ((4 - len) * 8);
}

// --- USB Gecko primitives ---

static bool gecko_usb_probe(void)
{
    gecko_exi_select();
    u16 val = gecko_exi_imm(0x9 << 28, 2);
    gecko_exi_deselect();
    return val == 0x470;
}

static bool gecko_tx_byte(u8 byte)
{
    gecko_exi_select();
    u16 val = gecko_exi_imm(0xB << 28 | (u32)byte << 20, 2);
    gecko_exi_deselect();
    return (val & 0x400) != 0;  // bit 26 of 32-bit result = sent ok
}

static bool gecko_tx_check(void)
{
    gecko_exi_select();
    u8 val = gecko_exi_imm(0xC << 28, 1);
    gecko_exi_deselect();
    return !(val & 0x4);
}

static bool gecko_rx_byte(u8 *out)
{
    gecko_exi_select();
    u16 val = gecko_exi_imm(0xA << 28, 2);
    gecko_exi_deselect();
    *out = (u8)val;
    return (val & 0x800) != 0;  // bit 27 of 32-bit result = byte received
}

static bool gecko_rx_check(void)
{
    gecko_exi_select();
    u8 val = gecko_exi_imm(0xD << 28, 1);
    gecko_exi_deselect();
    return !(val & 0x4);
}

// --- Blocking bulk transfer ---

// Send exactly `len` bytes.  Spins until complete.
static void gecko_send(const void *buf, u32 len)
{
    const u8 *p = (const u8 *)buf;
    for (u32 i = 0; i < len; i++) {
        while (!gecko_tx_byte(p[i])) {
            // FIFO full — wait for space
            while (gecko_tx_check())
                ;
        }
    }
}

// Receive exactly `len` bytes.  Yields periodically to avoid starving
// other threads (the boot animation runs on the main thread).
static void gecko_recv(void *buf, u32 len)
{
    u8 *p = (u8 *)buf;
    for (u32 i = 0; i < len; i++) {
        while (!gecko_rx_byte(&p[i])) {
            // Nothing available — yield to let other threads run
            OSYieldThread();
            while (gecko_rx_check())
                ;
        }
    }
}

// --- Protocol helpers ---
// Frame: [1 byte cmd] [2 bytes BE payload length] [N bytes payload]

static void gecko_send_frame(u8 cmd, const void *payload, u16 payload_len)
{
    u8 hdr[3];
    hdr[0] = cmd;
    hdr[1] = (u8)(payload_len >> 8);
    hdr[2] = (u8)(payload_len & 0xFF);
    gecko_send(hdr, 3);
    if (payload_len > 0 && payload != NULL) {
        gecko_send(payload, payload_len);
    }
}

static u8 gecko_recv_frame_header(u16 *out_len)
{
    u8 hdr[3];
    gecko_recv(hdr, 3);
    *out_len = ((u16)hdr[1] << 8) | hdr[2];
    return hdr[0];
}

// --- Public API ---

bool gecko_link_probe(void)
{
    return gecko_usb_probe();
}

bool gecko_link_hello(void)
{
    // Send HELLO with type byte
    u8 type = GECKO_TYPE_GC;
    gecko_send_frame(GECKO_CMD_HELLO, &type, 1);

    // Wait for Helper to acknowledge — it sends back the first game list
    // frame or LIST_END.  We consider HELLO successful if we get any
    // valid response command byte.
    //
    // (The caller will immediately call gecko_link_receive_games which
    //  reads the actual frames, so we just need to confirm the link is
    //  alive.  We peek at one byte then push it back by... actually,
    //  we can't push back.  Instead, the contract is: after hello()
    //  succeeds, the caller MUST call receive_games() which reads the
    //  response frames.  hello() itself is fire-and-send.)
    return true;
}

int gecko_link_receive_games(gecko_add_game_fn add_game_cb)
{
    int count = 0;
    // Temporary buffer for one entry — must be aligned for DMA later
    gecko_game_entry_t entry __attribute__((aligned(32)));

    while (1) {
        u16 payload_len = 0;
        u8 cmd = gecko_recv_frame_header(&payload_len);

        if (cmd == GECKO_CMD_LIST_END) {
            // No more games
            break;
        }

        if (cmd != GECKO_CMD_GAME_LIST) {
            // Unexpected command — protocol error.  Skip payload.
            // In practice this shouldn't happen.
            u8 discard;
            for (u16 i = 0; i < payload_len; i++)
                gecko_recv(&discard, 1);
            continue;
        }

        // Sanity check: payload should be exactly one entry
        if (payload_len != sizeof(gecko_game_entry_t)) {
            // Size mismatch — skip this entry
            u8 discard;
            for (u16 i = 0; i < payload_len; i++)
                gecko_recv(&discard, 1);
            continue;
        }

        gecko_recv(&entry, sizeof(gecko_game_entry_t));

        if (add_game_cb) {
            add_game_cb(&entry, count);
        }
        count++;

        // Yield after each entry so the boot animation stays smooth.
        OSYieldThread();
    }

    return count;
}

void gecko_link_launch(u16 game_index)
{
    u8 payload[2];
    payload[0] = (u8)(game_index >> 8);
    payload[1] = (u8)(game_index & 0xFF);
    gecko_send_frame(GECKO_CMD_LAUNCH, payload, 2);
}

void gecko_link_debug(const char *msg, u16 len)
{
    gecko_send_frame(GECKO_CMD_DBG, msg, len);
}
