/**
 * Orbit - MaahiOS Desktop Shell
 * 
 * Description:
 *   Desktop launcher (PID 7). Draws a solid desktop background,
 *   then spawns Terminal as a child process using libprocess.
 *   Reads the terminal binary address from a cell published by sysman.
 * 
 *   Later: taskbar, icons, window management via UIManager.
 *   For now: just background + launch terminal.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include <stdint.h>
#include "../../libraries/core/syscall_helpers.h"
#include "../../libraries/liblog/liblog.h"
#include "../../libraries/libcell/libcell.h"
#include "../../libraries/libprocess/libprocess.h"
#include "../../libraries/libgui/libgui.h"

/* Desktop background color: dark blue-gray (#2D3436) */
#define DESKTOP_BG_COLOR    0x002D3436

/*=============================================================================
 * CONVENIENCE
 *===========================================================================*/

static void yield(void) {
    syscall0(SYS_YIELD);
}

static void sleep_ticks(int ticks) {
    syscall1(SYS_SLEEP, ticks);
}

/*=============================================================================
 * TERMINAL LAUNCH
 *===========================================================================*/

static int orbit_launch_terminal(void) {
    /* Read terminal module index from cell (published by sysman).
     * No load address needed — Process Executive will create the process
     * with a per-process page directory at the standard virtual base. */
    uint32_t term_module = 0;
    
    int r1 = libcell_read("system.app.terminal.module",
                          &term_module, sizeof(uint32_t));
    
    if (r1 < 0 || term_module == 0) {
        liblog(LOG_ERROR, "ORBIT", "Terminal cell data not found");
        liblog_hex(LOG_ERROR, "ORBIT", "  module read result:", (uint32_t)r1);
        return -1;
    }
    
    liblog_hex(LOG_INFO, "ORBIT", "Terminal module index:", term_module);
    
    /* Create terminal process via Process Executive.
     * libprocess_create now uses SYS_PROCESS_EXEC path which gives
     * the new process its own page directory. Load address is not used
     * by the kernel — the binary is always mapped at the linker script base. */
    int term_pid = libprocess_create(term_module, 0);
    if (term_pid < 0) {
        liblog(LOG_ERROR, "ORBIT", "Failed to create Terminal process");
        liblog_hex(LOG_ERROR, "ORBIT", "Error code:", (uint32_t)term_pid);
        return -1;
    }
    
    liblog_hex(LOG_INFO, "ORBIT", "Terminal launched, PID:", (uint32_t)term_pid);
    return term_pid;
}

/*=============================================================================
 * MAIN ENTRY POINT
 *===========================================================================*/

void orbit_main_c(void) {
    liblog(LOG_INFO, "ORBIT", "========================================");
    liblog(LOG_INFO, "ORBIT", "  Orbit Desktop Shell Starting");
    liblog(LOG_INFO, "ORBIT", "========================================");
    
    /* Initialize display via GUI library (reads cells from GUI Executive) */
    if (gui_init() != 0) {
        liblog(LOG_ERROR, "ORBIT", "GUI init failed, halting");
        while (1) yield();
    }
    
    /* Draw desktop background */
    gui_fill_screen(DESKTOP_BG_COLOR);
    liblog(LOG_INFO, "ORBIT", "Desktop background drawn");
    
    /* Give executives a moment to finish init */
    sleep_ticks(5);
    
    /* Launch terminal */
    int term_pid = orbit_launch_terminal();
    if (term_pid < 0) {
        liblog(LOG_WARN, "ORBIT", "Terminal launch failed, desktop only");
    }
    
    liblog(LOG_INFO, "ORBIT", "Orbit running. Entering idle loop.");
    
    /* Idle loop — later this will handle taskbar, icon clicks, etc. */
    while (1) {
        yield();
    }
}
