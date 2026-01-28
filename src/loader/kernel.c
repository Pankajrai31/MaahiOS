/*=============================================================================
 * MaahiOS Kernel - Main Entry Point
 * 
 * Description:
 *   Bootstraps the operating system from multiboot handoff to userspace.
 *   Initializes core subsystems (GDT, IDT, PMM, paging, heap, processes)
 *   and displays a graphical loading screen during boot.
 * 
 * Boot Sequence:
 *   1. Multiboot validation & hardware detection
 *   2. Memory management (PMM, paging, framebuffer mapping)
 *   3. CPU setup (GDT, IDT, IRQ remapping)
 *   4. Graphics initialization & loading screen
 *   5. Kernel services (heap, klog, process manager, window manager)
 *   6. Device drivers (keyboard, mouse, disk)
 *   7. Ring 3 transition (sysman, uimanager, orbit)
 * 
 * Author: MaahiOS Team
 * Date: January 2026
 *===========================================================================*/

#include <stdint.h>
#include "kernel.h"
#include "managers/gui/font/font_manager.h"
#include "managers/gui/windows/windows_mgmt.h"
#include "drivers/disk/disk_subsystem.h"

/*=============================================================================
 * Hardware I/O Helpers
 *===========================================================================*/

static inline void outb(unsigned short port, unsigned char val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*=============================================================================
 * Serial Port Logging (for debugging when graphics fails)
 *===========================================================================*/

void serial_print(const char *str) {
    while (*str) {
        while ((inb(0x3FD) & 0x20) == 0);  /* Wait for transmit buffer empty */
        outb(0x3F8, *str++);
    }
}

void serial_hex(unsigned char value) {
    char hex[] = "0123456789ABCDEF";
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, hex[(value >> 4) & 0xF]);
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, hex[value & 0xF]);
}

/*=============================================================================
 * Global Variables
 *===========================================================================*/

/* Module addresses from multiboot */
unsigned int sysman_entry_point = 0;
unsigned int uimanager_module_address = 0;
unsigned int orbit_module_address = 0;
unsigned int file_manager_module_address = 0;
unsigned int file_manager_module_size = 0;
unsigned int disk_manager_module_address = 0;
unsigned int disk_manager_module_size = 0;

/* Loading screen state */
static int progress_step = 0;
static const int TOTAL_STEPS = 16;

/*=============================================================================
 * Loading Screen - Progress Bar & Status Messages
 *===========================================================================*/

/**
 * update_loading_screen - Update progress bar and status message
 * @message: User-friendly status message (e.g., "Memory Manager Initialized")
 * 
 * Draws a progress bar at the bottom of the loading box and updates the
 * status text. Call this after each major init step.
 */
static void update_loading_screen(const char *message) {
    progress_step++;
    
    /* Calculate progress bar geometry */
    int box_x = (1024 - 500) / 2;  /* 262 */
    int box_y = (768 - 250) / 2;   /* 259 */
    int bar_x = box_x + 50;
    int bar_y = box_y + 200;
    int bar_width = 400;
    int bar_height = 20;
    
    /* Draw progress bar background (dark gray) */
    gfx_fill_rect(bar_x, bar_y, bar_width, bar_height, 0x333333);
    
    /* Draw progress bar fill (gradient blue) */
    int fill_width = (bar_width * progress_step) / TOTAL_STEPS;
    gfx_fill_rect(bar_x, bar_y, fill_width, bar_height, 0x0099FF);
    
    /* Clear previous message area (draw black rectangle) */
    gfx_fill_rect(box_x + 50, box_y + 160, 400, 30, 0x001040);
    
    /* Draw new status message (centered, white, 12px Segoe UI) */
    font_draw_string(box_x + 60, box_y + 168, message, 0xFFFFFF, 12);
}

/*=============================================================================
 * Application Launchers
 *===========================================================================*/

/**
 * launch_file_manager - Launch file_manager.bin as Ring 3 process
 * 
 * Copies the binary to its linked address (0x00400000) and creates
 * a new process. Window management is handled by Orbit.
 */
int launch_file_manager(void) {
    serial_print("[KERNEL] Launching file_manager...\n");
    
    if (file_manager_module_address == 0 || file_manager_module_size == 0) {
        serial_print("[KERNEL] ERROR: file_manager module not loaded!\n");
        return -1;
    }
    
    /* Copy binary to linked address */
    serial_print("[KERNEL] Copying file_manager to 0x00400000...\n");
    uint8_t *src = (uint8_t *)file_manager_module_address;
    uint8_t *dst = (uint8_t *)0x00400000;
    for (uint32_t i = 0; i < file_manager_module_size; i++) {
        dst[i] = src[i];
    }
    
    /* Create process */
    int pid = process_create(0x00400000);
    if (pid < 0) {
        serial_print("[KERNEL] ERROR: Failed to create file_manager process!\n");
        return -1;
    }
    
    serial_print("[KERNEL] file_manager launched with PID: ");
    serial_hex(pid);
    serial_print("\n");
    return pid;
}

/**
 * launch_disk_manager - Launch disk_manager.bin as Ring 3 process
 * 
 * Copies the binary to its linked address (0x00500000) and creates
 * a new process. Window management is handled by Orbit.
 */
int launch_disk_manager(void) {
    serial_print("[KERNEL] Launching disk_manager...\n");
    
    if (disk_manager_module_address == 0 || disk_manager_module_size == 0) {
        serial_print("[KERNEL] ERROR: disk_manager module not loaded!\n");
        return -1;
    }
    
    /* Copy binary to linked address */
    serial_print("[KERNEL] Copying disk_manager to 0x00500000...\n");
    uint8_t *src = (uint8_t *)disk_manager_module_address;
    uint8_t *dst = (uint8_t *)0x00500000;
    for (uint32_t i = 0; i < disk_manager_module_size; i++) {
        dst[i] = src[i];
    }
    
    /* Create process */
    int pid = process_create(0x00500000);
    if (pid < 0) {
        serial_print("[KERNEL] ERROR: Failed to create disk_manager process!\n");
        return -1;
    }
    
    serial_print("[KERNEL] disk_manager launched with PID: ");
    serial_hex(pid);
    serial_print("\n");
    return pid;
}

/*=============================================================================
 * Kernel Main Entry Point
 *===========================================================================*/

/**
 * kernel_main - MaahiOS kernel entry point
 * @magic: Multiboot magic number (must be 0x2BADB002)
 * @mbi: Multiboot info structure from bootloader
 * 
 * This function is called by boot.s after the bootloader transfers control.
 * It initializes all kernel subsystems and transitions to Ring 3 userspace.
 */
void kernel_main(unsigned int magic, struct multiboot_info *mbi) {
    /*=========================================================================
     * STEP 1: Early Boot - VGA Text Mode Output
     *=======================================================================*/
    vga_print("MaahiOS - Starting boot sequence...\n");
    
    /* Validate multiboot magic */
    if (magic != 0x2BADB002) {
        vga_print("FATAL: Invalid multiboot magic!\n");
        while(1) __asm__ volatile("hlt");
    }
    
    /*=========================================================================
     * STEP 2: Hardware Detection
     *=======================================================================*/
    serial_print("[KERNEL] Checking for BGA hardware...\n");
    if (!bga_is_available()) {
        vga_print("FATAL: BGA hardware not found!\n");
        while(1) __asm__ volatile("hlt");
    }
    
    /*=========================================================================
     * STEP 3: Memory Management Initialization
     *=======================================================================*/
    uint32_t fb_addr = 0xFD000000;  /* BGA framebuffer address */
    uint32_t fb_size = 1024 * 768 * 4;
    
    /* Initialize Physical Memory Manager */
    serial_print("[KERNEL] Initializing PMM...\n");
    if (!pmm_init(mbi)) {
        vga_print("FATAL: PMM initialization failed!\n");
        while(1) __asm__ volatile("hlt");
    }
    
    /* Reserve framebuffer region in physical memory */
    pmm_mark_region_used(fb_addr, fb_addr + fb_size);
    
    /* Initialize paging (virtual memory) */
    serial_print("[KERNEL] Initializing paging...\n");
    if (!paging_init(mbi)) {
        vga_print("FATAL: Paging initialization failed!\n");
        while(1) __asm__ volatile("hlt");
    }
    
    /* Map framebuffer into virtual address space */
    identity_map_region(kernel_page_directory, fb_addr, fb_addr + fb_size);
    
    /*=========================================================================
     * STEP 4: CPU Setup - GDT & IDT
     *=======================================================================*/
    serial_print("[KERNEL] Initializing GDT...\n");
    if (!gdt_init() || !gdt_load()) {
        vga_print("FATAL: GDT initialization failed!\n");
        while(1) __asm__ volatile("hlt");
    }
    
    serial_print("[KERNEL] Initializing IDT...\n");
    if (!idt_init() || !idt_load()) {
        vga_print("FATAL: IDT initialization failed!\n");
        while(1) __asm__ volatile("hlt");
    }
    
    /* Remap PIC (IRQ 0-15 to INT 0x20-0x2F) */
    serial_print("[KERNEL] Remapping PIC...\n");
    irq_manager_init();
    
    /* Install exception handlers (divide by zero, page fault, etc.) */
    if (!idt_install_exception_handlers()) {
        vga_print("FATAL: Exception handler installation failed!\n");
        while(1) __asm__ volatile("hlt");
    }
    
    /*=========================================================================
     * STEP 5: Graphics Initialization & Loading Screen
     *=======================================================================*/
    serial_print("[KERNEL] Initializing BGA graphics...\n");
    if (!bga_init(1024, 768, 32)) {
        vga_print("FATAL: BGA init failed!\n");
        while(1) __asm__ volatile("hlt");
    }
    
    /* Initialize graphics abstraction layer */
    gfx_init(1024, 768, 32);
    
    /* Draw loading screen background */
    gfx_clear(0x001020);  /* Dark blue */
    
    /* Draw centered loading box (500x250) */
    int box_x = (1024 - 500) / 2;  /* 262 */
    int box_y = (768 - 250) / 2;   /* 259 */
    
    /* Draw multi-layer gradient border for depth effect */
    gfx_fill_rect(box_x - 8, box_y - 8, 516, 266, 0x0055AA);
    gfx_fill_rect(box_x - 6, box_y - 6, 512, 262, 0x0077CC);
    gfx_fill_rect(box_x - 4, box_y - 4, 508, 258, 0x0099EE);
    gfx_fill_rect(box_x - 2, box_y - 2, 504, 254, 0x00BBFF);
    gfx_fill_rect(box_x, box_y, 500, 250, 0x001040);  /* Dark center */
    
    /* Initialize font system (Segoe UI) */
    serial_print("[KERNEL] Initializing font manager...\n");
    font_init();
    
    /* Draw "MaahiOS" title (28px Segoe UI, white) */
    font_draw_string(box_x + 165, box_y + 50, "MaahiOS", 0xFFFFFF, 28);
    
    /* Draw subtitle (12px, light gray) */
    font_draw_string(box_x + 180, box_y + 95, "Version 0.1 Alpha", 0x999999, 12);
    
    update_loading_screen("Initializing Memory Manager");
    
    /*=========================================================================
     * STEP 6: Kernel Services Initialization
     *=======================================================================*/
    
    /* Initialize kernel heap */
    serial_print("[KERNEL] Initializing kernel heap...\n");
    kheap_init();
    update_loading_screen("Kernel Heap Initialized");
    
    /* Initialize kernel logger (silent buffering mode) */
    serial_print("[KERNEL] Initializing kernel logger...\n");
    klog_manager_init();
    update_loading_screen("Kernel Logger Initialized");
    
    /* Initialize process manager */
    serial_print("[KERNEL] Initializing process manager...\n");
    process_manager_init();
    update_loading_screen("Process Manager Initialized");
    
    /* Initialize window management system */
    serial_print("[KERNEL] Initializing window manager...\n");
    windows_mgmt_init();
    update_loading_screen("Window Manager Initialized");
    
    /* Initialize scheduler */
    serial_print("[KERNEL] Initializing scheduler...\n");
    scheduler_init();
    update_loading_screen("Scheduler Initialized");
    
    /* Initialize PIT timer (50Hz for responsive multitasking) */
    serial_print("[KERNEL] Initializing PIT timer...\n");
    pit_init(50);
    update_loading_screen("Timer Initialized");
    
    /*=========================================================================
     * STEP 7: Device Drivers Initialization
     *=======================================================================*/
    
    /* Initialize disk subsystem */
    serial_print("[KERNEL] Initializing disk subsystem...\n");
    disk_subsystem_init();
    update_loading_screen("Disk Subsystem Initialized");
    
    /* Initialize PS/2 mouse */
    serial_print("[KERNEL] Initializing mouse driver...\n");
    idt_install_mouse_handler();
    irq_enable_mouse();
    mouse_init();
    update_loading_screen("Mouse Driver Initialized");
    
    /* Initialize BGA hardware cursor */
    serial_print("[KERNEL] Initializing hardware cursor...\n");
    bga_cursor_init();
    if (bga_cursor_is_supported()) {
        bga_cursor_enable(1);
    }
    update_loading_screen("Hardware Cursor Initialized");
    
    /* Initialize PS/2 keyboard */
    serial_print("[KERNEL] Initializing keyboard driver...\n");
    keyboard_init();
    update_loading_screen("Keyboard Driver Initialized");
    
    /*=========================================================================
     * STEP 8: Load Ring 3 Modules
     *=======================================================================*/
    
    update_loading_screen("Loading System Modules");
    
    serial_print("[KERNEL] Module count: ");
    serial_hex(mbi->mods_count);
    serial_print("\n");
    
    if (mbi->mods_count < 3) {
        serial_print("[KERNEL] FATAL: Not enough modules loaded by bootloader!\n");
        font_draw_string(box_x + 100, box_y + 180, "ERROR: Missing system modules", 0xFF0000, 14);
        while(1) asm volatile("hlt");
    }
    
    struct multiboot_module *modules = (struct multiboot_module *)mbi->mods_addr;
    
    /* Load sysman (System Manager) - PID 1 */
    serial_print("[KERNEL] Loading sysman module...\n");
    uint32_t sysman_addr = (modules[0].mod_start & 0xFFFFF000);  /* GRUB alignment fix */
    
    /* Load uimanager (UI Manager) */
    serial_print("[KERNEL] Loading uimanager module...\n");
    uint32_t uimanager_addr = modules[1].mod_start;
    uint32_t uimanager_size = modules[1].mod_end - uimanager_addr;
    
    /* Copy uimanager to linked address (0x00280000) */
    uint8_t *src_ui = (uint8_t *)uimanager_addr;
    uint8_t *dst_ui = (uint8_t *)0x00280000;
    for (uint32_t i = 0; i < uimanager_size; i++) {
        dst_ui[i] = src_ui[i];
    }
    uimanager_module_address = 0x00280000;
    update_loading_screen("UI Manager Loaded");
    
    /* Load orbit (Desktop Environment) */
    serial_print("[KERNEL] Loading orbit module...\n");
    uint32_t orbit_addr = modules[2].mod_start;
    uint32_t orbit_size = modules[2].mod_end - orbit_addr;
    
    /* Copy orbit to linked address (0x00300000) */
    uint8_t *src_orbit = (uint8_t *)orbit_addr;
    uint8_t *dst_orbit = (uint8_t *)0x00300000;
    for (uint32_t i = 0; i < orbit_size; i++) {
        dst_orbit[i] = src_orbit[i];
    }
    orbit_module_address = 0x00300000;
    update_loading_screen("Desktop Environment Loaded");
    
    /* Load file_manager (optional module 3) */
    if (mbi->mods_count >= 4) {
        serial_print("[KERNEL] Loading file_manager module...\n");
        file_manager_module_address = modules[3].mod_start;
        file_manager_module_size = modules[3].mod_end - modules[3].mod_start;
    }
    
    /* Load disk_manager (optional module 4) */
    if (mbi->mods_count >= 5) {
        serial_print("[KERNEL] Loading disk_manager module...\n");
        disk_manager_module_address = modules[4].mod_start;
        disk_manager_module_size = modules[4].mod_end - modules[4].mod_start;
    }
    
    /*=========================================================================
     * STEP 9: Transition to Ring 3
     *=======================================================================*/
    
    update_loading_screen("Starting OS...");
    
    /* Disable interrupts during critical process creation */
    __asm__ volatile("cli");
    
    /* Enable scheduler */
    scheduler_enable();
    
    /* Create sysman as PID 1 (first userspace process) */
    serial_print("[KERNEL] Creating sysman process (PID 1)...\n");
    int sysman_pid = process_create(sysman_addr);
    
    if (sysman_pid < 0) {
        serial_print("[KERNEL] FATAL: Failed to create sysman!\n");
        font_draw_string(box_x + 100, box_y + 180, "ERROR: Sysman failed to start", 0xFF0000, 14);
        while(1) asm volatile("hlt");
    }
    
    serial_print("[KERNEL] Sysman created with PID: ");
    serial_hex(sysman_pid);
    serial_print("\n");
    
    /* Enable timer IRQ (starts multitasking) */
    serial_print("[KERNEL] Enabling timer IRQ...\n");
    irq_enable_timer();
    
    /* Enable interrupts - scheduler takes control */
    serial_print("[KERNEL] Enabling interrupts, entering multitasking mode...\n");
    __asm__ volatile("sti");
    
    /*=========================================================================
     * STEP 10: Idle Loop (PID 0 - Kernel)
     *=======================================================================*/
    
    serial_print("[KERNEL] Boot complete - entering idle loop (PID 0)\n");
    
    /* Kernel idle loop: HLT until next interrupt */
    while(1) {
        asm volatile("hlt");  /* Wait for interrupts, scheduler runs userspace */
    }
}
