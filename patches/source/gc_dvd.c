/**
 * Wii64/Cube64 - gc_dvd.c
 * Copyright (C) 2007, 2008, 2009, 2010 emu_kidid
 * 
 * DVD Reading support for GC/Wii
 *
 * Wii64 homepage: http://www.emulatemii.com
 * email address: emukidid@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
**/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
// #include <ogc/dvd.h>
#include <malloc.h>
#include <string.h>
#include <gccore.h>
#include <unistd.h>
// #include <ogc/lwp_watchdog.h>
// #include <ogc/machine/processor.h>

#include "gc_dvd.h"
#include "reloc.h"

volatile unsigned long* dvd = (volatile unsigned long*)0xCC006000;

#define DVD_GCODE_FLASH_READ 0xB3000000

int dvd_flash_read(void* dst, unsigned int len, uint64_t offset)
{
  if(offset>>2 > 0xFFFFFFFF)
    return -1;
    
  if ((((int)dst) & 0xC0000000) == 0x80000000) // cached?
    dvd[0] = 0x2E;
  dvd[1] = 0;
  dvd[2] = DVD_GCODE_FLASH_READ;
  dvd[3] = offset >> 2;
  dvd[4] = len;
  dvd[5] = (unsigned long)dst & 0x1FFFFFFF;
  dvd[6] = len;
  dvd[7] = 3; // enable reading!
  DCInvalidateRange(dst, len);
  while (dvd[7] & 1);

  if (dvd[0] & 0x4)
    return 1;
  return 0;
}

#define DVD_FLIPPY_FILEAPI_BASE 0xB5000000

// IPC_FILE_READ        = 0x08,
// IPC_FILE_WRITE       = 0x09,
// TODO: switch this from embedded 4 byte path to a buffer handler
#define DVD_FLIPPY_OPEN (DVD_FLIPPY_FILEAPI_BASE | 0x0A)
// IPC_FILE_CLOSE       = 0x0B,
// IPC_FILE_STAT        = 0x0C,
// IPC_FILE_SEEK        = 0x0D,
// IPC_FILE_UNIMP1      = 0x0E,
// NOTE: must include expected length in request
#define DVD_FLIPPY_READDIR (DVD_FLIPPY_FILEAPI_BASE | 0x0F)

#define DVD_FLIPPY_READDIR (DVD_FLIPPY_FILEAPI_BASE | 0x0F)
#define DVD_FLIPPY_READDIR (DVD_FLIPPY_FILEAPI_BASE | 0x0F)
#define DVD_FLIPPY_READDIR (DVD_FLIPPY_FILEAPI_BASE | 0x0F)

#define DVD_FLIPPY_READDIR (DVD_FLIPPY_FILEAPI_BASE | 0x0F)


#define DVD_GCODE_WRITE_SETUP 0xB3000000
#define DVD_GCODE_WRITEBUFFEREX 0xBC000000

#define DVD_GCODE_BLKSIZE 0x200

#define DVD_DI_MODE  (1<<2)
#define DVD_DI_DMA   (1<<1)
#define DVD_DI_START (1<<0)

int dvd_custom_open(char *path)
{
  __attribute__((aligned(32))) static file_entry_t entry;
  strcpy(entry.name, path);

  OSReport("Opening: %s\n", entry.name);

  DCFlushRange(&entry, sizeof(file_entry_t));

  dvd[0] = 0x2E;
  dvd[1] = 0;
  dvd[2] = DVD_FLIPPY_OPEN;
  dvd[3] = 0;
  dvd[4] = sizeof(file_entry_t);
  dvd[5] = (u32)&entry;
  dvd[6] = sizeof(file_entry_t);
  dvd[7] = (DVD_DI_MODE|DVD_DI_DMA|DVD_DI_START);
  while ((dvd[7] & 1) > 0) ;

  if (dvd[0] & 0x4)
    return 1;
  return 0;
}

int dvd_custom_readdir(file_entry_t* target)
{
  __attribute__((aligned(32))) static file_entry_t dst;

  dvd[0] = 0x2E;
  dvd[1] = 0;
  dvd[2] = DVD_FLIPPY_READDIR;
  dvd[3] = 0;
  dvd[4] = 0;
  dvd[5] = (u32)&dst;
  dvd[6] = sizeof(file_entry_t);
  dvd[7] = 3; // enable reading!
  DCInvalidateRange((u8*)&dst, sizeof(file_entry_t));
  while (dvd[7] & 1);

  if (dvd[0] & 0x4) 
    return 1;

  memcpy(target, (u8*)&dst, sizeof(file_entry_t));
  return 0;
}

int dvd_read_id()
{
  dvd[0] = 0x2E;
  dvd[1] = 0;
  dvd[2] = 0xA8000040;
  dvd[3] = 0;
  dvd[4] = 0x20;
  dvd[5] = 0;
  dvd[6] = 0x20;
  dvd[7] = 3; // enable reading!
  while (dvd[7] & 1);
  if (dvd[0] & 0x4)
    return 1;
  return 0;
}

unsigned int dvd_get_error(void)
{
  dvd[2] = 0xE0000000;
  dvd[8] = 0;
  dvd[7] = 1; // IMM
  while (dvd[7] & 1);
  return dvd[8];
}


void dvd_motor_off()
{
  dvd[0] = 0x2E;
  dvd[1] = 0;
  dvd[2] = 0xe3000000;
  dvd[3] = 0;
  dvd[4] = 0;
  dvd[5] = 0;
  dvd[6] = 0;
  dvd[7] = 1; // IMM
  while (dvd[7] & 1);
}

/* 
DVD_LowRead64(void* dst, unsigned int len, uint64_t offset)
  Read Raw, needs to be on sector boundaries
  Has 8,796,093,020,160 byte limit (8 TeraBytes)
  Synchronous function.
    return -1 if offset is out of range
*/
int DVD_LowRead64(void* dst, unsigned int len, uint64_t offset)
{
  if(offset>>2 > 0xFFFFFFFF)
    return -1;
    
  if ((((int)dst) & 0xC0000000) == 0x80000000) // cached?
    dvd[0] = 0x2E;
  dvd[1] = 0;
  dvd[2] = DVD_READ_NORMAL;
  dvd[3] = offset >> 2;
  dvd[4] = len;
  dvd[5] = (unsigned long)dst & 0x1FFFFFFF;
  dvd[6] = len;
  dvd[7] = 3; // enable reading!
  DCInvalidateRange(dst, len);
  while (dvd[7] & 1);

  if (dvd[0] & 0x4)
    return 1;
  return 0;
}
