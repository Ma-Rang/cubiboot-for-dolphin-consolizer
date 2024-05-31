#include <gctypes.h>

// // taken from gcmm
// #define CARD_MAXICONS 8
// #define CARD_GetIconSpeed(icon_speed,n)	(((icon_speed)>>(2*(n))) & 0x03)
// #define CARD_GetIconSpeedBounce(icon_speed,n,i)	((n) < (i) ? (((icon_speed)>>(2*(n))) & 0x03) : (((icon_speed)>>(2*((i*2)-2-n)) & 0x03)))
// #define CARD_GetIconAnim(banner_fmt) ((banner_fmt) & 0x04)

// // taken from libogc2
// #define CARD_FILENAMELEN 32
// #define CARD_MAXFILES 127

// struct card_direntry {
// 	u8 gamecode[4];
// 	u8 company[2];
// 	u8 pad_00;
// 	u8 bannerfmt;
// 	u8 filename[CARD_FILENAMELEN];
// 	u32 lastmodified;
// 	u32 iconaddr;
// 	u16 iconfmt;
// 	u16 iconspeed;
// 	u8 permission;
// 	u8 copytimes;
// 	u16 block;
// 	u16 length;
// 	u16 pad_01;
// 	u32 commentaddr;
// } __attribute__((packed));

// typedef struct card_direntry card_direntry_t;

typedef union {
    struct {
        u8 gamecode[4];
        u8 company[2];
    } parts;
    u8 blob[6];
} gameid_t;

// typedef struct {
//     gameid_t id;
//     // u8 bannerfmt;
// 	// u16 iconfmt;
// 	// u16 iconspeed;
// 	u16 length;
//     u8 frame_index[32]; // 32 for 60Hz and 24 entries for 50Hz
// } icon_map_slot_t;

// extern icon_map_slot_t icon_map[2][CARD_MAXFILES];
