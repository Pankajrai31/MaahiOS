# MaahiOS Complete Module Inventory

**Last updated: March 2026**

## Summary

| Category | Count |
|----------|-------|
| Kernel Managers | 16 |
| Driver Groups | 8 (13 driver files) |
| Executives | 10 + common |
| System Programs | 3 (sysman, orbit, terminal) |
| User Libraries | 20 |
| Applications | 15 |
| Syscalls | 63+ across 11 domains |
| GRUB Modules | 12 (indices 0–11) |

## Kernel Managers (src/managers/)

| Manager | Files | Key Public API |
|---------|-------|----------------|
| cell/ | cell_manager.c/.h | kernel_cell_write/read/delete/exists/list() |
| device/ | device_manager.c/.h | kernel_device_open/close/read/write/ioctl/poll/list() |
| gdt/ | gdt.c/.h | gdt_set_kernel_stack() |
| grub_module/ | grub_module_manager.c/.h | kernel_grub_get_module_count/info/addr/size/copy() |
| interrupt/ | exception_handler.c, idt.c/.h, interrupt_stubs.s | IDT setup, exception dispatch |
| irq/ | irq_manager.c/.h | irq_install/uninstall_handler() |
| klog/ | klog.c/.h | klog(), klog_hex(), kernel_klog_get_shm_id() |
| memory/ | pmm.c/.h, paging.c/.h, kheap.c/.h | pmm_alloc/free_page(), paging_init(), kheap_alloc/free() |
| network/ | network_manager.c/.h, tcp.c/.h | TCP/IP protocol stack |
| process/ | process_manager.c/.h, mex.h | process_create(), process_create_from_memory(), process_terminate() |
| ring3/ | ring3.c/.h | Ring 0→3 privilege switching |
| scheduler/ | scheduler.c/.h, switch_osdev.s | schedule(), scheduler_add_process() |
| shm/ | shm_manager.c/.h | kernel_shm_create/attach/detach/destroy/get_info() |
| syscall/ | syscall_manager.c/.h, 12 handler files | int 0x80 dispatch, 63+ syscalls |
| time/ | time_manager.c/.h | kernel_time_get_datetime/unix/uptime/ticks() |
| timer/ | pit.c/.h | pit_init(), pit_get_ticks() |

## Drivers (src/drivers/)

| Driver | Files | Purpose |
|--------|-------|---------|
| display/ | display.c/.h, bga.c/.h, vbe.c/.h | Framebuffer (BGA/VBE) |
| drive/ata/ | ata.c/.h | ATA/IDE disk controller |
| drive/disk/ | disk.c/.h, disk_subsystem.c/.h | Disk abstraction |
| drive/iso9660/ | iso9660.c/.h | CD-ROM filesystem |
| drive/mfs/ | mfs.c/.h | MaahiOS native filesystem |
| drive/partition/ | partdrive.c/.h | MBR partition table |
| drive/volume/ | voldrive.c/.h | Volume/mount manager |
| keyboard/ | keyboard.c/.h | PS/2 keyboard |
| mouse/ | mouse.c/.h | PS/2 mouse |
| network/ | e1000.c/.h | Intel E1000 NIC |
| pci/ | pci.c/.h | PCI bus enumeration |
| rtc/ | rtc.c/.h | Real-Time Clock |
| vga/ | vga.c/.h | VGA text mode (early boot) |

## Executives (src/system/executives/)

| Executive | GRUB Idx | Purpose |
|-----------|----------|---------|
| logexecutive/ | 1 | Aggregates kernel logs |
| cellexecutive/ | 2 | Cell registry access |
| processexecutive/ | 3 | Process creation/management |
| memoryexecutive/ | 4 | Memory operations |
| diskexecutive/ | 5 | Block I/O operations |
| fsexecutive/ | 6 | File/directory operations |
| guiexecutive/ | 7 | Framebuffer management |
| ioexecutive/ | 8 | Keyboard/mouse input |
| wmexecutive/ | 9 | Window compositor |
| networkexecutive/ | - | Network protocol handling |
| common/ | - | Shared SHM queue utilities |

## Libraries (src/system/libraries/)

| Library | Purpose |
|---------|---------|
| core/ | syscall_helpers.h — low-level syscall macros |
| libbmp/ | BMP image decoder |
| libcell/ | Cell registry client |
| libconsole/ | Console stdout (SHM → Terminal) |
| libdisk/ | Block device I/O |
| libfs/ | File/directory operations |
| libgui/ | Drawing, fonts, keyboard, console |
| libhtml/ | HTML tokenizer |
| libhttp/ | HTTP client (keep-alive, redirects) |
| libio/ | Device input via IO Executive |
| libjs/ | JavaScript interpreter |
| liblog/ | User-space logging |
| libmemory/ | Page alloc, malloc/free, SHM |
| libmex/ | MEX format parser/executor |
| libnet/ | Network socket API |
| libprocess/ | Process management |
| libtls/ | TLS 1.2 (RSA, AES-CBC, SHA-256) |
| libwindow/ | Windowed UI framework + controls |
| libwm/ | Window manager client |
| shared/ | Kernel-shared headers (io.h, types) |

## Applications (src/apps/)

| App | Type | Purpose |
|-----|------|---------|
| hello/ | Console | Hello World |
| hellogui/ | GUI | Hello World (windowed) |
| boxdrop/ | GUI | Box collision game |
| browser/ | GUI | Web browser (HTML/TLS) |
| diskman/ | Console | Disk manager |
| diskexp/ | GUI | Disk explorer |
| fileman/ | Console | File manager |
| fetch/ | Console | File transfer (wget-like) |
| logexp/ | Console | Log explorer |
| netexp/ | Console | Network statistics |
| ping/ | Console | ICMP ping |
| procman/ | Console | Process manager |
| procexp/ | Console | Process explorer |
| shutdown/ | Console | Shutdown/restart |
| sysinfo/ | Console | System information |
| wordwrite/ | GUI | Text editor |

## System Programs (src/system/systemprograms/)

| Program | GRUB Idx | Role |
|---------|----------|------|
| sysman/ | 0 | PID 1, loads all executives |
| orbit/ | 10 | Desktop shell/launcher |
| terminal/ | 11 | Console terminal |
