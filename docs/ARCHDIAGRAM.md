# MaahiOS Architecture Diagram Documentation

**Complete System Loading Order and Component Details**

This document describes every component of MaahiOS in the order they load, their responsibilities, and all functions within each module.

---

## Boot Sequence Overview

```
GRUB → boot.s → kernel_main() → Drivers → Process Manager → Sysman → UIManager → Orbit
```

---

## 1. BOOTLOADER STAGE

### **src/boot.s**
**Load Order:** 1st (Entry point from GRUB)  
**Purpose:** Multiboot-compliant bootloader entry point, sets up stack and extracts video mode information

#### Functions:
- `_start`: Entry point from GRUB, sets up stack at `stack_top`
- `load_sysman_module`: Loads sysman.bin from GRUB modules into memory at 0x00110000

#### Data Structures:
- `vbe_mode_info`: Stores framebuffer address, width, height, pitch, bpp (extracted from multiboot)
- `stack_bottom`/`stack_top`: 64KB kernel stack

#### Flow:
1. Extract multiboot info (magic, mbi pointer)
2. Parse VBE framebuffer info from multiboot structure (offset 88+)
3. Load sysman module into memory
4. Call `kernel_main(magic, mbi)`

---

## 2. KERNEL CORE

### **src/kernel.c**
**Load Order:** 2nd (Called from boot.s)  
**Purpose:** Main kernel initialization, driver setup, process creation, syscall handling

#### Main Functions:

**Initialization:**
- `kernel_main(unsigned int magic, struct multiboot_info *mbi)`: Main entry point, orchestrates all initialization

**Driver Initialization:**
- `bga_init(uint16_t width, uint16_t height, uint16_t bpp)`: Initialize BGA graphics
- `bga_is_available()`: Check if BGA device exists via PCI
- `pit_init(unsigned int frequency)`: Initialize Programmable Interval Timer
- `scheduler_init()`: Initialize task scheduler
- `gdt_init()`, `gdt_load()`: Global Descriptor Table setup
- `idt_init()`, `idt_load()`: Interrupt Descriptor Table setup
- `idt_install_exception_handlers()`: Install CPU exception handlers
- `pic_remap()`: Remap PIC IRQs (master 0x20-0x27, slave 0x28-0x2F)
- `pmm_init(struct multiboot_info *mbi)`: Physical Memory Manager initialization
- `paging_init(struct multiboot_info *mbi)`: Virtual memory paging setup

**Graphics:**
- `bga_clear(uint32_t color)`: Clear entire framebuffer
- `bga_print(const char *str, uint32_t fg, uint32_t bg)`: Draw text to framebuffer
- `bga_fill_rect(int x, int y, int width, int height, uint32_t color)`: Draw filled rectangle
- `bga_get_framebuffer_addr()`: Get physical framebuffer address
- `bga_get_framebuffer_size()`: Get framebuffer size in bytes

**UI Manager (Kernel-side):**
- `uiman_create_window_kernel(int x, int y, int w, int h, const char *title, int parent, int owner_pid)`: Create window in kernel state
- `uiman_create_button_kernel(int window_id, int x, int y, int w, int h, const char *text, int owner_pid)`: Create button in kernel state
- `uiman_create_label_kernel(int window_id, int x, int y, const char *text, int owner_pid)`: Create label in kernel state
- `uiman_poll_event_kernel(void *event_ptr, int calling_pid)`: Poll UI events for a process
- `uiman_get_kernel_windows()`: Return pointer to kernel's window array
- `uiman_get_kernel_controls()`: Return pointer to kernel's control array
- `uiman_get_kernel_event_queues()`: Return pointer to kernel's event queue array

**Utility:**
- `find_free_window()`: Find unused slot in g_kernel_windows array
- `find_free_control()`: Find unused slot in g_kernel_controls array
- `strcpy_safe(char *dest, const char *src, int max_len)`: Safe string copy
- `serial_print(const char *str)`: Debug output to serial port (COM1)
- `serial_hex(unsigned char value)`: Print hex byte to serial
- `outb(unsigned short port, unsigned char val)`: Write byte to I/O port
- `inb(unsigned short port)`: Read byte from I/O port

#### Global Data:
- `g_kernel_windows[MAX_WINDOWS]`: Array of all windows (MAX_WINDOWS=16)
- `g_kernel_controls[MAX_CONTROLS]`: Array of all controls (MAX_CONTROLS=128)
- `g_kernel_event_queues[MAX_PROCESSES]`: Event queues per process (MAX_PROCESSES=32)
- `gui_font_bitmap`: 8x16 bitmap font data for text rendering

#### Constants:
- `MAX_WINDOWS`: 16
- `MAX_CONTROLS`: 128
- `MAX_PROCESSES`: 32
- `EVENT_QUEUE_SIZE`: 32

---

## 3. MEMORY MANAGEMENT

### **src/managers/memory/pmm.c**
**Load Order:** 3rd (Called from kernel_main)  
**Purpose:** Physical Memory Manager - manages physical RAM pages

#### Functions:
- `pmm_init(struct multiboot_info *mbi)`: Initialize PMM with memory map from multiboot
- `pmm_alloc_page()`: Allocate a 4KB physical page
- `pmm_free_page(void *addr)`: Free a physical page
- `pmm_get_free_pages()`: Get count of free pages
- `pmm_get_total_pages()`: Get total pages in system

#### Data Structures:
- Bitmap tracking allocated/free pages
- Page size: 4096 bytes (4KB)

---

### **src/managers/memory/paging.c**
**Load Order:** 4th (Called from kernel_main)  
**Purpose:** Virtual Memory Manager - manages page tables and virtual address space

#### Functions:
- `paging_init(struct multiboot_info *mbi)`: Setup initial page tables
- `paging_map_page(void *virtual, void *physical, int flags)`: Map virtual to physical address
- `paging_unmap_page(void *virtual)`: Unmap a virtual page
- `paging_get_physical(void *virtual)`: Translate virtual to physical address
- `vmm_alloc_page()`: Allocate virtual page
- `vmm_free_page(void *addr)`: Free virtual page

#### Flags:
- `PAGE_PRESENT`: Page is present in memory
- `PAGE_WRITE`: Page is writable
- `PAGE_USER`: Page accessible from user mode (Ring 3)

---

## 4. DESCRIPTOR TABLES

### **src/managers/gdt/gdt.c**
**Load Order:** 5th (Called from kernel_main)  
**Purpose:** Global Descriptor Table - defines memory segments for protected mode

#### Functions:
- `gdt_init()`: Initialize GDT with kernel/user code/data segments
- `gdt_load()`: Load GDT into CPU (assembly wrapper)
- `gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)`: Configure GDT entry

#### Segments:
- Segment 0: Null segment (required)
- Segment 1: Kernel code segment (Ring 0, executable)
- Segment 2: Kernel data segment (Ring 0, writable)
- Segment 3: User code segment (Ring 3, executable)
- Segment 4: User data segment (Ring 3, writable)
- Segment 5: TSS (Task State Segment) for Ring 3 switching

---

### **src/managers/interrupt/idt.c**
**Load Order:** 6th (Called from kernel_main)  
**Purpose:** Interrupt Descriptor Table - handles CPU interrupts and exceptions

#### Functions:
- `idt_init()`: Initialize IDT with 256 entries
- `idt_load()`: Load IDT into CPU
- `idt_install_exception_handlers()`: Install handlers for CPU exceptions (0x00-0x1F)
- `idt_set_gate(int num, void *handler, uint16_t selector, uint8_t flags)`: Configure IDT entry

#### Interrupts:
- 0x00-0x1F: CPU exceptions (divide by zero, page fault, etc.)
- 0x20-0x2F: Hardware IRQs (after PIC remap)
- 0x80: Syscall interrupt

---

### **src/managers/interrupt/exception_handler.c**
**Purpose:** Handles CPU exceptions (page faults, divide by zero, etc.)

#### Functions:
- `exception_handler(int exception_num, struct registers *regs)`: Generic exception handler
- Individual handlers for each exception type (0-31)

---

### **src/managers/interrupt/interrupt_stubs.s**
**Purpose:** Assembly stubs for all interrupt handlers

#### Functions:
- `isr0` through `isr31`: CPU exception stubs
- `irq0` through `irq15`: Hardware IRQ stubs
- `isr128`: Syscall interrupt stub (0x80)
- `common_interrupt_handler`: Common handler that saves registers and calls C code

---

## 5. IRQ MANAGEMENT

### **src/managers/irq/irq_manager.c**
**Load Order:** 7th (Called from kernel_main)  
**Purpose:** Manages hardware interrupt requests, PIC configuration

#### Functions:
- `pic_remap()`: Remap PIC IRQs to 0x20-0x2F
- `irq_enable(int irq)`: Enable specific IRQ line
- `irq_disable(int irq)`: Disable specific IRQ line
- `irq_send_eoi(int irq)`: Send End-Of-Interrupt to PIC
- `irq_set_mask(int irq)`: Set IRQ mask bit
- `irq_clear_mask(int irq)`: Clear IRQ mask bit
- `irq_install_handler(int irq, void (*handler)(struct registers*))`: Install IRQ handler

#### IRQ Lines:
- IRQ0 (0x20): PIT Timer
- IRQ1 (0x21): Keyboard
- IRQ2 (0x22): Cascade (slave PIC)
- IRQ12 (0x2C): PS/2 Mouse
- IRQ14 (0x2E): Primary ATA
- IRQ15 (0x2F): Secondary ATA

---

## 6. TIMER & SCHEDULER

### **src/managers/timer/pit.c**
**Load Order:** 8th (Called from kernel_main)  
**Purpose:** Programmable Interval Timer - generates periodic timer interrupts

#### Functions:
- `pit_init(unsigned int frequency)`: Initialize PIT to generate IRQ0 at given frequency
- `pit_handler(struct registers *regs)`: IRQ0 handler, increments tick counter

#### Configuration:
- Default frequency: 100 Hz (every 10ms)
- Connected to IRQ0

---

### **src/managers/scheduler/scheduler.c**
**Load Order:** 9th (Called from kernel_main)  
**Purpose:** Preemptive multitasking scheduler

#### Functions:
- `scheduler_init()`: Initialize scheduler with empty task list
- `scheduler_create_task(void (*entry_point)(), const char *name)`: Create new task (process)
- `scheduler_enable()`: Enable scheduler (starts task switching on timer)
- `scheduler_yield()`: Voluntarily yield CPU
- `scheduler_tick()`: Called on each timer interrupt, performs context switch
- `scheduler_get_current_pid()`: Get current process ID
- `scheduler_exit()`: Terminate current process

#### Data Structures:
- `struct task`: Task Control Block (TCB)
  - PID, state, name
  - ESP (stack pointer), EBP (base pointer)
  - Entry point, priority
- Round-robin scheduling algorithm

---

### **src/managers/scheduler/switch_osdev.s**
**Purpose:** Assembly code for context switching

#### Functions:
- `switch_to_task(struct task *prev, struct task *next)`: Perform context switch
  - Saves all registers of current task
  - Loads all registers of next task
  - Switches stack pointers

---

## 7. PROCESS MANAGEMENT

### **src/managers/process/process_manager.c**
**Load Order:** 10th (Called from kernel_main)  
**Purpose:** Manages process creation, lifecycle, and Ring 3 execution

#### Functions:
- `process_create(void *entry_point, const char *name, int is_user_mode)`: Create new process
- `process_get_current()`: Get current process structure
- `process_exit(int exit_code)`: Terminate current process
- `process_setup_stack(struct process *proc, void *entry_point)`: Setup process stack
- `process_list()`: List all active processes

#### Data Structures:
- `struct process`: Process Control Block
  - PID, PPID (parent PID)
  - Name, state
  - Page directory (virtual memory)
  - Stack pointer, entry point
  - Privilege level (Ring 0 or Ring 3)

---

### **src/managers/ring3/ring3.c**
**Purpose:** Handles transition from Ring 0 (kernel) to Ring 3 (user mode)

#### Functions:
- `ring3_switch(unsigned int entry_point)`: Jump to Ring 3 with given entry point
- `ring3_setup_stack(uint32_t *stack)`: Setup Ring 3 stack frame

#### Process:
1. Setup user-mode stack
2. Push SS, ESP, EFLAGS, CS, EIP
3. Execute IRET instruction to switch to Ring 3

---

## 8. GRAPHICS DRIVERS

### **src/drivers/vga.c**
**Purpose:** VGA text mode driver (fallback, not actively used)

#### Functions:
- `vga_init()`: Initialize VGA text mode (80x25)
- `vga_clear()`: Clear screen
- `vga_print(const char *s)`: Print string
- `vga_set_color(unsigned char fg, unsigned char bg)`: Set text color
- `vga_putchar(char c)`: Print single character

---

### **src/drivers/bga.c**
**Load Order:** 11th (Called from kernel_main)  
**Purpose:** Bochs Graphics Adapter driver - primary graphics driver for QEMU

#### Functions:
- `bga_is_available()`: Check if BGA device exists via PCI
- `bga_init(uint16_t width, uint16_t height, uint16_t bpp)`: Initialize BGA mode
- `bga_write_register(uint16_t index, uint16_t value)`: Write to BGA register
- `bga_read_register(uint16_t index)`: Read from BGA register
- `bga_clear(uint32_t color)`: Clear framebuffer to color
- `bga_fill_rect(int x, int y, int width, int height, uint32_t color)`: Draw filled rectangle
- `bga_print(const char *str, uint32_t fg, uint32_t bg)`: Render text with font
- `bga_get_framebuffer_addr()`: Get framebuffer physical address (from PCI BAR0)
- `bga_get_framebuffer_size()`: Calculate framebuffer size
- `bga_set_pixel(int x, int y, uint32_t color)`: Set single pixel
- `bga_get_pixel(int x, int y)`: Get pixel color

#### Registers:
- `VBE_DISPI_INDEX_ID`: Device ID
- `VBE_DISPI_INDEX_XRES`: X resolution
- `VBE_DISPI_INDEX_YRES`: Y resolution
- `VBE_DISPI_INDEX_BPP`: Bits per pixel
- `VBE_DISPI_INDEX_ENABLE`: Enable/disable display

#### Default Mode:
- 800x600x32 (32-bit RGBA)
- Linear framebuffer (LFB) at physical address read from PCI

---

### **src/drivers/vbe.c**
**Purpose:** VESA BIOS Extensions driver (alternative to BGA)

#### Functions:
- `vbe_init()`: Initialize VBE mode
- `vbe_clear(uint32_t color)`: Clear screen
- `vbe_print(const char *str, uint32_t fg, uint32_t bg)`: Draw text
- `vbe_get_width()`, `vbe_get_height()`: Get screen dimensions
- `vbe_get_framebuffer_addr()`: Get framebuffer address

---

### **src/drivers/pci.c**
**Purpose:** PCI bus enumeration and device detection

#### Functions:
- `pci_config_read_dword(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset)`: Read PCI config space
- `pci_config_write_dword(...)`: Write PCI config space
- `pci_find_device(uint16_t vendor_id, uint16_t device_id)`: Find device by vendor/device ID
- `pci_get_bar(uint8_t bus, uint8_t device, uint8_t func, int bar_num)`: Read Base Address Register

#### PCI Devices:
- Bochs Graphics Adapter: Vendor 0x1234, Device 0x1111
- UHCI USB Controller: (if present)

---

## 9. INPUT DRIVERS

### **src/drivers/mouse.c**
**Load Order:** 12th (Called from kernel_main)  
**Purpose:** PS/2 mouse driver

#### Functions:
- `mouse_init()`: Initialize PS/2 mouse (IRQ12)
- `mouse_handler(struct registers *regs)`: IRQ12 handler, reads mouse packets
- `mouse_get_x()`, `mouse_get_y()`: Get mouse position
- `mouse_get_buttons()`: Get button state (left/right/middle)
- `mouse_poll()`: Poll mouse state
- `mouse_wait_input()`: Wait for input buffer ready
- `mouse_wait_output()`: Wait for output buffer ready
- `mouse_write(uint8_t data)`: Send command to mouse

#### Data:
- Mouse position: (x, y) coordinates
- Button state: 3 bits (left, right, middle)
- Mouse packets: 3-byte PS/2 format

---

### **src/drivers/usb.c**, **src/drivers/uhci.h**
**Purpose:** USB and UHCI controller support (basic, not fully implemented)

#### Functions:
- `usb_init()`: Initialize USB subsystem
- `uhci_init()`: Initialize UHCI controller
- `usb_detect_devices()`: Enumerate USB devices

---

## 10. SYSCALL INTERFACE

### **src/syscalls/syscall_numbers.h**
**Purpose:** Defines all syscall numbers

#### Syscall Numbers:
```c
#define SYSCALL_PUTS                    0   // Print string
#define SYSCALL_EXIT                    1   // Exit process
#define SYSCALL_FORK                    2   // Fork process
#define SYSCALL_EXEC                    3   // Execute program
#define SYSCALL_GETPID                  4   // Get process ID
#define SYSCALL_YIELD                   5   // Yield CPU
#define SYSCALL_SLEEP                   6   // Sleep milliseconds
#define SYSCALL_ALLOC_PAGE              7   // Allocate page
#define SYSCALL_FREE_PAGE               8   // Free page
#define SYSCALL_MOUSE_GET_X             20  // Get mouse X
#define SYSCALL_MOUSE_GET_Y             21  // Get mouse Y
#define SYSCALL_MOUSE_GET_BUTTONS       22  // Get mouse buttons
#define SYSCALL_POLL_MOUSE              23  // Poll mouse state
#define SYSCALL_GFX_CLEAR               30  // Clear screen
#define SYSCALL_GFX_FILL_RECT           31  // Fill rectangle
#define SYSCALL_GFX_DRAW_RECT           32  // Draw rectangle outline
#define SYSCALL_GFX_PRINT               33  // Print text
#define SYSCALL_GFX_SET_FG              34  // Set foreground color
#define SYSCALL_GFX_SET_BG              35  // Set background color
#define SYSCALL_UI_CREATE_WINDOW        40  // Create window
#define SYSCALL_UI_CREATE_BUTTON        41  // Create button
#define SYSCALL_UI_CREATE_LABEL         42  // Create label
#define SYSCALL_UI_POLL_EVENT           43  // Poll UI event
#define SYSCALL_UI_GET_WINDOWS_PTR      45  // Get windows array pointer
#define SYSCALL_UI_GET_CONTROLS_PTR     46  // Get controls array pointer
#define SYSCALL_UI_GET_EVENTS_PTR       47  // Get events array pointer
```

---

### **src/syscalls/syscall_handler.c**
**Load Order:** 13th (Registers with IDT)  
**Purpose:** Central syscall dispatcher, handles INT 0x80

#### Functions:
- `syscall_handler(struct registers *regs)`: Main syscall dispatcher
  - Reads EAX (syscall number)
  - Reads EBX, ECX, EDX, ESI, EDI (arguments)
  - Calls appropriate kernel function
  - Returns result in EAX

#### Handler Dispatch:
```c
switch (syscall_num) {
    case SYSCALL_PUTS: /* ... */ break;
    case SYSCALL_EXIT: /* ... */ break;
    case SYSCALL_MOUSE_GET_X: /* ... */ break;
    case SYSCALL_GFX_FILL_RECT: /* ... */ break;
    case SYSCALL_UI_CREATE_BUTTON: /* ... */ break;
    // ... etc
}
```

---

### **src/syscalls/user_syscalls.h** and **src/syscalls/user_syscalls.c**
**Purpose:** User-mode syscall wrappers (used by Ring 3 processes)

#### Functions (inline assembly wrappers):
- `syscall_puts(const char *str)`: Print string
- `syscall_exit(int code)`: Exit process
- `syscall_getpid()`: Get process ID
- `syscall_yield()`: Yield CPU
- `syscall_mouse_get_x()`, `syscall_mouse_get_y()`: Get mouse position
- `syscall_mouse_get_buttons()`: Get button state
- `syscall_poll_mouse()`: Poll mouse
- `syscall_fill_rect(int x, int y, int w, int h, uint32_t color)`: Draw rectangle
- `syscall_print_text(int x, int y, const char *text, uint32_t fg, uint32_t bg)`: Draw text
- `syscall_ui_create_window(int x, int y, int w, int h, const char *title)`: Create window
- `syscall_ui_create_button(int win_id, int x, int y, int w, int h, const char *text)`: Create button
- `syscall_ui_create_label(int win_id, int x, int y, const char *text)`: Create label
- `syscall_ui_poll_event(void *event_ptr)`: Poll UI event

#### Implementation:
Each function uses inline assembly:
```c
__asm__ volatile(
    "mov $SYSCALL_NUM, %%eax\n"  // Syscall number
    "mov %1, %%ebx\n"             // Arg 1
    "mov %2, %%ecx\n"             // Arg 2
    "int $0x80\n"                 // Trigger interrupt
    : "=a"(result)                // Output in EAX
    : "r"(arg1), "r"(arg2)        // Inputs
);
```

---

## 11. SYSTEM MANAGER (SYSMAN)

### **src/sysman/sysman_entry.s**
**Load Order:** 14th (First user process created by kernel)  
**Purpose:** Assembly entry point for Sysman process

#### Functions:
- `_start`: Entry point, sets up stack and calls `sysman_main_c()`

---

### **src/sysman/sysman.c**
**Load Order:** 15th (Called from sysman_entry.s)  
**Purpose:** System manager daemon - creates and manages UIManager and Orbit processes

#### Functions:
- `sysman_main_c()`: Main entry point
  1. Prints startup message
  2. Creates UIManager process (PID 2)
  3. Creates Orbit desktop process (PID 3)
  4. Enters idle loop

#### Process Creation:
```c
int uimanager_pid = syscall_fork();
if (uimanager_pid == 0) {
    // Child: exec UIManager
    syscall_exec(UIMANAGER_ADDR);
}

int orbit_pid = syscall_fork();
if (orbit_pid == 0) {
    // Child: exec Orbit
    syscall_exec(ORBIT_ADDR);
}
```

#### Addresses:
- UIManager: 0x00280000
- Orbit: 0x00300000

---

## 12. UI MANAGER PROCESS

### **src/uimanager/uimanager_entry.s**
**Load Order:** 16th (Created by Sysman)  
**Purpose:** Assembly entry point for UIManager process

#### Functions:
- `_start`: Sets up stack, calls `uimanager_main_c()`

---

### **src/uimanager/uimanager.c**
**Load Order:** 17th (Called from uimanager_entry.s)  
**Purpose:** Window server - owns framebuffer, renders all UI, routes input events

#### Main Functions:
- `uimanager_main_c()`: Main event loop
  1. Get kernel UI state pointers via syscalls
  2. Clear screen to background color (0x001020 - dark blue)
  3. Render all controls initially
  4. Enter event loop

#### Event Loop:
```c
while (1) {
    // Get mouse state
    int mx = syscall_mouse_get_x();
    int my = syscall_mouse_get_y();
    unsigned int buttons = syscall_mouse_get_buttons();
    
    // Poll mouse every 3 frames
    if (frame_count % 3 == 0) {
        syscall_poll_mouse();
    }
    
    // Hit test - find control under cursor
    int hit_control = hit_test(mx, my);
    
    // Update hover states
    if (hit_control != hover_control) {
        // Clear old hover, set new hover
        // Send hover enter/exit events to process
    }
    
    // Detect button press/release
    // Send click/double-click events
    
    // Erase old cursor position
    syscall_fill_rect(last_mouse_x, last_mouse_y, 11, 16, 0x001020);
    
    // Redraw controls that changed state
    for (controls with state changes) {
        render_control(i);
    }
    
    // Draw arrow cursor (11x16 pixel bitmap)
    for (each pixel in arrow_data) {
        syscall_fill_rect(mx+dx, my+dy, 1, 1, 0xFFFFFF);
    }
}
```

#### Rendering Functions:
- `render_control(int i)`: Render single control
- `render_all_controls()`: Render all active controls
- `hit_test(int x, int y)`: Find control at coordinates
- `queue_event(int owner_pid, uiman_event_t *event)`: Queue event to process

#### Control Rendering:
**Button:**
1. Draw 4-pixel border (color based on state: normal=gray, hover=blue, pressed=green)
2. Draw background (dark gray 0x404040)
3. Draw text centered

**Label:**
1. Draw text only (no background)

#### Cursor:
- 11x16 pixel arrow bitmap
- Drawn pixel-by-pixel using syscall_fill_rect
- Erased before redraw to prevent trail

#### Syscall Wrappers (Inline Assembly):
- `syscall_get_windows_ptr()`: Syscall 45
- `syscall_get_controls_ptr()`: Syscall 46
- `syscall_get_events_ptr()`: Syscall 47

#### Data Structures:
- `UIWindow`: Window metadata (x, y, width, height, title, visible, active)
- `UIControl`: Control metadata (id, type, x, y, width, height, state, text, owner_pid)
- `EventQueue`: Per-process event queue (head, tail, count, events[32])
- `uiman_event_t`: Event structure (type, control_id, x, y)

#### Event Types:
- `UIMAN_EVENT_CLICK`: Button click
- `UIMAN_EVENT_DBLCLICK`: Double-click
- `UIMAN_EVENT_HOVER`: Hover enter/exit

#### Control States:
- `UIMAN_STATE_NORMAL`: 0
- `UIMAN_STATE_HOVER`: 1
- `UIMAN_STATE_PRESSED`: 2

#### Optimization:
- Only redraws controls when state changes (reduces flicker)
- Tracks last state in `g_last_control_state[]`
- Initial render of all controls on startup

---

### **External Functions (from libgui):**
- `gui_draw_text(int x, int y, const char *text, unsigned int fg, unsigned int bg)`: Render text

---

## 13. ORBIT DESKTOP

### **src/orbit/orbit_entry.s**
**Load Order:** 18th (Created by Sysman)  
**Purpose:** Assembly entry point for Orbit desktop process

#### Functions:
- `_start`: Sets up stack, calls `orbit_main_c()`

---

### **src/orbit/orbit.c**
**Load Order:** 19th (Called from orbit_entry.s)  
**Purpose:** Desktop application - creates UI controls, handles user interactions

#### Functions:
- `orbit_main_c()`: Main entry point
  1. Create desktop window (800x600)
  2. Create buttons (Process Manager, Disk Manager, Settings, Terminal)
  3. Create welcome label
  4. Enter event polling loop

#### UI Creation:
```c
// Create desktop window
int window_id = syscall_ui_create_window(0, 0, 800, 600, "Orbit Desktop");

// Create buttons
syscall_ui_create_button(window_id, 50, 50, 200, 40, "Process Manager");
syscall_ui_create_button(window_id, 50, 100, 200, 40, "Disk Manager");
syscall_ui_create_button(window_id, 50, 150, 200, 40, "Settings");
syscall_ui_create_button(window_id, 50, 200, 200, 40, "Terminal");

// Create label
syscall_ui_create_label(window_id, 50, 250, "Welcome to MaahiOS!");
```

#### Event Loop:
```c
while (1) {
    uiman_event_t event;
    int has_event = syscall_ui_poll_event(&event);
    
    if (has_event && event.type == UIMAN_EVENT_CLICK) {
        // Handle button clicks
        switch (event.control_id) {
            case BUTTON_PROCESS_MANAGER: /* launch process manager */ break;
            case BUTTON_DISK_MANAGER: /* launch disk manager */ break;
            // etc.
        }
    }
    
    syscall_yield();  // Yield to other processes
}
```

---

## 14. GUI LIBRARY

### **src/libgui/libgui.h**
**Purpose:** Main header for GUI library

#### Includes:
- `window.c`, `controls.c`, `draw.c`, `cursor.c`, `bmp.c`, `icons.c`

---

### **src/libgui/window.c**
**Purpose:** Window management functions (higher-level than kernel UI)

#### Functions:
- `gui_create_window(int x, int y, int width, int height, const char *title)`: Create window
- `gui_destroy_window(int window_id)`: Destroy window
- `gui_show_window(int window_id)`: Show window
- `gui_hide_window(int window_id)`: Hide window

---

### **src/libgui/controls.c**
**Purpose:** Control creation and management

#### Functions:
- `gui_create_button(int window_id, int x, int y, int w, int h, const char *text)`: Create button
- `gui_create_label(int window_id, int x, int y, const char *text)`: Create label
- `gui_create_textbox(int window_id, int x, int y, int w, int h)`: Create textbox

---

### **src/libgui/draw.c**
**Purpose:** Drawing primitives and text rendering

#### Functions:
- `gui_draw_text(int x, int y, const char *text, unsigned int fg, unsigned int bg)`: Render text using 8x16 bitmap font
- `gui_draw_line(int x1, int y1, int x2, int y2, uint32_t color)`: Draw line
- `gui_draw_rect(int x, int y, int w, int h, uint32_t color)`: Draw rectangle outline
- `gui_fill_rect(int x, int y, int w, int h, uint32_t color)`: Draw filled rectangle

#### Font:
- 8x16 bitmap font (embedded in `gui_font_bitmap`)
- ASCII characters 0-127

---

### **src/libgui/cursor.c** and **src/libgui/cursor_compositor.c**
**Purpose:** Cursor rendering and composition (not actively used - UIManager handles cursor)

#### Functions:
- `cursor_init()`: Initialize cursor
- `cursor_draw(int x, int y)`: Draw cursor at position
- `cursor_erase()`: Erase cursor
- `cursor_compositor_render()`: Composite cursor over framebuffer

---

### **src/libgui/bmp.c**
**Purpose:** BMP image loading and rendering

#### Functions:
- `bmp_load(const char *filename)`: Load BMP from disk
- `bmp_draw(int x, int y, struct bmp_image *img)`: Draw BMP to screen
- `bmp_free(struct bmp_image *img)`: Free BMP memory

---

### **src/libgui/icons.c** and **src/orbit/embedded_icons.h**
**Purpose:** Icon resources (embedded in binary)

#### Data:
- Icon bitmaps (converted from PNG/BMP to C arrays)
- Used for buttons, toolbars, etc.

---

## 15. HEAP MEMORY

### **src/lib/heap.c**
**Purpose:** Kernel heap allocator (kmalloc/kfree)

#### Functions:
- `heap_init()`: Initialize heap
- `kmalloc(size_t size)`: Allocate memory from kernel heap
- `kfree(void *ptr)`: Free memory
- `krealloc(void *ptr, size_t size)`: Reallocate memory

#### Implementation:
- First-fit allocation algorithm
- Maintains free list
- Coalesces adjacent free blocks

---

### **src/lib/kheap.c**
**Purpose:** Alternative kernel heap (if heap.c is not used)

#### Functions:
- Similar to heap.c

---

## 16. BUILD SYSTEM

### **build/build.sh**
**Purpose:** Main build script

#### Steps:
1. Compile all .c files to .o (i686-elf-gcc)
2. Assemble all .s files to .o (i686-elf-as)
3. Link kernel.bin (i686-elf-ld with linker.ld)
4. Link sysman.bin (with sysman_linker.ld)
5. Link uimanager.bin (i686-elf-ld)
6. Link orbit.bin (with orbit_linker.ld)
7. Create GRUB ISO:
   - Copy kernel.bin to isodir/boot/
   - Copy modules to isodir/boot/ (sysman.bin, uimanager.bin, orbit.bin)
   - Run grub-mkrescue to create boot.iso

---

### **src/linker.ld**
**Purpose:** Linker script for kernel

#### Sections:
- `.text`: Code section at 0x00100000 (1MB)
- `.rodata`: Read-only data
- `.data`: Initialized data
- `.bss`: Uninitialized data

---

### **src/sysman_linker.ld**, **src/orbit/orbit_linker.ld**
**Purpose:** Linker scripts for user-mode processes

#### Sections:
- `.text`: Code section
- `.data`: Data section
- `.bss`: BSS section
- No kernel addresses, standalone binaries

---

### **isodir/boot/grub/grub.cfg**
**Purpose:** GRUB bootloader configuration

#### Contents:
```
menuentry "MaahiOS" {
    multiboot /boot/kernel.bin
    module /boot/sysman.bin
    module /boot/uimanager.bin
    module /boot/orbit.bin
    boot
}
```

---

## DATA FLOW DIAGRAM

```
┌─────────────────────────────────────────────────────────────────┐
│                         BOOT SEQUENCE                            │
└─────────────────────────────────────────────────────────────────┘

GRUB → boot.s → kernel_main()
                    ↓
        ┌───────────┴───────────┐
        ↓                       ↓
   PMM/Paging            GDT/IDT/PIC
        ↓                       ↓
   BGA Graphics          IRQ Handlers
        ↓                       ↓
   PIT Timer              Mouse Driver
        ↓                       ↓
   Scheduler           Syscall Handler
        ↓                       
   Create Sysman (PID 1)
        ↓
   ┌────┴────┐
   ↓         ↓
UIManager  Orbit
(PID 2)   (PID 3)


┌─────────────────────────────────────────────────────────────────┐
│                      PROCESS HIERARCHY                           │
└─────────────────────────────────────────────────────────────────┘

PID 0: Kernel (Ring 0)
   └─ PID 1: Sysman (Ring 3) - System Manager
       ├─ PID 2: UIManager (Ring 3) - Window Server
       └─ PID 3: Orbit (Ring 3) - Desktop


┌─────────────────────────────────────────────────────────────────┐
│                       UI DATA FLOW                               │
└─────────────────────────────────────────────────────────────────┘

Orbit (PID 3):
   syscall_ui_create_button(...)
            ↓ [INT 0x80]
   syscall_handler() [Kernel]
            ↓
   uiman_create_button_kernel()
            ↓
   g_kernel_controls[] ← new button
            ↓
   [Button stored in kernel memory]
            ↓
UIManager (PID 2):
   g_controls = syscall_get_controls_ptr()
            ↓
   render_control(i)
            ↓
   syscall_fill_rect(...) [draws button]
            ↓
   BGA framebuffer ← pixels


┌─────────────────────────────────────────────────────────────────┐
│                      MOUSE INPUT FLOW                            │
└─────────────────────────────────────────────────────────────────┘

Hardware Mouse Movement
        ↓
   IRQ12 Handler (mouse.c)
        ↓
   Update mouse_x, mouse_y, buttons
        ↓
UIManager polls: syscall_mouse_get_x/y()
        ↓
   Hit test: which control is under cursor?
        ↓
   Update control state (HOVER/NORMAL)
        ↓
   Render control with new state
        ↓
   Queue event to owner process (Orbit)
        ↓
Orbit polls: syscall_ui_poll_event()
        ↓
   Handle button click → launch app


┌─────────────────────────────────────────────────────────────────┐
│                   MEMORY LAYOUT (Physical)                       │
└─────────────────────────────────────────────────────────────────┘

0x00000000 - 0x000FFFFF  : Low memory (BIOS, VGA, etc.)
0x00100000 - 0x0012FFFF  : Kernel code/data (192KB)
0x00110000 - 0x00130FFF  : Sysman loaded here from module
0x00131000 - 0x00135FFF  : UIManager module in memory
0x00136000 - 0x00137FFF  : Orbit module in memory
0x00200000+              : Available RAM (PMM manages)
0x00280000               : UIManager process space (copied here)
0x00300000               : Orbit process space (copied here)
0xFD000000               : BGA framebuffer (800*600*4 = 1.92MB)


┌─────────────────────────────────────────────────────────────────┐
│                   VIRTUAL MEMORY LAYOUT                          │
└─────────────────────────────────────────────────────────────────┘

Ring 0 (Kernel):
   0x00000000 - 0x003FFFFF : Identity mapped (0-4MB)
   0x00100000+            : Kernel code/data
   0xC0000000+            : Kernel heap

Ring 3 (User):
   0x00280000             : UIManager code
   0x00300000             : Orbit code
   0x40000000+            : User heap (future)
   0xBFFFF000             : User stack (grows down)


┌─────────────────────────────────────────────────────────────────┐
│                     INTERRUPT VECTORS                            │
└─────────────────────────────────────────────────────────────────┘

0x00-0x1F : CPU Exceptions
   0x0E : Page Fault
   0x0D : General Protection Fault

0x20-0x2F : Hardware IRQs (after PIC remap)
   0x20 : IRQ0 - PIT Timer (scheduler tick)
   0x21 : IRQ1 - Keyboard
   0x22 : IRQ2 - Cascade
   0x2C : IRQ12 - PS/2 Mouse

0x80 : System Call (INT 0x80)


┌─────────────────────────────────────────────────────────────────┐
│                    KEY ALGORITHMS                                │
└─────────────────────────────────────────────────────────────────┘

Scheduler (Round-Robin):
   1. On timer IRQ (every 10ms)
   2. Save current task registers
   3. Find next READY task
   4. Load next task registers
   5. IRET to new task

Page Allocation:
   1. PMM maintains bitmap of 4KB pages
   2. Linear scan for free page
   3. Mark as allocated
   4. Return physical address

Syscall Dispatch:
   1. User: INT 0x80, EAX = syscall number
   2. IDT entry 0x80 → syscall_handler()
   3. Switch to kernel stack
   4. Dispatch based on EAX
   5. Return value in EAX
   6. IRET back to user mode

UI Event Queue:
   1. Mouse move → hit_test() → control ID
   2. State change (NORMAL → HOVER)
   3. queue_event(owner_pid, event)
   4. Process polls → get event → handle
```

---

## CRITICAL PATHS

### **Application Launch Path:**
1. User clicks button in Orbit
2. Mouse IRQ12 → mouse_handler() updates position
3. UIManager hit_test() finds button
4. State: NORMAL → PRESSED
5. UIManager queue_event(CLICK, button_id) to Orbit
6. Orbit syscall_ui_poll_event() retrieves event
7. Orbit handles click: syscall_exec(app_address)
8. Kernel process_create() for new app
9. Scheduler adds task to run queue

### **Button Render Path:**
1. Orbit: syscall_ui_create_button(...)
2. Kernel: uiman_create_button_kernel() stores in g_kernel_controls[]
3. UIManager: syscall_get_controls_ptr() reads kernel array
4. UIManager: render_control() → syscall_fill_rect() for border/background
5. UIManager: gui_draw_text() for button text
6. BGA: bga_fill_rect() writes to framebuffer
7. Pixels appear on screen

---

## FILE COUNT SUMMARY

**Total Source Files:** ~60 files

**Categories:**
- Boot: 1 file (boot.s)
- Kernel Core: 1 file (kernel.c)
- Memory: 2 files (pmm.c, paging.c)
- Descriptors: 3 files (gdt.c, idt.c, exception_handler.c + interrupt_stubs.s)
- IRQ: 1 file (irq_manager.c)
- Timer/Scheduler: 3 files (pit.c, scheduler.c, switch_osdev.s)
- Process: 2 files (process_manager.c, ring3.c)
- Graphics: 3 files (bga.c, vbe.c, vga.c)
- Input: 2 files (mouse.c, usb.c)
- Syscalls: 3 files (syscall_handler.c, user_syscalls.c, syscall_numbers.h)
- Sysman: 2 files (sysman_entry.s, sysman.c)
- UIManager: 2 files (uimanager_entry.s, uimanager.c)
- Orbit: 2 files (orbit_entry.s, orbit.c)
- LibGUI: 8 files (window.c, controls.c, draw.c, cursor.c, bmp.c, icons.c, etc.)
- Heap: 2 files (heap.c, kheap.c)
- Build: 4 files (build.sh, linker.ld, sysman_linker.ld, orbit_linker.ld)

---

## KEY DESIGN DECISIONS

1. **Microkernel-like UI**: UI state stored in kernel, but rendering done by separate UIManager process
2. **Ring 3 Desktop**: All user applications run in Ring 3 for safety
3. **Syscall-based Graphics**: All drawing goes through syscalls to ensure only UIManager writes to framebuffer
4. **Event Queue per Process**: Each process has its own event queue in kernel memory
5. **BGA for Graphics**: Uses Bochs Graphics Adapter (hardware-accelerated in QEMU)
6. **Preemptive Multitasking**: Timer-based scheduler with 10ms time slices
7. **Simple Paging**: 4KB pages, identity-mapped kernel, separate page directories per process

---

## FUTURE EXPANSION POINTS

- **File System**: VFS + ext2 driver (drivers/ folder)
- **Network Stack**: TCP/IP stack (net/ folder)
- **More Applications**: Terminal, text editor, file manager (apps/ folder)
- **Window Decorations**: Title bars, close buttons, minimize/maximize
- **Keyboard Input**: Keyboard driver + text input events
- **Multi-window Support**: Z-order, focus management, window dragging

---

**End of Architecture Documentation**

*This document represents the complete MaahiOS system as of the last commit.*
*Generated: 2026-01-03*
