# cubiboot

This is a fork of [cubeboot](https://github.com/OffBroadway/cubeboot) by [TeamOffBroadway](https://github.com/OffBroadway) with support for SD2SP2, SD Gecko or similar SD adapters.

If you have questions regarding this fork you can join the [Discord server](https://discord.gg/YtA9aU3BKZ)!

## Installation - [PicoLoader](https://github.com/makeo/PicoLoader)
1. Download the [```cubiboot_picoloader.uf2```](https://github.com/makeo/cubiboot/releases/latest/download/cubiboot_picoloader.uf2) file
2. Hold down the button on the RP Pico whilst plugging it into your PC
3. Copy the .uf2 file to the USB drive
4. Download the [latest Swiss](https://github.com/emukidid/swiss-gc/releases/latest) dol
5. Rename the Swiss dol to ```swiss-gc.dol``` and place it on your SD card

## Installation - [PicoLoader](https://github.com/makeo/PicoLoader)/[PicoBoot](https://github.com/webhdx/PicoBoot) with gekkoboot payload
1. Download the [```cubiboot.dol```](https://github.com/makeo/cubiboot/releases/latest/download/cubiboot.dol)
2. Rename it to ```ipl.dol```
3. Copy the ```ipl.dol``` onto your SD card
4. Download the [latest Swiss](https://github.com/emukidid/swiss-gc/releases/latest) dol
5. Rename the Swiss dol to ```swiss-gc.dol``` and place it on your SD card

## Other ODEs (e.g. GC Loader)
You can use the [```cubiboot.iso```](https://github.com/makeo/cubiboot/releases/latest/download/cubiboot.iso) and use it as appropriate for your ODE.\
ODEs besides PicoLoader are not supported, and issues specific to these devices might not be fixed.

## Known Bugs
- cube_logo option does not work
- no PicoBoot uf2

## Special Thanks
- [TeamOffBroadway](https://github.com/OffBroadway) for creating cubeboot
- [Extrems](https://github.com/Extrems), [emukidid](https://github.com/emukidid) and everyone involved in creating Swiss

## Acknowledgements
- [cubeboot](https://github.com/OffBroadway/cubeboot) (GPL-2.0)
- [apploader](https://github.com/makeo/cubeboot-tools) (GPL-2.0)
- For more, see [CREDIT.md](https://github.com/makeo/cubiboot/blob/main/CREDIT.md)
