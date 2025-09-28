#include <stdbool.h>
#include <ogc/system.h>

#include "flippy_sync.h"

extern syssram* __SYS_LockSram(void);
extern syssramex* __SYS_LockSramEx(void);
extern u32 __SYS_UnlockSram(u32 write);
extern u32 __SYS_UnlockSramEx(u32 write);
extern u32 __SYS_SyncSram(void);

#define DEVICE_ID_J			0x13 // FlippyDrive
#define DEVICE_ID_K			0x14 // FlippyDrive Flash

void set_sram_swiss(bool syncSram) {
    bool writeSram = false;
	syssramex *sramex = __SYS_LockSramEx();

	if (sramex->__padding0 != 0) {
		sramex->__padding0 = DEVICE_ID_J;
		writeSram = true;
	}

	__SYS_UnlockSramEx(writeSram);
	while (syncSram && !__SYS_SyncSram());
}

#define SWISS_GLOBAL_CONFIG_PATH "/swiss/settings/global.ini"

void create_swiss_config() {
    // mkdir all
    dvd_custom_mkdir("/swiss");
    dvd_custom_mkdir("/swiss/settings");

    // check swiss global config
    dvd_custom_open(SWISS_GLOBAL_CONFIG_PATH, FILE_ENTRY_TYPE_FILE, IPC_FILE_FLAG_DISABLESPEEDEMU);

	GCN_ALIGNED(file_status_t) status;
	int ret = dvd_custom_status(&status);
    if (ret != 0) return;

    // file exists
    if(status.result == 0 && status.fd != 0) {
        dvd_custom_close(status.fd);
        return;
    }

    // create swiss global config
    ret = dvd_custom_open(SWISS_GLOBAL_CONFIG_PATH, FILE_ENTRY_TYPE_FILE, IPC_FILE_FLAG_DISABLESPEEDEMU | IPC_FILE_FLAG_WRITE);
    if (ret != 0) return;

    // TODO: what do we put in this file?
    // I have checked and it can safely generate defaults from an empty file

    // file created!
    if(status.result == 0 && status.fd != 0) {
        dvd_custom_close(status.fd);
        return;
    }
    
    return;
}