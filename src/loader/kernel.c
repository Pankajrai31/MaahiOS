/*=============================================================================
 * MaahiOS Kernel - Main Entry Point
 * 
 * Boot Sequence:
 *   1. Multiboot validation
 *   2. Kernel Logger (klog) - FIRST, no dependencies
 *   3. Memory management (PMM, paging, kheap)
 *   4. CPU setup (GDT, IDT, IRQ)
 *   5. Graphics (BGA + Display driver)
 *   6. Kernel managers (process, scheduler, window)
 *   7. Device Manager auto-discovery (disk, mouse, keyboard, rtc)
 *   8. Load sysman and transition to Ring 3
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 *===========================================================================*/

#include <stdint.h>
#include "kernel.h"
#include "managers/klog/klog.h"
#include "managers/device/device_manager.h"
#include "managers/shm/shm_manager.h"
#include "managers/cell/cell_manager.h"
#include "managers/grub_module/grub_module_manager.h"
#include "managers/time/time_manager.h"
#include "managers/syscall/syscall_manager.h"

/* Port I/O */
#include "system/libraries/shared/io.h"

/*=============================================================================
 * Global Variables
 *===========================================================================*/

/*=============================================================================
 * Kernel Panic - Fatal error display (VGA text mode)
 *===========================================================================*/

static void kernel_panic(const char *msg) {
    vga_print("KERNEL PANIC: ");
    vga_print(msg);
    vga_print("\n");
    klog_dump();  /* Dump all logs to serial */
    while(1) __asm__ volatile("hlt");
}

/*=============================================================================
 * Kernel Main Entry Point
 *===========================================================================*/

void kernel_main(unsigned int magic, struct multiboot_info *mbi) {
    
    /*=========================================================================
     * STEP 1: Multiboot Validation (VGA text mode only)
     *=======================================================================*/
    vga_print("MaahiOS booting...\n");
    
    if (magic != 0x2BADB002) {
        vga_print("FATAL: Invalid multiboot magic!\n");
        while(1) __asm__ volatile("hlt");
    }
    
    /*=========================================================================
     * STEP 2: Kernel Logger - FIRST (no dependencies, static buffer)
     *=======================================================================*/
    klog_manager_init();
    KLOG_INFO("KERNEL", "=== MaahiOS Boot Started ===");
    KLOG_INFO("KERNEL", "Kernel logger initialized (256 entry buffer)");
    
    /*=========================================================================
     * STEP 3: Memory Management
     *=======================================================================*/
    /* Detect graphics framebuffer address (BGA on QEMU, VBE everywhere else) */
    uint32_t fb_addr = 0;
    uint32_t fb_size = 1280 * 800 * 4;
    
    if (bga_is_available()) {
        fb_addr = bga_get_framebuffer_addr();
        KLOG_INFO("KERNEL", "BGA framebuffer detected");
    } else if (vbe_is_available()) {
        fb_addr = vbe_get_framebuffer_addr();
        KLOG_INFO("KERNEL", "VBE framebuffer detected");
    } else {
        kernel_panic("No display hardware found!");
    }
    
    KLOG_INFO("PMM", "Initializing Physical Memory Manager");
    if (pmm_init(mbi) != 0) {
        kernel_panic("PMM initialization failed!");
    }
    KLOG_INFO("PMM", "PMM initialized successfully");
    
    /* Reserve framebuffer region */
    pmm_mark_region_used(fb_addr, fb_addr + fb_size);
    
    KLOG_INFO("PAGING", "Initializing virtual memory");
    if (paging_init(mbi) != 0) {
        kernel_panic("Paging initialization failed!");
    }
    KLOG_INFO("PAGING", "Paging initialized successfully");
    
    /* Map framebuffer into virtual address space */
    identity_map_region(kernel_page_directory, fb_addr, fb_addr + fb_size);
    
    /*=========================================================================
     * STEP 4: CPU Setup - GDT, IDT, IRQ
     *=======================================================================*/
    KLOG_INFO("GDT", "Initializing Global Descriptor Table");
    if (gdt_init() != 0 || gdt_load() != 0) {
        kernel_panic("GDT initialization failed!");
    }
    KLOG_INFO("GDT", "GDT loaded successfully");
    
    KLOG_INFO("IDT", "Initializing Interrupt Descriptor Table");
    if (idt_init() != 0 || idt_load() != 0) {
        kernel_panic("IDT initialization failed!");
    }
    KLOG_INFO("IDT", "IDT loaded successfully");
    
    KLOG_INFO("IRQ", "Remapping PIC (IRQ 0-15 to INT 0x20-0x2F)");
    irq_manager_init();
    
    if (idt_install_exception_handlers() != 0) {
        kernel_panic("Exception handler installation failed!");
    }
    KLOG_INFO("IDT", "Exception handlers installed");
    
    /*=========================================================================
     * STEP 5: Graphics Initialization (BGA on QEMU, VBE fallback elsewhere)
     *=======================================================================*/
    KLOG_INFO("GFX", "Initializing graphics layer");
    if (gfx_init(1280, 800, 32) != 0) {
        kernel_panic("Graphics initialization failed!");
    }
    gfx_clear(0x001020);  /* Dark blue background */
    KLOG_INFO("GFX", "Graphics layer initialized");
    
    /*=========================================================================
     * STEP 6: Kernel Managers
     *=======================================================================*/
    KLOG_INFO("KHEAP", "Initializing kernel heap");
    kheap_init();
    KLOG_INFO("KHEAP", "Kernel heap initialized");
    
    KLOG_INFO("SHM", "Initializing shared memory manager");
    shm_manager_init();
    
    KLOG_INFO("CELL", "Initializing cell manager (key-value store)");
    cell_manager_init();
    
    KLOG_INFO("PROC", "Initializing process manager");
    process_manager_init();
    KLOG_INFO("PROC", "Process manager initialized");
    
    KLOG_INFO("SCHED", "Initializing scheduler");
    scheduler_init();
    KLOG_INFO("SCHED", "Scheduler initialized");
    
    KLOG_INFO("PIT", "Initializing PIT timer (50Hz)");
    pit_init(50);
    KLOG_INFO("PIT", "PIT timer initialized");
    
    /*=========================================================================
     * STEP 7: Device Manager - Auto-Init All Drivers
     * 
     * The Device Manager acts like a HAL (Hardware Abstraction Layer).
     * It automatically initializes all drivers from its driver table:
     *   - disk (ATA/ISO9660)
     *   - mouse (PS/2 + cursor)
     *   - keyboard (PS/2)
     *   - rtc (Real-Time Clock)
     * 
     * Note: Display driver is initialized in STEP 5 (must be before klog/gfx)
     *=======================================================================*/
    KLOG_INFO("KERNEL", "Initializing Device Manager (auto-discovery)");
    device_manager_init();
    
    /* Initialize partition and volume layers (builds on disk_init from device_manager) */
    KLOG_INFO("PART", "Initializing partition driver");
    partdrive_init();
    
    KLOG_INFO("VOL", "Initializing volume driver (auto-mount)");
    voldrive_init();
    
    /* Initialize GRUB module manager (records module locations) */
    KLOG_INFO("GRUBMOD", "Initializing GRUB module manager");
    grub_module_manager_init(mbi->mods_count, mbi->mods_addr);
    
    /* Initialize time manager (needs RTC + PIT) */
    KLOG_INFO("TIME", "Initializing time manager");
    time_manager_init();
    
    /* Initialize syscall manager (table-based dispatch for INT 0x80) */
    KLOG_INFO("SYSCALL", "Initializing syscall manager");
    if (syscall_manager_init() != 0) {
        kernel_panic("Syscall manager initialization failed!");
    }
    
    /*=========================================================================
     * STEP 8: Load Sysman from GRUB Module 0
     *       - Each process gets its own page directory via process_create_from_memory
     *       - All user processes are linked at 0x10000000 (per-process virtual base)
     *=======================================================================*/
    KLOG_INFO("LOADER", "Loading system modules from GRUB");
    
    if (mbi->mods_count < 2) {
        kernel_panic("Need 2 modules (sysman + logexec)!");
    }
    
    struct multiboot_module *modules = (struct multiboot_module *)mbi->mods_addr;
    
    /* Module 0: sysman - create with per-process page directory.
     * process_create_from_memory allocates physical pages, copies the binary,
     * creates a cloned page directory with the binary mapped at 0x10000000,
     * and zeroes BSS region beyond the binary. */
    #define PROCESS_VIRTUAL_BASE 0x10000000
    uint32_t sysman_src  = modules[0].mod_start;
    uint32_t sysman_size = modules[0].mod_end - modules[0].mod_start;
    KLOG_INFO_HEX2("LOADER", "sysman module addr/size: ", sysman_src, sysman_size);
    
    /*=========================================================================
     * STEP 9: Transition to Ring 3
     *=======================================================================*/
    KLOG_INFO("KERNEL", "=== Transitioning to Ring 3 ===");
    
    __asm__ volatile("cli");
    
    scheduler_enable();
    KLOG_INFO("SCHED", "Scheduler enabled");
    
    /* Create sysman as PID 1 with per-process page directory */
    extern int process_create_from_memory(uint32_t base_address, const void *binary_data,
                                          uint32_t binary_size, uint32_t entry_offset);
    int sysman_pid = process_create_from_memory(PROCESS_VIRTUAL_BASE,
                                                (const void *)sysman_src,
                                                sysman_size, 0);
    if (sysman_pid < 0) {
        kernel_panic("Failed to create sysman process!");
    }
    KLOG_INFO("PROC", "sysman created with PID %d (per-process page dir)", sysman_pid);
    
    /* Enable timer IRQ - starts multitasking */
    irq_enable_timer();
    KLOG_INFO("IRQ", "Timer IRQ enabled");
    
    KLOG_INFO("KERNEL", "Enabling interrupts, entering idle loop (PID 0)");
    __asm__ volatile("sti");
    
    /*=========================================================================
     * STEP 10: Kernel Idle Loop (PID 0)
     *=======================================================================*/
    while(1) {
        asm volatile("hlt");
    }
}
