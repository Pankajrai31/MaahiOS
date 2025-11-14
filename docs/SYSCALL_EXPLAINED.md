# Syscall Mechanism Explained

## What is a Syscall?

A **syscall** (system call) is the bridge between unprivileged user-mode code (Ring 3) and privileged kernel code (Ring 0). It's how user programs request kernel services without crashing the system.

### The Problem It Solves

- **Ring 3 code** can only access its own memory and execute unprivileged instructions
- **Ring 0 code** can access hardware directly (disk, network, memory management, etc.)
- **Solution**: Ring 3 code must ask Ring 0 to do privileged operations safely

### Real-World Analogy

Think of your OS like a bank:
- **Ring 3** = Regular customer (unprivileged)
- **Ring 0** = Bank manager (privileged)
- **Syscall** = Formal request process (teller window)

A customer can't just go into the vault. They must follow the formal process:
1. Walk up to teller window
2. State their request clearly
3. Teller verifies and performs the operation
4. Teller returns the result

If a customer tries to break into the vault (like Ring 3 accessing privileged hardware), alarms go off (exceptions), and they get arrested (protection fault).

---

## How Syscalls Work (Technical)

### Basic Flow

```
Ring 3 Code                Ring 0 Kernel
-----------                ---------------
1. Prepare arguments
   (put in registers/stack)
                           
2. INT 0x80 instruction    
   (software interrupt)    → 3. CPU switches to Ring 0
                              Calls interrupt handler
                           
                           4. Validate request
                              (is this user allowed to do this?)
                           
                           5. Execute kernel operation
                              (write to disk, allocate memory, etc.)
                           
6. ← Get result in EAX     ← 7. Put result in EAX
                              IRET back to Ring 3
```

### x86 Protected Mode Details

**Step 1: Ring 3 prepares syscall**
```c
// Ring 3 user code wants to write to VGA buffer
// Can't do it directly (privilege violation)
// Must ask kernel

int syscall_write(int fd, char* buffer, int size) {
    // Put syscall parameters in registers (calling convention)
    // syscall_number in EAX
    // param1 in EBX (or EBX, ECX, EDX, ESI, EDI depending on ABI)
    
    asm volatile(
        "mov $4, %%eax\n\t"      // syscall #4 = write
        "mov %0, %%ebx\n\t"      // fd in EBX
        "mov %1, %%ecx\n\t"      // buffer in ECX
        "mov %2, %%edx\n\t"      // size in EDX
        "int $0x80\n\t"          // Trigger software interrupt
        : "=a"(result)           // Get return value from EAX
        : "r"(fd), "r"(buffer), "r"(size)
    );
    return result;
}
```

**Step 2: INT 0x80 triggers**
- CPU recognizes INT 0x80 (software interrupt)
- CPU looks up IDT entry 128 (0x80)
- IDT entry must have DPL=3 (so Ring 3 can call it)
- CPU switches to Ring 0 automatically
- CPU pushes return address and flags
- CPU jumps to syscall_handler()

**Step 3: Ring 0 dispatches to correct function**
```c
// Inside kernel
void syscall_handler(int eax, int ebx, int ecx, int edx, ...) {
    switch(eax) {
        case 1: return syscall_exit(ebx);
        case 2: return syscall_fork();
        case 3: return syscall_read(ebx, ecx, edx);
        case 4: return syscall_write(ebx, ecx, edx);
        case 5: return syscall_open(ebx, ecx, edx);
        // ... hundreds more
    }
}
```

**Step 4: Kernel executes the operation**
```c
int syscall_write(int fd, char* buffer, int size) {
    // Now in Ring 0 - can access hardware directly!
    if (fd == 1) {  // stdout = VGA buffer
        for (int i = 0; i < size; i++) {
            vga_write_char(buffer[i]);  // Direct VGA access OK here
        }
        return size;
    }
    return -1;  // Error
}
```

**Step 5: Return to Ring 3**
```c
// Set return value
return_value = size;  // Put in EAX

// IRET automatically switches back to Ring 3
// EFLAGS, CS, EIP are restored from stack
// Ring 3 code continues where it left off
```

---

## Real OS Implementations

### Linux Syscalls

**How it works:**
- Uses INT 0x80 (older) or SYSCALL instruction (modern x86-64)
- ~400+ syscalls defined (read, write, open, close, fork, exec, etc.)
- Each syscall has a number and calling convention
- User calls libc wrapper functions (printf → write syscall)

**Example: Write to stdout**
```c
// C code
printf("Hello\n");

// Translates to libc code (glibc)
write(1, "Hello\n", 6);

// Which compiles to assembly
mov $4, %eax        // syscall 4 = write
mov $1, %ebx        // fd 1 = stdout
mov $str, %ecx      // buffer
mov $6, %edx        // size
int $0x80           // Trigger syscall

// Kernel handles it, writes to terminal
// Returns 6 (bytes written) in EAX
```

**Linux syscall table (some examples):**
| Number | Name | Purpose |
|--------|------|---------|
| 1 | exit | Terminate process |
| 2 | fork | Create child process |
| 3 | read | Read from file descriptor |
| 4 | write | Write to file descriptor |
| 5 | open | Open file |
| 6 | close | Close file descriptor |
| 9 | link | Create file link |
| 11 | execve | Execute program |
| 20 | getpid | Get process ID |
| 37 | kill | Send signal to process |

**Architecture:**
```
Ring 3: User app (bash, gcc, firefox)
  ↓ (syscall)
Ring 0: Kernel (filesystem, memory, process management)
  ↓ (device drivers)
Hardware (CPU, disk, network, RAM)
```

### Windows Syscalls (System Calls)

**How it works:**
- Uses SYSCALL/SYSRET instructions on x64 (or INT 0x2E on x86)
- Has 600+ system calls (called differently internally)
- User calls Windows API (WriteFile, CreateProcess, etc.)
- These Windows API calls then invoke syscalls internally

**Example: Write to console**
```c
// C code
printf("Hello\n");

// Translates to Windows API
WriteFile(hStdOut, "Hello\n", 6, &bytesWritten, NULL);

// Which internally calls syscall (on x64)
mov r10, rcx        // Shadow space for parameters
mov eax, <syscall#> // Syscall number in EAX
syscall             // Invoke syscall (newer x64 instruction)

// Kernel handles it, writes to console
```

**Key Difference from Linux:**
- Windows exposes **Windows API** layer publicly
- Linux exposes **syscalls** directly in libc
- Both use the same CPU mechanism underneath

**Windows architecture:**
```
Ring 3: User app (.exe)
  ↓ (Windows API call)
Ring 3: ntdll.dll (transition layer)
  ↓ (syscall)
Ring 0: Kernel (executive, drivers)
  ↓ (device drivers)
Hardware
```

---

## Comparison: Linux vs Windows Syscalls

| Aspect | Linux | Windows |
|--------|-------|---------|
| **Mechanism** | INT 0x80 or SYSCALL | INT 0x2E or SYSCALL |
| **Public API** | libc (syscall numbers exposed) | Windows API (syscalls hidden) |
| **Syscall Count** | ~400 | ~600 |
| **Abstraction** | Low-level (syscalls visible) | High-level (API abstracts syscalls) |
| **Philosophy** | "Everything is a file" | Object-oriented (handles, tokens) |
| **Example** | `read()` syscall | `ReadFile()` API → syscall |

---

## For Your MaahiOS

### Simple Syscall Implementation

```c
// In kernel
void syscall_handler(int number, int arg1, int arg2, int arg3) {
    switch(number) {
        case 1:  // putchar
            vga_putchar(arg1);
            break;
        case 2:  // puts
            vga_puts((char*)arg1);
            break;
        case 3:  // putint
            vga_putint(arg1);
            break;
        // ... more syscalls
    }
}
```

```c
// In Ring 3 user code
void syscall_putchar(char c) {
    asm volatile(
        "mov $1, %%eax\n\t"  // syscall 1 = putchar
        "mov %0, %%ebx\n\t"  // char in EBX
        "int $0x80\n\t"
        : : "r"(c)
    );
}

void syscall_puts(char* str) {
    asm volatile(
        "mov $2, %%eax\n\t"  // syscall 2 = puts
        "mov %0, %%ebx\n\t"  // string in EBX
        "int $0x80\n\t"
        : : "r"(str)
    );
}

// User code
int main() {
    syscall_puts("Hello from Ring 3!\n");  // Safe VGA access via syscall
    return 0;
}
```

---

## Summary

| Stage | What Happens | Who's in Control |
|-------|-------------|-----------------|
| 1 | User code prepares syscall | Ring 3 (user) |
| 2 | INT 0x80 instruction triggered | Hardware |
| 3 | Kernel takes over | Ring 0 (kernel) |
| 4 | Kernel validates and executes | Ring 0 (kernel) |
| 5 | Result returned in registers | Ring 0 (kernel) |
| 6 | IRET back to user code | Hardware |
| 7 | User code continues | Ring 3 (user) |

**Key Point:** The syscall mechanism is the **permission check** - it ensures only the kernel can do privileged operations, preventing user code from breaking the system.
