# Kernel Shared Memory Manager

## Files
- `managers/shm/shm_manager.c/.h`

## Purpose
Allocates shared memory regions accessible by multiple processes.
Used for all executive IPC (SHM queues) and special buffers (framebuffer, log).

## API
- `kernel_shm_create(size)` → shm_id
- `kernel_shm_attach(pid, shm_id)` → virtual address
- `kernel_shm_detach(pid, shm_id)`
- `kernel_shm_destroy(shm_id)`
- `kernel_shm_get_info(shm_id, info)` → size, ref count

## Limits
- MAX_SHM_REGIONS = 64
- Each region is page-aligned (4KB granularity)

## Known Issues
*(Agents add issues here)*
