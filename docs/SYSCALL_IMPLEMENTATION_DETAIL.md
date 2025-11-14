# Syscall Implementation - Detailed Breakdown

## Your Understanding (Mostly Correct!)

Your mental model:
```
Ring 3 Code
    ↓
printv(string)  [in syscall file]
    ↓
INT 0x80 instruction
    ↓
CPU switches to Ring 0
    ↓
vga_print()  [kernel function]
    ↓
Back to Ring 3
```

**This is correct!** But let's be more precise about the structure.

---

## The Actual Complete Flow

### 1. Ring 3 User Code Calls Syscall Wrapper

**File: `src/syscalls/user_syscalls.c`** (or `src/syscalls/syscalls.c`)

This file runs in **Ring 3** and contains user-facing functions:

```c
// File: src/syscalls/user_syscalls.c
// This file is COMPILED INTO RING 3 CODE

#define SYSCALL_PUTCHAR 1
#define SYSCALL_PUTS 2

// User calls this function
void syscall_putchar(char c) {
    // Store syscall number in EAX
    // Store argument in EBX
    // Call INT 0x80
    asm volatile(
        "mov $1, %%eax\n\t"      // EAX = syscall number 1 (PUTCHAR)
        "mov %0, %%ebx\n\t"      // EBX = character to print
        "int $0x80\n\t"          // INT 0x80 - software interrupt
        : 
        : "r"(c)                 // Input: c goes to EBX
    );
}

void syscall_puts(const char* str) {
    // Store syscall number in EAX
    // Store argument in EBX
    // Call INT 0x80
    asm volatile(
        "mov $2, %%eax\n\t"      // EAX = syscall number 2 (PUTS)
        "mov %0, %%ebx\n\t"      // EBX = pointer to string
        "int $0x80\n\t"          // INT 0x80 - software interrupt
        : 
        : "r"(str)               // Input: str goes to EBX
    );
}

// Ring 3 user code can call this
int main() {
    syscall_puts("Hello from Ring 3!\n");
    return 0;
}
```

### 2. INT 0x80 Instruction Triggers

When Ring 3 code executes `INT 0x80`:

```
1. CPU recognizes INT 0x80 (software interrupt 128)
2. CPU looks up IDT entry 128
3. IDT entry 128 must have:
   - DPL = 3 (so Ring 3 can call it)
   - Type = 32-bit trap gate or interrupt gate
   - Handler address = pointer to syscall_handler
4. CPU automatically:
   - Saves current EFLAGS, CS, EIP on Ring 3 stack
   - Switches to Ring 0
   - Jumps to syscall_handler function
```

### 3. Kernel Receives Interrupt

**File: `src/managers/interrupt/interrupt_stubs.s`** (assembly)

Add a new entry for INT 0x80:

```asm
# Assembly stub for syscall (INT 0x80)
syscall_stub:
    # No error code pushed by CPU for INT 0x80
    # EAX = syscall number (user already set this)
    # EBX = first argument (user already set this)
    # ECX = second argument (if needed)
    # EDX = third argument (if needed)
    
    # Jump to C handler
    jmp syscall_handler_asm
    
syscall_handler_asm:
    # Preserve registers
    pusha                    # Push all general purpose registers
    
    # Call C function
    # Pass syscall number as first argument
    # Note: We need to extract arguments from EAX, EBX, ECX, EDX
    call syscall_dispatcher
    
    # Restore registers
    popa
    
    # Return to Ring 3
    iret
```

### 4. Syscall Dispatcher (C Code)

**File: `src/managers/interrupt/syscall_handler.c`** (or `src/syscalls/syscall_handler.c`)

This file runs in **Ring 0** (kernel):

```c
// File: src/managers/interrupt/syscall_handler.c
// This file is COMPILED INTO RING 0 CODE

#define SYSCALL_PUTCHAR 1
#define SYSCALL_PUTS 2
#define SYSCALL_EXIT 3

// Called from assembly
// EAX has syscall number
void syscall_dispatcher(unsigned int syscall_num, 
                        unsigned int arg1, 
                        unsigned int arg2, 
                        unsigned int arg3) {
    switch(syscall_num) {
        case SYSCALL_PUTCHAR:
            // arg1 = character to print
            kernel_putchar((char)arg1);
            break;
            
        case SYSCALL_PUTS:
            // arg1 = pointer to string
            kernel_puts((const char*)arg1);
            break;
            
        case SYSCALL_EXIT:
            // arg1 = exit code
            kernel_exit((int)arg1);
            break;
            
        default:
            // Unknown syscall
            kernel_puts("Unknown syscall: ");
            kernel_putint(syscall_num);
            break;
    }
}

// These are KERNEL functions (in Ring 0)
void kernel_putchar(char c) {
    // Now in Ring 0 - CAN access VGA directly
    vga_putchar(c);
}

void kernel_puts(const char* str) {
    // Now in Ring 0 - CAN access VGA directly
    while (*str) {
        vga_putchar(*str);
        str++;
    }
}

void kernel_exit(int code) {
    // Halt or clean up
    asm volatile("hlt");
}
```

### 5. IDT Setup - Enable INT 0x80

**File: `src/managers/interrupt/idt.c`** (modify existing file)

```c
// File: src/managers/interrupt/idt.c

void setup_idt_syscall() {
    // Create IDT entry for INT 0x80 (syscall)
    
    struct idt_entry syscall_gate;
    syscall_gate.offset_low = (unsigned int)syscall_stub & 0xFFFF;
    syscall_gate.offset_high = ((unsigned int)syscall_stub >> 16) & 0xFFFF;
    syscall_gate.selector = 0x08;           // Kernel code segment
    syscall_gate.zero = 0;
    syscall_gate.type_attr = 0xEE;          // 
                                             // Bit 7: Present = 1
                                             // Bit 6-5: DPL = 3 (Ring 3 can call!)
                                             // Bit 4-0: Type = 01110 (32-bit trap gate)
    
    idt[128] = syscall_gate;  // Entry 128 = INT 0x80
}
```

---

## File Structure You'll Create

```
src/
├── syscalls/                           # NEW FOLDER
│   ├── user_syscalls.c                 # Ring 3 syscall wrappers
│   ├── user_syscalls.h                 # Declarations
│   ├── syscall_handler.c               # Ring 0 dispatcher (kernel)
│   ├── syscall_handler.h               # Declarations
│   └── syscall_numbers.h               # #define SYSCALL_*
│
├── managers/
│   └── interrupt/
│       ├── idt.c                       # MODIFIED: Add INT 0x80 setup
│       ├── interrupt_stubs.s           # MODIFIED: Add syscall_stub
│       └── syscall_handler.c           # Alternative location
│
└── ... rest of kernel
```

---

## Step-by-Step Flow with Code

### Step 1: Ring 3 Code Calls Syscall

```c
// In src/sysman/sysman.c (runs in Ring 3)
int main() {
    syscall_puts("Hello from Ring 3!\n");  // ← Ring 3 code
    return 0;
}
```

### Step 2: Syscall Wrapper Executes INT 0x80

```c
// In src/syscalls/user_syscalls.c (runs in Ring 3)
void syscall_puts(const char* str) {
    asm volatile(
        "mov $2, %%eax\n\t"      // ← Set EAX = 2 (SYSCALL_PUTS)
        "mov %0, %%ebx\n\t"      // ← Set EBX = str pointer
        "int $0x80\n\t"          // ← INT 0x80: TRIGGER!
        : 
        : "r"(str)
    );
    // Execution STOPS here
    // CPU switches to Ring 0
    // Jumps to syscall_stub
}
```

### Step 3: CPU Switches to Ring 0

```
INT 0x80 triggers
    ↓
CPU looks up IDT[128]
    ↓
IDT[128].offset = address of syscall_stub
IDT[128].selector = 0x08 (Ring 0 code)
IDT[128].type_attr = 0xEE (DPL=3, trap gate)
    ↓
CPU switches from Ring 3 to Ring 0
CPU pushes: [EFLAGS] [CS] [EIP] onto Ring 0 stack
    ↓
CPU jumps to syscall_stub (now in Ring 0!)
```

### Step 4: Assembly Stub Handles Interrupt

```asm
; In src/managers/interrupt/interrupt_stubs.s
; Now executing in Ring 0

syscall_stub:
    pusha                    ; Save all registers
    
    ; EAX = 2 (SYSCALL_PUTS)
    ; EBX = pointer to string
    ; ECX = ? (unused)
    ; EDX = ? (unused)
    
    call syscall_dispatcher  ; Jump to C function
    
    popa                     ; Restore all registers
    iret                     ; Return to Ring 3
```

### Step 5: Kernel Dispatcher Handles It

```c
// In src/managers/interrupt/syscall_handler.c
// Now executing in Ring 0

void syscall_dispatcher(unsigned int syscall_num, ...) {
    // syscall_num = EAX = 2
    
    switch(2) {  // SYSCALL_PUTS
        case 2:
            kernel_puts((const char*)arg1);  // arg1 = EBX = string pointer
            break;
    }
}

void kernel_puts(const char* str) {
    // Now in Ring 0!
    // CAN access VGA buffer directly
    
    while (*str) {
        vga_putchar(*str);  // Direct VGA access - allowed in Ring 0
        str++;
    }
}
```

### Step 6: Return to Ring 3

```
vga_putchar() finishes writing to VGA
    ↓
kernel_puts() returns
    ↓
syscall_dispatcher() returns
    ↓
Assembly pops registers
    ↓
IRET instruction executes
    ↓
CPU restores: [EIP] [CS] [EFLAGS]
CPU switches back to Ring 3
    ↓
Control returns to Ring 3 code
(Line after "int $0x80" in user_syscalls.c)
```

---

## Key Differences: Syscall vs Direct Call

### Without Syscalls (What You Were Doing Before - WRONG)

```c
// Ring 3 code tries to call kernel function directly
void sysman_main() {
    vga_putchar('H');  // ← Ring 3 trying to access VGA
                       // ← PRIVILEGE VIOLATION!
                       // ← Exception triggered
}
```

**Problem:** Ring 3 code cannot access VGA buffer (0xB8000). Attempting to causes privilege violation exception.

### With Syscalls (What We're Building - RIGHT)

```c
// Ring 3 code calls syscall wrapper
void sysman_main() {
    syscall_putchar('H');  // ← Safe! Syscall wrapper
                           // ← Triggers INT 0x80
                           // ← CPU switches to Ring 0
                           // ← Kernel handles actual VGA access
}
```

**Solution:** Ring 3 calls INT 0x80 → CPU switches to Ring 0 → Kernel does VGA access → CPU switches back to Ring 3.

---

## Summary: Your Understanding + Clarifications

| Aspect | Your Understanding | Clarification |
|--------|-------------------|---------------|
| **File location** | `syscall` folder in `src` | ✓ Correct! Also need interrupt handler modifications |
| **Function name** | `printv` or `puts` | ✓ Correct! Use `syscall_puts()` in Ring 3 |
| **INT 0x80** | Triggers CPU mode switch | ✓ Correct! + parameters via registers (EAX=number, EBX=arg1) |
| **Ring 0 switch** | Automatic by CPU | ✓ Correct! CPU looks up IDT[128] and jumps |
| **Kernel function** | `vga_print()` | ✓ Correct! But called from `kernel_puts()` dispatcher |
| **Parameter passing** | Implicit somehow | Need to clarify: EAX=syscall#, EBX/ECX/EDX=arguments |
| **Assembly involved** | Not mentioned | Important! Assembly stub handles INT 0x80 interrupt |

---

## Files to Create/Modify

| File | Type | Action |
|------|------|--------|
| `src/syscalls/user_syscalls.c` | NEW | Ring 3 syscall wrappers (printchar, puts) |
| `src/syscalls/user_syscalls.h` | NEW | Declarations for Ring 3 to use |
| `src/managers/interrupt/syscall_handler.c` | NEW | Ring 0 dispatcher (kernel side) |
| `src/managers/interrupt/interrupt_stubs.s` | MODIFY | Add syscall_stub for INT 0x80 |
| `src/managers/interrupt/idt.c` | MODIFY | Add INT 0x80 entry setup (DPL=3) |
| `src/sysman/sysman.c` | MODIFY | Use syscall_puts() instead of direct access |

---

## Next Steps to Implement

1. **Create `src/syscalls/` folder with files**
2. **Define syscall numbers** (SYSCALL_PUTCHAR=1, SYSCALL_PUTS=2, etc.)
3. **Write Ring 3 wrappers** that do INT 0x80
4. **Add assembly stub** in interrupt_stubs.s
5. **Write Ring 0 dispatcher** that handles syscalls
6. **Modify IDT** to include INT 0x80 entry (DPL=3!)
7. **Modify sysman.c** to use syscall_puts()
8. **Test** by building and running in QEMU

**Ready to implement?** Or do you want more clarification on any part?
