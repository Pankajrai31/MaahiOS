# Kernel Process Manager

## Files
- `managers/process/process_manager.c/.h` — Process lifecycle
- `managers/process/mex.h` — MEX binary format header
- `managers/scheduler/scheduler.c/.h` — Round-robin scheduler
- `managers/scheduler/switch_osdev.s` — Context switch (assembly)
- `managers/ring3/ring3.c/.h` — Ring 0 → Ring 3 transition

## Key API
- `process_create(name, code, size)` — Create from raw binary
- `process_create_from_memory(name, code, size, bss_size)` — Create with explicit BSS
- `process_terminate(pid)` — Kill a process
- `process_manager_list(buf, max)` — List active processes
- `schedule()` — Pick next process to run
- `scheduler_add_process(pcb)` — Add to run queue

## MEX Format
- Magic: `MEX\0`
- Header fields: entry_offset, code_size, bss_size
- Code starts at 0x10000000 in user virtual memory
- BSS follows code, zeroed on load

## Known Issues
*(Agents add issues here)*
