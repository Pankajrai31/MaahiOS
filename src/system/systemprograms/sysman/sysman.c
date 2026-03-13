/**
 * MaahiOS System Manager (sysman)
 * 
 * First ring-3 process (PID 1).
 * Loads executives and manages system lifecycle.
 * 
 * Boot sequence:
 *   1. Start Log Executive (PID 2) — handles all user-space logging
 *   2. Start Cell Executive (PID 3) — manages cell registry
 *   3. Start GUI Executive — manages display/framebuffer access
 *   4. Start I/O Executive — manages device input (keyboard, future: mouse)
 *   5. Start Process Executive (PID 4) — manages process lifecycle
 *   6. Start Memory Executive (PID 5) — manages heap & SHM
 *   7. Start Disk Executive (PID 7) — manages block-level disk access
 *   8. Start Filesystem Executive — manages file I/O (ISO9660 + future MFS)
 *   9. Prepare & launch Orbit desktop shell
 *      - Publish terminal module index cell
 *      - Launch Orbit (which launches Terminal itself)
 *  10. Idle
 * 
 * All executives are loaded with per-process page directories via SYS_PROCESS_EXEC.
 * Each process gets its own address space mapped at 0x10000000.
 * Libraries (liblog, libcell, libprocess, libmemory) auto-initialize on first use.
 * No explicit init calls needed.
 */

#include <stdint.h>
#include "../../libraries/core/syscall_helpers.h"
#include "../../libraries/liblog/liblog.h"
#include "../../libraries/libcell/libcell.h"
#include "../../libraries/libprocess/libprocess.h"
#include "../../libraries/libmemory/libmemory.h"
#include "../../libraries/shared/taskbar_types.h"
#include "module_bss_sizes.h"  /* Auto-generated: MOD_BSS_LOGEXEC, MOD_BSS_ORBIT, etc. */

/* All user processes are linked at this virtual address.
 * Per-process page directories provide isolated address spaces. */
#define PROCESS_VIRTUAL_BASE    0x10000000

/* GRUB module indices (must match grub.cfg order) */
#define GRUB_MOD_SYSMAN         0
#define GRUB_MOD_LOGEXEC        1
#define GRUB_MOD_CELLEXEC       2
#define GRUB_MOD_PROCEXEC       3
#define GRUB_MOD_MEMEXEC        4
#define GRUB_MOD_DISKEXEC       5
#define GRUB_MOD_FSEXEC         6
#define GRUB_MOD_GUIEXEC        7
#define GRUB_MOD_IOEXEC         8
#define GRUB_MOD_WMEXEC         9
#define GRUB_MOD_ORBIT          10
#define GRUB_MOD_TERMINAL       11

/*=============================================================================
 * Convenience wrappers (thin layer over syscall_helpers)
 *===========================================================================*/

static void yield(void) {
    syscall0(SYS_YIELD);
}

static void sleep_ticks(int ticks) {
    syscall1(SYS_SLEEP, ticks);
}

/**
 * Create a process from a GRUB module with per-process page directory.
 * Uses SYS_PROCESS_EXEC (which calls process_create_from_memory in kernel).
 * Each process gets its own page directory with binary mapped at PROCESS_VIRTUAL_BASE.
 * bss_size is passed so the kernel allocates the correct amount of memory.
 */
static int create_process_from_module(int module_index, const char *name, uint32_t bss_size) {
    uint32_t mod_addr = (uint32_t)syscall1(SYS_MOD_GET_ADDR, module_index);
    if (mod_addr == 0) {
        liblog(LOG_ERROR, "SYSMAN", "Module not found!");
        liblog_hex(LOG_ERROR, "SYSMAN", "Module index:", (uint32_t)module_index);
        return -1;
    }
    
    uint32_t mod_size = (uint32_t)syscall1(SYS_MOD_GET_SIZE, module_index);
    if (mod_size == 0) {
        liblog(LOG_ERROR, "SYSMAN", "Module has zero size!");
        return -1;
    }
    
    liblog_hex(LOG_INFO, "SYSMAN", "Module GRUB addr:", mod_addr);
    liblog_hex(LOG_INFO, "SYSMAN", "Module size:", mod_size);
    liblog_hex(LOG_INFO, "SYSMAN", "Module BSS:", bss_size);
    
    int pid = syscall5(SYS_PROCESS_EXEC, PROCESS_VIRTUAL_BASE,
                       (int)mod_addr, (int)mod_size, 0, (int)bss_size);
    if (pid < 0) {
        liblog(LOG_ERROR, "SYSMAN", "Failed to create process!");
        return -1;
    }
    
    /* Set process name and type (system process) */
    syscall3(SYS_PROCESS_SET_NAME, pid, (int)name, 0 /* PROC_TYPE_SYSTEM */);
    
    liblog_hex(LOG_INFO, "SYSMAN", "Process started, PID:", (uint32_t)pid);
    return pid;
}

/*=============================================================================
 * Main Entry Point
 *===========================================================================*/

void sysman_main_c(void) {
    /* Early log via liblog — will fallback to direct klog since
     * Log Executive isn't running yet. That's fine. */
    liblog(LOG_INFO, "SYSMAN", "========================================");
    liblog(LOG_INFO, "SYSMAN", "  Welcome to Sysman (PID 1)");
    liblog(LOG_INFO, "SYSMAN", "========================================");
    
    /* Name ourselves */
    syscall3(SYS_PROCESS_SET_NAME, 1, (int)"Sysman", 0 /* PROC_TYPE_SYSTEM */);
    
    /*=========================================================================
     * STEP 1: Start Log Executive
     *=======================================================================*/
    liblog(LOG_INFO, "SYSMAN", "Starting Log Executive...");
    
    int log_pid = create_process_from_module(GRUB_MOD_LOGEXEC, "LogExec", MOD_BSS_LOGEXEC);
    if (log_pid < 0) {
        liblog(LOG_ERROR, "SYSMAN", "Failed to start Log Executive!");
        while (1) yield();
    }
    
    /* Give Log Executive a moment to initialize its SHM queues */
    sleep_ticks(5);
    
    /*=========================================================================
     * STEP 2: Start Cell Executive
     *=======================================================================*/
    liblog(LOG_INFO, "SYSMAN", "Starting Cell Executive...");
    
    int cell_pid = create_process_from_module(GRUB_MOD_CELLEXEC, "CellExec", MOD_BSS_CELLEXEC);
    if (cell_pid < 0) {
        liblog(LOG_ERROR, "SYSMAN", "Failed to start Cell Executive!");
        while (1) yield();
    }
    
    /* Give Cell Executive a moment to initialize */
    sleep_ticks(5);
    
    /*=========================================================================
     * STEP 3: Start GUI Executive
     *   - No executive dependencies (uses kernel SYS_SHM_*, SYS_CELL_WRITE,
     *     SYS_DEV_* syscalls directly; libcell falls back to kernel syscall)
     *   - Started early so framebuffer cells are published well before
     *     Orbit/Terminal need them
     *=======================================================================*/
    liblog(LOG_INFO, "SYSMAN", "Starting GUI Executive...");
    
    int gui_pid = create_process_from_module(GRUB_MOD_GUIEXEC, "GUIExec", MOD_BSS_GUIEXEC);
    if (gui_pid < 0) {
        liblog(LOG_ERROR, "SYSMAN", "Failed to start GUI Executive!");
        while (1) yield();
    }
    
    /* Give GUI Executive a moment to initialize and publish cells */
    sleep_ticks(5);
    
    /*=========================================================================
     * STEP 4: Start I/O Executive
     *   - No executive dependencies (uses kernel SYS_SHM_*, SYS_CELL_WRITE,
     *     SYS_DEV_* syscalls directly; libcell falls back to kernel syscall)
     *   - Started early so keyboard input is available before apps launch
     *=======================================================================*/
    liblog(LOG_INFO, "SYSMAN", "Starting I/O Executive...");
    
    int io_pid = create_process_from_module(GRUB_MOD_IOEXEC, "IOExec", MOD_BSS_IOEXEC);
    if (io_pid < 0) {
        liblog(LOG_ERROR, "SYSMAN", "Failed to start I/O Executive!");
        while (1) yield();
    }
    
    /* Give I/O Executive a moment to initialize and publish cells */
    sleep_ticks(5);
    
    /*=========================================================================
     * STEP 4b: Start WM Executive
     *   - Depends on: GUI Executive (display cells), Cell Executive
     *   - Creates SHM queues for window registration
     *   - Must be running before any windowed app starts
     *=======================================================================*/
    liblog(LOG_INFO, "SYSMAN", "Starting WM Executive...");
    
    int wm_pid = create_process_from_module(GRUB_MOD_WMEXEC, "WMExec", MOD_BSS_WMEXEC);
    if (wm_pid < 0) {
        liblog(LOG_ERROR, "SYSMAN", "Failed to start WM Executive!");
        while (1) yield();
    }
    
    /* Give WM Executive time to init display, create SHM queues,
     * and signal readiness via cell */
    sleep_ticks(5);
    
    /*=========================================================================
     * STEP 5: Start Process Executive
     *=======================================================================*/
    liblog(LOG_INFO, "SYSMAN", "Starting Process Executive...");
    
    int proc_pid = create_process_from_module(GRUB_MOD_PROCEXEC, "ProcExec", MOD_BSS_PROCEXEC);
    if (proc_pid < 0) {
        liblog(LOG_ERROR, "SYSMAN", "Failed to start Process Executive!");
        while (1) yield();
    }
    
    /* Give Process Executive a moment to initialize and register cells */
    sleep_ticks(5);
    
    /*=========================================================================
     * STEP 6: Start Memory Executive
     *=======================================================================*/
    liblog(LOG_INFO, "SYSMAN", "Starting Memory Executive...");
    
    int mem_pid = create_process_from_module(GRUB_MOD_MEMEXEC, "MemExec", MOD_BSS_MEMEXEC);
    if (mem_pid < 0) {
        liblog(LOG_ERROR, "SYSMAN", "Failed to start Memory Executive!");
        while (1) yield();
    }
    
    /* Give Memory Executive a moment to initialize and register cells */
    sleep_ticks(5);
    
    /*=========================================================================
     * STEP 7: Start Disk Executive
     *=======================================================================*/
    liblog(LOG_INFO, "SYSMAN", "Starting Disk Executive...");
    
    int disk_pid = create_process_from_module(GRUB_MOD_DISKEXEC, "DiskExec", MOD_BSS_DISKEXEC);
    if (disk_pid < 0) {
        liblog(LOG_ERROR, "SYSMAN", "Failed to start Disk Executive!");
        while (1) yield();
    }
    
    /* Give Disk Executive a moment to initialize and scan disks */
    sleep_ticks(5);
    
    /*=========================================================================
     * STEP 8: Start Filesystem Executive
     *=======================================================================*/
    liblog(LOG_INFO, "SYSMAN", "Starting Filesystem Executive...");
    
    int fs_pid = create_process_from_module(GRUB_MOD_FSEXEC, "FSExec", MOD_BSS_FSEXEC);
    if (fs_pid < 0) {
        liblog(LOG_ERROR, "SYSMAN", "Failed to start FS Executive!");
        while (1) yield();
    }
    
    /* Give FS Executive a moment to initialize */
    sleep_ticks(5);
    
    /*=========================================================================
     * STEP 9: Prepare & Launch Orbit Desktop Shell
     *       - Publish terminal module index so Orbit can discover it
     *       - Launch Orbit with per-process page directory
     *       - Orbit will spawn Terminal itself via Process Executive
     *=======================================================================*/
    liblog(LOG_INFO, "SYSMAN", "Preparing Orbit + Terminal...");
    
    /* Publish terminal module index so Orbit can discover it via cells.
     * No need to pre-copy terminal — Orbit will use libprocess_create
     * which now goes through Process Executive → SYS_PROCESS_EXEC. */
    uint32_t term_mod_val = GRUB_MOD_TERMINAL;
    
    libcell_write("system.app.terminal.module",
                  &term_mod_val, sizeof(uint32_t));
    liblog(LOG_INFO, "SYSMAN", "Terminal module cell published");

    /* Publish desktop app shortcuts for Orbit to render as clickable icons */
    {
        desktop_app_list_t apps;
        int i;
        for (i = 0; i < (int)sizeof(apps); i++) ((uint8_t *)&apps)[i] = 0;

        apps.count = 2;

        /* Terminal — GRUB module */
        {
            const char *n = "Terminal";
            int j;
            for (j = 0; n[j] && j < DESKTOP_APP_NAME_MAX - 1; j++)
                apps.entries[0].name[j] = n[j];
            apps.entries[0].name[j] = '\0';
        }
        {
            const char *ic = "TERMINAL.BMP";
            int j;
            for (j = 0; ic[j] && j < DESKTOP_ICON_NAME_MAX - 1; j++)
                apps.entries[0].icon_file[j] = ic[j];
            apps.entries[0].icon_file[j] = '\0';
        }
        apps.entries[0].app_type   = DESKTOP_APP_TYPE_MODULE;
        apps.entries[0].module_idx = GRUB_MOD_TERMINAL;
        apps.entries[0].is_gui     = 1;
        apps.entries[0].bss_size   = MOD_BSS_TERMINAL;

        /* HelloGUI — MEX file on filesystem */
        {
            const char *n = "HelloGUI";
            int j;
            for (j = 0; n[j] && j < DESKTOP_APP_NAME_MAX - 1; j++)
                apps.entries[1].name[j] = n[j];
            apps.entries[1].name[j] = '\0';
        }
        {
            const char *c = "HELLOGUI.MEX";
            int j;
            for (j = 0; c[j] && j < DESKTOP_APP_CMD_MAX - 1; j++)
                apps.entries[1].command[j] = c[j];
            apps.entries[1].command[j] = '\0';
        }
        {
            const char *ic = "HELLOGUI.BMP";
            int j;
            for (j = 0; ic[j] && j < DESKTOP_ICON_NAME_MAX - 1; j++)
                apps.entries[1].icon_file[j] = ic[j];
            apps.entries[1].icon_file[j] = '\0';
        }
        apps.entries[1].app_type   = DESKTOP_APP_TYPE_MEX;
        apps.entries[1].module_idx = 0;
        apps.entries[1].is_gui     = 1;

        libcell_write(CELL_DESKTOP_APPS, &apps,
                     (uint32_t)(sizeof(int32_t) +
                      apps.count * sizeof(desktop_app_entry_t)));
        liblog(LOG_INFO, "SYSMAN", "Desktop app shortcuts cell published");
    }

    /* Initialize empty taskbar window list and restore cells */
    {
        taskbar_window_list_t empty_list;
        int i;
        for (i = 0; i < (int)sizeof(empty_list); i++) ((uint8_t *)&empty_list)[i] = 0;
        empty_list.count = 0;
        libcell_write(CELL_TASKBAR_WINDOWS, &empty_list, sizeof(int32_t));

        taskbar_restore_t restore;
        restore.pid = 0;
        libcell_write(CELL_TASKBAR_RESTORE, &restore, sizeof(restore));
    }
    
    /* Launch Orbit with per-process page directory */
    int orbit_pid = create_process_from_module(GRUB_MOD_ORBIT, "Orbit", MOD_BSS_ORBIT);
    if (orbit_pid < 0) {
        liblog(LOG_ERROR, "SYSMAN", "Failed to start Orbit!");
        while (1) yield();
    }
    liblog_hex(LOG_INFO, "SYSMAN", "Orbit started, PID:", (uint32_t)orbit_pid);
    
    /*=========================================================================
     * STEP 10: All loaded — report and idle
     *=======================================================================*/
    int total = libprocess_get_count();
    liblog(LOG_INFO, "SYSMAN", "========================================");
    liblog_hex(LOG_INFO, "SYSMAN", "  All loaded. Process count:", (uint32_t)total);
    liblog(LOG_INFO, "SYSMAN", "  Executives + Orbit running");
    liblog(LOG_INFO, "SYSMAN", "========================================");
    
    /* Idle forever */
    while (1) {
        yield();
    }
}
