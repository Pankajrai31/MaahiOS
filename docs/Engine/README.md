# MaahiOS Engineering Knowledge Base

**Living documentation maintained by the agent engineering team.**

This folder is the single source of truth for every module in MaahiOS.
Each file documents one subsystem — its purpose, API contract, file locations,
known issues, and architectural rules.

Agents read and update these docs as they work. If a doc contradicts the code,
the code wins — update the doc.

## Structure

```
docs/Engine/
├── README.md                  ← You are here
├── vision.md                  ← OS goals, roadmap, long-term direction
├── architecture.md            ← 6-layer model, memory map, boot sequence
├── inventory.md               ← Complete module inventory (auto-updated)
├── audit-findings.md          ← Architecture violations and issues found
├── build-system.md            ← Build pipeline, cross-compiler, MEX packing
│
├── kernel/                    ← Kernel-space documentation
│   ├── boot.md                ← Boot sequence (GRUB → kernel.c → sysman)
│   ├── gdt-idt.md             ← GDT, IDT, IRQ, exceptions
│   ├── memory.md              ← PMM, paging, kernel heap, memory map
│   ├── process.md             ← Process manager, scheduler, MEX format
│   ├── syscall.md             ← Syscall table, handler structure, domains
│   ├── cell.md                ← Cell manager (key-value registry)
│   ├── device.md              ← Device manager, device_ops_t interface
│   ├── shm.md                 ← Shared memory manager
│   ├── time.md                ← Time manager, PIT, RTC
│   ├── klog.md                ← Kernel logging system
│   └── network.md             ← Network manager, TCP/IP stack
│
├── drivers/                   ← Hardware driver documentation
│   ├── display.md             ← BGA, VBE, framebuffer
│   ├── storage.md             ← ATA, disk, iso9660, MFS, partitions, volumes
│   ├── input.md               ← Keyboard, mouse (PS/2)
│   ├── network.md             ← E1000 NIC driver
│   └── pci-rtc.md             ← PCI bus, Real-Time Clock
│
├── executives/                ← Executive layer documentation
│   ├── overview.md            ← Executive pattern, SHM queue protocol
│   ├── sysman.md              ← PID 1, boot orchestration
│   ├── log.md                 ← Log Executive
│   ├── cell.md                ← Cell Executive
│   ├── process.md             ← Process Executive
│   ├── memory.md              ← Memory Executive
│   ├── disk.md                ← Disk Executive
│   ├── fs.md                  ← FS Executive
│   ├── gui.md                 ← GUI Executive
│   ├── io.md                  ← I/O Executive
│   ├── wm.md                  ← WM Executive (compositor)
│   └── network.md             ← Network Executive
│
├── libraries/                 ← User-space library documentation
│   ├── overview.md            ← Library patterns, naming conventions
│   ├── core.md                ← syscall_helpers, shared headers
│   ├── libcell.md
│   ├── libconsole.md
│   ├── libdisk.md
│   ├── libfs.md
│   ├── libgui.md              ← Drawing, fonts, keyboard
│   ├── libhtml.md
│   ├── libhttp.md
│   ├── libio.md
│   ├── libjs.md
│   ├── liblog.md
│   ├── libmemory.md
│   ├── libmex.md
│   ├── libnet.md
│   ├── libprocess.md
│   ├── libtls.md
│   ├── libwindow.md           ← Windowed UI, controls, theme
│   └── libwm.md
│
├── systemprograms/            ← System program documentation
│   ├── orbit.md               ← Desktop shell
│   └── terminal.md            ← Console terminal
│
└── apps/                      ← Application documentation
    └── overview.md            ← App patterns, MEX entry, build
```

## Rules

1. **One file per module** — never mix two subsystems in one doc
2. **Code is truth** — docs describe what IS, not what SHOULD BE
3. **Update on change** — any agent that modifies code must update the matching doc
4. **Audit findings** go in `audit-findings.md` with date and severity
5. **Vision changes** need explicit user approval before updating `vision.md`
