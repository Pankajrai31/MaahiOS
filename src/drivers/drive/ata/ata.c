#include "ata.h"
#include "../../../managers/klog/klog.h"
#include "../../../system/libraries/shared/io.h"

// Global drive array (4 possible drives: Primary Master/Slave, Secondary Master/Slave)
static ata_drive_t g_ata_drives[4] = {0};

// Wait for drive to be ready
static int ata_wait_ready(uint16_t base_port) {
    uint8_t status;
    int timeout = 100000;
    
    while (timeout-- > 0) {
        status = inb(base_port + 7);  // Read status register
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRDY)) {
            return 0;  // Ready
        }
        // Small delay (~400ns per OSDev)
        for (volatile int i = 0; i < 100; i++);
    }
    return -1;  // Timeout
}

// Wait for DRQ (data request)
static int ata_wait_drq(uint16_t base_port) {
    uint8_t status;
    int timeout = 10000;
    
    while (timeout-- > 0) {
        status = inb(base_port + 7);
        if (status & ATA_SR_DRQ) {
            return 0;  // DRQ set
        }
        if (status & ATA_SR_ERR) {
            return -2;  // Error
        }
        for (volatile int i = 0; i < 100; i++);
    }
    return -1;  // Timeout
}

/**
 * Detect if a drive exists on the given bus
 */
int ata_detect_drive(uint16_t base_port, uint8_t is_slave) {
    // Select drive
    outb(base_port + 6, is_slave ? ATA_SLAVE : ATA_MASTER);
    
    // Small delay after drive select
    for (volatile int i = 0; i < 1000; i++);
    
    // Read status
    uint8_t status = inb(base_port + 7);
    
    // If status is 0xFF, no drive present
    if (status == 0xFF) {
        return ATA_DEVICE_TYPE_UNKNOWN;
    }
    
    // Try to identify the drive
    outb(base_port + 6, is_slave ? ATA_SLAVE : ATA_MASTER);
    for (volatile int i = 0; i < 1000; i++); // Wait for select
    
    // Set LBA regs to 0 (Critical for some controllers)
    outb(base_port + 2, 0);
    outb(base_port + 3, 0);
    outb(base_port + 4, 0);
    outb(base_port + 5, 0);
    
    // Send IDENTIFY command
    outb(base_port + 7, ATA_CMD_IDENTIFY);
    
    status = inb(base_port + 7);
    if (status == 0) {
        return ATA_DEVICE_TYPE_UNKNOWN;  // Drive doesn't exist
    }
    
    // Poll until BSY clears
    int timeout = 100000;
    while (timeout-- > 0) {
        status = inb(base_port + 7);
        if (!(status & ATA_SR_BSY)) break;
    }
    
    // Check for ATAPI signature (LBA Mid=0x14, High=0xEB)
    // Must check this BEFORE checking for ERR, because ATAPI aborts IDENTIFY
    uint8_t lba_mid = inb(base_port + 4);
    uint8_t lba_high = inb(base_port + 5);
    
    if ((lba_mid == 0x14 && lba_high == 0xEB) || (lba_mid == 0x69 && lba_high == 0x96)) {
        return ATA_DEVICE_TYPE_ATAPI;  // It's an ATAPI device
    }
    
    // If Status indicates Error, and we didn't find a signature above, maybe Packet command works?
    if (status & ATA_SR_ERR) {
        uint8_t err = inb(base_port + 1); // Read Error Reg
        // If it aborted, double check signature again (just in case)
        lba_mid = inb(base_port + 4);
        lba_high = inb(base_port + 5);
        if ((lba_mid == 0x14 && lba_high == 0xEB) || (lba_mid == 0x69 && lba_high == 0x96)) {
            return ATA_DEVICE_TYPE_ATAPI;
        }
        return ATA_DEVICE_TYPE_UNKNOWN; 
    }
    
    // If DRQ is set, it's a standard ATA drive ready to transfer data
    if (status & ATA_SR_DRQ) {
        return ATA_DEVICE_TYPE_ATA;
    }
    
    return ATA_DEVICE_TYPE_UNKNOWN;
}

/**
 * Read IDENTIFY data from drive
 */
int ata_identify(uint16_t base_port, uint8_t is_slave, uint16_t *buffer) {
    // Select drive
    outb(base_port + 6, is_slave ? ATA_SLAVE : ATA_MASTER);
    for (volatile int i = 0; i < 1000; i++);
    
    // Send IDENTIFY command
    outb(base_port + 2, 0);
    outb(base_port + 3, 0);
    outb(base_port + 4, 0);
    outb(base_port + 5, 0);
    outb(base_port + 7, ATA_CMD_IDENTIFY);
    
    // Wait for DRQ
    if (ata_wait_drq(base_port) != 0) {
        return -1;
    }
    
    // Read 256 words (512 bytes)
    for (int i = 0; i < 256; i++) {
        buffer[i] = inw(base_port);
    }
    
    return 0;
}

/**
 * Initialize ATA subsystem and detect all drives
 */
int ata_init(void) {
    KLOG_INFO("ATA", "Initializing ATA/ATAPI driver");
    
    /* Probe all 4 drive slots */
    uint16_t ports[4]   = { ATA_PRIMARY_DATA, ATA_PRIMARY_DATA,
                            ATA_SECONDARY_DATA, ATA_SECONDARY_DATA };
    uint8_t  slaves[4]  = { 0, 1, 0, 1 };
    const char *names[4] = { "Primary Master", "Primary Slave",
                             "Secondary Master", "Secondary Slave" };

    for (int i = 0; i < 4; i++) {
        g_ata_drives[i].base_port = ports[i];
        g_ata_drives[i].is_slave  = slaves[i];
        g_ata_drives[i].type = ata_detect_drive(ports[i], slaves[i]);

        if (g_ata_drives[i].type == ATA_DEVICE_TYPE_UNKNOWN)
            continue;

        g_ata_drives[i].exists = 1;
        KLOG_INFO("ATA", "%s: %s", names[i],
                  g_ata_drives[i].type == ATA_DEVICE_TYPE_ATAPI ? "ATAPI" : "ATA");

        /* For ATA (HDD) drives, read IDENTIFY data to get capacity */
        if (g_ata_drives[i].type == ATA_DEVICE_TYPE_ATA) {
            uint16_t id_buf[256];
            if (ata_identify(ports[i], slaves[i], id_buf) == 0) {
                /* Words 60-61: Total addressable LBA28 sectors */
                uint32_t total_sectors = (uint32_t)id_buf[60]
                                       | ((uint32_t)id_buf[61] << 16);
                g_ata_drives[i].total_sectors = total_sectors;
                g_ata_drives[i].size_mb = total_sectors / 2048; /* 512-byte sectors → MB */

                /* Words 27-46: Model string (40 ASCII chars, byte-swapped) */
                for (int w = 0; w < 20; w++) {
                    g_ata_drives[i].model[w * 2]     = (char)(id_buf[27 + w] >> 8);
                    g_ata_drives[i].model[w * 2 + 1] = (char)(id_buf[27 + w] & 0xFF);
                }
                g_ata_drives[i].model[40] = '\0';
                /* Trim trailing spaces */
                for (int j = 39; j >= 0 && g_ata_drives[i].model[j] == ' '; j--)
                    g_ata_drives[i].model[j] = '\0';

                KLOG_INFO("ATA", "  Model: %s", g_ata_drives[i].model);
                KLOG_INFO("ATA", "  Size: %u MB (%u sectors)",
                          g_ata_drives[i].size_mb, total_sectors);
            }
        }
    }

    KLOG_INFO("ATA", "ATA driver initialization complete");
    return 0;
}

/**
 * Get drive information by ID
 */
ata_drive_t* ata_get_drive(uint8_t drive_id) {
    if (drive_id >= 4) return 0;
    if (!g_ata_drives[drive_id].exists) return 0;
    return &g_ata_drives[drive_id];
}

/**
 * Read a sector from ATA drive (not implemented for ATAPI yet)
 */
int ata_read_sector(uint8_t drive_id, uint32_t lba, uint16_t *buffer) {
    if (drive_id >= 4) return -1;
    if (!g_ata_drives[drive_id].exists) return -1;
    
    ata_drive_t *drive = &g_ata_drives[drive_id];
    
    // ATAPI reading requires different commands (not implemented yet)
    if (drive->type == ATA_DEVICE_TYPE_ATAPI) {
        return -2;  // Not supported yet
    }
    
    uint16_t base = drive->base_port;
    uint8_t slave = drive->is_slave;
    
    // Select drive first (must happen BEFORE wait_ready)
    outb(base + 6, (slave ? 0xF0 : 0xE0) | ((lba >> 24) & 0x0F));
    
    // 400ns delay after drive select (read alternate status 4 times)
    for (volatile int i = 0; i < 1000; i++);
    
    // Wait for drive ready
    if (ata_wait_ready(base) != 0) return -3;
    
    // Set sector count and LBA
    outb(base + 2, 1);  // Read 1 sector
    outb(base + 3, (uint8_t)lba);
    outb(base + 4, (uint8_t)(lba >> 8));
    outb(base + 5, (uint8_t)(lba >> 16));
    outb(base + 7, ATA_CMD_READ_PIO);
    
    // Wait for DRQ
    if (ata_wait_drq(base) != 0) return -4;
    
    // Read 256 words (512 bytes)
    for (int i = 0; i < 256; i++) {
        buffer[i] = inw(base);
    }
    
    return 0;
}

/**
 * Write a sector to ATA drive (PIO mode, LBA28)
 * Per OSDev: select drive → wait ready → set LBA → send WRITE PIO
 *   → wait DRQ → write 256 words → flush cache
 */
int ata_write_sector(uint8_t drive_id, uint32_t lba, const uint16_t *buffer) {
    if (drive_id >= 4) return -1;
    if (!g_ata_drives[drive_id].exists) return -1;

    ata_drive_t *drive = &g_ata_drives[drive_id];

    /* ATAPI (CD-ROM) is read-only */
    if (drive->type == ATA_DEVICE_TYPE_ATAPI) {
        return -2;
    }

    uint16_t base = drive->base_port;
    uint8_t slave = drive->is_slave;

    /* Select drive with top 4 bits of LBA */
    outb(base + 6, (slave ? 0xF0 : 0xE0) | ((lba >> 24) & 0x0F));

    /* Delay after drive select */
    for (volatile int i = 0; i < 1000; i++);

    /* Wait for drive ready */
    if (ata_wait_ready(base) != 0) return -3;

    /* Set sector count and LBA */
    outb(base + 2, 1);                    /* Write 1 sector */
    outb(base + 3, (uint8_t)lba);         /* LBA low */
    outb(base + 4, (uint8_t)(lba >> 8));  /* LBA mid */
    outb(base + 5, (uint8_t)(lba >> 16)); /* LBA high */
    outb(base + 7, ATA_CMD_WRITE_PIO);    /* Send WRITE SECTORS command */

    /* Wait for DRQ */
    if (ata_wait_drq(base) != 0) return -4;

    /* Write 256 words (512 bytes) */
    for (int i = 0; i < 256; i++) {
        outw(base, buffer[i]);
    }

    /* Flush write cache per OSDev recommendation */
    outb(base + 7, ATA_CMD_CACHE_FLUSH);

    /* Wait for BSY to clear after flush */
    if (ata_wait_ready(base) != 0) return -5;

    return 0;
}
