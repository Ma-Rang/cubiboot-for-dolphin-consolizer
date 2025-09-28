#pragma once

#include <stdint.h>

#define ISO_SECTOR_SIZE 2048

// Use #pragma pack to ensure the structs are packed as defined in the spec.
#pragma pack(push, 1)

// Boot Record Volume Descriptor (2048 bytes)
typedef struct {
    uint8_t  type;                     // 0 for Boot Record Volume Descriptor
    char     standard_id[5];           // should be "CD001"
    uint8_t  version;                  // typically 1
    char     boot_system_identifier[32]; // e.g., "EL TORITO SPECIFICATION"
    char     boot_identifier[32];      // usually unused or informational
    uint32_t boot_catalog_ptr;         // Logical Block Address (LBA) of Boot Catalog (at offset 71)
    uint8_t  reserved[1973];           // padding to complete 2048 bytes
} BootRecordVolumeDescriptor;

// Boot Catalog Validation Entry (first 32 bytes of the Boot Catalog)
typedef struct {
    uint8_t  header_id;    // must be 0x01
    uint8_t  platform_id;  // e.g. 0 for x86
    uint16_t reserved;     // reserved, typically 0
    char     id_string[24];// identification string
    uint16_t checksum;     // checksum over this entry
    uint8_t  key55;        // must be 0x55
    uint8_t  keyAA;        // must be 0xAA
} BootCatalogValidationEntry;

// Boot Catalog Default Entry (next 32 bytes)
typedef struct {
    uint8_t  boot_indicator; // 0x88 indicates bootable; 0x00 non-bootable
    uint8_t  boot_media_type;// 0 = no emulation, others for specific media types
    uint16_t load_segment;   // typically 0x07C0 (address where boot image is loaded)
    uint8_t  system_type;    // system type (often 0 for x86)
    uint8_t  unused;         // should be 0
    uint16_t sector_count;   // number of virtual sectors to load
    uint32_t load_rba;       // starting logical block address of the boot image
    uint8_t  reserved[20];   // reserved for future use
} BootCatalogEntry;

#pragma pack(pop)
