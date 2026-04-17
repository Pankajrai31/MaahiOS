# Application Development

## MEX App Structure

Every app follows this pattern:
```
src/apps/appname/
├── appname.c          ← Application source
```

Shared files (all apps use):
```
src/apps/
├── mex_entry.s        ← Ring 3 entry point (calls main, then SYS_EXIT)
├── mex_app.ld         ← Linker script (base address 0x10000000)
```

## Build Pipeline
1. Compile: `i686-elf-gcc -c appname.c -o appname.o $UFLAGS`
2. Compile libraries the app uses: libgui.o, libfs.o, etc.
3. Link: `i686-elf-ld -T mex_app.ld -o appname.elf mex_entry.o appname.o [libs...]`
4. Pack: `python3 tools/mex_pack.py --elf appname.elf -o appname.mex`
5. Copy: appname.mex → isodir/ (gets included in boot ISO)

## App Types

### Console Apps
- Use `libconsole` for text output (routes to Terminal via SHM)
- Use `libgui/keyboard` for input
- Examples: hello, diskman, fileman, procman, sysinfo, shutdown

### GUI (Windowed) Apps
- Use `libwindow` for window creation, `surface` for drawing
- Use `libwm` to register window with WM Executive
- Get keyboard/mouse via `libio`
- Examples: hellogui, boxdrop, browser, diskexp, wordwrite

## Available Libraries for Apps
- libcell, libconsole, libdisk, libfs, libgui, libhtml, libhttp
- libio, libjs, liblog, libmemory, libmex, libnet, libprocess
- libtls, libwindow, libwm, libbmp

## Rules
- Apps NEVER call syscalls directly — always use libraries
- Apps NEVER talk to executives directly — libraries handle IPC
- Apps linked against mex_entry.s which calls main() and then SYS_EXIT

## Known Issues
*(Agents add issues here)*
