/**
 * Wii64/Cube64 - gc_dvd.h
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
#ifndef GC_DVD_H
#define GC_DVD_H

#include <stdint.h>

#define DVD_READ_NORMAL 0xA8000000
#define MAX_FILE_NAME 256

enum file_entry_type_enum {
    FILE_ENTRY_TYPE_FILE = 0,
    FILE_ENTRY_TYPE_DIR = 1,
    FILE_ENTRY_TYPE_MAX = 0xFF
};

typedef struct
{
    uint32_t result;
	uint64_t fsize;
    uint8_t pad[20];
} file_status_t;

enum {
    IPC_FILE_FLAG_DISABLECACHE = 0x01,
};
typedef struct {
    char name[MAX_FILE_NAME];
    uint8_t type;
    uint8_t flags;
    uint64_t size;
    uint32_t date;
    uint32_t time;
    uint8_t pad[14];
} file_entry_t;

int init_dvd();
// int dvd_write_test(char* buf, int len);

int dvd_flash_read(void* dst, unsigned int len, uint64_t offset);

int dvd_custom_open(char *path);
int dvd_custom_readdir(file_entry_t* target);

void dvd_motor_off();
unsigned int dvd_get_error(void);
int dvd_read_directoryentries(uint64_t offset, int size);
void read_directory(int sector, int len);
int read_safe(void* dst, uint64_t offset, int len);
int read_direntry(unsigned char* direntry);
int read_sector(void* buffer, uint32_t sector);
int dvd_read(void* dst,unsigned int len, unsigned int offset);
int dvd_read_id();
int DVD_LowRead64(void* dst, unsigned int len, uint64_t offset);

struct pvd_s
{
	char id[8];
	char system_id[32];
	char volume_id[32];
	char zero[8];
	unsigned long total_sector_le, total_sect_be;
	char zero2[32];
	unsigned long volume_set_size, volume_seq_nr;
	unsigned short sector_size_le, sector_size_be;
	unsigned long path_table_len_le, path_table_len_be;
	unsigned long path_table_le, path_table_2nd_le;
	unsigned long path_table_be, path_table_2nd_be;
	unsigned char root_direntry[34];
	char volume_set_id[128], publisher_id[128], data_preparer_id[128], application_id[128];
	char copyright_file_id[37], abstract_file_id[37], bibliographical_file_id[37];
	// some additional dates, but we don't care for them :)
}  __attribute__((packed));

#endif

