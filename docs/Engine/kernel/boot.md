# Boot Sequence

## Files
- `src/loader/boot.s` — GRUB multiboot entry point
- `src/loader/kernel.c` — kernel_main() initialization
- `src/loader/linker/linker.ld` — Kernel linker script

## Boot Flow

1. GRUB loads kernel ELF + 12 modules into physical memory
2. `boot.s`: Set up stack, pass multiboot info, call `kernel_main()`
3. `kernel_main()` initialization order:
   - GDT (Global Descriptor Table)
   - IDT (Interrupt Descriptor Table)
   - IRQ (PIC remapping)
   - PIT (Programmable Interval Timer, 50Hz)
   - PMM (Physical Memory Manager — bitmap from multiboot memory map)
   - Paging (identity map kernel space, enable paging)
   - Kernel heap (4MB at 0x01000000)
   - PCI bus enumeration
   - Device registration (display, keyboard, mouse, disk, network)
   - SHM manager init
   - Cell manager init
   - Time manager init
   - GRUB module manager (catalog all 12 modules)
   - Create sysman process from module 0
   - Start scheduler

4. Sysman (Ring 3, PID 1) takes over:
   - Loads executives (modules 1–9) in order
   - Loads orbit (module 10) and terminal (module 11)
   - Monitors executive health

## Known Issues
*(Agents add issues here)*
