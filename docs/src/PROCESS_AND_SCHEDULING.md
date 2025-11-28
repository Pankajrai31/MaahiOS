# Process Management and Scheduling Documentation

## Overview

MaahiOS implements:
- **Process Manager** - PCB management and process creation
- **Scheduler** - Simple round-robin scheduling
- **Ring 3 Transition** - Privilege level switching

---

## process_manager.c / process_manager.h

**Purpose:** Manage process control blocks and process lifecycle.

### Constants
```c
#define MAX_PROCESSES 64
#define USER_STACK_BASE 0x00200000   // 2MB for user stacks
#define USER_STACK_SIZE 0x00010000   // 64KB per process
#define KERNEL_INT_STACK_BASE 0x00280000  // 2.5MB for kernel stacks
#define KERNEL_INT_STACK_SIZE 0x00004000  // 16KB per process
```

### Process Control Block (PCB)
```c
typedef struct {
    int pid;                    // Process ID
    uint32_t entry_point;       // Start address
    uint32_t state;             // READY or RUNNING
    uint32_t user_stack_top;    // User stack pointer
    uint32_t kernel_stack_top;  // Kernel interrupt stack
} process_t;

#define PROCESS_STATE_READY    1
#define PROCESS_STATE_RUNNING  2
```

### Global State
```c
static process_t *process_table[MAX_PROCESSES];
static int next_pid = 1;
static uint32_t next_stack_top = USER_STACK_BASE;
static uint32_t next_kernel_stack_top = KERNEL_INT_STACK_BASE;
```

### Key Functions

| Function | Description |
|----------|-------------|
| `process_manager_init()` | Initialize process table |
| `process_create_sysman(addr)` | Create PID 1 and start immediately |
| `process_create(entry)` | Create generic process (returns to caller) |
| `process_get_by_pid(pid)` | Get PCB by process ID |
| `process_manager_get_count()` | Count active processes |

### Process Creation Flow

#### `process_create_sysman()`
1. Allocate PCB via kmalloc
2. Assign PID 1
3. Allocate user stack (64KB)
4. Allocate kernel interrupt stack (16KB)
5. Set TSS.esp0 to kernel stack
6. Enable interrupts
7. Jump to Ring 3 (never returns)

#### `process_create()`
1. Allocate PCB via kmalloc
2. Assign next PID
3. Allocate stacks
4. Mark as READY (not RUNNING)
5. Add to scheduler queue
6. Return to caller (process started by scheduler)

### Issues Identified

1. **Fixed Stack Layout Conflict (Lines 19-25)**
   ```c
   #define USER_STACK_BASE 0x00200000
   #define KERNEL_INT_STACK_BASE 0x00280000
   ```
   - Hardcoded addresses may conflict with other regions.
   - Orbit starts at 0x00300000 (too close to kernel stacks).
   - **Suggestion:** Dynamically allocate stacks from heap.

2. **No Process Termination**
   - No `process_exit()` or cleanup function.
   - Dead processes remain in table forever.
   - **Suggestion:** Implement process termination and resource cleanup.

3. **Serial Debug in Production (Lines 40-53)**
   - Heavy serial output during process creation.
   - **Suggestion:** Make conditional or remove.

4. **Never-returning Function (Lines 58-112)**
   - `process_create_sysman()` never returns but returns value.
   - **Suggestion:** Mark as `__attribute__((noreturn))`.

---

## scheduler.c / scheduler.h

**Purpose:** Simple round-robin task scheduler.

### Process Queue
```c
#define MAX_QUEUED_PROCESSES 16

typedef struct {
    int pid;
    uint32_t entry_point;
    uint32_t user_stack;
    uint32_t kernel_stack;
} queued_process_t;

static queued_process_t process_queue[MAX_QUEUED_PROCESSES];
static int queue_head = 0;
static int queue_tail = 0;
static int queue_count = 0;
```

### Global State
```c
static int current_pid = -1;
static int scheduling_enabled = 0;
```

### Key Functions

| Function | Description |
|----------|-------------|
| `scheduler_init()` | Initialize scheduler |
| `scheduler_tick()` | Called on timer interrupt |
| `scheduler_enable()` | Enable scheduling |
| `scheduler_disable()` | Disable scheduling |
| `scheduler_get_current_pid()` | Get current process PID |
| `scheduler_add_process(pid, entry, user_stack, kernel_stack)` | Add to ready queue |

### Scheduling Algorithm
```c
void scheduler_tick(void) {
    if (!scheduling_enabled) return;
    
    if (queue_count > 0) {
        // Get next process from queue
        queued_process_t *proc = &process_queue[queue_head];
        
        // Set TSS kernel stack
        gdt_set_kernel_stack(proc->kernel_stack);
        
        // Remove from queue
        queue_head = (queue_head + 1) % MAX_QUEUED_PROCESSES;
        queue_count--;
        
        current_pid = proc->pid;
        
        // Switch to Ring 3 - doesn't return
        ring3_switch_with_stack(proc->entry_point, proc->user_stack);
    }
}
```

### Issues Identified

1. **No Preemption (Lines 55-82)**
   - Only starts queued processes, no context switching.
   - Current process runs until it yields or blocks.
   - **Suggestion:** Implement full context switch with saved state.

2. **No Time Quantum**
   - Processes aren't switched based on time.
   - **Suggestion:** Add time slice tracking.

3. **Queue Overflow Not Handled (Lines 104-116)**
   ```c
   if (queue_count >= MAX_QUEUED_PROCESSES) {
       return;  // Queue full - silently fails
   }
   ```
   - **Suggestion:** Return error code or expand queue.

4. **VBE Print in Scheduler (Lines 42-43)**
   - Printing in scheduler could cause issues.

---

## ring3.c

**Purpose:** Transition from Ring 0 (kernel) to Ring 3 (user) mode.

### Key Functions

| Function | Description |
|----------|-------------|
| `ring3_switch_with_stack(entry, stack)` | Switch to Ring 3 with specified stack |
| `ring3_switch(entry)` | Switch with default stack (backward compat) |

### IRET Frame
The function builds an IRET stack frame:
```
[ESP] = SS (0x23 - user data segment)
[ESP+4] = ESP (user stack pointer)
[ESP+8] = EFLAGS (with IF set)
[ESP+12] = CS (0x1B - user code segment)
[ESP+16] = EIP (entry point)
```

### Implementation
```c
void ring3_switch_with_stack(unsigned int entry_point, unsigned int stack_top) {
    __asm__ __volatile__(
        "pushl $0x23\n\t"           // SS
        "pushl %0\n\t"              // ESP
        "pushf\n\t"                 // EFLAGS
        "popl %%eax\n\t"
        "orl $0x00000200, %%eax\n\t"// Set IF (interrupts enabled)
        "pushl %%eax\n\t"
        "pushl $0x1B\n\t"           // CS
        "pushl %1\n\t"              // EIP
        "iret\n\t"                  // Switch to Ring 3
        :
        : "r"(stack_top), "r"(entry_point)
        : "eax", "memory"
    );
    
    while(1) { __asm__ volatile("hlt"); }  // Never reached
}
```

### Issues Identified

1. **Data Segments Not Set**
   - DS/ES/FS/GS remain as kernel (0x10).
   - User code must set them to 0x23.
   - **Suggestion:** Set data segments in assembly or document requirement.

2. **Default Stack Address (Line 61)**
   ```c
   ring3_switch_with_stack(entry_point, 0x001FFFF0);
   ```
   - Hardcoded default stack.
   - **Suggestion:** Remove or document.

---

## switch_osdev.s

**Purpose:** Context switch assembly routine (unused currently).

### Function
```assembly
switch_to_task(old_task, new_task):
    # Save current state
    push ebx, esi, edi, ebp
    mov esp, [old_task]->esp
    
    # Load new state
    mov [new_task]->esp, esp
    pop ebp, edi, esi, ebx
    ret
```

### Task Structure Expected
```
Offset 0: uint32_t esp  (kernel stack pointer)
```

### Issues Identified

1. **Not Used**
   - Context switch assembly exists but isn't called.
   - Scheduler uses `ring3_switch_with_stack` instead.
   - **Suggestion:** Remove or integrate properly.

---

## pit.c / pit.h

**Purpose:** Programmable Interval Timer for scheduling ticks.

### Constants
```c
#define PIT_FREQUENCY 1193182  // Base frequency in Hz
#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
```

### Key Functions

| Function | Description |
|----------|-------------|
| `pit_init(frequency)` | Set timer frequency |
| `pit_handler()` | IRQ0 handler |
| `pit_get_ticks()` | Get tick count |
| `pit_wait(ticks)` | Busy-wait delay |

### Timer Setup
```c
void pit_init(unsigned int frequency) {
    unsigned int divisor = PIT_FREQUENCY / frequency;
    outb(PIT_COMMAND, 0x36);  // Channel 0, lobyte/hibyte, rate generator
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}
```

### Handler
```c
void pit_handler(void) {
    pit_ticks++;
    scheduler_tick();  // Trigger scheduler
}
```

### Issues Identified

1. **Busy Wait (Lines 52-57)**
   ```c
   void pit_wait(unsigned int ticks) {
       unsigned int end_tick = pit_ticks + ticks;
       while (pit_ticks < end_tick) {
           asm volatile("pause");
       }
   }
   ```
   - Wastes CPU time.
   - **Suggestion:** Implement sleep queue.

2. **No Tick Overflow Handling**
   - `pit_ticks` will wrap after ~49 days at 1000Hz.
   - `pit_wait` will break on wrap.
   - **Suggestion:** Handle wrap-around or use 64-bit counter.
