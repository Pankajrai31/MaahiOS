# MaahiOS Development Roadmap
**Based on OSDev.org Best Practices**

---

## 🎯 Vision: Hybrid OS Architecture

MaahiOS aims to be a hybrid between Linux and Windows philosophies:
- **Closed-source kernel** (like Windows NT) - Secure, proprietary core
- **Open-source user mode** (like Linux) - Community can build their own distributions
- **Modular design** - Compartmentalized components for stability and maintainability

### Core Architecture Components:
- **kernel.bin** - Core kernel (equivalent to ntoskrnl.exe in Windows)
- **sysman.bin** - System Manager in Ring 3 (equivalent to smss.exe in Windows)
- **orbit.bin** - GUI Shell (equivalent to explorer.exe in Windows)
- **Executives** - MaahiOS term for services/daemons (like Windows Services)

---

## 📋 DEVELOPMENT PHASES (CRITICAL ORDER)

### ⚠️ LESSONS FROM PAST FAILURE:
Previously spent 2 months stuck on page faults because:
1. Built entire kernel with all managers first
2. Implemented paging last without proper foundation
3. No physical memory manager
4. Poor debugging infrastructure

**OSDev Recommendation:** Build incrementally, test thoroughly at each stage.

---

## Phase 1: Physical Memory Manager 🧱
**Priority:** CRITICAL - DO THIS FIRST!
**Location:** kernel.bin (core kernel)
**Status:** NOT YET IMPLEMENTED

### Why This Comes First:
According to OSDev, the physical memory manager is the absolute foundation. Without it:
- Paging cannot allocate page tables dynamically
- You'll hit mysterious crashes when kernel runs out of pre-allocated pages
- No way to track which physical memory is in use
- Cannot implement heap allocator later

### What to Implement:

#### 1. Memory Detection
**Purpose:** Discover how much RAM is available and what regions are usable.

**Method:** Parse GRUB Multiboot memory map (E820 entries)
- GRUB provides memory map in multiboot info structure
- E820 entries describe: base address, length, type (usable/reserved/ACPI/etc.)
- Build internal memory map data structure
- Identify: low memory (< 1MB), high memory (> 1MB), reserved regions, ACPI tables

**Critical Details:**
- Some memory regions are reserved for hardware (VGA buffer, BIOS, etc.)
- ACPI tables must not be overwritten
- Memory-mapped I/O regions must be marked unusable
- Kernel itself occupies memory that must be marked as used

#### 2. Page Frame Allocator
**Purpose:** Track and allocate 4KB physical memory pages (page frames).

**Implementation Options (choose one):**

**Option A: Bitmap Allocator (Recommended by OSDev for simplicity)**
- One bit per physical page frame (4KB)
- Bit 0 = free, Bit 1 = allocated
- For 4GB RAM = 1,048,576 pages = 128KB bitmap
- Fast allocation: scan bitmap for first 0 bit
- Fast deallocation: clear bit
- Memory overhead: ~0.003% of RAM

**Option B: Stack Allocator**
- Stack of free page frame addresses
- Push address when freeing, pop when allocating
- Very fast O(1) operations
- Higher memory overhead (~0.4% of RAM)
- Good for systems with less fragmentation

**Option C: Buddy Allocator**
- Used by Linux kernel
- Allocates power-of-2 sized blocks
- Reduces fragmentation
- More complex, implement later if needed

**Functions to Implement:**
```
pmm_init() - Initialize memory manager with E820 map
pmm_alloc_page() - Allocate one 4KB page frame, returns physical address
pmm_free_page(physical_addr) - Free a page frame
pmm_alloc_pages(count) - Allocate multiple contiguous pages
pmm_free_pages(physical_addr, count) - Free multiple pages
pmm_get_total_memory() - Return total RAM in bytes
pmm_get_free_memory() - Return available RAM in bytes
```

**Critical Implementation Details:**
- Must handle page frame 0x0 specially (null pointer detection)
- Mark kernel's physical memory as allocated
- Mark multiboot modules (sysman.bin, etc.) as allocated
- Handle memory below 1MB carefully (real mode artifacts)
- Thread-safe (use spinlock) for future multitasking

#### 3. Debugging & Statistics
**Must have before moving forward:**
- Memory usage statistics (total, used, free)
- Ability to print memory map
- Function to verify allocator integrity
- Panic handler if allocation fails

**Expected Output:**
```
Physical Memory Manager Initialized
Total Memory: 128 MB
Usable Memory: 125 MB
Kernel Memory: 2 MB
Reserved: 1 MB
Free Pages: 30720
```

---

## Phase 2: Paging (Virtual Memory) 🗺️
**Priority:** CRITICAL
**Location:** kernel.bin (core kernel)
**Status:** NOT YET IMPLEMENTED

### Why After PMM:
Paging requires allocating page directory and page tables dynamically. Without PMM, you'd have to pre-allocate these statically, which:
- Wastes memory
- Limits scalability
- Breaks when kernel grows

### Understanding x86 Paging (32-bit):

**Two-Level Page Tables:**
1. **Page Directory (PD):** 1024 entries, each points to a Page Table
2. **Page Table (PT):** 1024 entries, each points to a 4KB physical page

**Virtual Address Translation:**
- Bits 22-31 (10 bits): Page Directory Index
- Bits 12-21 (10 bits): Page Table Index
- Bits 0-11 (12 bits): Offset within 4KB page

**Each entry has flags:**
- Present (P): Page is in physical memory
- Read/Write (R/W): Page permissions
- User/Supervisor (U/S): Ring 3 can access if set
- Accessed (A): CPU sets when page accessed
- Dirty (D): CPU sets when page written to
- Page Size (PS): 4KB or 4MB page

### What to Implement:

#### 1. Identity Mapping (First 4MB)
**Purpose:** Kernel must remain accessible after paging enabled.

**Method:**
- Map virtual 0x00000000-0x003FFFFF to physical 0x00000000-0x003FFFFF
- 1:1 mapping
- Allows kernel to continue executing
- Maps VGA buffer at 0xB8000
- Maps GRUB modules

**Why:** When you enable paging, CPU starts using virtual addresses. If kernel code isn't mapped, instant triple fault.

#### 2. Higher-Half Kernel Mapping
**Purpose:** Map kernel to 3GB mark (0xC0000000) to free low memory for user programs.

**Method:**
- Map virtual 0xC0000000-0xC03FFFFF to physical 0x00100000-0x004FFFFF
- Kernel runs in high memory
- User programs get clean 0x00000000-0xBFFFFFFF space
- Standard convention used by Linux, many hobby OSes

**Implementation Steps:**
- Create page directory at fixed physical address (use PMM to allocate)
- Create page tables for identity mapping
- Create page tables for higher-half mapping
- Set each entry with proper flags (Present, R/W, Supervisor)
- Load page directory address into CR3 register
- Set PG bit in CR0 to enable paging

**Critical:** After enabling paging, must immediately jump to higher-half address or use position-independent code.

#### 3. Page Fault Handler (MOST CRITICAL!)
**Purpose:** Debug paging issues, handle invalid accesses, implement demand paging later.

**What it does:**
- CPU triggers interrupt 14 when invalid page access occurs
- Error code pushed on stack (Present, Write, User, Reserved, InstructionFetch bits)
- CR2 register contains faulting virtual address

**Handler must print:**
```
PAGE FAULT!
Faulting Address: 0xC0001234
Error Code: 0x00000002 (Write to non-present page)
EIP: 0xC0100567
Process: kernel
```

**Error Code Bits:**
- Bit 0: Page present (1) or not present (0)
- Bit 1: Write (1) or read (0) access
- Bit 2: User mode (1) or supervisor mode (0)
- Bit 3: Reserved bits violation
- Bit 4: Instruction fetch

**OSDev Warning:** Without detailed page fault handler, you'll waste weeks debugging. Print EVERYTHING!

#### 4. Dynamic Page Table Management
**Functions to implement:**
```
paging_init() - Initialize paging with identity + higher-half
paging_map_page(virt, phys, flags) - Map single page
paging_unmap_page(virt) - Unmap page
paging_get_physical(virt) - Translate virtual to physical
paging_create_address_space() - Create new page directory for process
paging_switch_directory(page_directory) - Switch to different address space
paging_clone_directory() - Copy page directory (for fork)
```

**INVLPG Instruction:**
After modifying page tables, must invalidate TLB (Translation Lookaside Buffer):
```assembly
invlpg [virtual_address]
```
Otherwise CPU uses stale cached translations.

#### 5. Testing Strategy (CRITICAL!)
**Test each in isolation:**
1. Enable paging with identity mapping only - should work perfectly
2. Add higher-half mapping - jump to high address
3. Try accessing unmapped page - should trigger page fault with correct info
4. Map new page dynamically - should be accessible
5. Unmap page - access should fault

**Don't proceed until paging is rock-solid!**

---

## Phase 3: Kernel Heap Allocator (kmalloc/kfree) 🎯
**Priority:** HIGH
**Location:** kernel.bin (core kernel)
**Status:** NOT YET IMPLEMENTED

### Why Now:
With PMM and paging working, need dynamic memory allocation for:
- Process structures
- File descriptors
- Device driver data
- Dynamic data structures (linked lists, trees)

### ⚠️ CRITICAL: Identity Mapping Verification
**Before implementing kernel heap, verify identity mapping boundaries:**

**Current Implementation Status (as of Nov 2025):**
- ✅ Identity mapping: 0x00000000 - 0x02800000 (40MB)
- ✅ Includes: BIOS, VGA, kernel, sysman, page directory, page tables
- ✅ PMM allocations start at 0x02800000 (after identity-mapped region)
- ✅ All new allocations are still identity-mapped (V=P) currently

**What to Check:**
1. **Verify identity map scope:** Ensure only kernel/sysman area is identity-mapped, NOT entire RAM
2. **Check allocation region:** Confirm PMM allocations are identity-mapped
3. **Document virtual strategy:** Current design: V=P (Virtual = Physical) for ALL memory
4. **Plan for Phase 4:** Per-process virtual memory will implement V≠P (Virtual ≠ Physical)

**Key Understanding:**
- **Now (Phase 2-3):** All memory identity-mapped, simple V=P model
- **Future (Phase 4+):** Per-process page tables with V≠P, isolation between processes
- **Kernel heap:** Will use virtual addresses from identity-mapped region initially
- **User heap:** Already implemented in Ring 3 (heap.c), uses syscall for page allocation

**Questions to Answer:**
- Where will kernel heap virtual address range start? (Recommend: 0xC0400000 in higher-half)
- Will kernel heap be identity-mapped or use separate virtual mapping?
- How much virtual space to reserve for kernel heap? (Recommend: 192MB initially)
- When to transition from global identity mapping to per-process virtual memory?

### Heap vs Page Allocator:
- **Page Allocator (PMM):** Allocates 4KB chunks, physical memory
- **Heap Allocator:** Allocates variable-sized chunks (8 bytes to megabytes), virtual memory

### What to Implement:

#### 1. Heap Region Setup
**Purpose:** Reserve virtual address range for kernel heap.

**Typical Layout:**
```
0x00000000 - 0x00400000: Identity mapped (4MB)
0x00400000 - 0xC0000000: Unmapped (user space future)
0xC0000000 - 0xC0400000: Kernel code/data (4MB)
0xC0400000 - 0xD0000000: Kernel heap (192MB)
0xD0000000 - 0xFFFFFFFF: Kernel modules, stacks (768MB)
```

**Implementation:**
- Start with small heap (1MB)
- Expand on demand using PMM + paging
- Track heap size and usage

#### 2. Heap Allocator Algorithm

**Option A: First-Fit Free List (Simple, recommended)**
- Maintain linked list of free blocks
- Each block has header: size, magic number, free flag
- Allocation: scan list for first block large enough
- Deallocation: mark block free, coalesce with neighbors
- Simple, predictable, good for beginners

**Option B: Best-Fit**
- Find smallest block that fits request
- Less fragmentation than first-fit
- Slower allocation (must scan entire list)

**Option C: Buddy Allocator**
- Allocate power-of-2 sizes only
- Fast allocation/deallocation
- Some internal fragmentation
- Used by Linux slab allocator

**Block Header Structure:**
```
- Magic number (0xDEADBEEF) - detect corruption
- Size of block (including header)
- Flags: FREE, USED, GUARD (for debugging)
- Pointer to next free block (if free)
```

#### 3. Functions to Implement
```
heap_init(start_addr, size) - Initialize heap region
kmalloc(size) - Allocate memory, returns virtual address
kfree(ptr) - Free previously allocated memory
krealloc(ptr, new_size) - Resize allocation
kcalloc(count, size) - Allocate and zero memory
```

**Additional Safety Functions:**
```
heap_verify() - Check heap integrity (detect corruption)
heap_stats() - Print statistics (total, used, free, fragmentation)
kstrdup(str) - Duplicate string (allocate + copy)
```

#### 4. Memory Alignment
**Critical:** All allocations should be aligned (4 or 8 bytes)
- Improves CPU cache performance
- Required for some data structures
- Round up size to multiple of alignment

#### 5. Debugging Features
**Essential during development:**
- Magic numbers before/after each block (detect overruns)
- Fill freed memory with pattern (0xDEADDEAD) to catch use-after-free
- Track allocation source (file, line number) with macro
- Maintain allocation statistics per source

**Macro example:**
```
#define KMALLOC(size) kmalloc_debug(size, __FILE__, __LINE__)
```

#### 6. Guard Pages
**Optional but recommended:**
- Place unmapped guard page after heap
- Detects heap overflow via page fault
- Better than silent corruption

---

## Phase 4: Multitasking & Scheduling ⚙️
**Priority:** HIGH (before device drivers!)
**Location:** kernel.bin (core kernel)
**Status:** NOT YET IMPLEMENTED

### Why Before Drivers:
OSDev recommends early multitasking so:
- Can test drivers in isolated processes
- Driver bugs don't crash kernel
- Can implement watchdog timers for hung drivers
- User programs can run concurrently

### What to Implement:

#### 1. Task Control Block (TCB/Process Structure)
**Purpose:** Store all state for a process/thread.

**Must contain:**
- **Process ID (PID)** - Unique identifier
- **State** - RUNNING, READY, BLOCKED, TERMINATED
- **Registers** - EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP, EIP, EFLAGS
- **Page Directory** - Pointer to process's page tables
- **Kernel Stack** - Separate stack for kernel mode (syscalls, interrupts)
- **User Stack** - Stack pointer when in user mode
- **Priority** - Scheduling priority
- **Time Slice** - Remaining quantum
- **Parent/Child** - Process hierarchy
- **File Descriptors** - Open files (future)
- **Working Directory** - Current directory (future)

**Data Structure:**
```
Maintain linked list or array of TCBs
Current process pointer: struct process *current_process
Ready queue: Processes waiting to run
```

#### 2. Context Switching
**Purpose:** Save current process state, load next process state.

**Steps:**
1. **Save current process:**
   - Push all registers to kernel stack
   - Save ESP to TCB
   - Save other state (page directory, etc.)

2. **Select next process:**
   - Call scheduler to pick next process
   - Update current_process pointer

3. **Load next process:**
   - Load page directory (CR3) - switch address space
   - Load ESP from TCB
   - Pop all registers from stack
   - Return from interrupt (loads EIP, CS, EFLAGS, ESP, SS)

**Implementation:**
- Done in assembly (need to manipulate stack carefully)
- Called from timer interrupt handler
- Must be atomic (disable interrupts)

#### 3. Timer Interrupt (PIT - Programmable Interval Timer)
**Purpose:** Periodic interrupt for preemptive multitasking.

**How it works:**
- PIT generates IRQ 0 at configurable frequency
- Typical: 100 Hz (every 10ms) or 1000 Hz (every 1ms)
- Each interrupt = one "tick"
- Scheduler runs every tick

**PIT Programming:**
- Uses I/O ports 0x40-0x43
- Set frequency by writing divisor
- Formula: frequency = 1193182 / divisor
- For 100 Hz: divisor = 11932

**Timer Interrupt Handler:**
```
1. Acknowledge interrupt (send EOI to PIC)
2. Increment system tick counter
3. Decrement current process time slice
4. If time slice expired:
   - Save current process state
   - Call scheduler
   - Load next process state
5. Return from interrupt
```

#### 4. Scheduler (Round-Robin to start)
**Purpose:** Decide which process runs next.

**Round-Robin Algorithm (simplest):**
- Maintain circular queue of ready processes
- Each process gets equal time slice (e.g., 10ms)
- When time slice expires, move to back of queue
- Pick process at front of queue

**Implementation:**
```
scheduler_pick_next():
    - Find next READY process in queue
    - If none, run idle process
    - Reset time slice
    - Return process
```

**Future Enhancements:**
- Priority-based scheduling
- Real-time scheduling
- Multi-level feedback queues
- CPU affinity (SMP)

#### 5. Idle Process
**Purpose:** Process that runs when nothing else can.

**Characteristics:**
- PID 0
- Infinite loop with HLT instruction
- Never blocks
- Lowest priority
- Reduces power consumption

**Implementation:**
```
idle_process:
    while(1) {
        asm("hlt");  // Halt until next interrupt
    }
```

#### 6. Process Creation (Later Phase)
**Functions for future:**
```
process_create(entry_point) - Create new process
process_fork() - Duplicate current process (like Unix)
process_exec(path) - Replace process image with new program
process_exit(code) - Terminate process
process_wait(pid) - Wait for child process
```

**Fork Implementation (complex):**
- Copy parent's page directory (copy-on-write)
- Copy parent's TCB
- Assign new PID
- Return 0 to child, child PID to parent

#### 7. Testing Strategy
**Test incrementally:**
1. Create 2 processes with infinite loops printing different letters
2. Enable timer interrupt - should see letters interleaved
3. Add process priorities - should see higher priority more often
4. Test with many processes (10+) - should round-robin fairly
5. Test process termination and cleanup

---

## Phase 5: Basic Keyboard Driver ⌨️
**Priority:** MEDIUM
**Location:** Could be in kernel.bin or loadable module
**Status:** NOT YET IMPLEMENTED

### Why Now:
Need user input to interact with OS. Keyboard is simpler than mouse/network/disk.

### What to Implement:

#### 1. PS/2 Keyboard Controller
**Purpose:** Interface with PS/2 keyboard hardware.

**Hardware Details:**
- Uses IRQ 1 (interrupt vector 33)
- Data port: 0x60 (read scan codes)
- Status port: 0x64 (read keyboard status)
- Command port: 0x64 (write commands)

**Initialization:**
```
1. Disable keyboard interrupts
2. Flush output buffer (read port 0x60 until empty)
3. Set keyboard LED state
4. Enable keyboard interrupts
5. Install IRQ 1 handler in IDT
```

#### 2. Scan Code Handling
**Purpose:** Convert hardware scan codes to key codes.

**Scan Code Sets:**
- Set 1 (default): IBM PC/XT scan codes
- Set 2: IBM PC/AT scan codes  
- Set 3: Modern keyboards

**How it works:**
- Key press: Single byte scan code
- Key release: 0xF0 + scan code (Set 2) or high bit set (Set 1)
- Extended keys (arrows, etc.): 0xE0 prefix + scan code

**Implementation:**
```
keyboard_irq_handler():
    1. Read scan code from port 0x60
    2. Handle special cases:
       - 0xE0: Extended key prefix
       - 0xF0: Key release
    3. Translate scan code to key code
    4. Update keyboard state (shift, ctrl, alt pressed)
    5. Add to input buffer
    6. Send EOI to PIC
```

#### 3. Scan Code to ASCII Translation
**Purpose:** Convert key codes to characters.

**Considerations:**
- Shift state (uppercase/lowercase)
- Caps Lock state
- Numpad state
- Special keys (F1-F12, arrows, etc.)
- Non-ASCII keys (need key codes, not ASCII)

**Translation Tables:**
```
- scan_code_to_keycode[256] - Scan code to key code
- keycode_to_ascii_normal[128] - Normal characters
- keycode_to_ascii_shift[128] - Shifted characters
```

#### 4. Input Buffer (Keyboard Queue)
**Purpose:** Store key presses until user program reads them.

**Implementation:**
- Circular buffer (ring buffer)
- Store key events: {keycode, pressed, modifiers}
- Thread-safe (IRQ handler writes, user process reads)

**Functions:**
```
keyboard_init() - Initialize keyboard driver
keyboard_getchar() - Read one character (blocking)
keyboard_getch_nonblock() - Read character (non-blocking)
keyboard_buffer_has_data() - Check if keys available
keyboard_flush() - Clear input buffer
```

#### 5. Special Keys Handling
**Keys to handle:**
- **Modifier keys:** Shift, Ctrl, Alt - Track state
- **Toggle keys:** Caps Lock, Num Lock, Scroll Lock - Toggle state, update LEDs
- **Function keys:** F1-F12 - Generate special key codes
- **Arrow keys:** Up, Down, Left, Right
- **Control keys:** Backspace, Enter, Tab, Escape

#### 6. Keyboard LEDs
**Purpose:** Provide visual feedback for toggle keys.

**LED Control:**
- Caps Lock, Num Lock, Scroll Lock
- Set via command to port 0x60
- Command 0xED + LED byte

---

## Phase 6: Sysman Enhancement (Ring 3 Mini-Kernel) 🔧
**Priority:** MEDIUM
**Location:** sysman.bin (separate module loaded by GRUB)
**Status:** BASIC IMPLEMENTATION (syscalls working)

### Current State:
- ✅ Ring 3 transition working
- ✅ Syscall mechanism (INT 0x80) functional
- ✅ Basic output via syscalls
- ❌ No executive loading
- ❌ No IPC mechanism
- ❌ No process management

### Architecture Philosophy:
Sysman is inspired by Windows SMSS.EXE (Session Manager Subsystem):
- First user-mode process
- Acts as "mini-kernel" in Ring 3
- Spawns and manages critical system services
- Coordinates system initialization
- Intermediary between kernel and user programs

### What to Implement:

#### 1. Executive Loader
**Purpose:** Load and start Executive (service) binaries.

**Executive Definition:**
An "Executive" in MaahiOS is equivalent to:
- Windows Services (services.exe spawned processes)
- Linux/Unix daemons (systemd units)
- MacOS LaunchDaemons

**Executive Types:**
- **System Executives:** Critical services (file system, network, security)
- **User Executives:** User-level services (print spooler, background tasks)
- **Driver Executives:** User-mode drivers (future)

**Loading Process:**
```
1. Read executive binary from file system (future) or module
2. Parse executable format (ELF or custom format)
3. Allocate memory in executive's address space
4. Load executable sections (.text, .data, .bss)
5. Set up initial stack
6. Create process structure
7. Register executive in executive registry
8. Start executive process
```

**Functions to Implement:**
```
executive_load(path) - Load executive binary
executive_start(executive_id) - Start executive process
executive_stop(executive_id) - Gracefully stop executive
executive_kill(executive_id) - Force kill executive
executive_restart(executive_id) - Restart failed executive
```

#### 2. Executive Registry
**Purpose:** Track all running executives, their state, and dependencies.

**Registry Data:**
```
- Executive ID (unique identifier)
- Executive Name (e.g., "filesystem", "network")
- Process ID (kernel-assigned PID)
- State: STOPPED, STARTING, RUNNING, STOPPING, FAILED
- Dependencies: List of executives this depends on
- Restart Policy: NEVER, ALWAYS, ON_FAILURE
- Failure Count: Track crashes for restart policy
- Start Time: When executive started
- Memory Usage: Resource tracking
```

**Dependency Management:**
```
Example:
- NetworkExecutive depends on DeviceManager
- FileSystemExecutive depends on DiskDriver
- Start order calculated from dependency graph
```

**Functions:**
```
registry_register(executive_info) - Add executive to registry
registry_unregister(executive_id) - Remove executive
registry_get_state(executive_id) - Query executive state
registry_get_dependencies(executive_id) - Get dependency list
registry_wait_for(executive_id, timeout) - Wait until executive running
```

#### 3. Inter-Process Communication (IPC)
**Purpose:** Allow executives to communicate with each other and with sysman.

**IPC Mechanisms to Implement:**

**Option A: Message Passing (Recommended for MaahiOS)**
- Executive sends message to another executive via sysman
- Sysman routes message to recipient
- Synchronous or asynchronous
- Similar to Windows LPC (Local Procedure Call)

**Message Structure:**
```
- Source PID
- Destination PID (or executive name)
- Message Type (REQUEST, RESPONSE, NOTIFICATION, etc.)
- Message ID (for request-response matching)
- Data Length
- Data Payload (variable size)
```

**Functions:**
```
ipc_send(dest, message, size, flags) - Send message
ipc_receive(message, size, flags) - Receive message (blocking)
ipc_receive_timeout(message, size, timeout) - Receive with timeout
ipc_peek() - Check for pending messages without receiving
```

**Option B: Shared Memory**
- Two processes map same physical pages
- Faster than message passing
- Requires careful synchronization (semaphores/mutexes)
- Implement later for performance

**Option C: Named Pipes**
- FIFO queue with name
- Multiple writers, multiple readers
- Good for stream data
- Implement later

#### 4. Executive Lifecycle Management
**Purpose:** Monitor executives and restart if they crash.

**Watchdog Timer:**
```
- Executives must send "heartbeat" periodically
- If heartbeat missed, executive assumed hung
- Sysman can restart hung executive
```

**Crash Detection:**
```
1. Kernel notifies sysman when process terminates abnormally
2. Sysman checks restart policy
3. If policy is ALWAYS or ON_FAILURE:
   - Increment failure count
   - Wait exponential backoff (1s, 2s, 4s, 8s...)
   - Restart executive
4. If too many failures (e.g., 5 in 60s):
   - Mark executive as FAILED
   - Log error
   - Notify administrator
```

#### 5. Logging and Diagnostics
**Purpose:** Track system events and executive behavior.

**Log Levels:**
- DEBUG: Verbose internal state
- INFO: Normal operations (executive started, stopped)
- WARNING: Potential issues (executive slow to start)
- ERROR: Failures (executive crashed)
- CRITICAL: System-wide failures (sysman malfunction)

**Log Destinations:**
```
- Console (VGA text mode for now)
- Memory buffer (for later retrieval)
- Serial port (for debugging)
- File system (future)
```

**Functions:**
```
log_debug(format, ...) - Log debug message
log_info(format, ...) - Log info message
log_warning(format, ...) - Log warning
log_error(format, ...) - Log error
log_critical(format, ...) - Log critical error
```

#### 6. Sysman Initialization Sequence
**Purpose:** Boot system in correct order.

**Boot Sequence:**
```
1. Sysman starts (launched by kernel)
2. Print banner/version
3. Initialize IPC subsystem
4. Initialize executive registry
5. Load core executives:
   a. Device Manager Executive
   b. Disk Driver Executive (future)
   c. File System Executive (future)
   d. Network Executive (future)
6. Wait for core executives to reach RUNNING state
7. Load optional executives:
   a. Print Spooler
   b. Background Task Scheduler
   c. etc.
8. Start Orbit shell (user interface)
9. Enter main loop (monitor executives)
```

**Main Loop:**
```
while(1) {
    - Check for IPC messages
    - Check executive heartbeats
    - Restart failed executives if needed
    - Handle shutdown requests
    - Sleep briefly (yield CPU)
}
```

---

## Phase 7: Orbit Shell (User Interface) 🖥️
**Priority:** MEDIUM
**Location:** orbit.bin (separate module loaded by sysman)
**Status:** NOT YET IMPLEMENTED

### Philosophy:
Orbit is the MaahiOS shell, equivalent to:
- Windows Explorer.exe (Windows shell)
- Bash/Zsh (Unix shells)
- Finder (macOS shell)

### What to Implement:

#### 1. Text-Mode Shell (Phase 7a)
**Purpose:** Basic command-line interface before graphics.

**Features:**
- Command prompt
- Built-in commands (cd, ls, ps, kill, etc.)
- Executive launcher
- Command history
- Tab completion (future)

**Command Parser:**
```
1. Read input line from keyboard
2. Tokenize into command + arguments
3. Look up command in:
   a. Built-in commands (handled by shell)
   b. Executive commands (IPC to executive)
   c. External programs (execute binary)
4. Execute command
5. Display output
6. Return to prompt
```

**Built-in Commands:**
```
help - Display command list
clear - Clear screen
echo <text> - Print text
ps - List processes
kill <pid> - Terminate process
exec <name> - Start executive
shutdown - Shutdown system
reboot - Reboot system
meminfo - Display memory usage
version - Display OS version
```

#### 2. Graphics Mode Shell (Phase 7b - FUTURE)
**Purpose:** GUI shell with windows, icons, mouse support.

**Features:**
- Desktop with icons
- Window manager
- Start menu
- Taskbar
- File browser
- Settings panel

**Defer until:**
- Graphics driver working
- Mouse driver working
- Windowing system implemented

---

## Phase 8: Graphics & GUI (FUTURE) 🎨
**Priority:** LOW (Do MUCH later!)
**Location:** TBD (likely separate graphics executive)
**Status:** NOT YET PLANNED

### Why Last:
OSDev strongly warns that graphics is complex and buggy drivers destabilize everything. Do this only when core OS is solid.

### Future Work:
- VESA/VBE framebuffer
- Graphics driver architecture
- 2D graphics primitives
- Font rendering
- Window manager
- Mouse cursor

### Defer Until:
- All previous phases complete and stable
- Thorough testing of kernel/sysman/orbit in text mode
- Have patience and realistic expectations

---

## 🏗️ ARCHITECTURAL DECISIONS

### What Goes in kernel.bin:
✅ **Include:**
- Boot code (boot.s)
- GDT/IDT setup
- Physical memory manager
- Paging (virtual memory)
- Kernel heap allocator
- Multitasking/scheduler
- Timer interrupt handler
- Exception handlers
- Syscall dispatcher
- Core kernel functions

❌ **Don't Include:**
- Device drivers (except PIC/PIT)
- File system implementation
- Network stack
- User programs
- Shell
- Applications

### What Goes in Separate Modules:
✅ **Separate Modules:**
- sysman.bin - System Manager (Ring 3 mini-kernel)
- orbit.bin - Shell (loaded by sysman)
- exec_*.bin - Executives/services (loaded by sysman)
- drivers/*.bin - Hardware drivers (future, loaded by device manager)

### Closed-Source vs Open-Source:
**Closed-Source (Proprietary):**
- kernel.bin - Core kernel (your IP)
- Critical security components
- Proprietary drivers (if any)

**Open-Source (Community):**
- sysman.bin source code
- orbit.bin source code
- Executive source code
- Headers/libraries for development
- Documentation

**Distribution Model:**
- Provide kernel.bin as binary only
- Provide source for everything else
- Community can create custom distributions
- Similar to Linux distributions using binary firmware

---

## 🧪 TESTING STRATEGY

### Test After Each Phase:
1. **After PMM:** Allocate/free thousands of pages, verify no leaks
2. **After Paging:** Access valid/invalid pages, verify page faults work
3. **After Heap:** Allocate various sizes, verify no corruption
4. **After Multitasking:** Run multiple processes, verify round-robin
5. **After Keyboard:** Type characters, verify correct input
6. **After Sysman:** Load/stop executives, verify IPC works
7. **After Orbit:** Run commands, verify executive launching

### Debugging Tools to Build:
- Memory dump command (print physical memory)
- Page table walker (print virtual-to-physical mappings)
- Process list (print all processes and their states)
- Heap verifier (detect corruption)
- Stack trace on panic (print call stack)

---

## 📊 PROGRESS TRACKING

### Completed:
- ✅ Basic kernel boot (GRUB multiboot)
- ✅ VGA text mode output
- ✅ GDT setup (4 segments)
- ✅ IDT setup (256 entries)
- ✅ Exception handlers (0-19)
- ✅ Ring 3 transition (IRET)
- ✅ Syscall mechanism (INT 0x80)
- ✅ Basic sysman (prints via syscalls)
- ✅ Module loading (GRUB multiboot modules)
- ✅ Dynamic module address handling

### In Progress:
- ⏳ None (decide on next phase)

### Not Started:
- ❌ Physical memory manager
- ❌ Paging/virtual memory
- ❌ Kernel heap allocator
- ❌ Multitasking/scheduler
- ❌ Keyboard driver
- ❌ Sysman executive loader
- ❌ Orbit shell
- ❌ Graphics

---

## 🎯 IMMEDIATE NEXT STEPS

### Step 1: Physical Memory Manager
**Estimated Time:** 1-2 weeks
**Goals:**
1. Parse GRUB E820 memory map
2. Implement bitmap allocator
3. Test allocation/deallocation
4. Print memory statistics
5. Verify no memory leaks

**Success Criteria:**
- Can allocate 1000+ pages without failure
- Free pages are reusable
- Memory statistics are accurate
- No crashes or corruption

### Step 2: Paging
**Estimated Time:** 2-3 weeks
**Goals:**
1. Identity map first 4MB
2. Enable paging
3. Implement page fault handler with detailed output
4. Map additional pages dynamically
5. Test unmapping and remapping

**Success Criteria:**
- Kernel continues running after paging enabled
- Page faults print useful information
- Can map/unmap pages without crashes
- No unexpected page faults during normal operation

### Step 3: Test Thoroughly!
**Estimated Time:** 1 week
**Goals:**
- Stress test PMM and paging together
- Allocate many pages
- Map/unmap pages in various patterns
- Run for extended period without crashes

**Success Criteria:**
- System stable for 1+ hour of continuous operation
- No memory leaks
- No unexpected crashes

---

## ⚠️ CRITICAL WARNINGS FROM OSDEV

### Don't Do These:
1. ❌ **Don't implement everything then test** - Build incrementally!
2. ❌ **Don't skip physical memory manager** - You'll regret it!
3. ❌ **Don't rush into graphics** - Stabilize text mode first!
4. ❌ **Don't ignore page faults** - Print detailed information!
5. ❌ **Don't hardcode addresses** - Use dynamic allocation!
6. ❌ **Don't skip testing** - Each phase must be solid before next!

### Do These:
1. ✅ **Test after every change** - Catch bugs early!
2. ✅ **Print debug information** - You can't debug what you can't see!
3. ✅ **Read OSDev thoroughly** - Don't reinvent the wheel!
4. ✅ **Keep it simple initially** - Optimize later!
5. ✅ **Document your code** - Future you will thank you!
6. ✅ **Ask for help** - OSDev community is helpful!

---

## 📚 REFERENCES

### OSDev Wiki Essential Pages:
- [Meaty Skeleton](https://wiki.osdev.org/Meaty_Skeleton) - Project structure
- [Physical Memory Management](https://wiki.osdev.org/Page_Frame_Allocation)
- [Paging](https://wiki.osdev.org/Paging) - Virtual memory
- [Higher Half Kernel](https://wiki.osdev.org/Higher_Half_Kernel)
- [Multitasking](https://wiki.osdev.org/Multitasking)
- [Keyboard Input](https://wiki.osdev.org/PS/2_Keyboard)
- [Going Further on x86](https://wiki.osdev.org/Going_Further_on_x86)

### Intel/AMD Manuals:
- Intel Software Developer Manual Volume 3A (System Programming)
- AMD64 Architecture Programmer's Manual Volume 2 (System Programming)

### Books:
- "Operating Systems: Design and Implementation" by Tanenbaum
- "Modern Operating Systems" by Tanenbaum
- "Operating System Concepts" by Silberschatz

---

## 🎉 FINAL THOUGHTS

### Stay Motivated:
- You've already achieved Ring 3 and syscalls - that's a major milestone!
- Each phase will take time, but builds on previous work
- The journey is as valuable as the destination
- OS development teaches you how computers really work

### Learn from Past:
- Previous attempt: Built everything, stuck on page faults for 2 months
- This attempt: Build incrementally, test thoroughly, solid foundation
- OSDev wisdom: "Crawl, walk, run" - Don't skip steps!

### Your Unique Vision:
- Hybrid closed/open source model is innovative
- "Executive" terminology is distinctive
- Windows-inspired architecture with Unix principles
- Potential for community distributions

### Good Luck!
You have a clear roadmap, solid foundation, and the determination to succeed.
Take it one phase at a time, test thoroughly, and don't give up!

---

**Document Version:** 1.0
**Last Updated:** November 12, 2025
**Author:** GitHub Copilot (based on OSDev research)
**For:** MaahiOS Development Team
