# Kernel Syscall System

## Overview

MaahiOS uses `int 0x80` for all user → kernel transitions.
The syscall dispatcher is in `managers/syscall/syscall_manager.c`.
Handler functions are in `managers/syscall/handlers/`.

## Syscall Numbers

Defined in: `managers/syscall/syscall_numbers.h`

| Domain | Range | Count | Handler File |
|--------|-------|-------|-------------|
| Core | 0–5 | 6 | core_handlers.c |
| Process | 16–22 | 7 | process_handlers.c |
| Memory | 32–35 | 4 | memory_handlers.c |
| SHM | 48–52 | 5 | shm_handlers.c |
| Cell | 64–69 | 6 | cell_handlers.c |
| Device | 80–87 | 8 | device_handlers.c |
| Module | 96–100 | 5 | module_handlers.c |
| Time | 112–117 | 6 | time_handlers.c |
| Filesystem | 128–137 | 10 | fs_handlers.c |
| Network | 144–156 | 13 | network_handlers.c |
| Debug/KLog | 240–246 | 7 | debug_handlers.c |

## Adding a New Syscall

1. Add number to `syscall_numbers.h`
2. Add case to the appropriate handler file in `handlers/`
3. Add dispatch entry in `syscall_manager.c` if new domain
4. Implement the actual logic in the relevant manager
5. Update this doc

## Handler Rules

- Handlers are thin dispatch — validate params, call manager, return result
- No business logic in handlers
- Always validate user-space pointers before dereferencing
- Return negative values for errors

## Known Issues
*(Agents add issues here)*
