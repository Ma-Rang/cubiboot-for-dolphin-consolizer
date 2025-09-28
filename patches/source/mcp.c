/* 
 * Copyright (c) 2022, Extrems <extrems@extremscorner.org>
 * 
 * This file is part of Swiss.
 * 
 * Swiss is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Swiss is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * with Swiss.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <string.h>
#include "usbgecko.h"
#include "mcp.h"
#include "gcm.h"
#include "attr.h"
#include "reloc.h"

static const char digits[16] = "0123456789ABCDEF";

// defs from libogc2
__attribute_reloc__ s32 (*CARDProbe)(s32 chn);
__attribute_reloc__ s32 (*EXILock)(s32 nChn,s32 nDev,EXICallback unlockCB);
__attribute_reloc__ s32 (*EXIUnlock)(s32 nChn);
__attribute_reloc__ s32 (*EXISelect)(s32 nChn,s32 nDev,s32 nFrq);
__attribute_reloc__ s32 (*EXIDeselect)(s32 nChn);
__attribute_reloc__ s32 (*EXISync)(s32 nChn);
__attribute_reloc__ s32 (*EXIImm)(s32 nChn,void *pData,u32 nLen,u32 nMode,EXICallback tc_cb);
__attribute_reloc__ s32 (*EXIImmEx)(s32 nChn,void *pData,u32 nLen,u32 nMode);

s32 MCP_ProbeEx(s32 chan)
{
	return CARDProbe(chan);
}

s32 MCP_GetDeviceID(s32 chan, u32 *id)
{
	bool err = false;
	u8 cmd[2];

	if (!EXILock(chan, EXI_DEVICE_0, NULL)) return MCP_RESULT_BUSY;
	if (!EXISelect(chan, EXI_DEVICE_0, EXI_SPEED_16MHZ)) {
		EXIUnlock(chan);
		return MCP_RESULT_NOCARD;
	}

	cmd[0] = 0x8B;
	cmd[1] = 0x00;

	err |= !EXIImm(chan, cmd, 2, EXI_WRITE, NULL);
	err |= !EXISync(chan);
	err |= !EXIImm(chan, id, 4, EXI_READ, NULL);
	err |= !EXISync(chan);
	err |= !EXIDeselect(chan);
	EXIUnlock(chan);

	if (err)
		return MCP_RESULT_NOCARD;
	else if (*id >> 16 != 0x3842)
		return MCP_RESULT_WRONGDEVICE;
	else
		return MCP_RESULT_READY;
}

s32 MCP_SetDiskID(s32 chan, const struct gcm_disk_info *diskID)
{
	bool err = false;
	u8 cmd[12];

	if (!EXILock(chan, EXI_DEVICE_0, NULL)) return MCP_RESULT_BUSY;
	if (!EXISelect(chan, EXI_DEVICE_0, EXI_SPEED_16MHZ)) {
		EXIUnlock(chan);
		return MCP_RESULT_NOCARD;
	}

	memset(cmd, 0, sizeof(cmd));
	cmd[0] = 0x8B;
	cmd[1] = 0x11;

	if (diskID) {
		memcpy(&cmd[2], diskID->game_code, 4);
		memcpy(&cmd[6], diskID->maker_code,  2);
		cmd[8]  = digits[diskID->disk_id / 16];
		cmd[9]  = digits[diskID->disk_id % 16];
		cmd[10] = digits[diskID->version / 16];
		cmd[11] = digits[diskID->version % 16];
	}

	err |= !EXIImmEx(chan, cmd, sizeof(cmd), EXI_WRITE);
	err |= !EXIDeselect(chan);
	EXIUnlock(chan);

	return err ? MCP_RESULT_NOCARD : MCP_RESULT_READY;
}

s32 MCP_GetDiskInfo(s32 chan, char diskInfo[64])
{
	bool err = false;
	u8 cmd[2];

	OSReport("bopA\n");
	if (!EXILock(chan, EXI_DEVICE_0, NULL)) return MCP_RESULT_BUSY;
	OSReport("bopB\n");
	if (!EXISelect(chan, EXI_DEVICE_0, EXI_SPEED_16MHZ)) {
		OSReport("bopC\n");
		EXIUnlock(chan);
		return MCP_RESULT_NOCARD;
	}


	OSReport("bopD\n");
	cmd[0] = 0x8B;
	cmd[1] = 0x12;

	err |= !EXIImm(chan, cmd, 2, EXI_WRITE, NULL);
	OSReport("bopE\n");
	err |= !EXISync(chan);
	OSReport("bopF\n");
	err |= !EXIImmEx(chan, diskInfo, 64, EXI_READ);
	OSReport("bopG\n");
	err |= !EXIDeselect(chan);
	OSReport("bopH\n");
	EXIUnlock(chan);
	OSReport("bopI, err=%d\n", err);

	return err ? MCP_RESULT_NOCARD : MCP_RESULT_READY;
}

s32 MCP_SetDiskInfo(s32 chan, const char diskInfo[64])
{
	bool err = false;
	u8 cmd[2];

	if (!EXILock(chan, EXI_DEVICE_0, NULL)) return MCP_RESULT_BUSY;
	if (!EXISelect(chan, EXI_DEVICE_0, EXI_SPEED_16MHZ)) {
		EXIUnlock(chan);
		return MCP_RESULT_NOCARD;
	}

	cmd[0] = 0x8B;
	cmd[1] = 0x13;

	err |= !EXIImm(chan, cmd, 2, EXI_WRITE, NULL);
	err |= !EXISync(chan);
	err |= !EXIImmEx(chan, (char *)diskInfo, 64, EXI_WRITE);
	err |= !EXIDeselect(chan);
	EXIUnlock(chan);

	return err ? MCP_RESULT_NOCARD : MCP_RESULT_READY;
}
