#include "time.h"

volatile unsigned long* dvd = (volatile unsigned long*)0xCC006000;

int dvd_cover_status() {
  return (dvd[1] & 1); // 0: cover closed, 1: cover opened
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
	while (dvd[7] & 1)
    ;
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


void dvd_reset()
{
	dvd[1] = 2;
	volatile unsigned long v = *(volatile unsigned long*)0xcc003024;
	*(volatile unsigned long*)0xcc003024 = (v & ~4) | 1;
	udelay(12);
	*(volatile unsigned long*)0xcc003024 = v | 5;
}
