/**
 * MaahiOS Syscall Numbers
 * Single source of truth for all syscall numbers
 * 
 * Convention:
 *   - Numbers grouped by domain (16 syscalls per domain = room to grow)
 *   - Handlers: sys_<name>() in managers/syscall/handlers/
 *   - Manager APIs: kernel_<name>() in respective managers
 * 
 * ONLY add syscalls for managers that EXIST!
 */

#ifndef SYSCALL_NUMBERS_H
#define SYSCALL_NUMBERS_H

/* ═══════════════════════════════════════════════════════════════════════════
 * ERROR CODES
 * ═══════════════════════════════════════════════════════════════════════════ */
#define SYSCALL_OK              0
#define SYSCALL_ERR_INVALID    -1   /* Invalid argument */
#define SYSCALL_ERR_NOSYS      -2   /* Syscall not implemented */
#define SYSCALL_ERR_PERM       -3   /* Permission denied */
#define SYSCALL_ERR_NOMEM      -4   /* Out of memory */
#define SYSCALL_ERR_NOTFOUND   -5   /* Resource not found */
#define SYSCALL_ERR_BUSY       -6   /* Resource busy */
#define SYSCALL_ERR_IO         -7   /* I/O error */

/* ═══════════════════════════════════════════════════════════════════════════
 * DOMAIN 0-15: CORE (Process lifecycle, scheduling)
 * Manager: Scheduler, Process Manager
 * ═══════════════════════════════════════════════════════════════════════════ */
#define SYS_EXIT                0   /* exit(code) - Terminate process */
#define SYS_YIELD               1   /* yield() - Yield CPU to scheduler */
#define SYS_GETPID              2   /* getpid() - Get current process ID */
#define SYS_SLEEP               3   /* sleep(ticks) - Sleep for N ticks */

/* ═══════════════════════════════════════════════════════════════════════════
 * DOMAIN 16-31: PROCESS management
 * Manager: Process Manager
 * ═══════════════════════════════════════════════════════════════════════════ */
#define SYS_PROCESS_CREATE      16  /* process_create(entry) - Create process */
#define SYS_PROCESS_KILL        17  /* process_kill(pid) - Terminate process */
#define SYS_PROCESS_INFO        19  /* process_info(pid, info) - Get process info */

/* ═══════════════════════════════════════════════════════════════════════════
 * DOMAIN 32-47: MEMORY management
 * Manager: PMM, Paging Manager
 * ═══════════════════════════════════════════════════════════════════════════ */
#define SYS_MEM_ALLOC_PAGE      32  /* alloc_page() - Allocate single 4KB page */
#define SYS_MEM_FREE_PAGE       33  /* free_page(addr) - Free allocated page */
#define SYS_MEM_ALLOC           34  /* alloc_memory(size) - Allocate memory block */
#define SYS_MEM_ATOMIC_COPY     35  /* atomic_copy(dst, src, size) - Atomic memcpy */

/* ═══════════════════════════════════════════════════════════════════════════
 * DOMAIN 48-63: SHM (Shared Memory)
 * Manager: SHM Manager
 * ═══════════════════════════════════════════════════════════════════════════ */
#define SYS_SHM_CREATE          48  /* shm_create(size) - Create SHM region */
#define SYS_SHM_ATTACH          49  /* shm_attach(id, addr) - Attach to SHM */
#define SYS_SHM_DETACH          50  /* shm_detach(id) - Detach from SHM */
#define SYS_SHM_DESTROY         51  /* shm_destroy(id) - Destroy SHM region */
#define SYS_SHM_INFO            52  /* shm_info(id, info) - Get SHM info */

/* ═══════════════════════════════════════════════════════════════════════════
 * DOMAIN 64-79: CELL (Key-Value Store)
 * Manager: Cell Manager
 * ═══════════════════════════════════════════════════════════════════════════ */
#define SYS_CELL_WRITE          64  /* cell_write(key, val, size) - Write cell */
#define SYS_CELL_READ           65  /* cell_read(key, buf, size) - Read cell */
#define SYS_CELL_DELETE         66  /* cell_delete(key) - Delete cell */
#define SYS_CELL_EXISTS         67  /* cell_exists(key) - Check if cell exists */
#define SYS_CELL_GET_SHM_ID     68  /* cell_get_shm_id() - Get Cell manager SHM ID */
#define SYS_CELL_LIST           69  /* cell_list(prefix, keys, max) - List cells */

/* ═══════════════════════════════════════════════════════════════════════════
 * DOMAIN 80-95: DEVICE I/O (Unified hardware access)
 * Manager: Device Manager
 * ═══════════════════════════════════════════════════════════════════════════ */
#define SYS_DEV_OPEN            80  /* dev_open(id, flags) - Open device */
#define SYS_DEV_CLOSE           81  /* dev_close(id, handle) - Close device */
#define SYS_DEV_READ            82  /* dev_read(id, buf, size) - Read from device */
#define SYS_DEV_WRITE           83  /* dev_write(id, buf, size) - Write to device */
#define SYS_DEV_IOCTL           84  /* dev_ioctl(id, cmd, arg) - Device control */
#define SYS_DEV_POLL            85  /* dev_poll(id) - Poll device readiness */
#define SYS_DEV_LIST            86  /* dev_list(list, max) - List devices */

/* ═══════════════════════════════════════════════════════════════════════════
 * DOMAIN 96-111: GRUB MODULE (Boot module management)
 * Manager: GRUB Module Manager
 * ═══════════════════════════════════════════════════════════════════════════ */
#define SYS_MOD_GET_COUNT       96  /* mod_get_count() - Get module count */
#define SYS_MOD_GET_INFO        97  /* mod_get_info(idx, info) - Get module info */
#define SYS_MOD_GET_ADDR        98  /* mod_get_addr(idx) - Get module address */
#define SYS_MOD_GET_SIZE        99  /* mod_get_size(idx) - Get module size */
#define SYS_MOD_COPY            100 /* mod_copy(idx, target) - Copy module to addr */

/* ═══════════════════════════════════════════════════════════════════════════
 * DOMAIN 112-127: TIME (System time and uptime)
 * Manager: Time Manager
 * ═══════════════════════════════════════════════════════════════════════════ */
#define SYS_TIME_GET_DATETIME   112 /* time_get_datetime(dt) - Get date/time struct */
#define SYS_TIME_GET_UNIX       113 /* time_get_unix() - Get Unix timestamp */
#define SYS_TIME_GET_UPTIME     114 /* time_get_uptime(up) - Get uptime struct */
#define SYS_TIME_GET_TICKS      115 /* time_get_ticks() - Get raw tick count */

/* ═══════════════════════════════════════════════════════════════════════════
 * DOMAIN 240-255: DEBUG/KLOG
 * Manager: Klog Manager
 * ═══════════════════════════════════════════════════════════════════════════ */
#define SYS_KLOG                240 /* klog(level, tag, msg) - Log message [K] */
#define SYS_KLOG_HEX            241 /* klog_hex(level, tag, msg, val) - Log with hex [K] */
#define SYS_KLOG_GET_SHM        242 /* klog_get_shm() - Get KLOG SHM ID (deprecated) */
#define SYS_GET_CPU_INFO        243 /* get_cpu_info() - Get CPU information */
#define SYS_GET_MEM_INFO        244 /* get_mem_info() - Get memory information */
#define SYS_GET_PIC_MASK        245 /* get_pic_mask() - Get PIC interrupt mask */
#define SYS_ULOG                246 /* ulog(level, tag, msg) - User log message [U] */
#define SYS_ULOG_HEX            247 /* ulog_hex(level, tag, msg, val) - User log with hex [U] */

/* Maximum syscall number */
#define SYSCALL_MAX             255

#endif /* SYSCALL_NUMBERS_H */
