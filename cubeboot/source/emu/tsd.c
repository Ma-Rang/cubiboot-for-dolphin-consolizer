#include "tsd.h"

#include <string.h>

#ifdef IPL_CODE
#include "../usbgecko.h"
#include "../reloc.h"
#include "../time.h"
#else
#include <stdio.h>
#include <sdcard/card_cmn.h>
#include <sdcard/card_io.h>
#include <ogc/exi.h>
#endif

#ifdef IPL_CODE
extern s32 (*EXILock)(s32 nChn, s32 nDev, EXICallback unlockCB);
extern s32 (*EXIUnlock)(s32 nChn);
extern s32 (*EXISelect)(s32 nChn, s32 nDev, s32 nFrq);
extern s32 (*EXIDeselect)(s32 nChn);
extern s32 (*EXISync)(s32 nChn);
extern s32 (*EXIImm)(s32 nChn, void *pData, u32 nLen, u32 nMode, EXICallback tc_cb);
extern s32 (*EXIImmEx)(s32 nChn, void *pData, u32 nLen, u32 nMode);
extern s32 (*CARDProbe)(s32 chn);

extern volatile u32 EXI[3][5];

static bool tsd_native = false;
void tsd_set_native(bool native) {
    tsd_native = native;
}

bool tsd_select(exi_port port) {
    if (tsd_native) {
        EXI[port.chn][0] = (EXI[port.chn][0] & 0x405) | ((1 << port.dev) << 7) | (EXI_SPEED_16MHZ << 4);
        return true;
    }

    if (!EXILock(port.chn, port.dev, NULL))
        return false;
    if (!EXISelect(port.chn, port.dev, EXI_SPEED_16MHZ)) {
        EXIUnlock(port.chn);
        return false;
    }
    return true;
}

bool tsd_deselect(exi_port port) {
    if (tsd_native) {
        EXI[port.chn][0] &= 0x405;
        return true;
    }

    EXIDeselect(port.chn);
    EXIUnlock(port.chn);
    return true;
}

static uint8_t native_exi_imm_rw(exi_port port, uint8_t data) {
    EXI[port.chn][4] = data << 24;
    EXI[port.chn][3] = ((1 - 1) << 4) | (EXI_READ_WRITE << 2) | 1;
    while (EXI[port.chn][3] & 1);
    return EXI[port.chn][4] >> ((4 - 1) * 8);
}

bool tsd_read(exi_port port, uint8_t* data, uint32_t len) {
    if (tsd_native) {
        for (int i = 0; i < len; i++)
            data[i] = native_exi_imm_rw(port, 0xFF);
        return true;
    }
    
    memset(data, 0xFF, len);
    return EXIImmEx(port.chn, data, len, EXI_READ_WRITE);
}

bool tsd_write(exi_port port, const uint8_t* data, uint32_t len) {
    if (tsd_native) {
        for (int i = 0; i < len; i++)
            native_exi_imm_rw(port, data[i]);
        return true;
    }

    return EXIImmEx(port.chn, (uint8_t*)data, len, EXI_WRITE);
}

bool tsd_recv_data(exi_port port, uint8_t* data, uint32_t len) {
    uint8_t token;
    do {
        if (!tsd_read(port, &token, 1))
            return false;
    } while(token != 0xFE);

    if (!tsd_read(port, data, len))
        return false;

    uint16_t crc;
    return tsd_read(port, (uint8_t*)&crc, 2);
}

bool tsd_send_data(exi_port port, const uint8_t* data, uint8_t len) {
    const uint8_t token = 0xFE;
    if (!tsd_write(port, &token, 1))
        return false;

    if (!tsd_write(port, data, len))
        return false;

    const uint16_t crc = 0xFFFF;
    return tsd_write(port, (uint8_t*)&crc, 2);
}

// copied from libogc2 (https://github.com/extremscorner/libogc2/) and licenced under GPL-2.0
static uint8_t tsd_crc7(void* buffer, uint32_t len) {
	uint32_t mask,cnt,bcnt;
	uint32_t res,val;
	uint8_t *ptr = (uint8_t*)buffer;

	cnt = 0;
	res = 0;
	while(cnt<len) {
		bcnt = 0;
		mask = 0x80;
		while(bcnt<8) {
			res <<= 1;
			val = *ptr^((res>>bcnt)&0xff);
			if(mask&val) {
				res |= 0x01;
				if(!(res&0x0008)) res |= 0x0008;
				else res &= ~0x0008;
				
			} else if(res&0x0008) res |= 0x0008;
			else res &= ~0x0008;
			
			mask >>= 1;
			bcnt++;
		}
		ptr++;
		cnt++;
	}
	return (res<<1)&0xff;
}

bool tsd_send_cmd(exi_port port, uint8_t cmd, uint32_t arg, uint8_t* resp, uint8_t resp_len) {
    static uint8_t dummy[100];
    tsd_read(port, dummy, 100);

    uint8_t cmd_buf[6];
    cmd_buf[0] = 0x40 | cmd;
    cmd_buf[1] = (arg >> 24) & 0xFF;;
    cmd_buf[2] = (arg >> 16) & 0xFF;
    cmd_buf[3] = (arg >> 8) & 0xFF;;
    cmd_buf[4] = (arg >> 0) & 0xFF;;
    cmd_buf[5] = tsd_crc7(cmd_buf, 5) | 1;

    if (!tsd_write(port, cmd_buf, 6))
        return false;
    
    int timeout = 0;
    do {
        if (timeout++ > 16)
            return false;

        if (!tsd_read(port, resp, 1))
            return false;
    } while((resp[0] & 0x80) != 0);

    if (timeout)

    if (resp_len > 1)
        return tsd_read(port, &resp[1], resp_len - 1);

    return true;
}

bool tsd_send_app_cmd(exi_port port, uint8_t cmd, uint32_t arg, uint8_t* resp, uint8_t resp_len) {
    if (!tsd_send_cmd(port, 55, 0, resp, 1))
        return false;
    if ((resp[0] & 0xFE) != 0)
        return false;

    return tsd_send_cmd(port, cmd, arg, resp, resp_len);
}

static bool addressing_block = false;

bool tsd_sd_init(exi_port port) {
    if (!tsd_select(port))
        return false;

    uint8_t resp[5];
    if (!tsd_send_cmd(port, 58, 0, resp, 3) || resp[0] != 0) // READ_OCR
        goto error;
    addressing_block = (resp[1] & 0x40) != 0;
    
    tsd_deselect(port);
    return true;

    error:
    tsd_deselect(port);
    return false;
}

bool tsd_sd_read(exi_port port, uint32_t addr, uint8_t* data, uint32_t len) {
    if (!tsd_select(port))
        return false;
    if (!addressing_block)
        addr *= 512;

    uint8_t resp;
    for (int i = 0; i < len; i++) {
        if (!tsd_send_cmd(port, 17, addr, &resp, 1) || resp != 0)
            goto error;
        if (!tsd_recv_data(port, &data[i * 512], 512))
            goto error;

        addr += addressing_block ? 1 : 512;
    }

    tsd_deselect(port);
    return true;

    error:
    tsd_deselect(port);
    return false;
}

bool tsd_sd_write(exi_port port, uint32_t addr, const uint8_t* data, uint32_t len) {
    return false;
}

#else

bool tsd_sd_init(exi_port port) {
    static bool initialized = false;
    if (!initialized) {
        sdgecko_initIODefault();
        initialized = true;
    }

    if (port.dev != EXI_DEVICE_0)
        return false;

    return sdgecko_initIO(port.chn) == CARDIO_ERROR_READY;
}

bool tsd_sd_read(exi_port port, uint32_t addr, uint8_t* data, uint32_t len) {
    if (port.dev != EXI_DEVICE_0)
        return false;

    // do not use multi block read here due to CMD12
    for (int i = 0; i < len; i++) {
        if (sdgecko_readSector(port.chn, data, addr) != CARDIO_ERROR_READY)
            return false;

        data += 512;
        addr += 1;
    }

    return true;
}

bool tsd_sd_write(exi_port port, uint32_t addr, const uint8_t* data, uint32_t len) {
    if (port.dev != EXI_DEVICE_0)
        return false;

    // do not use multi block write here due to stop transaction
    for (int i = 0; i < len; i++) {
        if (sdgecko_writeSector(port.chn, data, addr) != CARDIO_ERROR_READY)
            return false;

        data += 512;
        addr += 1;
    }

    return true;
}

#endif
