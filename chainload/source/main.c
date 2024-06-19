#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <ogc/lwp_threads.h>
#include <gccore.h>

#include "os.h"

#include "ipc.h"
#include "gc_dvd.h"

__attribute__((aligned(32))) static u8 apploader_buf[0x20];
void* LoadGame_Apploader() {
    // variables
    int err;
    void *buffer = &apploader_buf[0];
    void (*app_init)(void (*report)(const char *fmt, ...));
    int (*app_main)(void **dst, int *size, int *offset);
    void *(*app_final)();
    void (*app_entry)(void(**init)(void (*report)(const char *fmt, ...)), int (**main)(void**,int*,int*), void *(**final)());

    // start disc drive & read apploader
    err = dvd_read(buffer,0x20,0x2440);
    if (err) {
        kprintf("Could not load apploader header\n");
        while(1);
    }
    err = dvd_read((void*)0x81200000,((*(unsigned long*)((u32)buffer+0x14)) + 31) &~31,0x2460);
    if (err) {
        kprintf("Could not load apploader data\n");
        while(1);
    }

    // run apploader
    app_entry = (void (*)(void(**)(void(*)(const char*,...)),int(**)(void**,int*,int*),void*(**)()))(*(unsigned long*)((u32)buffer + 0x10));
    app_entry(&app_init,&app_main,&app_final);
    app_init((void(*)(const char*,...))&kprintf);
    for (;;)
    {
        void *dst = 0;
        int len = 0,offset = 0;
        int res = app_main(&dst,&len,&offset);
		kprintf("res = %d\n", res);
        if (!res) break;
        err = dvd_read(dst,len,offset);
        if (err) {
            kprintf("Apploader read failed\n");
            while(1);
        }
        DCFlushRange(dst,len);
    }
	kprintf("GOT ENTRY\n");

    void* entrypoint = app_final();
    kprintf("THIS ENTRY, %p\n", entrypoint);
    return entrypoint;
}

int main(int argc, char **argv) {
	VIDEO_Init();
	VIDEO_SetBlack(TRUE);
	VIDEO_Flush();
	VIDEO_WaitVSync();

	GXRModeObj *rmode = NULL;

	CON_EnableGecko(EXI_CHANNEL_1, FALSE);

	// kprintf("\n");
	// kprintf("Hello World! argv[%d] = {\n", argc);
	// for (int i = 0; i < argc; i++)
	// 	kprintf("\t\"%s\"\n", argv[i]);
	// kprintf("}\n");

	char *iso_path = argv[1];
	kprintf("Loading %s\n", iso_path);

	int ret = dvd_custom_open(IPC_DEVICE_SD, iso_path, FILE_ENTRY_TYPE_FILE, 0);
	kprintf("OPEN ret: %08x\n", ret);
	(void)ret;

	// read boot info into lowmem
	struct dolphin_lowmem *lowmem = (struct dolphin_lowmem*)0x80000000;
	lowmem->a_boot_magic = 0x0D15EA5E;
	lowmem->a_version = 0x00000001;
	lowmem->b_physical_memory_size = 0x01800000;

	// set video mode PAL
	u32 mode = VIDEO_GetCurrentTvMode();
	if (mode == VI_PAL || mode == VI_MPAL) {
		if (mode == VI_MPAL) {
			lowmem->tv_mode = 5;
            rmode = &TVPal528IntDf;
		} else {
			lowmem->tv_mode = 1;
			rmode = &TVMpal480IntDf;
		}

		// from Nintendont
		syssram *sram;
		sram = __SYS_LockSram();
		sram->flags	|= 1; //PAL Video Mode
		__SYS_UnlockSram(1); // 1 -> write changes
		while(!__SYS_SyncSram());
	} else {
		rmode = &TVNtsc480IntDf;
	}

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(0);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	dvd_read(&lowmem->b_disk_info, 0x20, 0);

	void* entrypoint = LoadGame_Apploader();

	SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
	__lwp_thread_stopmultitasking(entrypoint);

	__builtin_unreachable();

	while(1) {}
	return 0;
}
