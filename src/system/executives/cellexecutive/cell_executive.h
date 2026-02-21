/**
 * MaahiOS Cell Executive Header
 * 
 * Description:
 *   Cell Executive manages the cell registry - a hierarchical key-value store
 *   for system and application data (similar to Windows Registry).
 * 
 * Cell Executive is the FIRST executive loaded by sysman because all other
 * executives register their information in cells.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef CELL_EXECUTIVE_H
#define CELL_EXECUTIVE_H

#include "../common/executive_common.h"

/*=============================================================================
 * CELL EXECUTIVE OPCODES
 *===========================================================================*/

/* Cell operations (starting at EXEC_OP_CUSTOM_BASE = 16) */
#define CELL_OP_REGISTER        (EXEC_OP_CUSTOM_BASE + 0)   /* Register a cell */
#define CELL_OP_UNREGISTER      (EXEC_OP_CUSTOM_BASE + 1)   /* Unregister a cell */
#define CELL_OP_LOOKUP          (EXEC_OP_CUSTOM_BASE + 2)   /* Find cell by name */
#define CELL_OP_GET_INFO        (EXEC_OP_CUSTOM_BASE + 3)   /* Get cell metadata */
#define CELL_OP_LIST            (EXEC_OP_CUSTOM_BASE + 4)   /* List cells */
#define CELL_OP_WRITE           (EXEC_OP_CUSTOM_BASE + 5)   /* Write data to cell */
#define CELL_OP_READ            (EXEC_OP_CUSTOM_BASE + 6)   /* Read data from cell */
#define CELL_OP_WRITE_INT       (EXEC_OP_CUSTOM_BASE + 7)   /* Write integer */
#define CELL_OP_READ_INT        (EXEC_OP_CUSTOM_BASE + 8)   /* Read integer */
#define CELL_OP_DELETE          (EXEC_OP_CUSTOM_BASE + 9)   /* Delete cell */
#define CELL_OP_EXISTS          (EXEC_OP_CUSTOM_BASE + 10)  /* Check if exists */

/*=============================================================================
 * CELL TYPES
 *===========================================================================*/

typedef enum {
    CELL_TYPE_DATA      = 0,    /* Raw binary data */
    CELL_TYPE_INT       = 1,    /* 32-bit integer */
    CELL_TYPE_STRING    = 2,    /* Null-terminated string */
    CELL_TYPE_BLOB      = 3,    /* Large binary object */
    CELL_TYPE_LINK      = 4     /* Link to another cell */
} cell_type_t;

/*=============================================================================
 * CELL FLAGS
 *===========================================================================*/

#define CELL_FLAG_PERSISTENT    (1 << 0)    /* Survives reboot (not yet impl) */
#define CELL_FLAG_READONLY      (1 << 1)    /* Cannot be modified */
#define CELL_FLAG_SYSTEM        (1 << 2)    /* System cell (kernel use only) */
#define CELL_FLAG_VOLATILE      (1 << 3)    /* Cleared on reboot */

/*=============================================================================
 * CELL INFO STRUCTURE
 *===========================================================================*/

#define CELL_NAME_MAX   64
#define CELL_DATA_MAX   256

typedef struct {
    char name[CELL_NAME_MAX];   /* Cell name (hierarchical: "system.exec.cell") */
    cell_type_t type;           /* Cell type */
    uint32_t flags;             /* Cell flags */
    uint32_t size;              /* Data size */
    uint32_t owner_pid;         /* Owner process ID */
    uint32_t created_time;      /* Creation timestamp */
    uint32_t modified_time;     /* Last modification timestamp */
} cell_info_t;

/*=============================================================================
 * REQUEST PAYLOADS
 *===========================================================================*/

/* Register cell request */
typedef struct {
    char name[CELL_NAME_MAX];
    cell_type_t type;
    uint32_t flags;
} cell_register_req_t;

/* Write cell request */
typedef struct {
    char name[CELL_NAME_MAX];
    uint32_t size;
    uint8_t data[CELL_DATA_MAX];
} cell_write_req_t;

/* Read cell request */
typedef struct {
    char name[CELL_NAME_MAX];
    uint32_t max_size;
} cell_read_req_t;

/* Write integer request */
typedef struct {
    char name[CELL_NAME_MAX];
    int32_t value;
} cell_write_int_req_t;

/* Lookup/delete/exists request */
typedef struct {
    char name[CELL_NAME_MAX];
} cell_name_req_t;

/* List cells request */
typedef struct {
    char prefix[CELL_NAME_MAX]; /* List cells starting with this prefix */
    uint32_t offset;            /* Start from this index */
    uint32_t max_count;         /* Maximum cells to return */
} cell_list_req_t;

/*=============================================================================
 * RESPONSE PAYLOADS
 *===========================================================================*/

/* Read response */
typedef struct {
    uint32_t size;
    uint8_t data[CELL_DATA_MAX];
} cell_read_resp_t;

/* Read integer response */
typedef struct {
    int32_t value;
} cell_read_int_resp_t;

/* Get info response */
typedef struct {
    cell_info_t info;
} cell_info_resp_t;

/* List response */
typedef struct {
    uint32_t total_count;       /* Total matching cells */
    uint32_t returned_count;    /* Number in this response */
    char names[8][CELL_NAME_MAX]; /* Up to 8 cell names */
} cell_list_resp_t;

/*=============================================================================
 * EXECUTIVE ENTRY POINT
 *===========================================================================*/

/**
 * exe_cell_main - Cell Executive main function
 * 
 * Called after Ring 3 entry. Sets up queues and enters main loop.
 */
void exe_cell_main(void);

#endif /* CELL_EXECUTIVE_H */
