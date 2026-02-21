/**
 * MaahiOS Disk Executive Header
 * 
 * Description:
 *   Disk Executive provides storage access services.
 *   Handles disk I/O, filesystem operations, and file access.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef DISK_EXECUTIVE_H
#define DISK_EXECUTIVE_H

#include "../common/executive_common.h"

/*=============================================================================
 * DISK EXECUTIVE OPCODES
 *===========================================================================*/

#define DISK_OP_READ_SECTOR     (EXEC_OP_CUSTOM_BASE + 0)   /* Read raw sector */
#define DISK_OP_WRITE_SECTOR    (EXEC_OP_CUSTOM_BASE + 1)   /* Write raw sector */
#define DISK_OP_READ_FILE       (EXEC_OP_CUSTOM_BASE + 2)   /* Read file */
#define DISK_OP_WRITE_FILE      (EXEC_OP_CUSTOM_BASE + 3)   /* Write file */
#define DISK_OP_LIST_DIR        (EXEC_OP_CUSTOM_BASE + 4)   /* List directory */
#define DISK_OP_FILE_EXISTS     (EXEC_OP_CUSTOM_BASE + 5)   /* Check if file exists */
#define DISK_OP_FILE_INFO       (EXEC_OP_CUSTOM_BASE + 6)   /* Get file info */
#define DISK_OP_GET_DISK_INFO   (EXEC_OP_CUSTOM_BASE + 7)   /* Get disk info */
#define DISK_OP_LIST_DISKS      (EXEC_OP_CUSTOM_BASE + 8)   /* List available disks */

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

#define DISK_PATH_MAX       256
#define DISK_NAME_MAX       64
#define DISK_SECTOR_SIZE    2048    /* CD-ROM sector size */
#define DISK_MAX_READ       4096    /* Max bytes per read */

/*=============================================================================
 * DISK INFO STRUCTURE
 *===========================================================================*/

typedef struct {
    char name[DISK_NAME_MAX];
    uint32_t disk_id;
    uint32_t type;          /* 0=ATA, 1=ATAPI (CD-ROM), etc. */
    uint32_t sector_size;
    uint32_t total_sectors;
    uint32_t flags;
} disk_info_t;

/*=============================================================================
 * FILE INFO STRUCTURE
 *===========================================================================*/

typedef struct {
    char name[DISK_NAME_MAX];
    char path[DISK_PATH_MAX];
    uint32_t size;
    uint32_t type;          /* 0=file, 1=directory */
    uint32_t flags;
} file_info_t;

/*=============================================================================
 * REQUEST PAYLOADS
 *===========================================================================*/

/* Read sector request */
typedef struct {
    uint32_t disk_id;
    uint32_t sector;
    uint32_t count;
} disk_read_sector_req_t;

/* Read file request */
typedef struct {
    char path[DISK_PATH_MAX];
    uint32_t offset;
    uint32_t size;
} disk_read_file_req_t;

/* List directory request */
typedef struct {
    char path[DISK_PATH_MAX];
    uint32_t offset;
    uint32_t max_entries;
} disk_list_dir_req_t;

/* File exists/info request */
typedef struct {
    char path[DISK_PATH_MAX];
} disk_path_req_t;

/*=============================================================================
 * RESPONSE PAYLOADS
 *===========================================================================*/

/* Read sector response */
typedef struct {
    uint32_t bytes_read;
    uint8_t data[DISK_MAX_READ];
} disk_read_resp_t;

/* List directory response */
typedef struct {
    uint32_t total_entries;
    uint32_t returned_count;
    file_info_t entries[4];  /* Up to 4 entries per response */
} disk_list_resp_t;

/* File info response */
typedef struct {
    file_info_t info;
} disk_file_info_resp_t;

/* Disk info response */
typedef struct {
    disk_info_t info;
} disk_info_resp_t;

/*=============================================================================
 * EXECUTIVE ENTRY POINT
 *===========================================================================*/

void exe_disk_main(void);

#endif /* DISK_EXECUTIVE_H */
