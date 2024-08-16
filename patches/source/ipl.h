typedef enum {
	IPL_UNKNOWN,
	IPL_NTSC_10_001,
	IPL_NTSC_10_002,
	IPL_DEV_10,
	IPL_NTSC_11_001,
	IPL_PAL_10_001,
	IPL_PAL_10_002,
	IPL_MPAL_11,
	IPL_TDEV_11,
	IPL_NTSC_12_001,
	IPL_NTSC_12_101,
	IPL_PAL_12_101
} ipl_revision_t;

ipl_revision_t get_ipl_revision();
