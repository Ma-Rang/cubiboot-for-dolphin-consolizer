void run(register void* entry_point) {
    // ICFlashInvalidate
    asm("mfhid0	4");
    asm("ori 4, 4, 0x0800");
    asm("mthid0	4");
    // hwsync
    asm("sync");
    asm("isync");
    // boot
    asm("mtlr %0" : : "r" (entry_point));
    asm("blr");
}
