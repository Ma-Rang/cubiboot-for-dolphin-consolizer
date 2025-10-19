#ifndef TSD_H
#define TSD_H

#include <stdint.h>
#include <stdbool.h>
#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	u8 chn;
	u8 dev;
} exi_port;

bool tsd_sd_init(exi_port port);
bool tsd_sd_read(exi_port port, uint32_t addr, uint8_t* data, uint32_t len);
bool tsd_sd_write(exi_port port, uint32_t addr, const uint8_t* data, uint32_t len);

#ifdef IPL_CODE
void tsd_set_native(bool native);
#endif

#ifdef __cplusplus
}
#endif

#endif // TSD_H
