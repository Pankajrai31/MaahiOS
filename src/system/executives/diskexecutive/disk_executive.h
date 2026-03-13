/**
 * MaahiOS Disk Executive Header
 * 
 * Description:
 *   Disk Executive provides block-level storage access services.
 *   Manages physical disks: enumerate, query info, read/write raw sectors.
 *   Does NOT handle filesystems — that's a future FS Executive.
 * 
 *   PID 7 - loaded 5th by sysman (after Log, Cell, Process, Memory)
 *   Uses liblog for logging (auto-init)
 *   Uses libcell for cell registration (auto-init)
 *   Uses SYS_DEV_* syscalls to talk to kernel Device Manager
 *   Dual SHM queues (request + response)
 * 
 * Data Flow:
 *   App -> libdisk -> SHM queue -> Disk Executive -> SYS_DEV_* syscalls
 *     -> Device Manager -> disk_subsystem -> ATA driver -> hardware
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef DISK_EXECUTIVE_H
#define DISK_EXECUTIVE_H

#include "../common/executive_common.h"

/*=============================================================================
 * DISK EXECUTIVE OPCODES (starting at EXEC_OP_CUSTOM_BASE = 16)
 *
 * These are BLOCK DEVICE operations only. No filesystem ops here.
 *===========================================================================*/

#define DISK_OP_LIST_DISKS      (EXEC_OP_CUSTOM_BASE + 0)   /* List all disks */
#define DISK_OP_GET_INFO        (EXEC_OP_CUSTOM_BASE + 1)   /* Get disk info */
#define DISK_OP_GET_STATUS      (EXEC_OP_CUSTOM_BASE + 2)   /* Get disk status */
#define DISK_OP_READ_SECTOR     (EXEC_OP_CUSTOM_BASE + 3)   /* Read raw sector */
#define DISK_OP_WRITE_SECTOR    (EXEC_OP_CUSTOM_BASE + 4)   /* Write raw sector */
#define DISK_OP_GET_SECTOR_SIZE (EXEC_OP_CUSTOM_BASE + 5)   /* Get sector size */
#define DISK_OP_FORMAT          (EXEC_OP_CUSTOM_BASE + 6)   /* Format disk (MBR+MFS) */

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define DISK_NAME_MAX           32
#define DISK_MAX_DISKS          8

/* Disk types (matches disk_subsystem.h) */
#define DISK_TYPE_UNKNOWN       0
#define DISK_TYPE_HDD           1
#define DISK_TYPE_CDROM         2
#define DISK_TYPE_FLOPPY        3

/* Disk status */
#define DISK_STATUS_OFFLINE     0
#define DISK_STATUS_ONLINE      1
#define DISK_STATUS_ERROR       2

/*=============================================================================
 * DISK INFO STRUCTURE (returned to callers)
 *===========================================================================*/

typedef struct {
    uint8_t  index;             /* Disk index (0-based) */
    uint8_t  disk_type;         /* DISK_TYPE_* */
    uint8_t  status;            /* DISK_STATUS_* */
    uint8_t  reserved;
    uint32_t sector_size;       /* Bytes per sector */
    uint32_t size_mb;           /* Size in MB */
    char     name[DISK_NAME_MAX]; /* Human-readable name */
} disk_exec_info_t;

/*=============================================================================
 * REQUEST PAYLOADS
 *===========================================================================*/

/* LIST_DISKS request — no payload needed, result = count */

/* GET_INFO request */
typedef struct {
    uint8_t disk_index;         /* Which disk (0-based) */
} disk_get_info_req_t;

/* GET_STATUS request */
typedef struct {
    uint8_t disk_index;         /* Which disk (0-based) */
} disk_get_status_req_t;

/* READ_SECTOR request
 * NOTE: The executive reads the sector via SYS_DEV_READ and puts
 * the data into a SHM block. The response result = SHM ID.
 * Caller must SHM_ATTACH to read the data, then SHM_DETACH. */
typedef struct {
    uint8_t  disk_index;        /* Which disk */
    uint8_t  reserved[3];
    uint32_t lba;               /* Starting LBA */
    uint32_t count;             /* Number of sectors (currently max 1) */
} disk_read_sector_req_t;

/* WRITE_SECTOR request — future */
typedef struct {
    uint8_t  disk_index;        /* Which disk */
    uint8_t  reserved[3];
    uint32_t lba;               /* Starting LBA */
    uint32_t count;             /* Number of sectors (max 1) */
    int32_t  data_shm_id;      /* SHM ID containing sector data to write */
} disk_write_sector_req_t;

/* GET_SECTOR_SIZE request */
typedef struct {
    uint8_t disk_index;         /* Which disk */
} disk_get_sector_size_req_t;

/* FORMAT request */
typedef struct {
    uint8_t  disk_index;        /* Which disk to format */
    uint8_t  reserved[3];
    char     label[32];         /* Volume label (null-terminated) */
} disk_format_req_t;

/*=============================================================================
 * RESPONSE PAYLOADS
 *===========================================================================*/

/* LIST_DISKS response — fits in payload (each info is ~44 bytes, 8 max = 352)
 * Since EXEC_MSG_MAX_PAYLOAD = 256, we limit to 5 disks per response.
 * result = total disk count. */
typedef struct {
    uint32_t count;
    disk_exec_info_t disks[5];  /* Up to 5 disk infos in payload */
} disk_list_resp_t;

/* GET_INFO response */
typedef struct {
    disk_exec_info_t info;
} disk_info_resp_t;

/* GET_STATUS response — result = status code (DISK_STATUS_*) */

/* READ_SECTOR response — result = SHM ID containing sector data */

/* GET_SECTOR_SIZE response — result = sector size in bytes */

/*=============================================================================
 * EXECUTIVE ENTRY POINT
 *===========================================================================*/

void exe_disk_main(void);

#endif /* DISK_EXECUTIVE_H */
