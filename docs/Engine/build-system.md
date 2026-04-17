# MaahiOS Build System

## Cross-Compiler

- **Toolchain**: i686-elf-gcc / i686-elf-ld
- **Location**: `C:\i686-elf-tools\bin`
- **Target**: 32-bit x86, freestanding (no libc, no libgcc)
- **Flags**: `-ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32`

## Build Command

```powershell
cd c:\Maahi\MaahiOS\build
$env:PATH = "C:\i686-elf-tools\bin;C:\msys64\usr\bin;" + $env:PATH
bash.exe ./build.sh
```

Or use the convenience script:
```powershell
.\run_maahios.ps1
```

## Build Pipeline (build.sh)

1. Compile kernel sources → .o files
2. Link kernel → kernel.elf
3. Compile each executive → .elf → .bin (raw binary via objcopy)
4. Compile system programs (orbit, terminal) → .elf → .bin
5. Compile each app → .elf → .mex (via mex_pack.py)
6. Copy icons to isodir/icons/
7. Generate GRUB config (grub.cfg) with module list
8. Build ISO via grub-mkrescue

## MEX Format

MEX (MaahiOS EXecutable) is the user-space binary format:
- Header: magic, entry point, code size, BSS size
- Body: raw code + data
- Tool: `tools/mex_pack.py` — packs ELF into MEX, auto-detects BSS from ELF

### BSS Handling
- `mex_pack.py --elf <file>` reads ELF section headers to compute BSS size
- MEX header stores bss_size
- `process_create_from_memory()` allocates `code_size + bss_size` pages
- If bss_size > MIN_BSS_RESERVE, the process gets extra pages automatically

## Freestanding Constraints

- No libc functions (memcpy, strlen, printf, etc.) — must provide own
- No libgcc: no `__moddi3`, `__divdi3` (64-bit integer division)
  - Never use `int64_t` for division/modulo operations
  - Use `int` (32-bit) instead
- No `float.h` runtime — hardware FPU only, no soft-float helpers
- All user-space code runs from address 0x10000000

## QEMU Launch

```powershell
Start-Process -FilePath "C:\Program Files\qemu\qemu-system-i386.exe" `
  -ArgumentList "-cdrom","boot.iso","-m","512M",`
    "-serial","file:serial.log",`
    "-netdev","user,id=net0","-device","e1000,netdev=net0",`
    "-display","sdl" `
  -WorkingDirectory "c:\Maahi\MaahiOS\build"
```

## Serial Log

- Output: `build/serial.log`
- Contains all klog() output from kernel and executives
- Search with: `Get-Content serial.log | Select-String "PATTERN"`
