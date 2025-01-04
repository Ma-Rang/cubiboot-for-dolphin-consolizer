#include "dolphin_os.h"
#include "mcp.h"

#include "os.h"
#include "attr.h"

#include "reloc.h"
#include "picolibc.h"

__attribute_data__ u32 disable_mcp_select = 0;
void mcp_set_gameid() {
    if (disable_mcp_select) return;
    OSMessageQueue *card_thread_mq = (void*)0x8148bfb0;
    OSSendMessage(card_thread_mq, (OSMessage)0xc, 0);
}

static void setup_gameid_commands(struct gcm_disk_info *di, char diskName[64]) {
    const s32 chan = 0;
    u32 id;
    s32 ret;

    while ((ret = MCP_ProbeEx(chan)) == MCP_RESULT_BUSY);
    if (ret < 0) return;
    while ((ret = MCP_GetDeviceID(chan, &id)) == MCP_RESULT_BUSY);
    if (ret < 0) return;
    while ((ret = MCP_SetDiskID(chan, di)) == MCP_RESULT_BUSY);
    if (ret < 0) return;
    while ((ret = MCP_SetDiskInfo(chan, diskName)) == MCP_RESULT_BUSY);
    if (ret < 0) return;
}

BOOL pre_custom_card_OSSendMessage(OSMessageQueue* mq, OSMessage msg, s32 flags) {
    OSReport("Sending message %d to %08x\n", msg, mq);

    struct gcm_disk_info diskID = {
        .game_code = { 0x47, 0x58, 0x58, 0x45 },
        .maker_code = { 0x30, 0x31 },
        .disk_id = 0,
        .version = 0,
    };
    DCFlushRange(&diskID, sizeof(diskID));

    char diskInfo[64];
    memset(diskInfo, 0, 64);
    strcpy(diskInfo, "SWISS");
    DCFlushRange(diskInfo, 64);

    setup_gameid_commands(&diskID, diskInfo);

    return OSSendMessage(mq, msg, flags);
}

// void scrap() {
//     u8 (*check_card_thread)() = (void*)0x8131b5d8;

//     // disable stop commands
//     *(u32*)0x8131b7f4 = PPC_NOP;
//     ICBlockInvalidate((void*)0x8131b7f4);

//     *(u32*)0x8131b804 = PPC_NOP;
//     ICBlockInvalidate((void*)0x8131b804);
    
//     OSReport("Checking card thread\n");
//     do {
//         pause_card_thread();
//         update_card_thread();
//         OSYieldThread();

//         // OSThread *card_thread = (void*)0x8148c050;
//         // OSJoinThread(&thread_obj, NULL);
//     } while (check_card_thread());

//     OSReport("Card thread is paused\n");
// }