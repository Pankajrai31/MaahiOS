# MaahiOS Architecture

## 6-Layer Model

MaahiOS has 6 strict layers. Every request flows DOWN through them. Never skip a layer.

```
┌─────────────────────────────────────────────────────────────────┐
│ Layer 1: APP           diskman.c, fileman.c, browser.c          │
│   Calls: Libraries ONLY (libdisk, libfs, libcell, libgui)      │
│   NEVER: raw syscalls, executives, kernel functions             │
├─────────────────────────────────────────────────────────────────┤
│ Layer 2: LIBRARY       libdisk, libcell, libio, libfs, libgui   │
│   Calls: Executive via SHM queue push/pop                       │
│   For cells: use libcell (never raw SYS_CELL_* syscalls)       │
│   Exception: libcell.c bootstrapping (see Accepted Exceptions)  │
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

## Communication Rules

| From Layer | To Layer | Mechanism | Example |
|------------|----------|-----------|---------|
| App | Library | Direct function call | `libdisk_list(disks, 8)` |
| Library | Executive | SHM queue push/pop | `exe_request_queue_push(g_req_queue, &req)` |
| Library | libcell | Function call → Cell Exec | `libcell_read("key", &val, sizeof(val))` |
| Executive | Kernel | Syscall `int 0x80` | `syscall3(SYS_DEV_READ, dev_id, buf, size)` |
| Syscall Handler | Manager | Direct call (Ring 0) | `kernel_device_read(dev_id, buf, size)` |
| Manager | Driver | Function pointer ops | `device.ops.read()` → `disk_dev_read()` |
| Driver | Hardware | Port I/O | `outb(0x1F7, 0x20)` |
| Kernel code | Cell Manager | Direct call (Ring 0) | `kernel_cell_write(key, &val, sizeof(int))` |

## Accepted Architecture Exceptions

| Location | Bypass | Reason |
|----------|--------|--------|
| `libgui.c` gui_flip() | Calls SYS_DEV_IOCTL directly | Framebuffer flip is per-frame hot path |
| `libcell.c` bootstrap | Uses raw SYS_CELL_READ | Must discover Cell Executive SHM IDs before exec path exists |
| `mex_entry.s` | Calls SYS_EXIT directly | Bare-metal stub, no library available |
| `sysman.c` | Uses raw syscalls | First process, loads executives — no exec path yet |
| `libnet/libhttp/libtls` | Use raw network syscalls | Network Executive provides SHM-based packet path; these libraries use direct socket syscalls for TCP streams |

## Memory Map

| Region | Address | Size | Notes |
|--------|---------|------|-------|
| Kernel text/data/bss | 0x00100000+ | Variable | Ends at `kernel_end` symbol |
| Kernel heap | 0x01000000 | 4 MB | Bump + free-list allocator |
| Identity-mapped kernel | 0x00000000–0x08000000 | 128 MB | Full kernel-accessible range |
| User process base | 0x10000000 | Per-process | All user code mapped here |
| SHM regions | Dynamic (PMM) | Up to 64 | MAX_SHM_REGIONS=64 |
| Graphics framebuffer | Hardware-dependent | ~4 MB | BGA/VBE, identity-mapped |

## GRUB Module Assignments

| Idx | Binary | Component |
|-----|--------|-----------|
| 0 | sysman.bin | Sysman (PID 1) |
| 1 | logexec.bin | Log Executive |
| 2 | cellexec.bin | Cell Executive |
| 3 | procexec.bin | Process Executive |
| 4 | memexec.bin | Memory Executive |
| 5 | diskexec.bin | Disk Executive |
| 6 | fsexec.bin | FS Executive |
| 7 | guiexec.bin | GUI Executive |
| 8 | ioexec.bin | I/O Executive |
| 9 | wmexec.bin | WM Executive |
| 10 | orbit.bin | Orbit (desktop shell) |
| 11 | terminal.bin | Terminal |

## Boot Sequence

1. GRUB loads kernel + 12 modules into memory
2. `boot.s` sets up stack, calls `kernel_main()`
3. `kernel.c` initializes: GDT → IDT → IRQ → PIT → PMM → Paging → Kheap → Devices → SHM → Cells → Time
4. Creates sysman process from GRUB module 0
5. Sysman (PID 1, Ring 3) loads executives in order: log → cell → process → memory → disk → fs → gui → io → wm
6. Sysman loads orbit (module 10) and terminal (module 11)
7. Orbit presents the desktop; user can launch .mex apps

## Syscall Table

**63+ syscalls across 11 domains:**

| Domain | Range | Count | Key Operations |
|--------|-------|-------|----------------|
| Core | 0–5 | 6 | EXIT, YIELD, GETPID, SLEEP, SHUTDOWN, RESTART |
| Process | 16–22 | 7 | CREATE, KILL, INFO, GET_COUNT, EXEC, LIST, SET_NAME |
| Memory | 32–35 | 4 | ALLOC_PAGE, FREE_PAGE, ALLOC, ATOMIC_COPY |
| SHM | 48–52 | 5 | CREATE, ATTACH, DETACH, DESTROY, INFO |
| Cell | 64–69 | 6 | WRITE, READ, DELETE, EXISTS, GET_SHM_ID, LIST |
| Device | 80–87 | 8 | OPEN, CLOSE, READ, WRITE, IOCTL, POLL, LIST, DISK_FORMAT |
| Module | 96–100 | 5 | GET_COUNT, GET_INFO, GET_ADDR, GET_SIZE, COPY |
| Time | 112–117 | 6 | GET_DATETIME, GET_UNIX, GET_UPTIME, GET_TICKS, GET_TICK_FREQ, GET_SHM_ID |
| Filesystem | 128–137 | 10 | LIST_DIR, READ_FILE, FILE_COUNT, FIND_DIR, GET_ROOT_INFO, WRITE_FILE, DELETE_FILE, CREATE_DIR, VOL_COUNT, VOL_INFO |
| Network | 144–156 | 13 | GET_CONFIG, PING, GET_STATUS, SEND/RECV_PACKET, SOCK ops (CREATE, CONNECT, SEND, RECV, CLOSE, SENDTO, RECV_BULK) |
| Debug/KLog | 240–246 | 7 | KLOG, KLOG_HEX, KLOG_GET_SHM, GET_CPU_INFO, GET_MEM_INFO, GET_PIC_MASK, KLOG_READ |

## Concrete Flows

### App Reads Disk List
```
diskman.c           → libdisk_list(disks, 8)              [App → Library]
libdisk.c           → SHM queue push {LIST_DISKS}         [Library → Executive]
disk_executive.c    → libcell_read("device.disk.count")    [Executive → Cell lib]
disk_executive.c    → SHM queue push response              [Executive → Library]
libdisk.c           → copies result to caller's buffer     [Library → App]
```

### App Reads Keyboard
```
app.c               → kbd_read_event(&evt)                 [App → Library]
keyboard.c (lib)    → libio_dev_read(DEV_KEYBOARD, ...)    [Library → IO lib]
libio.c             → reads from g_kbd_ring (SHM)          [Fast path: SHM ring]
                       ↑ filled by IO Executive background loop:
io_executive.c      → syscall3(SYS_DEV_READ, DEV_KBD...)  [Executive → Kernel]
device_manager.c    → keyboard.ops.read()                  [Manager → Driver]
keyboard.c (driver) → returns key from IRQ ring buffer     [Driver → Hardware]
```
