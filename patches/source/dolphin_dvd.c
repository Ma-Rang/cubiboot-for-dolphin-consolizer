#include "dolphin_dvd.h"

#define assert(...)
// #include <assert.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <gcutil.h>
#include <ogc/system.h>

#include "reloc.h"
#include "flippy_sync.h"

#define DVD_FS_DUMP 1

#define GET_OFFSET(o) ((u32)((o[0] << 16) | (o[1] << 8) | o[2]))

#define ENTRY_IS_DIR(i) (entry_table[i].filetype == T_DIR)
#define FILE_POSITION(i) (entry_table[i].addr)
#define FILE_LENGTH(i) (entry_table[i].len)

// ????
