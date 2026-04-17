# Executive Layer Overview

## Architecture

Executives are Ring 3 privileged programs that serve as intermediaries between
user-space libraries and the kernel. They follow a common pattern:

```
Library → SHM Queue (request) → Executive → Syscall → Kernel
                                Executive → SHM Queue (response) → Library
```

## Common Pattern (all executives)

Every executive:
1. Starts as a GRUB module loaded by sysman
2. Creates two SHM regions: request queue + response queue
3. Publishes SHM IDs to Cell Registry (so libraries can find them)
4. Enters an infinite loop: read request → process → write response
5. Processing = making syscalls to the kernel

## SHM Queue Protocol
- Defined in `system/executives/common/executive_queue.c/.h`
- Fixed-size message slots
- Lock-free single-producer/single-consumer design
- Each queue has head/tail pointers in shared memory

## Executive List

| Executive | GRUB Idx | Serves | Makes These Syscalls |
|-----------|----------|--------|---------------------|
| Log (1) | 1 | liblog | SYS_KLOG, SYS_KLOG_HEX |
| Cell (2) | 2 | libcell | SYS_CELL_* |
| Process (3) | 3 | libprocess | SYS_PROC_*, SYS_EXEC |
| Memory (4) | 4 | libmemory | SYS_ALLOC_PAGE, SYS_FREE_PAGE |
| Disk (5) | 5 | libdisk | SYS_DEV_READ/WRITE (disk) |
| FS (6) | 6 | libfs | SYS_FS_* |
| GUI (7) | 7 | libgui | SYS_DEV_IOCTL (display) |
| IO (8) | 8 | libio | SYS_DEV_READ (keyboard, mouse) |
| WM (9) | 9 | libwm | SHM-based compositor |
| Network (-) | - | libnet | SYS_NET_* |

## Boot Order (loaded by sysman)
1. Log Executive — so other executives can log
2. Cell Executive — so other executives can publish SHM IDs
3. Process Executive
4. Memory Executive
5. Disk Executive
6. FS Executive
7. GUI Executive
8. IO Executive
9. WM Executive

## Adding a New Executive

1. Create `src/system/executives/newexecutive/`
2. Files needed: `new_executive.c`, `new_executive.h`, `new_executive_entry.s`, `new_executive_linker.ld`
3. Follow the SHM queue pattern from any existing executive
4. Assign next GRUB module index
5. Update sysman to load it
6. Update build.sh
7. Create matching library in `src/system/libraries/libnew/`
8. Update `docs/Engine/executives/new.md`

## Known Issues
*(Agents add issues here)*
