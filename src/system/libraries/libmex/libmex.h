/**
 * MaahiOS MEX Library - libmex.h
 * 
 * User-space library for parsing .mex (MaahiOS Executable) files.
 * All MEX format knowledge lives here — the kernel knows nothing
 * about the .mex format. It just creates processes from memory.
 *
 * Usage:
 *   #include "libmex.h"
 *
 *   uint8_t *file_data = ...;   // .mex file read from ISO
 *   uint32_t file_size = ...;
 *
 *   mex_info_t info;
 *   int result = libmex_parse(file_data, file_size, &info);
 *   if (result == 0) {
 *       // info.binary_data points to code after header
 *       // info.code_size, info.base_address, etc. are ready
 *       int pid = libmex_exec(&info);
 *   }
 */

#ifndef LIBMEX_H
#define LIBMEX_H

#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════
 * MEX Constants (mirrors kernel mex.h)
 * ═══════════════════════════════════════════════════════════════════ */

#define MEX_MAGIC           0x0058454D  /* "MEX\0" little-endian */
#define MEX_VERSION_1_0     0x0100
#define MEX_HEADER_SIZE     64
#define MEX_APP_BASE        0x10000000

#define MEX_TYPE_APP        1
#define MEX_TYPE_SERVICE    2
#define MEX_TYPE_DRIVER     3

#define MEX_FLAG_GUI        0x01
#define MEX_FLAG_CONSOLE    0x02

/* Error codes */
#define MEX_OK              0
#define MEX_ERR_NULL        -1   /* NULL pointer passed */
#define MEX_ERR_TOO_SMALL   -2   /* File too small for header */
#define MEX_ERR_BAD_MAGIC   -3   /* Invalid magic number */
#define MEX_ERR_BAD_VERSION -4   /* Unsupported version */
#define MEX_ERR_BAD_TYPE    -5   /* Invalid type field */
#define MEX_ERR_BAD_SIZE    -6   /* code_size doesn't match file */
#define MEX_ERR_BAD_CRC     -7   /* CRC32 mismatch */
#define MEX_ERR_EXEC_FAIL   -8   /* Kernel refused to create process */

/* ═══════════════════════════════════════════════════════════════════
 * Parsed MEX Info (output of libmex_parse)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Parsed from header */
    uint16_t version;
    uint16_t type;           /* MEX_TYPE_APP, MEX_TYPE_SERVICE, MEX_TYPE_DRIVER */
    uint32_t entry_offset;   /* Entry point offset from base */
    uint32_t code_size;      /* Size of binary after header */
    uint32_t bss_size;       /* Additional zeroed memory */
    uint32_t stack_size;     /* Requested stack size */
    uint32_t base_address;   /* Virtual load address (0x10000000) */
    uint32_t flags;          /* MEX_FLAG_GUI | MEX_FLAG_CONSOLE */
    char     name[24];       /* Application name */
    uint32_t checksum;       /* CRC32 from header */

    /* Computed by parser */
    uint8_t *binary_data;    /* Pointer to code (file_data + 64) */
    uint32_t total_memory;   /* code_size + bss_size (pages needed) */
} mex_info_t;

/* ═══════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * libmex_parse - Parse and validate a .mex file buffer
 *
 * @file_data: Pointer to raw .mex file contents (header + binary)
 * @file_size: Total size of the file buffer
 * @info:      Output structure filled with parsed fields
 *
 * Validates: magic, version, type, size consistency, CRC32.
 * On success, info->binary_data points into file_data (no copy).
 *
 * Returns: MEX_OK (0) on success, negative MEX_ERR_* on failure
 */
int libmex_parse(const uint8_t *file_data, uint32_t file_size, mex_info_t *info);

/**
 * libmex_exec - Load and execute a parsed .mex application
 *
 * @info: Previously parsed mex_info (from libmex_parse)
 *
 * Calls SYS_PROCESS_EXEC to ask the kernel to:
 *   1. Clone a page directory
 *   2. Allocate physical memory
 *   3. Map at info->base_address
 *   4. Copy info->binary_data
 *   5. Create process with entry at base_address + entry_offset
 *
 * Returns: PID on success, negative MEX_ERR_EXEC_FAIL on failure
 */
int libmex_exec(const mex_info_t *info);

/**
 * libmex_error_string - Get human-readable error description
 *
 * @error: MEX_ERR_* code from libmex_parse or libmex_exec
 *
 * Returns: Static string describing the error
 */
const char *libmex_error_string(int error);

#endif /* LIBMEX_H */
