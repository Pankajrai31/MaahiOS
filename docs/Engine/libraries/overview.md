# Library Layer Overview

## Architecture

Libraries are the user-facing API layer. Apps call library functions, which
internally communicate with executives via SHM queues.

```
App → Library Function → SHM Queue Push → Executive → Syscall → Kernel
                         SHM Queue Poll ← Executive ←
```

## Library Inventory (20 total)

### Core Infrastructure
| Library | Purpose | Talks To |
|---------|---------|----------|
| core/ | syscall_helpers.h macros | Direct kernel (syscall0-7) |
| libmex/ | MEX parser/executor | Process Executive |
| shared/ | io.h, type headers | N/A (headers only) |

### System Services
| Library | Purpose | Talks To |
|---------|---------|----------|
| libcell/ | Cell registry | Cell Executive |
| liblog/ | User logging | Log Executive |
| libprocess/ | Process management | Process Executive |
| libmemory/ | Memory allocation | Memory Executive |

### Storage & Files
| Library | Purpose | Talks To |
|---------|---------|----------|
| libdisk/ | Block device I/O | Disk Executive |
| libfs/ | File/directory ops | FS Executive |

### Graphics & UI
| Library | Purpose | Talks To |
|---------|---------|----------|
| libgui/ | Drawing, fonts, FB | GUI Executive (+ direct IOCTL for flip) |
| libwindow/ | Windowed UI framework | WM Executive (via libwm) |
| libwm/ | Window manager client | WM Executive |
| libbmp/ | BMP image decoder | N/A (pure computation) |

### Input/Output
| Library | Purpose | Talks To |
|---------|---------|----------|
| libio/ | Device input events | IO Executive |
| libconsole/ | Console stdout | Terminal (via SHM) |

### Networking
| Library | Purpose | Talks To |
|---------|---------|----------|
| libnet/ | Socket API | Network syscalls (direct) |
| libhttp/ | HTTP client | libnet → kernel |
| libtls/ | TLS 1.2 encryption | libnet → kernel |

### Advanced
| Library | Purpose | Talks To |
|---------|---------|----------|
| libhtml/ | HTML tokenizer | N/A (pure computation) |
| libjs/ | JavaScript interpreter | N/A (pure computation) |

## Naming Conventions
- All libraries prefixed with `lib`
- Public functions prefixed with library name: `libfs_read_file()`, `libdisk_list()`
- Headers: `libname.h` in library folder
- Implementation: `libname.c` in library folder

## Adding a New Library
1. Create `src/system/libraries/libname/`
2. Create `libname.h` (public API) and `libname.c` (implementation)
3. If it talks to an executive: follow the SHM queue client pattern
4. Add compilation to `build.sh`
5. Link into apps that use it
6. Update `docs/Engine/libraries/libname.md`

## Known Issues
*(Agents add issues here)*
