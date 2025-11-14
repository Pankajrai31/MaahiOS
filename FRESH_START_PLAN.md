# MaahiOS - Fresh Start Plan

## Why We're Starting Over

**Problems with Previous Approach:**
- Too many managers added at once (HAL, Process, Scheduler, Debug, I/O, etc.)
- Complex abstraction layers (HAL, usercalls, kernel calls) added before basic stability
- Page table setup accessing unmapped memory causing random reboots
- Multiple ISO folders, boot folders, test files causing confusion
- Dual VGA debug system added unnecessary complexity
- Mixing C and Assembly in single files made debugging harder
- Custom C libraries instead of using standard kernel patterns

**What We Should Have Done:**
1. ✅ Boot kernel with VGA output
2. ✅ Simple paging setup (identity mapping) 
3. ✅ Transition to Ring 3
4. ✅ Load and run user program
5. ❌ Then add managers as needed (not before)

---

## Vision - Simple Clean Architecture

```
┌─────────────────────────────────┐
│   User Program (Ring 3)          │
│   sysmgr.bin / orbit.bin         │
└──────────────┬──────────────────┘
               │ INT 0x80 syscall
               ↓
┌─────────────────────────────────┐
│   Kernel (Ring 0)               │
│ ┌─────────────────────────────┐ │
│ │ boot.s  - Multiboot entry  │ │
│ │ kernel.c - VGA, paging,    │ │
│ │           GDT, IDT, Ring3  │ │
│ └─────────────────────────────┘ │
└────────────────┬────────────────┘
                 │
            ┌────┴────┐
            ↓         ↓
        [ VGA ]   [ GRUB ]
```

**NO managers yet.** Just core kernel that boots and transitions to Ring 3.

---

## Folder Structure (Clean)

```
MaahiOS/
├── backup_current/              # Old code for reference only
├── build/                       # Build output
├── root/
│   └── kernel/
│       ├── boot.s              # ONLY: Multiboot, stack, jump to kernel_main
│       ├── kernel.c            # ONLY: VGA, paging, GDT, IDT, Ring3 launch
│       ├── linker.ld           # Simple link script
│       └── kernel.bin          # Built kernel
├── root/sysmgr/
│   ├── sysmgr.c                # User program (prints "Hello from SysMgr")
│   └── sysmgr.bin
├── root/orbit/
│   ├── orbit.c                 # Second user program (prints "Hello from Orbit")
│   └── orbit.bin
├── build.sh                     # Build script
└── create-iso.sh                # Create ISO script
```

---

## What Each File Does (Minimal Implementation)

### boot.s
```asm
# Multiboot header (GRUB loads this)
# Set up minimal stack
# Jump to kernel_main(magic, mb_info)
# Total: ~30 lines, no more
```

### kernel.c
```c
void kernel_main(uint32_t magic, multiboot_info_t *mb_info) {
    // 1. Initialize VGA for printing
    vga_init();
    vga_print("MaahiOS Booting...\n");
    
    // 2. Set up GDT (3 entries: NULL, Ring0 code, Ring3 code+data)
    gdt_init();
    vga_print("GDT: OK\n");
    
    // 3. Set up IDT (only exception handlers + INT 0x80 for syscalls)
    idt_init();
    vga_print("IDT: OK\n");
    
    // 4. Enable paging (identity map 0-4MB)
    paging_init();
    vga_print("Paging: OK\n");
    
    // 5. Load SysMgr binary from GRUB modules
    void *sysmgr_addr = grub_find_module("sysmgr");
    vga_print("SysMgr: 0x");
    vga_print_hex((uint32_t)sysmgr_addr);
    vga_print("\n");
    
    // 6. Map SysMgr code to Ring 3 space (0x400000)
    map_user_code(sysmgr_addr, 0x400000);
    
    // 7. Allocate Ring 3 stack at 0x500000
    allocate_user_stack(0x500000);
    
    // 8. Transition to Ring 3 with IRET
    ring3_execute(0x400000, 0x500FFC);
    
    // Should not return
    while(1) vga_print(".");
}
```

### sysmgr.c (User Mode)
```c
void _start(void) {
    // Simple syscall to print
    syscall_print("Hello from SysMgr!\n");
    
    // Load orbit
    syscall_load_program("orbit", 0x300000);
    
    // Infinite loop
    while(1) {
        // Wait for keyboard interrupt or something
    }
}
```

---

## Implementation Sequence (Do ONE per day, test between each)

### Day 1: Boot + VGA
- [ ] Simple boot.s (Multiboot only)
- [ ] kernel.c with vga_init(), vga_print()
- [ ] Test: See "MaahiOS Booting..." on screen ✅

### Day 2: GDT
- [ ] Implement gdt_init() with 3 descriptors
- [ ] Test: System still boots, no crashes ✅

### Day 3: IDT + Exceptions
- [ ] Implement idt_init() 
- [ ] Add exception handlers (just print for now)
- [ ] Test: No immediate exceptions during boot ✅

### Day 4: Basic Paging
- [ ] Implement paging_init() - identity map 0-4MB only
- [ ] Enable CR0.PG
- [ ] Test: Kernel still runs after paging enabled ✅

### Day 5: Load SysMgr Binary
- [ ] Implement grub_find_module("sysmgr")
- [ ] Find SysMgr address from GRUB
- [ ] Test: Print SysMgr address to screen ✅

### Day 6: User Code Mapping
- [ ] Implement map_user_code(src, 0x400000)
- [ ] Copy SysMgr to 0x400000 in page tables
- [ ] Test: SysMgr code accessible at new address ✅

### Day 7: User Stack Allocation
- [ ] Implement allocate_user_stack(0x500000)
- [ ] Allocate physical page, map to 0x500000
- [ ] Test: Stack accessible ✅

### Day 8: Ring 3 Transition (The Critical Part)
- [ ] Implement ring3_execute(entry, stack_top)
- [ ] Build IRET frame: SS(0x23), ESP, EFLAGS, CS(0x1B), EIP
- [ ] Execute IRET
- [ ] Add infinite loop so we know it worked
- [ ] Test: System boots, doesn't crash ✅

### Day 9: Syscall Handler (INT 0x80)
- [ ] Implement INT 0x80 handler
- [ ] Simple syscall: print to VGA
- [ ] Test: User mode can call print ✅

### Day 10: SysMgr Program
- [ ] Simple sysmgr.c: `syscall_print("Hello from SysMgr!")`
- [ ] Test: Text appears on screen ✅

---

## Key Differences from Before

| Before | After |
|--------|-------|
| HAL layer | Direct hardware access |
| Complex managers | Only what's needed |
| Multiple page regions | Simple identity map (0-4MB) |
| Dual VGA debug | Simple vga_print() |
| Separate usercalls lib | Direct INT 0x80 in C |
| 1000s of lines | <1000 lines total |

---

## References (Follow These Exactly)

- **Multiboot**: https://www.gnu.org/software/grub/manual/multiboot/
- **GDT**: https://wiki.osdev.org/Global_Descriptor_Table
- **Paging**: https://wiki.osdev.org/Paging
- **IDT**: https://wiki.osdev.org/Interrupt_Descriptor_Table
- **Ring Transitions**: https://wiki.osdev.org/Handling_Exceptions

---

## Success Criteria

**Minimal System Running:**
```
[Boot...]
MaahiOS Booting...
GDT: OK
IDT: OK
Paging: OK
SysMgr: 0x12AB34
[Ring 3 transition...]
Hello from SysMgr!
```

**Then we can add:**
- Orbit program
- Keyboard input
- Disk I/O
- Process scheduling
- (etc.)

---

## You Are NOT Allowed to Add

- ❌ HAL layer
- ❌ Manager abstractions
- ❌ Complex libraries
- ❌ Debug menus
- ❌ Dual VGA systems
- ❌ Functions that aren't immediately needed

**Only add if you NEED IT to boot and run SysMgr.**

---

## Important Notes

1. **Every step tests in QEMU** - no skipping
2. **Print everything** - trace the execution
3. **Keep it stupid simple** - 5 lines beats 500
4. **One feature at a time** - don't try to build Ring3 + paging + GDT at once
5. **Reference osdev.wiki constantly** - it's 100% correct

---

## Success Message

When you see this, we've won:

```
MaahiOS Booting...
GDT: OK
IDT: OK
Paging: OK
SysMgr: 0xXXXXXX
Hello from SysMgr!
```

That's it. Simple. Clean. Working.

---

You've got this. Let's build it right this time.
