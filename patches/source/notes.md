# BS2 Notes (old)

BS2 uses low-mem as heap for some reason
> EDIT: the reason is obvious -- it's a bootloader

Heap is at 0x80700000
There is a secondary heap at 0x81100000
Program is loaded at 0x81300000
High-mem is only used up to around 0x81600000

BS2 only needs ~5MB of Heap, so `heap_start` can safely be moved to 0x80800000 or 0x80803100 (or exactly up to ~0x80877FF0)
BS2 runs OSInit which clears high-mem up to 0x81700000

Patches for BS2 can safely be placed at 0x81700000

In game, VIConfigure panics if 0x800000CC doesn't match the TV mode about to be configured

NTSC10 uses 0x80700000 as slab memory for decoding font data

Keeping the IPL loaded is interesting
- ~~You need to reset lowmem <1800h by resetting the interrupt routines at 0x100, 0xc00 etc back to `rfi` (OSInit handles this)~~
- ~~Next you need to fix the lowmem global variables at 0x3000-0x3100~~
- ~~The program has started running so Heap is setup at 0x80700000 ala OSInitAlloc so it can be cleared up to 0x81300000~~
- ~~Code is running in the Apploader so you need to work around 0x81200000 before clearing (jump to secondary routine)~~
- Some data blobs have been poked so we need to make sure the Yay0 data is still good in the asset offset table
- ~~Some values and pointers may need to be set back to default in .data~~
- ~~Clear from end of BSS up to top of RAM~~

Simple search the ~320kb for b'Yay0' headers whenever you find an entry that is `*(u8*)offset_word == 0xff`

---

Stack Pointers:
```
NTSC_10 = 0x81566550
NTSC_11 = 0x8159b288
NTSC_12_001 = 0x8159c970
NTSC_12_101 = 0x8159ce10
PAL_10 = 0x815edca8
PAL_11 = 0x81595f48
PAL_12 = 0x815ef570
```
