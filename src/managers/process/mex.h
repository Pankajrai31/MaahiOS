/**
 * MaahiOS Executable (.mex) Format Header
 * 
 * Defines the 64-byte header structure for .mex files.
 * A .mex file is a flat binary with this header prepended.
 * 
 * File layout:
 *   [0x00 - 0x3F]  MEX Header (64 bytes)
 *   [0x40 - EOF]   Flat binary (code + data)
 * 
 * Used by:
 *   - tools/mex_pack.py (host-side, reads this for field offsets)
 *   - kernel .mex loader (parses header at load time)
 */

#ifndef MEX_H
#define MEX_H

#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════ */

#define MEX_MAGIC           0x0058454D  /* "MEX\0" in little-endian */
#define MEX_VERSION_1_0     0x0100      /* Version 1.0 */
#define MEX_HEADER_SIZE     64          /* Fixed header size in bytes */
#define MEX_APP_BASE        0x10000000  /* Standard virtual load address for all .mex apps */
#define MEX_DEFAULT_STACK   0x4000      /* Default stack size: 16KB */

/* Application types */
#define MEX_TYPE_APP        1   /* User application (console or GUI) */
#define MEX_TYPE_SERVICE    2   /* Background service */
#define MEX_TYPE_DRIVER     3   /* User-mode driver */

/* Flags */
#define MEX_FLAG_GUI        0x01    /* Has GUI (creates window) */
#define MEX_FLAG_CONSOLE    0x02    /* Console application (uses terminal) */

/* ═══════════════════════════════════════════════════════════════════
 * MEX Header Structure (64 bytes, packed)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct __attribute__((packed)) {
    uint32_t magic;         /* 0x00: "MEX\0" (0x0058454D) */
    uint16_t version;       /* 0x04: Format version (0x0100 = v1.0) */
    uint16_t type;          /* 0x06: MEX_TYPE_APP, MEX_TYPE_SERVICE, MEX_TYPE_DRIVER */
    uint32_t entry_offset;  /* 0x08: Entry point offset from base (usually 0) */
    uint32_t code_size;     /* 0x0C: Size of code+data in file (after header) */
    uint32_t bss_size;      /* 0x10: Additional zero-initialized memory needed */
    uint32_t stack_size;    /* 0x14: Requested stack size (0 = use default 16KB) */
    uint32_t base_address;  /* 0x18: Virtual load address (0x10000000) */
    uint32_t flags;         /* 0x1C: MEX_FLAG_GUI, MEX_FLAG_CONSOLE */
    char     name[24];      /* 0x20: Null-terminated application name */
    uint32_t checksum;      /* 0x38: CRC32 of the binary portion (after header) */
    uint32_t reserved;      /* 0x3C: Reserved for future use */
} mex_header_t;

/* Compile-time check: header must be exactly 64 bytes */
_Static_assert(sizeof(mex_header_t) == MEX_HEADER_SIZE, "mex_header_t must be 64 bytes");

#endif /* MEX_H */
