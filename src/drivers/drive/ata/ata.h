#ifndef ATA_H
#define ATA_H

#include <stdint.h>

// ATA I/O Ports (Primary Bus)
#define ATA_PRIMARY_DATA       0x1F0
#define ATA_PRIMARY_ERROR      0x1F1
#define ATA_PRIMARY_SECCOUNT   0x1F2
#define ATA_PRIMARY_LBA_LOW    0x1F3
#define ATA_PRIMARY_LBA_MID    0x1F4
#define ATA_PRIMARY_LBA_HIGH   0x1F5
#define ATA_PRIMARY_DRIVE      0x1F6
#define ATA_PRIMARY_STATUS     0x1F7
#define ATA_PRIMARY_COMMAND    0x1F7

// ATA I/O Ports (Secondary Bus)
#define ATA_SECONDARY_DATA     0x170
#define ATA_SECONDARY_ERROR    0x171
#define ATA_SECONDARY_SECCOUNT 0x172
#define ATA_SECONDARY_LBA_LOW  0x173
#define ATA_SECONDARY_LBA_MID  0x174
#define ATA_SECONDARY_LBA_HIGH 0x175
#define ATA_SECONDARY_DRIVE    0x176
#define ATA_SECONDARY_STATUS   0x177
#define ATA_SECONDARY_COMMAND  0x177

// ATA Commands
#define ATA_CMD_READ_PIO       0x20
#define ATA_CMD_WRITE_PIO      0x30
#define ATA_CMD_CACHE_FLUSH    0xE7
#define ATA_CMD_IDENTIFY       0xEC
#define ATA_CMD_IDENTIFY_PACKET 0xA1

// ATAPI Commands
#define ATAPI_CMD_READ         0xA8

// Status Register Bits
#define ATA_SR_BSY   0x80  // Busy
#define ATA_SR_DRDY  0x40  // Drive ready
#define ATA_SR_DF    0x20  // Drive fault
#define ATA_SR_DSC   0x10  // Drive seek complete
#define ATA_SR_DRQ   0x08  // Data request ready
#define ATA_SR_CORR  0x04  // Corrected data
#define ATA_SR_IDX   0x02  // Index
#define ATA_SR_ERR   0x01  // Error

// Drive Select Bits
#define ATA_MASTER 0xA0
#define ATA_SLAVE  0xB0

// Device Types
#define ATA_DEVICE_TYPE_UNKNOWN 0
#define ATA_DEVICE_TYPE_ATA     1
#define ATA_DEVICE_TYPE_ATAPI   2

// Drive structure
typedef struct {
    uint8_t exists;           // 1 if drive detected
    uint8_t type;             // ATA_DEVICE_TYPE_*
    uint16_t base_port;       // Base I/O port
    uint8_t is_slave;         // 0=master, 1=slave
    uint32_t size_mb;         // Size in MB
    uint32_t total_sectors;   // Total LBA28 addressable sectors
    char model[41];           // Model string
} ata_drive_t;

// Function prototypes
int ata_init(void);
int ata_detect_drive(uint16_t base_port, uint8_t is_slave);
int ata_identify(uint16_t base_port, uint8_t is_slave, uint16_t *buffer);
int ata_read_sector(uint8_t drive_id, uint32_t lba, uint16_t *buffer);
int ata_write_sector(uint8_t drive_id, uint32_t lba, const uint16_t *buffer);
ata_drive_t* ata_get_drive(uint8_t drive_id);

#endif // ATA_H
