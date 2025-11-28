# Boot Process and Kernel Documentation

## Overview

MaahiOS uses the Multiboot specification to be loaded by GRUB. The boot process involves:
1. GRUB loads the kernel at 1MB (0x100000)
2. boot.s sets up initial environment and extracts VBE info
3. kernel.c initializes all subsystems
4. sysman is started as the first Ring 3 process

---

## boot.s

**Purpose:** Assembly entry point that sets up the Multiboot header, stack, and transitions to C code.

### Key Components

#### Multiboot Header
```assembly
.set FLAGS,      ALIGN | MEMINFO    # Request memory info from GRUB
.set MAGIC,      0x1BADB002         # Multiboot magic number
```

- Requests aligned modules and memory map
- Video mode fields present but FLAGS doesn't include VIDEO_MODE bit (intentional)

#### VBE Information Storage
```assembly
.global vbe_mode_info
vbe_mode_info:
    .long 0     # framebuffer address
    .long 0     # width
    .long 0     # height
    .long 0     # pitch
    .long 0     # bpp
```

Stores framebuffer information extracted from the Multiboot info structure.

#### Stack Setup
- 64KB stack allocated in BSS section
- Stack grows downward from `stack_top`

#### Module Loading
The `load_sysman_module` function copies the sysman module from where GRUB loaded it to a fixed address (0x00110000).

### Issues Identified

1. **Hardcoded Module Address (Line 141)**
   ```assembly
   mov $0x00110000, %edi /* Destination: 0x00110000 */
   ```
   - **Issue:** The module destination is hardcoded, which could cause conflicts if the kernel grows.
   - **Suggestion:** Calculate a safe address based on kernel_end from the linker.

2. **Unused VBE Mode Request (Lines 18-21)**
   - Video mode parameters (1024x768x32) are specified but VIDEO_MODE flag is not set in FLAGS.
   - This is intentional as BGA driver is used instead, but could cause confusion.
   - **Suggestion:** Add a comment explaining this is placeholder for future VBE mode setup.

---

## kernel.c

**Purpose:** Main kernel entry point that initializes all subsystems and starts the first user process.

### Initialization Sequence

1. **VGA Print** - Initial message
2. **BGA Check** - Verify Bochs Graphics Adapter availability
3. **PMM Init** - Physical memory manager
4. **Paging Init** - Virtual memory setup
5. **GDT Init/Load** - Global Descriptor Table
6. **IDT Init/Load** - Interrupt Descriptor Table
7. **IRQ Manager Init** - PIC remapping
8. **Exception Handlers** - CPU exception handling
9. **Mouse Handler** - IRQ12 installation
10. **Kernel Heap** - kmalloc initialization
11. **Process Manager** - Process table setup
12. **Scheduler** - Task scheduling
13. **PIT Init** - 1000Hz timer
14. **BGA Graphics** - 1024x768x32 mode
15. **Mouse Init** - PS/2 mouse driver
16. **Process Creation** - Start sysman

### Key Functions

#### `kernel_main(unsigned int magic, struct multiboot_info *mbi)`

Main entry point receiving Multiboot magic and info structure.

#### Serial Debug Functions
```c
static void serial_print(const char *str);
static void serial_hex(unsigned char value);
```
Used for debugging via COM1 serial port (0x3F8).

### Structures

#### `multiboot_info`
```c
struct multiboot_info {
    unsigned int flags;
    unsigned int mem_lower;
    unsigned int mem_upper;
    // ... framebuffer info at offset 88+
} __attribute__((packed));
```

#### `multiboot_module`
```c
struct multiboot_module {
    unsigned int mod_start;
    unsigned int mod_end;
    char *string;
    unsigned int reserved;
};
```

### Issues Identified

1. **Hardcoded Framebuffer Address (Line 130)**
   ```c
   uint32_t fb_addr = 0xFD000000;  // QEMU default BGA framebuffer
   ```
   - **Issue:** Relies on QEMU-specific address.
   - **Suggestion:** Use `bga_get_framebuffer_addr()` from PCI scan.

2. **Inline Assembly for I/O (Lines 94-102)**
   ```c
   static inline void outb(unsigned short port, unsigned char val) {
       asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
   }
   ```
   - **Issue:** Duplicated across multiple files.
   - **Suggestion:** Create a shared `io.h` header for port I/O functions.

3. **Redundant `serial_print` Declaration (Line 224)**
   - Declared both as static and extern within the same file.
   - **Suggestion:** Use consistent declaration pattern.

4. **Magic Number 0x00090000 for Ring 0 Stack**
   - Used in GDT TSS setup but not clearly documented.
   - **Suggestion:** Define as constant with documentation.

5. **Interrupt Enable/Disable Pattern (Lines 192, 285)**
   - Interrupts enabled, then disabled, then enabled again.
   - **Suggestion:** Document the reasoning for this pattern more clearly.

---

## linker.ld

**Purpose:** Kernel linker script that defines memory layout.

### Layout
```
. = 0x00100000;        # Kernel starts at 1MB

.text   BLOCK(4K)      # Code section
.data   BLOCK(4K)      # Initialized data
.bss    BLOCK(4K)      # Uninitialized data

kernel_end = .;        # Symbol for end of kernel
```

### Issues Identified

1. **No RODATA Section**
   - Read-only data mixed with writable data.
   - **Suggestion:** Add `.rodata` section for constants.

---

## sysman_linker.ld

**Purpose:** Linker script for sysman Ring 3 process.

### Layout
```
ENTRY(sysman_main)
. = 0;                 # Position-independent (starts at 0)
```

### Notes
- Uses position-independent addressing
- Converted to flat binary for loading
