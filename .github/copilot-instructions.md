# MaahiOS - Global Copilot Instructions
**Auto-loaded for all interactions in this repository**

## Architecture Overview

MaahiOS is a 32-bit x86 operating system with a layered, component-based architecture:

```
┌─────────────────────────────────────────────────────────────┐
│                    USER SPACE (Ring 3)                       │
├─────────────────────────────────────────────────────────────┤
│  Apps              │  System Programs      │  Executives     │
│  - disk_manager    │  - sysman (PID 1)     │  - cell         │
│  - file_manager    │  - orbit              │  - disk         │
│                    │  - uimanager          │  - log          │
│                    │                       │  - memory       │
│                    │                       │  - process      │
│                    │                       │  - queue        │
├─────────────────────────────────────────────────────────────┤
│                 SYSCALL INTERFACE (int 0x80)                 │
├─────────────────────────────────────────────────────────────┤
│                    KERNEL SPACE (Ring 0)                     │
├─────────────────────────────────────────────────────────────┤
│  Managers (Kernel Services)                                  │
│  - syscall_manager    - process_manager    - scheduler       │
│  - cell_manager       - device_manager     - time_manager    │
│  - shm_manager        - grub_module_manager                  │
│  - klog               - pmm/paging/kheap                     │
├─────────────────────────────────────────────────────────────┤
│  Drivers                                                     │
│  - display (BGA/VGA)  - keyboard    - mouse                  │
│  - disk (ATA/ISO9660) - pci         - rtc                    │
├─────────────────────────────────────────────────────────────┤
│  Core (GDT, IDT, IRQ, PIT)                                   │
└─────────────────────────────────────────────────────────────┘
```

## Directory Structure

```
src/
├── loader/           # Kernel entry (boot.s, kernel.c)
├── managers/         # Kernel-space managers [✅ VERIFIED]
│   ├── cell/         # Cell manager
│   ├── device/       # Device manager  
│   ├── gdt/          # Global Descriptor Table
│   ├── grub_module/  # GRUB module loading
│   ├── interrupt/    # IDT, exceptions
│   ├── irq/          # IRQ handling
│   ├── klog/         # Kernel logging
│   ├── memory/       # PMM, paging, kheap
│   ├── process/      # Process management
│   ├── ring3/        # Ring 3 switching
│   ├── scheduler/    # Process scheduling
│   ├── shm/          # Shared memory
│   ├── syscall/      # Syscall dispatcher + 9 handler files (47 syscalls) [✅ VERIFIED]
│   ├── time/         # Time management
│   └── timer/        # PIT timer driver
├── drivers/          # Hardware drivers [✅ VERIFIED]
│   ├── display/      # BGA, VGA text, cursor
│   ├── drive/        # ATA, ISO9660, disk_subsystem
│   ├── keyboard/     # PS/2 keyboard
│   ├── mouse/        # PS/2 mouse
│   ├── pci/          # PCI bus
│   ├── rtc/          # Real-time clock
│   └── vga/          # VGA text mode (early boot)
├── system/
│   ├── executives/   # Ring 3 executives (one per service)
│   ├── libraries/    # User-space libraries (gui, liblog, etc.)
│   │   └── shared/   # Kernel-shared headers (io.h)
│   └── systemprograms/  # sysman, orbit, uimanager
├── apps/             # User applications
└── Filetypes/        # File format handlers (BMP, etc.)
```

## Key Patterns - ALWAYS FOLLOW

### 1. Syscall Flow (User → Kernel → User)
```
User Code → Library Function → int 0x80 → syscall_manager → handler → Manager → return
```
**47 syscalls** across 9 domains: Core(0-3), Process(16-19), Memory(32-35), SHM(48-52), Cell(64-69), Device(80-86), Module(96-100), Time(112-117), Debug(240-245)

### 2. Logging Convention
- Kernel: Use `klog()` from `managers/klog/`
- User: Use `liblog` library → SYSCALL_DEBUG_LOG → klog

### 3. New Feature Checklist
When adding a feature, consider ALL layers:
- [ ] Syscall number in `managers/syscall/syscall_numbers.h`
- [ ] Handler in `managers/syscall/handlers/`
- [ ] Manager function in `managers/*/`
- [ ] Library wrapper in `system/libraries/`
- [ ] Executive using the library (if needed)
- [ ] Port I/O via `system/libraries/shared/io.h` (never inline your own)

### 4. Memory Regions
- Kernel: 0x00100000 - 0x00400000
- User processes: 0x00800000+ (each gets own address space)
- Kernel heap: 0x00400000+

## Build & Test
```powershell
cd c:\Maahi\MaahiOS\build
.\run_maahios.ps1
```

## Component Prompts
For detailed work on specific components, include the relevant prompt file:
- `#file:prompts/executive.prompt.md` - Working on executives
- `#file:prompts/manager.prompt.md` - Working on kernel managers
- `#file:prompts/syscall.prompt.md` - Adding/modifying syscalls
- `#file:prompts/driver.prompt.md` - Hardware drivers
- `#file:prompts/uimanager.prompt.md` - UI system
