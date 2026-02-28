/**
 * MaahiOS MEX Library (libmex) - Implementation
 *
 * User-space parser for .mex (MaahiOS Executable) files.
 * The kernel has NO knowledge of the MEX format — this library
 * handles all parsing, validation, and CRC checking. Then it
 * calls the generic SYS_PROCESS_EXEC syscall to load the binary.
 */

#include "libmex.h"
#include "../libprocess/libprocess.h"

/* ═══════════════════════════════════════════════════════════════════
 * CRC32 (same algorithm as mex_pack.py uses via zlib)
 * Standard CRC32 with 0xEDB88320 polynomial.
 * ═══════════════════════════════════════════════════════════════════ */

static uint32_t crc32_table[256];
static int crc32_initialized = 0;

static void crc32_init(void) {
    if (crc32_initialized) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
        crc32_table[i] = crc;
    }
    crc32_initialized = 1;
}

static uint32_t crc32_compute(const uint8_t *data, uint32_t size) {
    crc32_init();
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < size; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

/* ═══════════════════════════════════════════════════════════════════
 * MEX Header (packed, matches mex.h)
 * Defined locally to avoid pulling in kernel headers.
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint32_t entry_offset;
    uint32_t code_size;
    uint32_t bss_size;
    uint32_t stack_size;
    uint32_t base_address;
    uint32_t flags;
    char     name[24];
    uint32_t checksum;
    uint32_t reserved;
} mex_raw_header_t;

/* ═══════════════════════════════════════════════════════════════════
 * libmex_parse
 * ═══════════════════════════════════════════════════════════════════ */

int libmex_parse(const uint8_t *file_data, uint32_t file_size, mex_info_t *info) {
    /* Null checks */
    if (!file_data || !info) {
        return MEX_ERR_NULL;
    }

    /* Must have at least the 64-byte header */
    if (file_size < MEX_HEADER_SIZE) {
        return MEX_ERR_TOO_SMALL;
    }

    /* Cast to header structure */
    const mex_raw_header_t *hdr = (const mex_raw_header_t *)file_data;

    /* Validate magic */
    if (hdr->magic != MEX_MAGIC) {
        return MEX_ERR_BAD_MAGIC;
    }

    /* Validate version (only 1.0 supported) */
    if (hdr->version != MEX_VERSION_1_0) {
        return MEX_ERR_BAD_VERSION;
    }

    /* Validate type */
    if (hdr->type < MEX_TYPE_APP || hdr->type > MEX_TYPE_DRIVER) {
        return MEX_ERR_BAD_TYPE;
    }

    /* Validate code_size against file size */
    if (hdr->code_size != (file_size - MEX_HEADER_SIZE)) {
        return MEX_ERR_BAD_SIZE;
    }

    /* Validate CRC32 of binary portion */
    const uint8_t *binary = file_data + MEX_HEADER_SIZE;
    uint32_t computed_crc = crc32_compute(binary, hdr->code_size);
    if (computed_crc != hdr->checksum) {
        return MEX_ERR_BAD_CRC;
    }

    /* All checks passed — fill output structure */
    info->version      = hdr->version;
    info->type         = hdr->type;
    info->entry_offset = hdr->entry_offset;
    info->code_size    = hdr->code_size;
    info->bss_size     = hdr->bss_size;
    info->stack_size   = hdr->stack_size;
    info->base_address = hdr->base_address;
    info->flags        = hdr->flags;
    info->checksum     = hdr->checksum;
    info->binary_data  = (uint8_t *)binary;
    info->total_memory = hdr->code_size + hdr->bss_size;

    /* Copy name (ensure null-terminated) */
    for (int i = 0; i < 23; i++) {
        info->name[i] = hdr->name[i];
    }
    info->name[23] = '\0';

    return MEX_OK;
}

/* ═══════════════════════════════════════════════════════════════════
 * libmex_exec
 * ═══════════════════════════════════════════════════════════════════ */

int libmex_exec(const mex_info_t *info) {
    if (!info || !info->binary_data) {
        return MEX_ERR_NULL;
    }

    /*
     * Route through Process Executive for validation and security.
     * The executive checks memory pressure, size limits, and base address
     * before calling the kernel to create the process.
     */
    int pid = libprocess_exec(
        info->base_address,
        info->binary_data,
        info->code_size,
        info->entry_offset
    );

    if (pid < 0) {
        return MEX_ERR_EXEC_FAIL;
    }

    return pid;
}

/* ═══════════════════════════════════════════════════════════════════
 * libmex_error_string
 * ═══════════════════════════════════════════════════════════════════ */

const char *libmex_error_string(int error) {
    switch (error) {
        case MEX_OK:              return "Success";
        case MEX_ERR_NULL:        return "NULL pointer";
        case MEX_ERR_TOO_SMALL:   return "File too small for MEX header";
        case MEX_ERR_BAD_MAGIC:   return "Invalid MEX magic (not a .mex file)";
        case MEX_ERR_BAD_VERSION: return "Unsupported MEX version";
        case MEX_ERR_BAD_TYPE:    return "Invalid application type";
        case MEX_ERR_BAD_SIZE:    return "Code size mismatch with file size";
        case MEX_ERR_BAD_CRC:     return "CRC32 checksum mismatch (corrupt file)";
        case MEX_ERR_EXEC_FAIL:   return "Kernel refused to create process";
        default:                  return "Unknown error";
    }
}
