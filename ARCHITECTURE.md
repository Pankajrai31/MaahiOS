# MaahiOS Architecture

This document describes the architecture and design of MaahiOS.

## Overview

MaahiOS is a minimal 32-bit x86 operating system that demonstrates fundamental OS concepts including bootloading, memory management, and hardware interfacing.

## System Boot Flow

```
┌─────────────────────┐
│   Computer Powers   │
│        On           │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│   BIOS/UEFI         │
│   Initializes       │
│   Hardware          │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│   GRUB Bootloader   │
│   Loads Kernel      │
│   (Multiboot)       │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│   boot.asm          │
│   - Setup Stack     │
│   - Call kernel_main│
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│   kernel.c          │
│   - Init Terminal   │
│   - Print Message   │
│   - Halt CPU        │
└─────────────────────┘
```

## Memory Layout

The kernel is loaded at 1MB (0x00100000) as per the multiboot specification:

```
0x00000000  ┌─────────────────────┐
            │   BIOS/Reserved     │
            │      (1 MB)         │
0x00100000  ├─────────────────────┤  ← Kernel Start
            │   .multiboot        │  ← Multiboot Header
            ├─────────────────────┤
            │   .text             │  ← Code Section
            ├─────────────────────┤
            │   .rodata           │  ← Read-only Data
            ├─────────────────────┤
            │   .data             │  ← Initialized Data
            ├─────────────────────┤
            │   .bss              │  ← Uninitialized Data
            │                     │
            │   Stack (16 KB)     │  ← Grows downward
0xB8000     ├─────────────────────┤
            │   VGA Text Buffer   │  ← Video Memory
            │   (80x25 chars)     │
            └─────────────────────┘
```

## Component Details

### boot.asm (Bootloader Entry Point)

**Purpose**: Provides the multiboot header and initial entry point for the kernel.

**Key Features**:
- Multiboot header (magic number, flags, checksum)
- Stack allocation (16 KB in .bss section)
- Entry point that sets up ESP and calls kernel_main
- Halt loop for after kernel returns

**Multiboot Header**:
```
Magic:    0x1BADB002
Flags:    0x00000003 (ALIGN + MEMINFO)
Checksum: -(Magic + Flags)
```

### kernel.c (Kernel Main Code)

**Purpose**: Core kernel functionality including VGA text output.

**Key Components**:

1. **VGA Text Mode Driver**:
   - 80x25 character display
   - 16 color support (foreground and background)
   - Character attributes (color + character)
   - Buffer at physical address 0xB8000

2. **Terminal Functions**:
   - `terminal_initialize()`: Clears screen
   - `terminal_putchar()`: Writes single character
   - `terminal_writestring()`: Writes null-terminated string
   - `terminal_setcolor()`: Changes text color

3. **Kernel Entry**:
   - `kernel_main()`: Entry point called from boot.asm
   - Displays welcome message
   - Returns control to bootloader halt loop

### linker.ld (Linker Script)

**Purpose**: Defines memory layout and section placement.

**Sections**:
- `.multiboot`: Placed at the very beginning for bootloader detection
- `.text`: Code section, 4KB aligned
- `.rodata`: Read-only data, 4KB aligned
- `.data`: Initialized data, 4KB aligned
- `.bss`: Uninitialized data and stack, 4KB aligned

## VGA Text Mode

MaahiOS uses VGA text mode (Mode 3) which provides:
- 80 columns × 25 rows
- 16 foreground colors
- 8 background colors
- Memory-mapped buffer at 0xB8000

Each character cell is 2 bytes:
```
Byte 0: ASCII character code
Byte 1: Attribute byte (4 bits background, 4 bits foreground)
```

## Color Codes

```
0 = Black          8 = Dark Grey
1 = Blue           9 = Light Blue
2 = Green          10 = Light Green
3 = Cyan           11 = Light Cyan
4 = Red            12 = Light Red
5 = Magenta        13 = Light Magenta
6 = Brown          14 = Yellow
7 = Light Grey     15 = White
```

## Build Process

```
boot.asm  ──[NASM]──▶  boot.o  ──┐
                                  │
                                  ├──[LD + linker.ld]──▶  maahios.bin
                                  │
kernel.c  ──[GCC]───▶  kernel.o  ─┘
```

## Current Limitations

1. **No Interrupt Handling**: IDT not implemented
2. **No Input**: Keyboard driver not present
3. **No Memory Management**: No paging or heap
4. **No Multitasking**: Single-threaded execution
5. **No File System**: No persistent storage support
6. **Basic Output Only**: VGA text mode only

## Future Enhancements

### Short Term
- Implement Global Descriptor Table (GDT)
- Set up Interrupt Descriptor Table (IDT)
- Add keyboard input handler
- Implement basic string functions

### Medium Term
- Memory management (paging)
- Timer interrupt handling
- Basic shell/command interpreter
- Syscall interface

### Long Term
- Process scheduler
- File system support
- Device driver framework
- User mode support
- Multiprocessor support

## References

- [OSDev Wiki](https://wiki.osdev.org/)
- [Multiboot Specification](https://www.gnu.org/software/grub/manual/multiboot/)
- [Intel 64 and IA-32 Architectures Software Developer Manuals](https://software.intel.com/content/www/us/en/develop/articles/intel-sdm.html)
- [VGA Text Mode](https://wiki.osdev.org/Text_mode)
