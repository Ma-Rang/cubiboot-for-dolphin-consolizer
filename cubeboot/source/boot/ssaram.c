/****************************************************************************
* SSARAM
***************************************************************************/
#ifndef HW_RVL
#include <string.h>
#include <malloc.h>
#include <gctypes.h>
#include <gcutil.h>
#include <ogc/aram.h>
#include <ogc/cache.h>
#include "ssaram.h"

static u8 aramfix[2048] ATTRIBUTE_ALIGN(32);

#define ARAMSTART  0x8000
#define ARAM_READ  1
#define ARAM_WRITE 0

// DSPCR bits
#define DSPCR_DSPRESET      0x0800        // Reset DSP
#define DSPCR_DSPDMA        0x0200        // ARAM dma in progress, if set
#define DSPCR_DSPINTMSK     0x0100        // * interrupt mask   (RW)
#define DSPCR_DSPINT        0x0080        // * interrupt active (RWC)
#define DSPCR_ARINTMSK      0x0040
#define DSPCR_ARINT         0x0020
#define DSPCR_AIINTMSK      0x0010
#define DSPCR_AIINT         0x0008
#define DSPCR_HALT          0x0004        // halt DSP
#define DSPCR_PIINT         0x0002        // assert DSP PI interrupt
#define DSPCR_RES           0x0001        // reset DSP

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

static vu16* const _dspReg = (u16*)0xCC005000;

static __inline__ void __ARClearInterrupt()
{
	u16 cause;

	cause = _dspReg[5]&~(DSPCR_DSPINT|DSPCR_AIINT);
	_dspReg[5] = (cause|DSPCR_ARINT);
}

static __inline__ void __ARWaitDma()
{
	while(_dspReg[5]&DSPCR_DSPDMA);
}

static void __ARReadDMA(u32 memaddr,u32 aramaddr,u32 len)
{
	// set main memory address
	_dspReg[16] = (_dspReg[16]&~0x03ff)|_SHIFTR(memaddr,16,16);
	_dspReg[17] = (_dspReg[17]&~0xffe0)|_SHIFTR(memaddr, 0,16);

	// set aram address
	_dspReg[18] = (_dspReg[18]&~0x03ff)|_SHIFTR(aramaddr,16,16);
	_dspReg[19] = (_dspReg[19]&~0xffe0)|_SHIFTR(aramaddr, 0,16);

	// set cntrl bits
	_dspReg[20] = (_dspReg[20]&~0x8000)|0x8000;
	_dspReg[20] = (_dspReg[20]&~0x03ff)|_SHIFTR(len,16,16);
	_dspReg[21] = (_dspReg[21]&~0xffe0)|_SHIFTR(len, 0,16);

	__ARWaitDma();
	__ARClearInterrupt();
}

static void __ARWriteDMA(u32 memaddr,u32 aramaddr,u32 len)
{
	// set main memory address
	_dspReg[16] = (_dspReg[16]&~0x03ff)|_SHIFTR(memaddr,16,16);
	_dspReg[17] = (_dspReg[17]&~0xffe0)|_SHIFTR(memaddr, 0,16);

	// set aram address
	_dspReg[18] = (_dspReg[18]&~0x03ff)|_SHIFTR(aramaddr,16,16);
	_dspReg[19] = (_dspReg[19]&~0xffe0)|_SHIFTR(aramaddr, 0,16);

	// set cntrl bits
	_dspReg[20] = (_dspReg[20]&~0x8000);
	_dspReg[20] = (_dspReg[20]&~0x03ff)|_SHIFTR(len,16,16);
	_dspReg[21] = (_dspReg[21]&~0xffe0)|_SHIFTR(len, 0,16);

	__ARWaitDma();
	__ARClearInterrupt();
}

/****************************************************************************
* ARAMClear
*
* To make life easy, just clear out the Auxilliary RAM completely.
****************************************************************************/
void ARAMClear(u32 start, u32 length)
{
  int i;
  unsigned char *buffer = memalign(32, 2048); /*** A little 2k buffer ***/

  memset(buffer, 0, 2048);
  DCFlushRange(buffer, 2048);

  for (i = start; i < 0x1000000 - length; i += 2048)
  {
    ARAMPut(buffer, (char *) i, 2048);
    while (AR_GetDMAStatus());
  }

  free(buffer);
}

/****************************************************************************
* ARAMPut
*
* This version of ARAMPut fixes up those segments which are misaligned.
* The IPL rules for DOLs is that they must contain 32byte aligned sections with
* 32byte aligned lengths.
*
* The reality is that most homebrew DOLs don't adhere to this.
****************************************************************************/
void ARAMPut(unsigned char *src, char *dst, int len)
{
  u32 misalignaddress;
  u32 misalignedbytes;
  u32 misalignedbytestodo;

  int i, block;
  int offset = 0;

  /*** Check destination alignment ***/
  if ((u32) dst & 0x1f)
  {
    /*** Retrieve previous 32 byte section ***/
    misalignaddress = ((u32) dst & ~0x1f);
    misalignedbytestodo = 32 - ((u32) dst & 0x1f);
    misalignedbytes = ((u32) dst & 0x1f);
    ARAMFetch(aramfix, (char *) misalignaddress, 32);

    /*** Update from source ***/
    memcpy(aramfix + misalignedbytes, src, misalignedbytestodo);

    /*** Put it back ***/
    DCFlushRange(aramfix, 32);
    __ARWriteDMA((u32) aramfix, (u32) dst & ~0x1f, 32);

    /*** Update pointers ***/
    src += misalignedbytestodo;
    len -= misalignedbytestodo;
    dst = (char *) (((u32) dst & ~0x1f) + 32);
  }

  /*** Move 2k blocks - saves aligning source buffer ***/
  block = (len >> 11);
  for (i = 0; i < block; i++)
  {
    memcpy(aramfix, src + offset, 2048);
    DCFlushRange(aramfix, 2048);
    __ARWriteDMA((u32) aramfix, (u32) dst + offset, 2048);
    while (AR_GetDMAStatus());
    offset += 2048;
  }

  /*** Clean up remainder ***/
  len &= 0x7ff;
  if (len)
  {
    block = len & 0x1f;		/*** Is length aligned ? ***/
    memcpy(aramfix, src + offset, len & ~0x1f);
    DCFlushRange(aramfix, len & ~0x1f);
    __ARWriteDMA((u32) aramfix, (u32) dst + offset, len & ~0x1f);

    if (block)
    {
      offset += len & ~0x1f;
      misalignedbytes = len & 0x1f;

      /*** Do same shuffle as destination alignment ***/
      ARAMFetch(aramfix, dst + offset, 32);
      memcpy(aramfix, src + offset, misalignedbytes);
      DCFlushRange(aramfix, 32);
      __ARWriteDMA((u32) aramfix, (u32) dst + offset, 32);
    }
  }
}

/****************************************************************************
* ARAMFetch
****************************************************************************/
void ARAMFetch(unsigned char *dst, char *src, int len)
{
    DCInvalidateRange(dst, len);
    __ARReadDMA((u32) dst, (u32) src, len);
}
#endif
