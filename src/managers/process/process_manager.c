/**
 * MaahiOS Process Manager
 * 
 * Manages Process Control Blocks (PCBs) and process lifecycle.
 * - Creates processes with user + kernel stacks
 * - Builds initial interrupt frames for Ring 3 entry via context switch
 * - Integrates with scheduler for process queuing
 */

#include "process_manager.h"
#include "../klog/klog.h"
#include "../memory/kheap.h"

/* Process table */
#define MAX_PROCESSES 64
static process_t *process_table[MAX_PROCESSES];
static int next_pid = 1;

/* Stack allocators */
#define USER_STACK_BASE 0x00200000
#define USER_STACK_SIZE 0x00010000
static uint32_t next_stack_top = USER_STACK_BASE;

#define KERNEL_INT_STACK_BASE 0x00600000
#define KERNEL_INT_STACK_SIZE 0x00004000
static uint32_t next_kernel_stack_top = KERNEL_INT_STACK_BASE;

/* External PMM function to reserve memory */
extern void pmm_mark_region_used(uint32_t start, uint32_t end);

/**
 * Initialize process manager
 */
int process_manager_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i] = 0;
    }
    next_pid = 1;
    
    /* Reserve kernel interrupt stack region in PMM
     * Prevents PMM from allocating this memory for other uses.
     * Space for MAX_PROCESSES kernel stacks (16KB each).
     */
    uint32_t kernel_stack_region_end = KERNEL_INT_STACK_BASE + (MAX_PROCESSES * KERNEL_INT_STACK_SIZE);
    pmm_mark_region_used(KERNEL_INT_STACK_BASE, kernel_stack_region_end);
    
    /* Reserve user stack region */
    uint32_t user_stack_region_end = USER_STACK_BASE + (MAX_PROCESSES * USER_STACK_SIZE);
    pmm_mark_region_used(USER_STACK_BASE, user_stack_region_end);
    
    KLOG_INFO("PROC", "Initialized: kernel stacks 0x%x-0x%x, user stacks 0x%x-0x%x",
              KERNEL_INT_STACK_BASE, kernel_stack_region_end,
              USER_STACK_BASE, user_stack_region_end);
    return 0;
}

/**
 * Create a process with a specific page directory.
 *
 * @param entry_point  EIP for the new process
 * @param page_dir     Page directory for the new process.
 *                     If NULL, uses kernel_page_directory (shared address space).
 *                     For per-process isolation, pass a cloned page directory.
 *
 * The PCB is fully initialized with the correct page directory BEFORE
 * being added to the scheduler, eliminating any race conditions.
 */
int process_create(uint32_t entry_point, uint32_t *page_dir) {
    /* Check limits */
    if (next_pid >= MAX_PROCESSES) {
        KLOG_ERROR("PROC", "Max processes reached");
        return -1;
    }
    
    /* Allocate PCB */
    process_t *pcb = kmalloc(sizeof(process_t));
    if (!pcb) {
        KLOG_ERROR("PROC", "kmalloc failed for PCB");
        return -1;
    }
    
    /* Initialize PCB */
    pcb->pid = next_pid++;
    pcb->entry_point = entry_point;
    pcb->state = PROCESS_STATE_READY;
    pcb->type = PROC_TYPE_SYSTEM;  /* Default to system; caller overrides for user apps */
    pcb->memory_alloc = USER_STACK_SIZE + KERNEL_INT_STACK_SIZE; /* Stacks allocated below */
    
    /* Default name: "process_<pid>" */
    pcb->name[0] = 'p'; pcb->name[1] = 'i'; pcb->name[2] = 'd';
    pcb->name[3] = '_';
    int pn = pcb->pid;
    int ni = 4;
    if (pn >= 10) { pcb->name[ni++] = '0' + (char)(pn / 10); }
    pcb->name[ni++] = '0' + (char)(pn % 10);
    pcb->name[ni] = '\0';
    
    /* Set page directory: caller-provided or kernel default */
    extern uint32_t *kernel_page_directory;
    pcb->page_directory = page_dir ? page_dir : kernel_page_directory;
    
    /* Allocate user stack */
    uint32_t stack_base = next_stack_top;
    next_stack_top += USER_STACK_SIZE;
    uint32_t user_stack_top = stack_base + USER_STACK_SIZE - 4;  // Top of stack (stacks grow down)
    pcb->user_stack_top = user_stack_top;
    
    /* Allocate kernel interrupt stack */
    uint32_t kernel_stack_base = next_kernel_stack_top;
    next_kernel_stack_top += KERNEL_INT_STACK_SIZE;
    uint32_t kernel_stack_top = kernel_stack_base + KERNEL_INT_STACK_SIZE;  // Top of kernel stack
    pcb->kernel_stack_top = kernel_stack_top;
    
    /* Build initial interrupt frame on kernel stack */
    /* This makes the process look like it was interrupted while running in Ring 3 */
    /* When context switched to, IRET will "return" to Ring 3 entry point */
    uint32_t *stack = (uint32_t *)(kernel_stack_top);
    
    /* IRET frame (pushed by CPU on interrupt) */
    *(--stack) = 0x23;                   /* SS (user data segment with RPL=3) */
    *(--stack) = user_stack_top;         /* ESP (user stack - at TOP of allocated region) */
    *(--stack) = 0x00003202;             /* EFLAGS (IF=1, IOPL=3) */
    *(--stack) = 0x1B;                   /* CS (user code segment with RPL=3) */
    *(--stack) = entry_point;            /* EIP (entry point) */
    
    /* PUSHA frame (matches popa order: EDI, ESI, EBP, skip, EBX, EDX, ECX, EAX) */
    *(--stack) = 0;  /* EAX  (highest addr, popped last) */
    *(--stack) = 0;  /* ECX */
    *(--stack) = 0;  /* EDX */
    *(--stack) = 0;  /* EBX */
    *(--stack) = 0;  /* ESP  (ignored by popa) */
    *(--stack) = 0;  /* EBP */
    *(--stack) = 0;  /* ESI */
    *(--stack) = 0;  /* EDI  (lowest addr, popped first) */
    
    /* Set PCB esp to this prepared stack */
    pcb->esp = (uint32_t)stack;
    
    /* Store in process table */
    process_table[pcb->pid - 1] = pcb;
    
    /* Add to scheduler queue */
    extern void scheduler_add_process(int pid, uint32_t entry_point, uint32_t user_stack, uint32_t kernel_stack);
    scheduler_add_process(pcb->pid, entry_point, user_stack_top, kernel_stack_top);
    
    KLOG_INFO("PROC", "Created PID %d, entry=0x%x, esp=0x%x", pcb->pid, entry_point, pcb->esp);
    
    return pcb->pid;
}

/**
 * Create a process from a binary blob in a new address space.
 *
 * Allocates physical memory for binary + BSS reserve, zeros it all,
 * copies binary data, creates a cloned page directory with the region
 * mapped at base_address. Each process gets its own isolated address space.
 *
 * BSS note: objcopy -O binary strips the BSS section from ELF output.
 * We allocate extra space (BSS_RESERVE) beyond binary_size and zero it,
 * so the BSS section at runtime contains proper zeros.
 *
 * This function is format-agnostic — it knows nothing about .mex.
 * User-space libraries handle format parsing.
 */
#define MIN_BSS_RESERVE     0x4000    /* 16KB minimum BSS reserve for small apps */

int process_create_from_memory(uint32_t base_address, const void *binary_data,
                               uint32_t binary_size, uint32_t entry_offset,
                               uint32_t bss_size) {
    if (!binary_data || binary_size == 0) {
        KLOG_ERROR("PROC", "create_from_memory: invalid binary data");
        return -1;
    }

    /* Step 1: Allocate binary + actual BSS (with minimum safety floor), page-aligned */
    uint32_t effective_bss = (bss_size > MIN_BSS_RESERVE) ? bss_size : MIN_BSS_RESERVE;
    uint32_t alloc_size = (binary_size + effective_bss + 0xFFF) & ~0xFFF;

    KLOG_INFO("PROC", "create_from_memory: bin=%d, bss=%d (eff=%d), alloc=%d",
              binary_size, bss_size, effective_bss, alloc_size);

    extern void *pmm_alloc_size(uint32_t size_bytes);
    void *phys_mem = pmm_alloc_size(alloc_size);
    if (!phys_mem) {
        KLOG_ERROR("PROC", "create_from_memory: failed to alloc physical pages");
        return -1;
    }

    /* Step 2: Zero ALL allocated memory first (ensures BSS is zeroed) */
    uint8_t *dst = (uint8_t *)phys_mem;
    uint32_t *dst32 = (uint32_t *)phys_mem;
    for (uint32_t i = 0; i < alloc_size / 4; i++) {
        dst32[i] = 0;
    }

    /* Step 3: Copy binary data over the zeroed region (word-aligned fast copy)
     * Both source (caller's buffer) and dest (phys_mem) are identity-mapped
     * in the kernel page directory, so a simple memcpy works here. */
    const uint8_t *src = (const uint8_t *)binary_data;
    uint32_t words = binary_size / 4;
    const uint32_t *src32 = (const uint32_t *)src;
    dst32 = (uint32_t *)phys_mem;
    for (uint32_t i = 0; i < words; i++) {
        dst32[i] = src32[i];
    }
    for (uint32_t i = words * 4; i < binary_size; i++) {
        dst[i] = src[i];
    }

    /* Step 4: Clone kernel page directory for the new process */
    extern uint32_t *paging_clone_kernel_directory(void);
    uint32_t *new_page_dir = paging_clone_kernel_directory();
    if (!new_page_dir) {
        KLOG_ERROR("PROC", "create_from_memory: failed to clone page dir");
        return -1;
    }

    /* Step 5: Map full allocation at the requested virtual base address */
    extern void paging_map_user_region(uint32_t *page_dir, uint32_t virt_start,
                                       uint32_t phys_start, uint32_t size, uint32_t flags);
    paging_map_user_region(new_page_dir, base_address, (uint32_t)phys_mem,
                           alloc_size, 0x7); /* PAGE_PRESENT | PAGE_WRITE | PAGE_USER */

    /* Step 6: Create the process with the per-process page directory.
     * Pass new_page_dir directly to process_create() so the PCB is fully
     * initialized (correct page directory) BEFORE being added to the scheduler.
     * No race condition — the process never runs with the wrong page directory. */
    uint32_t entry_point = base_address + entry_offset;
    
    int pid = process_create(entry_point, new_page_dir);
    if (pid < 0) {
        KLOG_ERROR("PROC", "create_from_memory: process_create failed");
        extern void paging_destroy_directory(uint32_t *page_dir);
        paging_destroy_directory(new_page_dir);
        return -1;
    }

    KLOG_INFO("PROC", "Created from memory PID %d, base=0x%x, bin=%d, alloc=%d",
              pid, base_address, binary_size, alloc_size);

    /* Update memory_alloc on PCB to include binary+BSS allocation */
    process_t *pcb = process_get_by_pid(pid);
    if (pcb) {
        pcb->memory_alloc += alloc_size;
        pcb->type = PROC_TYPE_USER; /* Loaded binaries default to user type */
    }

    return pid;
}

/**
 * Get process by PID
 */
process_t* process_get_by_pid(int pid) {
    if (pid < 1 || pid > MAX_PROCESSES) {
        return 0;
    }
    return process_table[pid - 1];
}

/**
 * Get total process count
 */
int process_manager_get_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] != 0) {
            count++;
        }
    }
    return count;
}

/**
 * Terminate a process and free its resources
 * Returns: 0 on success, -1 on failure
 */
int process_terminate(int pid) {
    if (pid < 1 || pid > MAX_PROCESSES) {
        KLOG_ERROR_HEX("PROC", "Terminate: invalid PID", pid);
        return -1;
    }
    
    process_t *pcb = process_table[pid - 1];
    if (!pcb) {
        KLOG_ERROR_HEX("PROC", "Terminate: process not found, PID", pid);
        return -1;
    }
    
    /* System processes cannot be killed */
    if (pcb->type == PROC_TYPE_SYSTEM) {
        KLOG_ERROR("PROC", "Terminate DENIED: PID %d (%s) is a system process", pid, pcb->name);
        return -2;  /* Protected */
    }
    
    /* Remove from scheduler */
    extern void scheduler_remove_process(int pid);
    scheduler_remove_process(pid);
    
    /* Free PCB */
    kfree(pcb);
    process_table[pid - 1] = 0;
    
    KLOG_INFO_HEX("PROC", "Terminated PID", pid);
    return 0;
}

/**
 * List all active processes.
 * Each entry is 48 bytes:
 *   [pid:i32][state:u32][name:char[32]][type:u8][pad:u8[3]][memory:u32]
 */
int process_manager_list(void *buffer, int max_entries) {
    if (!buffer || max_entries <= 0) return -1;
    
    uint8_t *buf = (uint8_t *)buffer;
    int count = 0;
    
    for (int i = 0; i < MAX_PROCESSES && count < max_entries; i++) {
        if (process_table[i] != 0) {
            process_t *pcb = process_table[i];
            uint8_t *entry = buf + (count * 48);
            
            /* pid (4 bytes) */
            *(int32_t *)(entry + 0) = pcb->pid;
            /* state (4 bytes) */
            *(uint32_t *)(entry + 4) = pcb->state;
            /* name (32 bytes) */
            for (int j = 0; j < PROC_NAME_MAX; j++)
                entry[8 + j] = (uint8_t)pcb->name[j];
            /* type (1 byte) */
            entry[40] = pcb->type;
            /* padding (3 bytes) */
            entry[41] = 0; entry[42] = 0; entry[43] = 0;
            /* memory_alloc (4 bytes) */
            *(uint32_t *)(entry + 44) = pcb->memory_alloc;
            
            count++;
        }
    }
    
    return count;
}

/**
 * Set process name and type.
 */
int process_set_name(int pid, const char *name, uint8_t type) {
    process_t *pcb = process_get_by_pid(pid);
    if (!pcb || !name) return -1;
    
    /* Safe copy name */
    int i;
    for (i = 0; i < PROC_NAME_MAX - 1 && name[i]; i++) {
        pcb->name[i] = name[i];
    }
    pcb->name[i] = '\0';
    
    pcb->type = type;
    
    KLOG_INFO("PROC", "Set name PID %d: %s (type=%d)", pid, pcb->name, type);
    return 0;
}
