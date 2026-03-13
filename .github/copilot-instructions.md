# MaahiOS - Global Copilot Instructions
**Auto-loaded for all interactions in this repository**

## Architecture Overview

MaahiOS is a 32-bit x86 operating system with a layered, component-based architecture:

```
┌─────────────────────────────────────────────────────────────┐
│                    USER SPACE (Ring 3)                       │
├─────────────────────────────────────────────────────────────┤
│  Apps (8)           │  System Programs      │  Executives(9) │
│  - diskman          │  - sysman (PID 1)     │  - cell         │
│  - fileman          │  - orbit              │  - disk         │
│  - hello            │  - terminal           │  - fs           │
│  - hellogui         │                       │  - gui          │
│  - boxdrop          │                       │  - io           │
│  - procman          │                       │  - log          │
│  - shutdown         │                       │  - memory       │
│  - sysinfo          │                       │  - process      │
│                     │                       │  - wm           │
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
│  - display (BGA/VBE)  - keyboard    - mouse                  │
│  - drive (ATA/ISO9660/MFS/partition/volume)                  │
│  - pci                - rtc         - vga (early boot)       │
├─────────────────────────────────────────────────────────────┤
│  Core (GDT, IDT, IRQ, PIT)                                   │
└─────────────────────────────────────────────────────────────┘
```

## GRUB Module Assignments (Indices 0–11)

| Idx | Binary         | Component                         |
|-----|----------------|-----------------------------------|
|  0  | sysman.bin     | Sysman (PID 1 — loads all others) |
|  1  | logexec.bin    | Log Executive                     |
|  2  | cellexec.bin   | Cell Executive                    |
|  3  | procexec.bin   | Process Executive                 |
|  4  | memexec.bin    | Memory Executive                  |
|  5  | diskexec.bin   | Disk Executive                    |
|  6  | fsexec.bin     | FS Executive                      |
|  7  | guiexec.bin    | GUI Executive                     |
|  8  | ioexec.bin     | I/O Executive                     |
|  9  | wmexec.bin     | WM Executive                      |
| 10  | orbit.bin      | Orbit (desktop shell)             |
| 11  | terminal.bin   | Terminal                          |

## Directory Structure

```
src/
├── loader/           # Kernel entry (boot.s, kernel.c)
├── managers/         # Kernel-space managers
│   ├── cell/         # Cell manager (key-value store)
│   ├── device/       # Device manager (unified device ops)
│   ├── gdt/          # Global Descriptor Table
│   ├── grub_module/  # GRUB module loading
│   ├── interrupt/    # IDT, exceptions
│   ├── irq/          # IRQ handling
│   ├── klog/         # Kernel logging
│   ├── memory/       # PMM, paging, kheap
│   ├── process/      # Process management + MEX header
│   ├── ring3/        # Ring 3 switching
│   ├── scheduler/    # Process scheduling
│   ├── shm/          # Shared memory
│   ├── syscall/      # Syscall dispatcher + 10 handler files (63 syscalls)
│   ├── time/         # Time management
│   └── timer/        # PIT timer driver
├── drivers/          # Hardware drivers
│   ├── display/      # BGA, VBE, display abstraction
│   ├── drive/        # Storage subsystem
│   │   ├── ata/      # ATA/IDE disk driver
│   │   ├── disk/     # Disk abstraction + disk_subsystem
│   │   ├── iso9660/  # ISO 9660 CD-ROM filesystem
│   │   ├── mfs/      # MaahiOS File System
│   │   ├── partition/ # MBR partition driver
│   │   └── volume/   # Volume/mount manager
│   ├── keyboard/     # PS/2 keyboard
│   ├── mouse/        # PS/2 mouse
│   ├── pci/          # PCI bus enumeration
│   ├── rtc/          # Real-time clock
│   └── vga/          # VGA text mode (early boot)
├── system/
│   ├── executives/   # Ring 3 executives (9 + common library)
│   │   ├── common/          # Executive common: SHM queue helpers
│   │   ├── cellexecutive/   # Cell Executive
│   │   ├── diskexecutive/   # Disk Executive
│   │   ├── fsexecutive/     # FS (Filesystem) Executive
│   │   ├── guiexecutive/    # GUI Executive
│   │   ├── ioexecutive/     # I/O Executive
│   │   ├── logexecutive/    # Log Executive
│   │   ├── memoryexecutive/ # Memory Executive
│   │   ├── processexecutive/# Process Executive
│   │   └── wmexecutive/     # WM (Window Manager) Executive
│   ├── libraries/    # User-space libraries (15)
│   │   ├── core/       # Low-level syscall helpers (syscall_helpers.h)
│   │   ├── libbmp/     # BMP image decoder (24/32-bit, no-malloc)
│   │   ├── libcell/    # Cell registry (key-value store) access
│   │   ├── libconsole/ # Stdout for console .mex apps (SHM → Terminal)
│   │   ├── libdisk/    # Block-level raw disk access via Disk Executive
│   │   ├── libfs/      # File/directory ops via FS Executive
│   │   ├── libgui/     # GUI: framebuffer, drawing, fonts, keyboard
│   │   │   ├── console/   # GUI console
│   │   │   ├── fonts/     # libfont (AA Segoe UI), font8x16 (VGA bitmap)
│   │   │   ├── keyboard/  # Keyboard input
│   │   │   └── printgui/  # gui_draw_text, gui_measure_text, gui_blit_icon
│   │   ├── libio/      # Device input via I/O Executive (ONLY input path)
│   │   ├── liblog/     # User-space logging via Log Executive
│   │   ├── libmemory/  # Memory: alloc/free pages, SHM ops
│   │   ├── libmex/     # MEX format parser (all .mex knowledge lives here)
│   │   ├── libprocess/ # Process management via Process Executive
│   │   ├── libwindow/  # Windowed GUI: surface, controls, theme, malloc/free
│   │   │   └── controls/  # button.c, label.c, control.h
│   │   ├── libwm/      # WM client library (SHM queue to WM Executive)
│   │   └── shared/     # Kernel-shared headers (io.h, taskbar_types.h, wm_types.h)
│   └── systemprograms/  # sysman, orbit, terminal
├── apps/             # User applications (.mex format)
│   ├── boxdrop/      # BoxDrop (GUI game, windowed)
│   ├── diskman/      # Disk Manager (console)
│   ├── fileman/      # File Manager (console)
│   ├── hello/        # Hello (console)
│   ├── hellogui/     # Hello GUI (windowed)
│   ├── procman/      # Process Manager (console)
│   ├── shutdown/     # Shutdown/Restart (console)
│   └── sysinfo/      # System Info (console)
└── images/icons/     # 32x32 BMP icons for desktop shortcuts
```

## Key Patterns - ALWAYS FOLLOW

### 1. Syscall Flow (User → Kernel → User)
```
User Code → Library Function → int 0x80 → syscall_manager → handler → Manager → return
```
**63 syscalls** across **10 domains** (10 handler files):

| Domain     | Range   | Count | Key syscalls                                        |
|------------|---------|-------|-----------------------------------------------------|
| Core       | 0–5     | 6     | EXIT, YIELD, GETPID, SLEEP, SHUTDOWN, RESTART       |
| Process    | 16–22   | 7     | CREATE, KILL, INFO, GET_COUNT, EXEC, LIST, SET_NAME |
| Memory     | 32–35   | 4     | ALLOC_PAGE, FREE_PAGE, ALLOC, ATOMIC_COPY           |
| SHM        | 48–52   | 5     | CREATE, ATTACH, DETACH, DESTROY, INFO               |
| Cell       | 64–69   | 6     | WRITE, READ, DELETE, EXISTS, GET_SHM_ID, LIST       |
| Device     | 80–87   | 8     | OPEN, CLOSE, READ, WRITE, IOCTL, POLL, LIST, DISK_FORMAT |
| Module     | 96–100  | 5     | GET_COUNT, GET_INFO, GET_ADDR, GET_SIZE, COPY       |
| Time       | 112–117 | 6     | GET_DATETIME, GET_UNIX, GET_UPTIME, GET_TICKS, GET_TICK_FREQ, GET_SHM_ID |
| Filesystem | 128–137 | 10    | LIST_DIR, READ_FILE, FILE_COUNT, FIND_DIR, GET_ROOT_INFO, WRITE_FILE, DELETE_FILE, CREATE_DIR, VOL_COUNT, VOL_INFO |
| Debug/KLog | 240–245 | 6     | KLOG, KLOG_HEX, KLOG_GET_SHM, GET_CPU_INFO, GET_MEM_INFO, GET_PIC_MASK |

### 2. Logging Convention
- Kernel: Use `klog()` from `managers/klog/`
- User: Use `liblog` library → Log Executive → SYSCALL_KLOG → klog

### 3. New Feature Checklist
When adding a feature, consider ALL layers:
- [ ] Syscall number in `managers/syscall/syscall_numbers.h`
- [ ] Handler in `managers/syscall/handlers/`
- [ ] Manager function in `managers/*/`
- [ ] Library wrapper in `system/libraries/`
- [ ] Executive using the library (if needed)
- [ ] Port I/O via `system/libraries/shared/io.h` (never inline your own)

### 4. Memory Regions

| Region                  | Address            | Size     | Notes                                          |
|-------------------------|--------------------|----------|-------------------------------------------------|
| Kernel text/data/bss    | 0x00100000+        | Variable | Linker entry point, ends at `kernel_end` symbol |
| Kernel heap             | 0x01000000         | 4 MB     | KHEAP_START, bump+free-list allocator            |
| Identity-mapped kernel  | 0x00000000–0x08000000 | 128 MB | Kernel-accessible identity mapping              |
| User process base       | 0x10000000         | Per-proc | Every user process (sysman, execs, .mex) mapped here |
| SHM regions             | Dynamic (PMM)      | Up to 64 | MAX_SHM_REGIONS=64, allocated on demand          |
| Graphics framebuffer    | Hardware-dependent  | ~4 MB    | BGA/VBE, identity-mapped at boot                |

## ⛔ MANDATORY Layering Rules — NEVER VIOLATE

MaahiOS has **6 layers**. Every request flows DOWN through them. **Never skip a layer.**

```
┌─────────────────────────────────────────────────────────────────┐
│ Layer 1: APP           diskman.c, fileman.c, boxdrop.c          │
│   Calls: Libraries ONLY (libdisk, libfs, libcell, libgui)      │
│   NEVER: raw syscalls, executives, kernel functions             │
├─────────────────────────────────────────────────────────────────┤
│ Layer 2: LIBRARY       libdisk, libcell, libio, libfs, libgui   │
│   Calls: Executive via SHM queue push/pop                       │
│   For cells: use libcell (never raw SYS_CELL_* syscalls)       │
│   Exception: libcell.c itself may use raw SYS_CELL_READ for    │
│              bootstrapping (finding Cell Executive's SHM IDs)   │
├─────────────────────────────────────────────────────────────────┤
│ Layer 3: EXECUTIVE     Disk Exec, IO Exec, Cell Exec, etc.     │
│   Receives: SHM queue requests from libraries                   │
│   Calls: Kernel via syscalls (SYS_DEV_*, SYS_CELL_*, etc.)    │
│   Executives are the ONLY user-space code that makes device    │
│   syscalls (SYS_DEV_OPEN/READ/WRITE/IOCTL)                    │
├─────────────────────────────────────────────────────────────────┤
│ Layer 4: SYSCALL       int 0x80 → syscall_manager → handlers   │
│   Bridges: Ring 3 → Ring 0. Validates params, routes to Manager│
│   No business logic here — thin dispatch only                   │
├─────────────────────────────────────────────────────────────────┤
│ Layer 5: MANAGER       device_manager, cell_manager, etc.      │
│   Routes: to correct Driver via device ops function pointers    │
│   Driver-agnostic dispatch layer                                │
├─────────────────────────────────────────────────────────────────┤
│ Layer 6: DRIVER        disk.c, ata.c, keyboard.c, mouse.c     │
│   Talks to: actual hardware via inb()/outb() port I/O          │
│   Kernel code (Ring 0). May call kernel_cell_write() directly. │
│   ONLY drivers touch hardware. Nobody else.                     │
└─────────────────────────────────────────────────────────────────┘
```

### Communication Rules (WHO talks to WHO, and HOW):

| Layer | Talks To | Via | Example |
|-------|----------|-----|---------|
| **App** | Library | Direct function call | `libdisk_list(disks, 8)` |
| **Library** | Executive | SHM queue (push request, poll response) | `exe_request_queue_push(g_req_queue, &req)` |
| **Library** | libcell (for cells) | Function call (libcell routes to Cell Exec) | `libcell_read("key", &val, sizeof(val))` |
| **Executive** | Kernel | Syscall `int 0x80` | `syscall3(SYS_DEV_READ, device_id, buf, size)` |
| **Syscall handler** | Manager | Direct function call (same Ring 0) | `kernel_device_read(dev_id, buf, size)` |
| **Manager** | Driver | Function pointer in ops table | `device.ops.read(buf, size)` → `disk_dev_read()` |
| **Driver** | Hardware | Port I/O | `outb(0x1F7, 0x20)` ATA read command |
| **Kernel code** | Cell Manager | Direct call (Ring 0 to Ring 0) | `kernel_cell_write("device.disk.count", &count, sizeof(int))` |

### ⚠️ Common Violations to AVOID:

1. **Library using raw `syscall3(SYS_CELL_READ, ...)`** → Use `libcell_read()` instead
2. **Library calling `SYS_DEV_READ` directly** → Use `libio_dev_read()` (routes via IO Executive)
3. **App calling syscalls directly** → Always go through a library
4. **Executive calling kernel functions directly** → Must use syscalls (it's Ring 3)
5. **Driver/Manager calling Executive functions** → Kernel never calls user-space
6. **App calling `SYS_SHUTDOWN`/`SYS_RESTART` directly** → Use `libprocess_system_shutdown()`/`libprocess_system_restart()`

### ✅ Accepted Performance Exceptions

These are documented architecture bypasses for hot paths:

| Location        | Bypass                     | Reason                                                       |
|----------------|----------------------------|--------------------------------------------------------------|
| `libgui.c`     | `gui_flip()` calls `SYS_DEV_IOCTL` directly | Display flip is called every frame; IPC round-trip would kill rendering. The entire framebuffer model is direct-access by design. |
| `libcell.c`    | Uses raw `SYS_CELL_READ` for bootstrapping  | Must discover Cell Executive's SHM IDs before the executive path is available. |
| `mex_entry.s`  | Calls `SYS_EXIT` directly  | Bare-metal entry stub — no library available at that point.  |
| `sysman.c`     | Uses raw syscalls          | First user process — loads executives, so no executive path exists yet. |

### Concrete Flow: App Reads Disk List

```
diskman.c           → libdisk_list(disks, 8)              [App → Library]
libdisk.c           → SHM queue push {LIST_DISKS}         [Library → Executive]
disk_executive.c    → libcell_read("device.disk.count")    [Executive → Cell lib → Cell Exec]
disk_executive.c    → SHM queue push response              [Executive → Library]
libdisk.c           → copies result to caller's buffer     [Library → App]
diskman.c           → renders disk list on screen
```

### Concrete Flow: App Reads Keyboard

```
app.c               → kbd_read_event(&evt)                 [App → Library]
keyboard.c (lib)    → libio_dev_read(DEV_KEYBOARD, ...)    [Library → IO lib]
libio.c             → reads from g_kbd_ring (SHM)          [Fast path: shared ring buffer]
                       ↑ filled by IO Executive in background loop:
io_executive.c      → syscall3(SYS_DEV_READ, DEV_KBD...)  [Executive → Kernel]
syscall handler     → kernel_device_read(DEV_KEYBOARD)     [Syscall → Manager]
device_manager.c    → keyboard.ops.read()                  [Manager → Driver]
keyboard.c (driver) → returns key from IRQ ring buffer     [Driver → Hardware data]
```

## Build & Test
```powershell
cd c:\Maahi\MaahiOS\build
.\run_maahios.ps1
```

## Font System
- **Legacy bitmap**: 8x16 fixed-width VGA font (`libgui/fonts/font8x16.c`) — used by Terminal
- **Proportional AA**: Segoe UI via `libfont` at 5 sizes: 12/14/16/18/24px
  - Data generated by `tools/generate_font.py` → `segoe_font_data.h`
  - API: `font_draw_string()`, `font_measure_string()`, `font_line_height()`
  - Wrappers: `surface_draw_text()` (libwindow), `gui_draw_text()` (Orbit/direct-draw)
  - Size enum: FONT_SMALL(12), FONT_BODY(14), FONT_H3(16), FONT_H2(18), FONT_TITLE(24)
  - Theme constants: THEME_FONT_BODY, THEME_FONT_TITLE, etc.

## Icon Pipeline
- **Generator**: `tools/create_icons.py` → 32x32 BMP files in `src/images/icons/`
- **Build**: Copied to ISO root by `build.sh` step 17
- **Runtime**: Orbit loads via `libfs_read_file()` + `libbmp_decode()` + `gui_blit_icon()`
- **Transparency**: Color-key (black 0x000000 = transparent)

## WM Executive (Compositor)
- Damage-rect compositor, SHM pixel surfaces per window
- Max 6 simultaneous windows, z-order stacking
- Client flow: `libwindow` → `libwm` → SHM queue → WM Executive
- Publishes `CELL_WM_REGISTRY` and `CELL_TASKBAR_WINDOWS` for Orbit

## Component Prompts
For detailed work on specific components, include the relevant prompt file:
- `#file:prompts/executive.prompt.md` - Working on executives
- `#file:prompts/manager.prompt.md` - Working on kernel managers
- `#file:prompts/syscall.prompt.md` - Adding/modifying syscalls
- `#file:prompts/driver.prompt.md` - Hardware drivers
- `#file:prompts/uimanager.prompt.md` - UI system
