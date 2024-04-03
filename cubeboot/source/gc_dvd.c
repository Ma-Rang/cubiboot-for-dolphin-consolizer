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
#include <ogc/dvd.h>
#include <malloc.h>
#include <string.h>
#include <gccore.h>
#include <unistd.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/machine/processor.h>
#include "gc_dvd.h"

#define DVD_DI_MODE (1 << 2)
#define DVD_DI_DMA (1 << 1)
#define DVD_DI_START (1 << 0)

// TODO: header file
extern int iprintf(const char *fmt, ...);

volatile unsigned long* dvd = (volatile unsigned long*)0xCC006000;

#define DVD_FLIPPY_STATUS_BLOB 0xB4000000

int dvd_get_status(firmware_status_blob_t* dst)
{
  u32 status_size = sizeof(firmware_status_blob_t);

	dvd[0] = 0x2E;
	dvd[1] = 0;
	dvd[2] = DVD_FLIPPY_STATUS_BLOB;
	dvd[3] = 0;
	dvd[4] = 0;
	dvd[5] = (u32)dst & 0x1FFFFFFF;
  dvd[6] = status_size;
  dvd[7] = (DVD_DI_DMA|DVD_DI_START); // enable reading!
  DCInvalidateRange(dst, status_size);
	while (dvd[7] & 1);

  dst->status_text[63] = 0;
  dst->status_sub[63] = 0;
	if (dvd[0] & 0x4)
		return 1;
	return 0;
}

int dvd_cover_status() {
  return (dvd[1] & 1); // 0: cover closed, 1: cover opened
}

int dvd_transaction_wait() {
  return 0;
}

#define DVD_FLIPPY_FILEAPI_BASE 0xB5000000

// FlippyDrive Util
#define DVD_FLIPPY_STATUS  (DVD_FLIPPY_FILEAPI_BASE | 0x00)

// FlippyDrive SD Card
#define DVD_FLIPPY_OPEN    (DVD_FLIPPY_FILEAPI_BASE | 0x0A)
#define DVD_FLIPPY_CLOSE   (DVD_FLIPPY_FILEAPI_BASE | 0x0B)
#define DVD_FLIPPY_STAT    (DVD_FLIPPY_FILEAPI_BASE | 0x0C)
#define DVD_FLIPPY_SEEK    (DVD_FLIPPY_FILEAPI_BASE | 0x0D)
#define DVD_FLIPPY_READDIR (DVD_FLIPPY_FILEAPI_BASE | 0x0F)

// FlippyDrive Embedded Flash
#define DVD_FLASH_OPEN     (DVD_FLIPPY_FILEAPI_BASE | 0x4A)
#define DVD_FLASH_CLOSE    (DVD_FLIPPY_FILEAPI_BASE | 0x4B)
#define DVD_FLASH_STAT     (DVD_FLIPPY_FILEAPI_BASE | 0x4C)
#define DVD_FLASH_SEEK     (DVD_FLIPPY_FILEAPI_BASE | 0x4D)
#define DVD_FLASH_READDIR  (DVD_FLIPPY_FILEAPI_BASE | 0x4F)

static file_status_t status __attribute__((aligned(32)));
file_status_t *dvd_custom_status()
{
	dvd[0] = 0x2E;
	dvd[1] = 0;
	dvd[2] = DVD_FLIPPY_STATUS;
	dvd[3] = 0;
	dvd[4] = sizeof(file_status_t);
	dvd[5] = (u32)&status & 0x1FFFFFFF;
	dvd[6] = sizeof(file_status_t);
	dvd[7] = 3; // enable reading!
  DCInvalidateRange((u8*)&status, sizeof(file_status_t));
	while (dvd[7] & 1);
	return &status;
}

int dvd_custom_open(char *path, uint8_t type)
{
  static file_entry_t entry __attribute__((aligned(32)));
  strncpy(entry.name, path, 256);
  entry.name[255] = 0;
  entry.type = type;

  iprintf("Opening: %s\n", entry.name);

  DCFlushRange(&entry, sizeof(file_entry_t));

  dvd[0] = 0x2E;
  dvd[1] = 0;
  dvd[2] = DVD_FLIPPY_OPEN;
  dvd[3] = 0;
  dvd[4] = sizeof(file_entry_t);
  dvd[5] = (u32)&entry & 0x1FFFFFFF;
  dvd[6] = sizeof(file_entry_t);
  dvd[7] = (DVD_DI_MODE|DVD_DI_DMA|DVD_DI_START);
  while (dvd[7] & 1) {
    if (ticks_to_millisecs(gettime()) % 1000 == 0)
      iprintf("Still reading: %u/%u\n", (u32)dvd[6], sizeof(file_entry_t));
  }

	if (dvd[0] & 0x4)
		return 1;
	return 0;
}

int dvd_custom_readdir(file_entry_t* target)
{
  static file_entry_t dst __attribute__((aligned(32)));

  dvd[0] = 0x2E;
	dvd[1] = 0;
	dvd[2] = DVD_FLIPPY_READDIR;
	dvd[3] = 0;
	dvd[4] = 0;
	dvd[5] = (u32)&dst & 0x1FFFFFFF;
	dvd[6] = sizeof(file_entry_t);
	dvd[7] = 3; // enable reading!
	DCInvalidateRange((u8*)&dst, sizeof(file_entry_t));
	while (dvd[7] & 1);

	if (dvd[0] & 0x4) 
		return 1;

  memcpy(target, &dst, sizeof(file_entry_t));
	return 0;
}

/*
int dvd_custom_status_async(file_status_t *target)
{
  dvd[0] = 0x2E;
  dvd[1] = 0;
  dvd[2] = DVD_FLIPPY_STATUS;
  dvd[3] = 0;
  dvd[4] = 0;
  dvd[5] = (u32)target & 0x1FFFFFFF;
  dvd[6] = sizeof(file_status_t);
  dvd[7] = 3; // enable reading!
  DCInvalidateRange(target, sizeof(file_status_t));
  return 0;
}

bool dvd_custom_status_poll()
{
  if(dvd[7] & 1) return true;

  //if(!(dvd[0] & 0x4)) return true;

  return false;
}*/

int dvd_flash_open(char *path, uint8_t type)
{
  static file_entry_t entry __attribute__((aligned(32)));
  strcpy(entry.name, path);
  entry.type = type;

  iprintf("Opening: %s\n", entry.name);

  DCFlushRange(&entry, sizeof(file_entry_t));

  dvd[0] = 0x2E;
  dvd[1] = 0;
  dvd[2] = DVD_FLASH_OPEN;
  dvd[3] = 0;
  dvd[4] = sizeof(file_entry_t);
  dvd[5] = (u32)&entry & 0x1FFFFFFF;
  dvd[6] = sizeof(file_entry_t);
  dvd[7] = (DVD_DI_MODE|DVD_DI_DMA|DVD_DI_START);
  while (dvd[7] & 1) {
    if (ticks_to_millisecs(gettime()) % 1000 == 0)
      iprintf("Still reading: %u/%u\n", (u32)dvd[6], sizeof(file_entry_t));
  }

	if (dvd[0] & 0x4)
		return 1;
	return 0;
}

int dvd_flash_readdir(file_entry_t* target)
{
  static file_entry_t dst __attribute__((aligned(32)));

  dvd[0] = 0x2E;
	dvd[1] = 0;
	dvd[2] = DVD_FLASH_READDIR;
	dvd[3] = 0;
	dvd[4] = 0;
  dvd[5] = (u32)&dst & 0x1FFFFFFF;
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

void dvd_bootloader_boot()
{
  dvd[0] = 0x2E;
  dvd[1] = 0;
  dvd[2] = 0x12000000;
  dvd[3] = 0xabadbeef;
  dvd[4] = 0xcafe6969;
  dvd[5] = 0;
  dvd[6] = 0;
  dvd[7] = 1; // IMM
  while (dvd[7] & 1)
    ;
}

static dvd_info info __attribute__((aligned(32))) = {0};
dvd_info *dvd_inquiry()
{
  dvd[0] = 0x2E;
  dvd[1] = 0;
  dvd[2] = 0x12000000;
  dvd[3] = 0;
  dvd[4] = 0;
  dvd[5] = ((u32)&info) & 0x1FFFFFFF;
  dvd[6] = sizeof(dvd_info);
  dvd[7] = 3; // enable reading!
  DCInvalidateRange((u8*)&info, sizeof(dvd_info));
	while (dvd[7] & 1);

  return &info;
}

void dvd_break() {
  dvd[0] = 1;
  // while((dvd[0] & 1) == 1);
}

/* 
dvd_read(void* dst, unsigned int len, uint64_t offset)
  Read Raw, needs to be on sector boundaries
  Has 8,796,093,020,160 byte limit (8 TeraBytes)
  Synchronous function.
    return -1 if offset is out of range
*/
int dvd_read(void* dst, unsigned int len, uint64_t offset)
{
  if(offset>>2 > 0xFFFFFFFF)
    return -1;
    
  if ((((int)dst) & 0xC0000000) == 0x80000000) // cached?
  {
		dvd[0] = 0x2E;
  }
	dvd[1] = 0;
	dvd[2] = DVD_READ_NORMAL;
	dvd[3] = offset >> 2;
	dvd[4] = len;
	dvd[5] = (u32)dst & 0x1FFFFFFF;
	dvd[6] = len;
	dvd[7] = 3; // enable reading!
	DCInvalidateRange(dst, len);
	while (dvd[7] & 1);

	if (dvd[0] & 0x4)
		return 1;
	return 0;
}

int dvd_flash_read(void* dst, unsigned int len, uint64_t offset)
{
  if(offset>>2 > 0xFFFFFFFF)
    return -1;

  if ((((int)dst) & 0xC0000000) == 0x80000000) // cached?
  {
		dvd[0] = 0x2E;
  }
	dvd[1] = 0;
	dvd[2] = DVD_READ_NORMAL | 0x01;
	dvd[3] = offset >> 2;
	dvd[4] = len;
	dvd[5] = (u32)dst & 0x1FFFFFFF;
	dvd[6] = len;
	dvd[7] = 3; // enable reading!
	DCInvalidateRange(dst, len);
	while (dvd[7] & 1);

	if (dvd[0] & 0x4)
		return 1;
	return 0;
}

static char error_str[256];
char *dvd_error_str() {
  unsigned int err = dvd_get_error();
  if(!err) return "OK";
  
  memset(&error_str[0],0,256);
  switch(err>>24) {
    case 0:
      break;
    case 1:
      strcpy(&error_str[0],"Lid open");
      break;
    case 2:
      strcpy(&error_str[0],"No disk/Disk changed");
      break;
    case 3:
      strcpy(&error_str[0],"No disk");
      break;  
    case 4:
      strcpy(&error_str[0],"Motor off");
      break;  
    case 5:
      strcpy(&error_str[0],"Disk not initialized");
      break;
  }
  switch(err&0xFFFFFF) {
    case 0:
      break;
    case 0x020400:
      strcat(&error_str[0]," Motor Stopped");
      break;
    case 0x020401:
      strcat(&error_str[0]," Disk ID not read");
      break;
    case 0x023A00:
      strcat(&error_str[0]," Medium not present / Cover opened");
      break;  
    case 0x030200:
      strcat(&error_str[0]," No Seek complete");
      break;  
    case 0x031100:
      strcat(&error_str[0]," Unrecovered read error");
      break;
    case 0x040800:
      strcat(&error_str[0]," Transfer protocol error");
      break;
    case 0x052000:
      strcat(&error_str[0]," Invalid command operation code");
      break;
    case 0x052001:
      strcat(&error_str[0]," Audio Buffer not set");
      break;  
    case 0x052100:
      strcat(&error_str[0]," Logical block address out of range");
      break;  
    case 0x052400:
      strcat(&error_str[0]," Invalid Field in command packet");
      break;
    case 0x052401:
      strcat(&error_str[0]," Invalid audio command");
      break;
    case 0x052402:
      strcat(&error_str[0]," Configuration out of permitted period");
      break;
    case 0x053000:
      strcat(&error_str[0]," DVD-R"); //?
      break;
    case 0x053100:
      strcat(&error_str[0]," Wrong Read Type"); //?
      break;
    case 0x056300:
      strcat(&error_str[0]," End of user area encountered on this track");
      break;  
    case 0x062800:
      strcat(&error_str[0]," Medium may have changed");
      break;  
    case 0x0B5A01:
      strcat(&error_str[0]," Operator medium removal request");
      break;
  }
  if(!error_str[0])
    sprintf(&error_str[0],"Unknown error %08X",err);
  return &error_str[0];
  
}
