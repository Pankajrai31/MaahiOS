# MaahiOS Cleanup Plan

## Changes to Make:

### 1. Remove CD-ROM Drivers
- Remove: src/drivers/pci.c
- Remove: src/drivers/ahci.c  
- Remove: src/drivers/iso9660.c
- Remove from build.sh compilation
- Remove from kernel.c initialization

### 2. Remove Task Files
- Delete entire: src/tasks/ folder
- Remove task1.c, task2.c, task3.c
- Remove from build.sh compilation
- Remove from kernel.c linking

### 3. Process Manager Changes
- **Heap for PCB**: Create process manager's own heap during init (NOT kernel heap)
- **process_create_sysman()**: Should use process manager's heap, NOT kernel heap
- **process_create()**: Generic function, uses process manager's heap
- **Remove all VGA prints** from process manager functions
- **Return values**: Simple success/failure codes

### 4. Syscalls for Process Management
Keep:
- SYSCALL_CREATE_PROCESS - create new process
- SYSCALL_EXIT - terminate current process (already exists)
- SYSCALL_GET_ORBIT_ADDR - get orbit address

Remove:
- SYSCALL_ISO_LIST_FILES (no CD-ROM)

### 5. Remove Verbose Prints
- No VGA prints in kheap.c
- No VGA prints in process_manager.c
- Keep minimal kernel boot messages only

### 6. Interrupt Handler
- Check if any CD-ROM interrupt handling exists, remove it

## Questions Before Proceeding:
1. Process manager heap - should it be fixed size array or dynamic?
2. Should process_create() just create PCB and let scheduler handle starting?
3. Keep scheduler or also simplify it?
