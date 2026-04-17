# GDT, IDT, IRQ

## Files
- `managers/gdt/gdt.c/.h` — Global Descriptor Table
- `managers/interrupt/idt.c/.h` — Interrupt Descriptor Table
- `managers/interrupt/exception_handler.c` — CPU exception handlers
- `managers/interrupt/interrupt_stubs.s` — ISR entry stubs (assembly)
- `managers/irq/irq_manager.c/.h` — PIC (8259) IRQ management

## GDT
- Kernel code segment (Ring 0)
- Kernel data segment (Ring 0)
- User code segment (Ring 3)
- User data segment (Ring 3)
- TSS (Task State Segment) for Ring 3→0 transitions
- `gdt_set_kernel_stack()` — update TSS stack pointer on context switch

## IDT
- 256 entries
- Exceptions 0–31 (CPU faults: divide-by-zero, page fault, GPF, etc.)
- IRQs 32–47 (hardware: timer, keyboard, mouse, disk, NIC)
- Syscall at 0x80 (int 0x80)

## IRQ
- PIC remapped to IRQs 32–47
- `irq_install_handler(irq, handler)` — register IRQ callback
- Key IRQs:
  - IRQ 0 (32): PIT timer
  - IRQ 1 (33): Keyboard
  - IRQ 12 (44): Mouse
  - IRQ 14 (46): ATA primary

## Known Issues
*(Agents add issues here)*
