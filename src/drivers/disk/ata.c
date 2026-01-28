#include "ata.h"

// External kernel functions
extern void serial_print(const char *str);

// Port I/O functions
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Global drive array (4 possible drives: Primary Master/Slave, Secondary Master/Slave)
static ata_drive_t g_ata_drives[4] = {0};

// Wait for drive to be ready
static int ata_wait_ready(uint16_t base_port) {
    uint8_t status;
    int timeout = 10000;
    
    while (timeout-- > 0) {
        status = inb(base_port + 7);  // Read status register
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRDY)) {
            return 0;  // Ready
        }
        // Small delay
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
void ata_init(void) {
    serial_print("[ATA] Initializing ATA/ATAPI driver...\n");
    
    // Detect Primary Master (drive 0)
    g_ata_drives[0].base_port = ATA_PRIMARY_DATA;
    g_ata_drives[0].is_slave = 0;
    g_ata_drives[0].type = ata_detect_drive(ATA_PRIMARY_DATA, 0);
    if (g_ata_drives[0].type != ATA_DEVICE_TYPE_UNKNOWN) {
        g_ata_drives[0].exists = 1;
        serial_print("[ATA] Primary Master detected: ");
        serial_print(g_ata_drives[0].type == ATA_DEVICE_TYPE_ATAPI ? "ATAPI\n" : "ATA\n");
    }
    
    // Detect Primary Slave (drive 1)
    g_ata_drives[1].base_port = ATA_PRIMARY_DATA;
    g_ata_drives[1].is_slave = 1;
    g_ata_drives[1].type = ata_detect_drive(ATA_PRIMARY_DATA, 1);
    if (g_ata_drives[1].type != ATA_DEVICE_TYPE_UNKNOWN) {
        g_ata_drives[1].exists = 1;
        serial_print("[ATA] Primary Slave detected: ");
        serial_print(g_ata_drives[1].type == ATA_DEVICE_TYPE_ATAPI ? "ATAPI\n" : "ATA\n");
    }
    
    // Detect Secondary Master (drive 2)
    g_ata_drives[2].base_port = ATA_SECONDARY_DATA;
    g_ata_drives[2].is_slave = 0;
    g_ata_drives[2].type = ata_detect_drive(ATA_SECONDARY_DATA, 0);
    if (g_ata_drives[2].type != ATA_DEVICE_TYPE_UNKNOWN) {
        g_ata_drives[2].exists = 1;
        serial_print("[ATA] Secondary Master detected: ");
        serial_print(g_ata_drives[2].type == ATA_DEVICE_TYPE_ATAPI ? "ATAPI\n" : "ATA\n");
    }
    
    // Detect Secondary Slave (drive 3)
    g_ata_drives[3].base_port = ATA_SECONDARY_DATA;
    g_ata_drives[3].is_slave = 1;
    g_ata_drives[3].type = ata_detect_drive(ATA_SECONDARY_DATA, 1);
    if (g_ata_drives[3].type != ATA_DEVICE_TYPE_UNKNOWN) {
        g_ata_drives[3].exists = 1;
        serial_print("[ATA] Secondary Slave detected: ");
        serial_print(g_ata_drives[3].type == ATA_DEVICE_TYPE_ATAPI ? "ATAPI\n" : "ATA\n");
    }
    
    serial_print("[ATA] Initialization complete\n");
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
    
    // Wait for drive ready
    if (ata_wait_ready(base) != 0) return -3;
    
    // Select drive and set LBA mode
    outb(base + 6, (slave ? 0xF0 : 0xE0) | ((lba >> 24) & 0x0F));
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
