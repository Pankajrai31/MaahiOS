# Interrupt Handling Documentation

## Overview

MaahiOS implements a complete interrupt handling system:
- **GDT** - Defines memory segments and privilege levels
- **IDT** - Maps interrupt vectors to handlers
- **Exception Handlers** - Handle CPU exceptions (0-19)
- **IRQ Manager** - PIC configuration and IRQ routing

---

## gdt.c

**Purpose:** Global Descriptor Table setup with Ring 0/3 segments and TSS.

### Constants
```c
#define GDT_ENTRIES 6
```

### GDT Entry Layout
| Index | Name | Base | Limit | Access | Description |
|-------|------|------|-------|--------|-------------|
| 0 | Null | 0 | 0 | 0 | Required null segment |
| 1 | Kernel Code | 0 | 4GB | 0x9A | Ring 0 execute/read |
| 2 | Kernel Data | 0 | 4GB | 0x92 | Ring 0 read/write |
| 3 | User Code | 0 | 4GB | 0xFA | Ring 3 execute/read |
| 4 | User Data | 0 | 4GB | 0xF3 | Ring 3 read/write |
| 5 | TSS | &tss | 104 | 0x89 | Task State Segment |

### Segment Selectors
```
Kernel CS = 0x08 (Index 1 << 3)
Kernel DS = 0x10 (Index 2 << 3)
User CS   = 0x1B (Index 3 << 3 | RPL 3)
User DS   = 0x23 (Index 4 << 3 | RPL 3)
TSS       = 0x28 (Index 5 << 3)
```

### TSS Structure
```c
struct tss_entry {
    unsigned int prev_tss;
    unsigned int esp0;     // Ring 0 stack for interrupts
    unsigned int ss0;      // Ring 0 stack segment
    // ... other fields
    unsigned short trap;
    unsigned short iomap_base;
} __attribute__((packed));
```

### Key Functions

| Function | Description |
|----------|-------------|
| `gdt_set_entry(index, base, limit, access, gran)` | Set GDT descriptor |
| `gdt_set_tss_entry(index, base, limit)` | Set TSS descriptor |
| `gdt_init()` | Initialize GDT and TSS |
| `gdt_load()` | Load GDT and TSS into CPU |
| `gdt_set_kernel_stack(esp0)` | Update TSS esp0 for process switch |

### Issues Identified

1. **Hardcoded Ring 0 Stack Address (Line 93)**
   ```c
   tss.esp0 = 0x00090000;  // Ring 0 stack at 576KB
   ```
   - Address is arbitrary and undocumented.
   - **Suggestion:** Define constant with comment about safe region.

2. **No I/O Permission Bitmap**
   - `iomap_base = sizeof(tss)` disables I/O bitmap.
   - User processes cannot do direct I/O (correct for security).

---

## idt.c

**Purpose:** Interrupt Descriptor Table setup for exceptions and interrupts.

### Constants
```c
#define IDT_ENTRIES 256
```

### IDT Entry Structure
```c
struct idt_entry {
    unsigned short offset_low;   // Handler address bits 0-15
    unsigned short selector;     // Code segment selector
    unsigned char zero;          // Reserved
    unsigned char type_attr;     // Type and attributes
    unsigned short offset_high;  // Handler address bits 16-31
} __attribute__((packed));
```

### Type Attributes
```
0x8E = Present, DPL 0, 32-bit Interrupt Gate
0x8F = Present, DPL 0, 32-bit Trap Gate
0xEE = Present, DPL 3, 32-bit Trap Gate (for INT 0x80)
```

### Key Functions

| Function | Description |
|----------|-------------|
| `idt_set_entry(index, handler, selector, type)` | Set IDT entry |
| `idt_init()` | Initialize IDT |
| `idt_load()` | Load IDT into CPU |
| `idt_install_exception_handlers()` | Install exception handlers |
| `idt_install_mouse_handler()` | Install IRQ12 handler |

### Installed Handlers
| Vector | Handler | Type | Description |
|--------|---------|------|-------------|
| 0-19 | exception_stub_N | Trap | CPU exceptions |
| 32 | irq0_stub | Interrupt | PIT Timer |
| 44 | irq12_stub | Interrupt | PS/2 Mouse |
| 128 | syscall_int | Trap | System calls |

### Issues Identified

1. **Trap vs Interrupt Gates (Lines 76-94)**
   - Exceptions use Trap Gates (0x8F) which don't disable interrupts.
   - **Issue:** Nested exceptions possible during handling.
   - **Suggestion:** Consider using Interrupt Gates for some exceptions.

2. **Missing IRQ Handlers**
   - Only IRQ0 and IRQ12 are installed.
   - Other IRQs (keyboard, disk, etc.) not handled.
   - **Suggestion:** Install handlers or at least spurious IRQ handlers.

---

## exception_handler.c

**Purpose:** C-level exception handling with BLACKHOLE panic screen.

### Key Functions

| Function | Description |
|----------|-------------|
| `exception_handler(num, error_code)` | Main exception dispatcher |
| `get_exception_name(num)` | Get exception name string |
| `get_exception_description(num)` | Get exception description |
| `handle_user_exception(num, code)` | Handle Ring 3 exception |
| `handle_kernel_exception(num, code, eip)` | Handle Ring 0 exception |

### Exception Names
| Number | Name |
|--------|------|
| 0 | Divide by Zero |
| 6 | Invalid Opcode |
| 13 | General Protection Fault |
| 14 | Page Fault |

### Privilege Detection
```c
if (cs & 0x3) {
    // Ring 3 - user mode exception
    handle_user_exception(exception_num, error_code);
} else {
    // Ring 0 - kernel mode exception
    handle_kernel_exception(exception_num, error_code, eip);
}
```

### BLACKHOLE Panic Screen
- Switches to VGA text mode
- Displays exception information
- Shows CPU registers (EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP)
- Shows control registers (CR0, CR2, CR3)
- Halts system

### Issues Identified

1. **Stack Frame Assumptions (Lines 210-221)**
   ```c
   __asm__ volatile(
       "movl 36(%%esp), %0\n"
       "movl 40(%%esp), %1"
       : "=r"(eip), "=r"(cs)
   );
   ```
   - Hardcoded stack offsets depend on exact stack layout.
   - **Issue:** Could break if compiler changes stack frame.
   - **Suggestion:** Use proper stack frame parameter passing.

2. **User Exception Recovery (Lines 71-80)**
   - Just restarts sysman without cleanup.
   - **Suggestion:** Implement proper process termination.

3. **Global sysman_entry_point Usage**
   - Relies on global variable being set correctly.

---

## interrupt_stubs.s

**Purpose:** Assembly interrupt stubs that save state and call C handlers.

### Exception Stubs (0-19)
```assembly
exception_no_error_code N    # For exceptions without error code
exception_with_error_code N  # For exceptions with error code
```

### Common Handler
```assembly
exception_common:
    push %eax, %ebx, %ecx, %edx, %esi, %edi, %ebp
    call exception_handler
    pop all registers
    add $8, %esp  # Remove error code and exception number
    iret
```

### Syscall Stub (INT 0x80)
```assembly
syscall_int:
    push %ebp, %ebx, %edi, %esi
    push user_esp, esi, edx, ecx, ebx, eax
    call syscall_dispatcher
    pop parameters
    iret
```

### IRQ Stubs
- **IRQ0 (Timer):** `pusha`, call `pit_handler`, EOI, `popa`, `iret`
- **IRQ12 (Mouse):** `pusha`, call `mouse_handler`, EOI to both PICs, `popa`, `iret`

### Issues Identified

1. **Double EOI for Mouse (Lines 208-220)**
   - Mouse handler sends EOI, then stub sends EOI again.
   - **Suggestion:** Remove EOI from mouse_handler.c.

2. **User ESP Calculation (Lines 156-158)**
   - Complex calculation of user stack pointer.
   - **Suggestion:** Document the stack layout more clearly.

---

## irq_manager.c / irq_manager.h

**Purpose:** PIC configuration and IRQ enable/disable.

### PIC Ports
```c
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
```

### PIC Remapping
```
IRQ 0-7  → INT 0x20-0x27 (Master PIC)
IRQ 8-15 → INT 0x28-0x2F (Slave PIC)
```

### Key Functions

| Function | Description |
|----------|-------------|
| `irq_manager_init()` | Initialize and remap PICs |
| `irq_enable(irq)` | Enable specific IRQ |
| `irq_disable(irq)` | Disable specific IRQ |
| `irq_enable_timer()` | Enable IRQ0 (timer) |
| `irq_enable_mouse()` | Enable IRQ12 (mouse) |
| `irq_get_pic_mask()` | Get current PIC mask |

### Initialization
1. Send ICW1 (initialization command) to both PICs
2. Send ICW2 (vector offset) - 0x20 for master, 0x28 for slave
3. Send ICW3 (cascade configuration)
4. Send ICW4 (8086 mode)
5. Mask all IRQs initially

### Issues Identified

1. **Debug Output in IRQ Enable (Lines 97-172)**
   - Serial printing in interrupt-related code.
   - **Issue:** Could cause timing issues.
   - **Suggestion:** Make debug output conditional.

2. **Retry Logic (Lines 134-136, 166-170)**
   - Multiple retries if PIC mask write fails.
   - **Issue:** Indicates underlying problem not addressed.
   - **Suggestion:** Investigate root cause of write failures.

3. **No Spurious IRQ Handling**
   - IRQ7 and IRQ15 can be spurious.
   - **Suggestion:** Add spurious IRQ detection.
