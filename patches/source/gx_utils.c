#include <stdint.h>
#include <ogc/gx.h>
#include <ogc/gx_struct.h>
#include "gx_regdef.h"
#include "reloc.h"

WGPipe* const wgPipe = (WGPipe*)0xCC008000;
struct __gx_regdef* const __gx = (struct __gx_regdef*)0x815829b0;

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

#define GX_LOAD_BP_REG(x)				\
	do {								\
		wgPipe->U8 = 0x61;				\
		asm volatile ("" ::: "memory" ); \
		wgPipe->U32 = (u32)(x);		\
		asm volatile ("" ::: "memory" ); \
	} while(0)

#define GX_LOAD_CP_REG(x, y)			\
	do {								\
		wgPipe->U8 = 0x08;				\
		asm volatile ("" ::: "memory" ); \
		wgPipe->U8 = (u8)(x);			\
		asm volatile ("" ::: "memory" ); \
		wgPipe->U32 = (u32)(y);		\
		asm volatile ("" ::: "memory" ); \
	} while(0)

#define GX_LOAD_XF_REG(x, y)			\
	do {								\
		wgPipe->U8 = 0x10;				\
		asm volatile ("" ::: "memory" ); \
		wgPipe->U32 = (u32)((x)&0xffff);		\
		asm volatile ("" ::: "memory" ); \
		wgPipe->U32 = (u32)(y);		\
		asm volatile ("" ::: "memory" ); \
	} while(0)

#define GX_LOAD_XF_REGS(x, n)			\
	do {								\
		wgPipe->U8 = 0x10;				\
		asm volatile ("" ::: "memory" ); \
		wgPipe->U32 = (u32)(((((n)&0xffff)-1)<<16)|((x)&0xffff));				\
		asm volatile ("" ::: "memory" ); \
	} while(0)

static inline void WriteMtxPS4x3(register const Mtx mt,register void *wgpipe)
{
	register f32 tmp0,tmp1,tmp2,tmp3,tmp4,tmp5;
	__asm__ __volatile__ (
		 "psq_l %0,0(%6),0,0\n\
		  psq_l %1,8(%6),0,0\n\
		  psq_l %2,16(%6),0,0\n\
		  psq_l %3,24(%6),0,0\n\
		  psq_l %4,32(%6),0,0\n\
		  psq_l %5,40(%6),0,0\n\
		  psq_st %0,0(%7),0,0\n\
		  psq_st %1,0(%7),0,0\n\
		  psq_st %2,0(%7),0,0\n\
		  psq_st %3,0(%7),0,0\n\
		  psq_st %4,0(%7),0,0\n\
		  psq_st %5,0(%7),0,0"
		  : "=&f"(tmp0),"=&f"(tmp1),"=&f"(tmp2),"=&f"(tmp3),"=&f"(tmp4),"=&f"(tmp5)
		  : "b"(mt), "b"(wgpipe)
		  : "memory"
	);
}

static inline void WriteMtxPS3x3from4x3(register const Mtx mt,register void *wgpipe)
{
	register f32 tmp0,tmp1,tmp2,tmp3,tmp4,tmp5;
	__asm__ __volatile__
		("psq_l %0,0(%6),0,0\n\
		  lfs	%1,8(%6)\n\
		  psq_l %2,16(%6),0,0\n\
		  lfs	%3,24(%6)\n\
		  psq_l %4,32(%6),0,0\n\
		  lfs	%5,40(%6)\n\
		  psq_st %0,0(%7),0,0\n\
		  stfs	 %1,0(%7)\n\
		  psq_st %2,0(%7),0,0\n\
		  stfs	 %3,0(%7)\n\
		  psq_st %4,0(%7),0,0\n\
		  stfs	 %5,0(%7)"
		  : "=&f"(tmp0),"=&f"(tmp1),"=&f"(tmp2),"=&f"(tmp3),"=&f"(tmp4),"=&f"(tmp5)
		  : "b"(mt), "b"(wgpipe)
		  : "memory"
	);
}

void GX_LoadPosMtxImm(const Mtx mt,u32 pnidx)
{
	GX_LOAD_XF_REGS((0x0000|(_SHIFTL(pnidx,2,8))),12);
	WriteMtxPS4x3(mt,(void*)wgPipe);
}

void GX_LoadNrmMtxImm(const Mtx mt,u32 pnidx)
{
	GX_LOAD_XF_REGS((0x0400|(pnidx*3)),9);
	WriteMtxPS3x3from4x3(mt,(void*)wgPipe);
}

static void __GX_SetMatrixIndex(u32 mtx)
{
    // OSReport("Got __gx = %08x\n", (u32)__gx);
	if(mtx<5) {
		GX_LOAD_CP_REG(0x30,__gx->mtxIdxLo);
		GX_LOAD_XF_REG(0x1018,__gx->mtxIdxLo);
	} else {
		GX_LOAD_CP_REG(0x40,__gx->mtxIdxHi);
		GX_LOAD_XF_REG(0x1019,__gx->mtxIdxHi);
	}
}

void GX_SetCurrentMtx(u32 mtx)
{
	__gx->mtxIdxLo = (__gx->mtxIdxLo&~0x3f)|(mtx&0x3f);
	__GX_SetMatrixIndex(0);
}
