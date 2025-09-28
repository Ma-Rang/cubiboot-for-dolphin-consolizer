
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "reloc.h"
#include "picolibc.h"

#include "iso9660_structs.h"
#include "flippy_sync.h"

static uint8_t iso_sector_buf[ISO_SECTOR_SIZE];

uint32_t get_dol_iso9660(uint8_t fd) {
__attribute__((aligned(32))) static char iso9660Buf[32];
    int ret = dvd_read(&iso9660Buf[0], sizeof(iso9660Buf), 0x8000, fd);
    if (ret != 0) {
        custom_OSReport("Could not read DVD at 0x8000\n");
        return 0;
    }

    if (strncmp(&iso9660Buf[1], "CD001", 5) != 0) {
        custom_OSReport("ISO9660 magic not found at 0x8000\n");
        return 0;
    } else {
        custom_OSReport("ISO9660 magic found at 0x8000\n"); // good!
    }

    BootRecordVolumeDescriptor *brvd = (void*)&iso_sector_buf[0];
    int found = 0;
    // Volume Descriptors start at sector 16 (offset = 16 * ISO_SECTOR_SIZE)
    for (int i = 16; i < 256; i++) {
        int ret = dvd_read_data(&iso_sector_buf[0], sizeof(iso_sector_buf), i * ISO_SECTOR_SIZE, fd);
        if (ret != 0) {
            custom_OSReport("Unable to read file at offgset %d\n", i * ISO_SECTOR_SIZE);
            return 0;
        }

        // Look for the Boot Record Volume Descriptor (type 0 and id "CD001")
        if (brvd->type == 0 && strncmp(brvd->standard_id, "CD001", 5) == 0) {
            found = 1;
            custom_OSReport("Found Boot Record Volume Descriptor at sector %d\n", i);
            break;
        }

        // If we reach the Volume Descriptor Set Terminator (type 255), stop searching.
        if (brvd->type == 255)
            break;
    }

    if (!found) {
        custom_OSReport("Boot Record Volume Descriptor not found.\n");
        return 0;
    }

    // validate boot system is "EL TORITO SPECIFICATION"
    if (strncmp(brvd->boot_system_identifier, "EL TORITO SPECIFICATION", 24) != 0) {
        custom_OSReport("Boot System Identifier is not 'EL TORITO SPECIFICATION'.\n");
        return 0;
    }

    // Print information from the Boot Record
    custom_OSReport("Boot System Identifier: %.32s\n", brvd->boot_system_identifier);
    custom_OSReport("Boot Identifier:        %.32s\n", brvd->boot_identifier);
    custom_OSReport("Boot Catalog Pointer (LBA): %u\n", __builtin_bswap32(brvd->boot_catalog_ptr));

    // Read the Boot Catalog sector from the boot catalog.
    uint8_t *catalog_sector = &iso_sector_buf[0]; 
    uint32_t catalog_lba = __builtin_bswap32(brvd->boot_catalog_ptr);
    uint32_t catalog_offset = catalog_lba * ISO_SECTOR_SIZE;
    ret = dvd_read_data(catalog_sector, ISO_SECTOR_SIZE, catalog_offset, fd);
    if (ret != 0) {
        custom_OSReport("Could not read ISO Catalog at offset %u\n", catalog_offset);
        return 0;
    }
    
    // The first 32 bytes: Validation Entry.
    BootCatalogValidationEntry *val = (BootCatalogValidationEntry *)catalog_sector;
    // The next 32 bytes: Default Boot Entry.
    BootCatalogEntry *entry = (BootCatalogEntry *)(catalog_sector + 32);

    // Display the Boot Catalog Validation Entry info.
    custom_OSReport("\nBoot Catalog Validation Entry:\n");
    custom_OSReport("  Header ID:   0x%02X\n", val->header_id);
    custom_OSReport("  Platform ID: 0x%02X\n", val->platform_id);
    custom_OSReport("  ID String:   %.24s\n", val->id_string);
    custom_OSReport("  Checksum:    0x%04X\n", val->checksum);
    custom_OSReport("  Key:         0x%02X 0x%02X\n", val->key55, val->keyAA);

    // Display the Boot Catalog Default Entry info.
    custom_OSReport("\nBoot Catalog Default Entry:\n");
    custom_OSReport("  Boot Indicator:  0x%02X\n", entry->boot_indicator);
    custom_OSReport("  Boot Media Type: 0x%02X\n", entry->boot_media_type);
    custom_OSReport("  Load Segment:    0x%04X\n", entry->load_segment);
    custom_OSReport("  System Type:     0x%02X\n", entry->system_type);
    custom_OSReport("  Sector Count:    %u\n", entry->sector_count);
    custom_OSReport("  Load RBA:        %u\n", __builtin_bswap32(entry->load_rba));

    // ----------------------------------------------------------------
    // Continue processing: Load and process the boot file.
    // ----------------------------------------------------------------

    // Check if the boot entry is marked bootable.
    if (entry->boot_indicator != 0x88) {
        custom_OSReport("Default boot entry not marked as bootable (boot_indicator=0x%02X).\n", entry->boot_indicator);
        // warning?
    }

    // Compute the offset (in bytes) where the boot file starts.
    uint32_t boot_file_offset = __builtin_bswap32(entry->load_rba) * ISO_SECTOR_SIZE;
    return boot_file_offset;
}
