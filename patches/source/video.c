#include <gctypes.h>
#include <ogc/gx_struct.h>
#include <ogc/video_types.h>

static vu16* const _viReg = (u16*)0xCC002000;

static const struct _timing {
	u8 equ;
	u16 acv;
	u16 prbOdd,prbEven;
	u16 psbOdd,psbEven;
	u8 bs1,bs2,bs3,bs4;
	u16 be1,be2,be3,be4;
	u16 nhlines,hlw;
	u8 hsy,hcs,hce,hbe640;
	u16 hbs640;
	u8 hbeCCIR656;
	u16 hbsCCIR656;
} video_timing[] = {
	{
		0x06,0x00F0,
		0x0018,0x0019,0x0003,0x0002,
		0x0C,0x0D,0x0C,0x0D,
		0x0208,0x0207,0x0208,0x0207,
		0x020D,0x01AD,
		0x40,0x47,0x69,0xA2,
		0x0175,
		0x7A,0x019C
	},
	{
		0x06,0x00F0,
		0x0018,0x0018,0x0004,0x0004,
		0x0C,0x0C,0x0C,0x0C,
		0x0208,0x0208,0x0208,0x0208,
		0x020E,0x01AD,
		0x40,0x47,0x69,0xA2,
		0x0175,
		0x7A,0x019C
	},
	{
		0x05,0x0120,
		0x0021,0x0022,0x0001,0x0000,
		0x0D,0x0C,0x0B,0x0A,
		0x026B,0x026A,0x0269,0x026C,
		0x0271,0x01B0,
		0x40,0x4B,0x6A,0xAC,
		0x017C,
		0x85,0x01A4
	},
	{
		0x05,0x0120,
		0x0021,0x0021,0x0000,0x0000,
		0x0D,0x0B,0x0D,0x0B,
		0x026B,0x026D,0x026B,0x026D,
		0x0270,0x01B0,
		0x40,0x4B,0x6A,0xAC,
		0x017C,
		0x85,0x01A4
	},
	{
		0x06,0x00F0,
		0x0018,0x0019,0x0003,0x0002,
		0x10,0x0F,0x0E,0x0D,
		0x0206,0x0205,0x0204,0x0207,
		0x020D,0x01AD,
		0x40,0x4E,0x70,0xA2,
		0x0175,
		0x7A,0x019C
	},
	{
		0x06,0x00F0,
		0x0018,0x0018,0x0004,0x0004,
		0x10,0x0E,0x10,0x0E,
		0x0206,0x0208,0x0206,0x0208,
		0x020E,0x01AD,
		0x40,0x4E,0x70,0xA2,
		0x0175,
		0x7A,0x019C
	},
	{
		0x0C,0x01E0,
		0x0030,0x0030,0x0006,0x0006,
		0x18,0x18,0x18,0x18,
		0x040E,0x040E,0x040E,0x040E,
		0x041A,0x01AD,
		0x40,0x47,0x69,0xA2,
		0x0175,
		0x7A,0x019C
	},
	{
		0x0C,0x01E0,
		0x002C,0x002C,0x000A,0x000A,
		0x18,0x18,0x18,0x18,
		0x040E,0x040E,0x040E,0x040E,
		0x041A,0x01AD,
		0x40,0x47,0x69,0xA8,
		0x017B,
		0x7A,0x019C
	},
	{
		0x0A,0x0240,
		0x0044,0x0044,0x0000,0x0000,
		0x14,0x14,0x14,0x14,
		0x04D8,0x04D8,0x04D8,0x04D8,
		0x04E2,0x01B0,
		0x40,0x4B,0x6A,0xAC,
		0x017C,
		0x85,0x01A4
	}
};

static const struct _timing* __gettiming(u32 vimode)
{
	switch(vimode) {
		case VI_TVMODE_NTSC_INT:
		case VI_TVMODE_EURGB60_INT:
			return &video_timing[0];
			break;
		case VI_TVMODE_NTSC_DS:
		case VI_TVMODE_EURGB60_DS:
			return &video_timing[1];
			break;
		case VI_TVMODE_PAL_INT:
		case VI_TVMODE_DEBUG_PAL_INT:
			return &video_timing[2];
			break;
		case VI_TVMODE_PAL_DS:
		case VI_TVMODE_DEBUG_PAL_DS:
			return &video_timing[3];
			break;
		case VI_TVMODE_MPAL_INT:
			return &video_timing[4];
			break;
		case VI_TVMODE_MPAL_DS:
			return &video_timing[5];
			break;
		case VI_TVMODE_NTSC_PROG:
		case VI_TVMODE_MPAL_PROG:
		case VI_TVMODE_EURGB60_PROG:
			return &video_timing[6];
			break;
		case VI_TVMODE_NTSC_3D:
			return &video_timing[7];
			break;
		case VI_TVMODE_PAL_PROG:
			return &video_timing[8];
			break;
		default:
			break;
	}
	return NULL;
}

void ogc__VIInit(u32 vimode) {
	vu32 cnt;
	u32 vi_mode,interlace,progressive;
	const struct _timing *cur_timing = NULL;

	vi_mode = ((vimode>>2)&0x07);
	interlace = (vimode&0x01);
	progressive = (vimode&0x02);

	cur_timing = __gettiming(vimode);

	//reset the interface
	_viReg[1] = 0x02;
	cnt = 0;
	while(cnt<1000) cnt++;
	_viReg[1] = 0x00;

	// now begin to setup the interface
	_viReg[2] = ((cur_timing->hcs<<8)|cur_timing->hce);		//set HCS & HCE
	_viReg[3] = cur_timing->hlw;							//set Half Line Width

	_viReg[4] = (cur_timing->hbs640<<1);					//set HBS640
	_viReg[5] = ((cur_timing->hbe640<<7)|cur_timing->hsy);	//set HBE640 & HSY

	_viReg[0] = cur_timing->equ;

	_viReg[6] = (cur_timing->psbOdd+2);							//set PSB odd field
	_viReg[7] = (cur_timing->prbOdd+((cur_timing->acv<<1)-2));	//set PRB odd field

	_viReg[8] = (cur_timing->psbEven+2);						//set PSB even field
	_viReg[9] = (cur_timing->prbEven+((cur_timing->acv<<1)-2));	//set PRB even field

	_viReg[10] = ((cur_timing->be3<<5)|cur_timing->bs3);		//set BE3 & BS3
	_viReg[11] = ((cur_timing->be1<<5)|cur_timing->bs1);		//set BE1 & BS1

	_viReg[12] = ((cur_timing->be4<<5)|cur_timing->bs4);		//set BE4 & BS4
	_viReg[13] = ((cur_timing->be2<<5)|cur_timing->bs2);		//set BE2 & BS2

	_viReg[24] = (0x1000|((cur_timing->nhlines/2)+1));
	_viReg[25] = (cur_timing->hlw+1);

	_viReg[26] = 0x1001;		//set DI1
	_viReg[27] = 0x0001;		//set DI1
	_viReg[36] = 0x2828;		//set HSR

	if(vi_mode<VI_PAL && vi_mode>=VI_DEBUG_PAL) vi_mode = VI_NTSC;
	if(progressive){
		_viReg[1] = ((vi_mode<<8)|0x0005);		//set MODE & INT & enable
		_viReg[54] = 0x0001;
	} else {
		_viReg[1] = ((vi_mode<<8)|(interlace<<2)|0x0001);
		_viReg[54] = 0x0000;
	}
}
