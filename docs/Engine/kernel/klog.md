# Kernel Logging (klog)

## Files
- `managers/klog/klog.c/.h`

## API
- `klog(level, tag, message)` — log a text message
- `klog_hex(level, tag, prefix, value)` — log with hex value
- `kernel_klog_get_shm_id()` — get SHM region for log buffer

## Log Levels
- 1 = ERROR
- 2 = WARN
- 3 = INFO
- 4 = DEBUG

## Output
- Serial port (COM1) — captured to `build/serial.log` in QEMU
- SHM buffer — readable by Log Executive and user-space (via KLOG_READ syscall)

## Format
```
[LEVEL][TAG] message
[LEVEL][TAG] prefix: 0xHEXVALUE
```

## Known Issues
*(Agents add issues here)*
