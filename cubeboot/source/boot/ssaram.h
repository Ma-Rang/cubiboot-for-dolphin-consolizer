/****************************************************************************
* SSARAM
***************************************************************************/
#ifndef HW_RVL
#ifndef __SSARAM__
#define __SSARAM__

void ARAMClear(u32 start, u32 length);
void ARAMPut(unsigned char *src, char *dst, int len);
void ARAMFetch(unsigned char *dst, char *src, int len);

#endif
#endif

