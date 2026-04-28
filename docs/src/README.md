# MaahiOS Source Code Documentation

This documentation provides a comprehensive overview of all source files in MaahiOS, including their purpose, architecture, issues identified, and suggested improvements.

## Project Architecture

MaahiOS is a hobby operating system that includes:
- **Multiboot-compliant bootloader** (GRUB)
- **32-bit protected mode kernel** with paging
- **Ring 0/Ring 3 privilege separation** with syscalls
- **Preemptive multitasking** via PIT timer
- **Graphics subsystem** (BGA/VBE drivers)
- **PS/2 mouse driver**

## Directory Structure

```
src/
├── boot.s                 # Assembly bootloader/entry point
├── kernel.c               # Main kernel initialization
├── linker.ld              # Kernel linker script
├── sysman_linker.ld       # Sysman linker script
├── drivers/               # Hardware drivers
│   ├── bga.c/h           # Bochs Graphics Adapter driver
│   ├── vga.c             # VGA text mode driver
│   ├── vbe.c             # VESA BIOS Extensions driver
│   ├── graphics.c        # Mode 13h legacy graphics
│   ├── mouse.c/h         # PS/2 mouse driver
│   ├── pci.c/h           # PCI configuration space access
│   └── usb.c/h, uhci.h   # USB/UHCI driver (incomplete)
├── lib/                   # Libraries
│   ├── heap.c/h          # User-space heap allocator
│   └── kheap.c/h         # Kernel heap allocator
├── libgui/               # GUI library
│   ├── libgui.h          # Main GUI header
│   ├── draw.c            # Drawing primitives
│   ├── window.c          # Window management
│   ├── controls.c        # Button/control widgets
│   ├── cursor.c/h        # Mouse cursor rendering
│   ├── bmp.c/h           # BMP image loading
│   └── icons.c/h         # Icon rendering
├── managers/             # Kernel subsystems
│   ├── gdt/gdt.c         # Global Descriptor Table
│   ├── interrupt/        # Interrupt handling
│   │   ├── idt.c         # Interrupt Descriptor Table
│   │   ├── exception_handler.c
│   │   └── interrupt_stubs.s
│   ├── irq/irq_manager.c/h  # IRQ management
│   ├── memory/           # Memory management
│   │   ├── pmm.c/h       # Physical Memory Manager
│   │   └── paging.c/h    # Virtual memory/paging
│   ├── process/          # Process management
│   │   └── process_manager.c/h
│   ├── ring3/ring3.c     # Ring 0→3 transition
│   ├── scheduler/        # Task scheduling
│   │   ├── scheduler.c/h
│   │   └── switch_osdev.s
│   └── timer/pit.c/h     # Programmable Interval Timer
├── syscalls/             # System call interface
│   ├── syscall_numbers.h # Syscall number definitions
│   ├── syscall_handler.c # Kernel-side dispatcher
│   └── user_syscalls.c/h # User-side wrappers
├── sysman/               # System Manager (PID 1)
│   ├── sysman.c
│   └── sysman_entry.s
└── orbit/                # Desktop Shell (PID 2)
    ├── orbit.c
    ├── orbit_entry.s
    ├── orbit_linker.ld
    └── embedded_icons.h
```

## Documentation Files

- [BOOT_AND_KERNEL.md](BOOT_AND_KERNEL.md) - Boot process and kernel initialization
- [DRIVERS.md](DRIVERS.md) - Hardware drivers documentation
- [MEMORY_MANAGEMENT.md](MEMORY_MANAGEMENT.md) - PMM, paging, and heap allocators
- [INTERRUPT_HANDLING.md](INTERRUPT_HANDLING.md) - GDT, IDT, exceptions, and IRQs
- [PROCESS_AND_SCHEDULING.md](PROCESS_AND_SCHEDULING.md) - Process management and scheduler
- [SYSCALLS.md](SYSCALLS.md) - System call interface
- [GUI_LIBRARY.md](GUI_LIBRARY.md) - LibGUI and user applications
- [ISSUES_AND_SUGGESTIONS.md](ISSUES_AND_SUGGESTIONS.md) - Code issues and improvement suggestions

## Quick Start

To build MaahiOS:
```bash
cd build
./build.sh
```

To run in QEMU:
```bash
qemu-system-i386 -cdrom build/boot.iso -serial stdio
```
