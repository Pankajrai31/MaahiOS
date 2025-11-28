# System Call Interface Documentation

## Overview

MaahiOS uses INT 0x80 for system calls, following the Linux convention:
- **EAX** = syscall number
- **EBX** = arg1
- **ECX** = arg2
- **EDX** = arg3
- **ESI** = arg4
- Return value in **EAX**

---

## syscall_numbers.h

**Purpose:** Define syscall number constants used by both kernel and user space.

### Basic I/O Syscalls
| Number | Name | Description |
|--------|------|-------------|
| 1 | SYSCALL_PUTCHAR | Print single character |
| 2 | SYSCALL_PUTS | Print string |
| 3 | SYSCALL_PUTINT | Print integer |
| 4 | SYSCALL_EXIT | Terminate program |
| 5 | SYSCALL_WRITE | Write to file descriptor |

### Memory Syscalls
| Number | Name | Description |
|--------|------|-------------|
| 6 | SYSCALL_ALLOC_PAGE | Allocate 4KB page |
| 7 | SYSCALL_FREE_PAGE | Free page |

### VGA Text Mode Syscalls
| Number | Name | Description |
|--------|------|-------------|
| 8 | SYSCALL_CLEAR | Clear screen |
| 9 | SYSCALL_SET_COLOR | Set text colors |
| 10 | SYSCALL_DRAW_RECT | Draw filled rectangle |
| 11 | SYSCALL_GRAPHICS_MODE | Switch to Mode 13h |
| 12 | SYSCALL_PUT_PIXEL | Draw pixel |
| 13 | SYSCALL_CLEAR_GFX | Clear graphics screen |
| 14 | SYSCALL_PRINT_AT | Print at position |
| 15 | SYSCALL_SET_CURSOR | Set cursor position |
| 16 | SYSCALL_DRAW_BOX | Draw box border |

### Process Syscalls
| Number | Name | Description |
|--------|------|-------------|
| 17 | SYSCALL_CREATE_PROCESS | Create new process |
| 18 | SYSCALL_GET_ORBIT_ADDR | Get orbit module address |

### Graphics (BGA) Syscalls
| Number | Name | Description |
|--------|------|-------------|
| 19 | SYSCALL_GFX_PUTC | Print character at cursor |
| 20 | SYSCALL_GFX_PUTS | Print string at cursor |
| 21 | SYSCALL_GFX_CLEAR | Clear to black |
| 22 | SYSCALL_GFX_SET_COLOR | Set fg/bg colors |
| 23 | SYSCALL_GFX_FILL_RECT | Draw filled rectangle |
| 24 | SYSCALL_GFX_DRAW_RECT | Draw rectangle outline |
| 25 | SYSCALL_GFX_PRINT_AT | Print at position |
| 26 | SYSCALL_GFX_CLEAR_COLOR | Clear to RGB color |
| 27 | SYSCALL_GFX_DRAW_BMP | Draw BMP image |

### Mouse Syscalls
| Number | Name | Description |
|--------|------|-------------|
| 28 | SYSCALL_MOUSE_GET_X | Get mouse X position |
| 29 | SYSCALL_MOUSE_GET_Y | Get mouse Y position |
| 30 | SYSCALL_MOUSE_GET_BUTTONS | Get button state |

### Scheduler Syscalls
| Number | Name | Description |
|--------|------|-------------|
| 31 | SYSCALL_YIELD | Yield CPU |

### Debug Syscalls
| Number | Name | Description |
|--------|------|-------------|
| 32 | SYSCALL_MOUSE_GET_IRQ_TOTAL | Get IRQ12 count |
| 33 | SYSCALL_GET_PIC_MASK | Get PIC mask |
| 34 | SYSCALL_RE_ENABLE_MOUSE | Re-enable IRQ12 |
| 35 | SYSCALL_POLL_MOUSE | Poll 8042 manually |

---

## syscall_handler.c

**Purpose:** Kernel-side syscall dispatcher that handles INT 0x80.

### Global State
```c
uint32_t current_fg_color = 0xFFFFFFFF;  // White
uint32_t current_bg_color = 0x00000000;  // Black
```

### Dispatcher Function
```c
unsigned int syscall_dispatcher(
    unsigned int syscall_num,
    unsigned int arg1,
    unsigned int arg2,
    unsigned int arg3,
    unsigned int arg4_esi,
    unsigned int user_esp
);
```

### Critical: Re-enable Interrupts
```c
// CRITICAL: Re-enable interrupts during syscall handling
// Using an Interrupt Gate (0x8E) in IDT clears IF on entry,
// but we need timer/mouse IRQs to work during syscall handling
__asm__ volatile("sti");
```

Note: The IDT entry type (Interrupt Gate vs Trap Gate) determines whether IF is cleared on interrupt entry. MaahiOS uses a Trap Gate (0xEE) for INT 0x80, which does not clear IF, but the explicit `sti` ensures interrupts are enabled regardless.

### Syscall Implementations

#### SYSCALL_PUTS (2)
```c
case SYSCALL_PUTS:
    serial_print("[SYSCALL_PUTS] str=");
    if (arg1 != 0) {
        serial_print((const char*)arg1);
    }
    kernel_puts((const char*)arg1);
    break;
```

#### SYSCALL_ALLOC_PAGE (6)
```c
case SYSCALL_ALLOC_PAGE:
    return_value = kernel_alloc_page();  // Via VMM
    break;
```

#### SYSCALL_GFX_FILL_RECT (23)
```c
case SYSCALL_GFX_FILL_RECT:
    int x = (int)arg1;
    int y = (int)arg2;
    unsigned int packed = arg3;
    uint32_t color = arg4_esi;
    
    int width = (int)(packed & 0xFFFF);
    int height = (int)(packed >> 16);
    
    bga_fill_rect(x, y, width, height, color);
    break;
```

#### SYSCALL_POLL_MOUSE (35)
```c
case SYSCALL_POLL_MOUSE:
    uint8_t status = inb(0x64);
    if ((status & 0x01) && (status & 0x20)) {
        mouse_handler();  // Call directly
        return_value = 1;
    } else {
        return_value = 0;
    }
    break;
```

### Issues Identified

1. **Hardcoded Color in PRINT_AT (Lines 287-289)**
   ```c
   uint32_t fg = 0xFFFFFF;  // Pure white
   uint32_t bg = 0x000000;  // Black (ignored)
   ```
   - Ignores actual color parameters.
   - **Suggestion:** Read colors from stack or registers.

2. **Same Issue in GFX_PRINT_AT (Lines 392-398)**
   - Colors are forced to white/black.

3. **Serial Debug Everywhere (Lines 212, 416-420)**
   - Heavy serial output slows down syscalls.
   - **Suggestion:** Make conditional.

4. **Unused Parameters (Lines 192-194)**
   ```c
   (void)arg2;
   (void)arg3;
   (void)arg4_esi;
   ```
   - Some syscalls don't use all parameters.

5. **Direct Handler Call in Poll (Line 481)**
   ```c
   mouse_handler();  // Call handler directly
   ```
   - Bypasses interrupt mechanism.
   - Works but could cause issues.

---

## user_syscalls.c / user_syscalls.h

**Purpose:** User-space syscall wrapper functions.

### Basic Pattern
```c
void syscall_putchar(char c) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PUTCHAR), "b"(c)
        : "memory"
    );
}
```

### Return Value Pattern
```c
int syscall_mouse_get_x(void) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_MOUSE_GET_X)
        : "memory"
    );
    return result;
}
```

### Complex Parameter Pattern (syscall_fill_rect)
```c
void syscall_fill_rect(int x, int y, int width, int height, unsigned int color) {
    // Pack width/height to avoid stack issues
    unsigned int packed_wh = ((unsigned int)height << 16) | 
                             ((unsigned int)width & 0xFFFF);
    
    asm volatile(
        "movl $23, %%eax\n\t"
        "movl %[x], %%ebx\n\t"
        "movl %[y], %%ecx\n\t"
        "movl %[packed], %%edx\n\t"
        "movl %[color], %%esi\n\t"
        "int $0x80\n\t"
        :
        : [x] "rm" (x), [y] "rm" (y), [packed] "rm" (packed_wh), [color] "rm" (color)
        : "eax", "ebx", "ecx", "edx", "esi", "memory"
    );
}
```

### Color Constants
```c
#define COLOR_BLACK     0
#define COLOR_WHITE     1
#define COLOR_RED       2
#define COLOR_GREEN     3
#define COLOR_BLUE      4
#define COLOR_YELLOW    5
#define COLOR_CYAN      6
#define COLOR_MAGENTA   7
```

### Key User Functions

| Function | Description |
|----------|-------------|
| `syscall_putchar(c)` | Print character |
| `syscall_puts(str)` | Print string |
| `syscall_putint(num)` | Print integer |
| `syscall_exit(code)` | Exit program |
| `syscall_alloc_page()` | Allocate memory |
| `syscall_free_page(addr)` | Free memory |
| `syscall_clear()` | Clear screen |
| `syscall_set_color(fg, bg)` | Set colors |
| `syscall_create_process(entry)` | Create process |
| `syscall_get_orbit_address()` | Get orbit address |
| `gfx_putc(c)` | Graphics character |
| `gfx_puts(str)` | Graphics string |
| `gfx_clear()` | Clear graphics |
| `gfx_set_color(fg, bg)` | Set graphics colors |
| `syscall_fill_rect(...)` | Draw filled rectangle |
| `syscall_draw_rect(...)` | Draw rectangle outline |
| `syscall_print_at(...)` | Print at position |
| `syscall_gfx_clear_color(rgb)` | Clear to color |
| `syscall_draw_bmp(...)` | Draw BMP |
| `syscall_mouse_get_x()` | Get mouse X |
| `syscall_mouse_get_y()` | Get mouse Y |
| `syscall_mouse_get_buttons()` | Get buttons |
| `syscall_yield()` | Yield CPU |
| `syscall_poll_mouse()` | Poll mouse manually |

### Issues Identified

1. **Stack Manipulation in draw_rect (Lines 186-200)**
   ```c
   "pushl %[color]\n\t"
   "pushl %[height]\n\t"
   // ...
   "addl $8, %%esp\n\t"
   ```
   - Manual stack push/pop could fail if interrupted.
   - **Suggestion:** Use register-only passing like fill_rect.

2. **Color Mapping in gfx_set_color (Lines 143-165)**
   - Only 8 colors supported.
   - **Suggestion:** Allow direct RGB values.

3. **Header Declares More Than Implemented**
   - Header has `gfx_puts` etc. that map to specific syscalls.
   - Implementation is complete but could be cleaner.

---

## Syscall Flow Summary

```
User Code:
    syscall_puts("Hello");
        ↓
    mov eax, 2          // SYSCALL_PUTS
    mov ebx, str_ptr    // Argument
    int 0x80
        ↓
CPU Interrupt:
    Save EFLAGS, CS, EIP to stack
    Switch to Ring 0
    Jump to IDT[0x80] handler
        ↓
interrupt_stubs.s (syscall_int):
    Save registers
    Call syscall_dispatcher(eax, ebx, ecx, edx, esi, esp)
        ↓
syscall_handler.c (syscall_dispatcher):
    sti  // Re-enable interrupts
    switch (syscall_num) {
        case SYSCALL_PUTS:
            kernel_puts((char*)arg1);
            break;
    }
    return value in eax
        ↓
interrupt_stubs.s:
    Restore registers
    iret
        ↓
CPU:
    Restore EFLAGS, CS, EIP
    Return to Ring 3
        ↓
User Code:
    Continue after int 0x80
```
