/**
 * MaahiOS Filesystem Executive Header
 * 
 * Description:
 *   Filesystem Executive provides file-level access services.
 *   Abstracts filesystem types (ISO9660 now, MFS future).
 *   Clients send path-based requests; executive resolves filesystem details.
 * 
 *   Uses SYS_FS_* syscalls to talk to kernel filesystem drivers.
 *   Dual SHM queues (request + response) for client communication.
 *   Returns file/directory data via SHM blocks.
 * 
 * Data Flow:
 *   App -> libfs -> SHM queue -> FS Executive -> SYS_FS_* syscalls
 *     -> ISO9660 driver -> ATAPI hardware
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef FS_EXECUTIVE_H
#define FS_EXECUTIVE_H

#include "../common/executive_common.h"

/*=============================================================================
 * FS EXECUTIVE OPCODES (starting at EXEC_OP_CUSTOM_BASE = 16)
 *===========================================================================*/

#define FS_OP_LIST_DIR          (EXEC_OP_CUSTOM_BASE + 0)   /* List directory */
#define FS_OP_READ_FILE         (EXEC_OP_CUSTOM_BASE + 1)   /* Read file data */
#define FS_OP_FILE_INFO         (EXEC_OP_CUSTOM_BASE + 2)   /* Get file info */
#define FS_OP_FILE_COUNT        (EXEC_OP_CUSTOM_BASE + 3)   /* Get file count */

/*=============================================================================
 * FILESYSTEM TYPE CONSTANTS
 *===========================================================================*/

#define FS_TYPE_UNKNOWN         0
#define FS_TYPE_ISO9660         1   /* ISO 9660 (CD-ROM) — supported now */
#define FS_TYPE_MFS             2   /* MaahiOS File System — future */

/*=============================================================================
 * FILE ENTRY STRUCTURE (user-facing, filesystem-agnostic)
 *
 * Used in SHM blocks returned by LIST_DIR operations.
 * 56 bytes per entry, well-aligned.
 *===========================================================================*/

typedef struct {
    char     name[44];          /* Filename (null-terminated) */
    uint32_t size;              /* File size in bytes */
    uint8_t  is_directory;      /* 1 if directory, 0 if file */
    uint8_t  fs_type;           /* FS_TYPE_* */
    uint8_t  reserved[2];       /* Padding for alignment */
} fs_file_entry_t;              /* 52 bytes total */

/*=============================================================================
 * REQUEST PAYLOADS
 *===========================================================================*/

/* LIST_DIR request — path in payload */
typedef struct {
    char path[128];             /* Directory path (e.g., "/" or "/BOOT") */
} fs_list_dir_req_t;

/* READ_FILE request — directory path + filename */
typedef struct {
    char dir_path[64];          /* Directory containing the file */
    char filename[64];          /* File to read */
    uint32_t max_size;          /* Maximum bytes to read */
} fs_read_file_req_t;

/* FILE_INFO request — path in payload */
typedef struct {
    char path[128];             /* File or directory path */
} fs_file_info_req_t;

/* FILE_COUNT request — path in payload */
typedef struct {
    char path[128];             /* Directory path */
} fs_file_count_req_t;

/*=============================================================================
 * RESPONSE PAYLOADS
 *===========================================================================*/

/* LIST_DIR response:
 *   result  = entry count
 *   payload = int32_t shm_id (SHM block with fs_file_entry_t array)
 *   Caller must SHM_ATTACH to read entries, then SHM_DETACH + SHM_DESTROY.
 */
typedef struct {
    int32_t shm_id;             /* SHM ID containing fs_file_entry_t[] */
} fs_list_dir_resp_t;

/* READ_FILE response:
 *   result  = bytes read
 *   payload = int32_t shm_id (SHM block with file data)
 *   Caller must SHM_ATTACH to read data, then SHM_DETACH + SHM_DESTROY.
 */
typedef struct {
    int32_t shm_id;             /* SHM ID containing file data */
} fs_read_file_resp_t;

/* FILE_INFO response — fits in payload */
typedef struct {
    fs_file_entry_t entry;      /* File/directory info */
} fs_file_info_resp_t;

/* FILE_COUNT response — result = count, no payload */

/*=============================================================================
 * EXECUTIVE ENTRY POINT
 *===========================================================================*/

void exe_fs_main(void);

#endif /* FS_EXECUTIVE_H */
