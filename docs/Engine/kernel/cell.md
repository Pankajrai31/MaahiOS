# Kernel Cell Manager

## Files
- `managers/cell/cell_manager.c/.h`

## Purpose
Global key-value registry. Cells are named data slots that any kernel
code can write and any user-space code can read (via Cell Executive).

## API
- `kernel_cell_write(key, data, size)` — set a cell value
- `kernel_cell_read(key, buf, max)` — read a cell value
- `kernel_cell_delete(key)` — remove a cell
- `kernel_cell_exists(key)` — check if cell exists
- `kernel_cell_list(buf, max)` — list all cell keys

## Common Cell Keys
- `device.disk.count` — number of disks
- `device.disk.N.name` — disk N name
- `system.memory.total/free/used` — memory stats
- `cell.exec.req_shm` / `cell.exec.resp_shm` — Cell Executive SHM queue IDs
- Similar patterns for all executives

## Known Issues
*(Agents add issues here)*
