/**
 * MaahiOS Syscall Numbers - User-Space Copy
 * 
 * Mirror of managers/syscall/syscall_numbers.h for user-space code.
 * If you add a new syscall, update BOTH files.
 */

#ifndef USER_SYSCALL_NUMBERS_H
#define USER_SYSCALL_NUMBERS_H

/* ═══════════════════════════════════════════════════════════════════
 * DOMAIN 0-15: CORE
 * ═══════════════════════════════════════════════════════════════════ */
#define SYS_EXIT                0
#define SYS_YIELD               1
#define SYS_GETPID              2
#define SYS_SLEEP               3
#define SYS_SHUTDOWN            4
#define SYS_RESTART             5

/* ═══════════════════════════════════════════════════════════════════
 * DOMAIN 16-31: PROCESS
 * ═══════════════════════════════════════════════════════════════════ */
#define SYS_PROCESS_CREATE      16
#define SYS_PROCESS_KILL        17
#define SYS_PROCESS_INFO        18
#define SYS_PROCESS_GET_COUNT   19
#define SYS_PROCESS_EXEC        20
#define SYS_PROCESS_LIST        21
#define SYS_PROCESS_SET_NAME    22

/* ═══════════════════════════════════════════════════════════════════
 * DOMAIN 32-47: MEMORY
 * ═══════════════════════════════════════════════════════════════════ */
#define SYS_MEM_ALLOC_PAGE      32
#define SYS_MEM_FREE_PAGE       33
#define SYS_MEM_ALLOC           34
#define SYS_MEM_ATOMIC_COPY     35

/* ═══════════════════════════════════════════════════════════════════
 * DOMAIN 48-63: SHARED MEMORY
 * ═══════════════════════════════════════════════════════════════════ */
#define SYS_SHM_CREATE          48
#define SYS_SHM_ATTACH          49
#define SYS_SHM_DETACH          50
#define SYS_SHM_DESTROY         51
#define SYS_SHM_INFO            52

/* ═══════════════════════════════════════════════════════════════════
 * DOMAIN 64-79: CELL (Key-Value Store)
 * ═══════════════════════════════════════════════════════════════════ */
#define SYS_CELL_WRITE          64
#define SYS_CELL_READ           65
#define SYS_CELL_DELETE         66
#define SYS_CELL_EXISTS         67
#define SYS_CELL_GET_SHM_ID     68
#define SYS_CELL_LIST           69

/* ═══════════════════════════════════════════════════════════════════
 * DOMAIN 80-95: DEVICE I/O
 * ═══════════════════════════════════════════════════════════════════ */
#define SYS_DEV_OPEN            80
#define SYS_DEV_CLOSE           81
#define SYS_DEV_READ            82
#define SYS_DEV_WRITE           83
#define SYS_DEV_IOCTL           84
#define SYS_DEV_POLL            85
#define SYS_DEV_LIST            86
#define SYS_DISK_FORMAT         87

/* ═══════════════════════════════════════════════════════════════════
 * DOMAIN 96-111: GRUB MODULE
 * ═══════════════════════════════════════════════════════════════════ */
#define SYS_MOD_GET_COUNT       96
#define SYS_MOD_GET_INFO        97
#define SYS_MOD_GET_ADDR        98
#define SYS_MOD_GET_SIZE        99
#define SYS_MOD_COPY            100

/* ═══════════════════════════════════════════════════════════════════
 * DOMAIN 112-127: TIME
 * ═══════════════════════════════════════════════════════════════════ */
#define SYS_TIME_GET_DATETIME   112
#define SYS_TIME_GET_UNIX       113
#define SYS_TIME_GET_UPTIME     114
#define SYS_TIME_GET_TICKS      115
#define SYS_TIME_GET_TICK_FREQ  116
#define SYS_TIME_GET_SHM_ID     117

/* ═══════════════════════════════════════════════════════════════════
 * DOMAIN 128-143: FILESYSTEM
 * ═══════════════════════════════════════════════════════════════════ */
#define SYS_FS_LIST_DIR         128
#define SYS_FS_READ_FILE        129
#define SYS_FS_FILE_COUNT       130
#define SYS_FS_FIND_DIR         131
#define SYS_FS_GET_ROOT_INFO    132
#define SYS_FS_WRITE_FILE       133
#define SYS_FS_DELETE_FILE      134
#define SYS_FS_CREATE_DIR       135
#define SYS_FS_VOL_COUNT        136
#define SYS_FS_VOL_INFO         137

/* ═══════════════════════════════════════════════════════════════════
 * DOMAIN 240-255: DEBUG/KLOG
 * ═══════════════════════════════════════════════════════════════════ */
#define SYS_KLOG                240
#define SYS_KLOG_HEX            241
#define SYS_KLOG_GET_SHM        242
#define SYS_GET_CPU_INFO        243
#define SYS_GET_MEM_INFO        244
#define SYS_GET_PIC_MASK        245

#define SYSCALL_MAX             255

#endif /* USER_SYSCALL_NUMBERS_H */
