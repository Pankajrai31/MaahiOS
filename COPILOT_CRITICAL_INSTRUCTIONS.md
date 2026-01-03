# CRITICAL INSTRUCTIONS FOR GITHUB COPILOT
## READ THIS BEFORE EVERY ACTION

---

## 0. PROFESSIONAL OS DEVELOPMENT - MANDATORY RULES

### 0.1 No Patches or Workarounds - EVER
- **MANDATORY**: Never implement workarounds or patch fixes
- **MANDATORY**: This is a professional OS development project, NOT a toy
- **MANDATORY**: When facing ANY issue or doubt:
  1. **First**: Check OSDev wiki for proper implementation guidance
  2. **Second**: Research how Linux kernel handles the same feature
  3. **Third**: Research how Windows handles the same feature
  4. **Fourth**: Implement the proper, industry-standard solution

### 0.2 Examples of Professional vs Amateur Approaches
- **Double Buffering Issue**:
  - ❌ Amateur: Allocate random "back buffer" without understanding memory management
  - ✅ Professional: Research OSDev framebuffer management, implement dirty rectangle tracking like Linux/Windows, understand when to use hardware page flipping vs software rendering
  
- **Mouse Cursor Trail**:
  - ❌ Amateur: Add hacks to "clear" cursor or refresh entire screen
  - ✅ Professional: Implement cursor compositor with saved background, restore previous pixels before drawing new position
  
- **Flickering UI**:
  - ❌ Amateur: Add delays or "sleep" calls
  - ✅ Professional: Implement VSync, dirty region tracking, only redraw changed areas

### 0.3 When In Doubt
- **DO NOT** implement quick fixes that "just work"
- **DO** research the correct solution that professional OSes use
- **DO** document the research and rationale for the approach chosen
- **DO** understand WHY the solution works, not just that it does

### 0.4 Code Quality Standards
- **DO NOT** create files like _new, _simple, _minimal, _backup, _copy while testing
- **DO NOT** leave test code in place when trying a new fix - clean up previous attempts
- **DO** when user says "MAJOR ISSUE", create a doc file under docs/ with issue details, tests done, current status
- **DO** update issue docs before every test run when fixing major issues
- **DO NOT** start editing immediately after QEMU test - wait for user to report what they see

---

## 1. MOST CRITICAL - FILE LOCATIONS
⚠️⚠️⚠️ **NEVER FORGET THIS** ⚠️⚠️⚠️

### Build System
- **build.sh location**: `c:\Maahi\MaahiOS\build\build.sh`
- **boot.iso location**: `c:\Maahi\MaahiOS\build\boot.iso`
- **ALWAYS cd to build directory FIRST**: `cd c:\Maahi\MaahiOS\build`

### QEMU Command Pattern
```powershell
# CORRECT WAY #1 (ALWAYS USE ABSOLUTE PATH):
& 'C:\Program Files\qemu\qemu-system-i386.exe' -cdrom c:\Maahi\MaahiOS\build\boot.iso -serial stdio

# CORRECT WAY #2 (cd first, then relative path):
cd c:\Maahi\MaahiOS\build
& 'C:\Program Files\qemu\qemu-system-i386.exe' -cdrom boot.iso -serial stdio

# ❌ WRONG - Semicolon command chaining gets stripped by the tool:
cd c:\Maahi\MaahiOS\build; & 'C:\Program Files\qemu\qemu-system-i386.exe' -cdrom boot.iso -serial stdio

# NEVER RUN FROM c:\Maahi\MaahiOS - boot.iso is NOT there!
```

**CRITICAL: The tool strips the `cd` part when using semicolon. ALWAYS use absolute path or run cd as separate command!**

### Build Command Pattern
```powershell
# CORRECT:
cd c:\Maahi\MaahiOS\build
bash build.sh

# Build output location: c:\Maahi\MaahiOS\build\
```

## KNOWN ISSUES - DO NOT FORGET



### QEMU Output
- **NEVER limit QEMU output** with `Select-Object -First` or similar
- Let QEMU run fully to see all serial output
- Don't close QEMU early by adding filters

## PROJECT STRUCTURE

### Process Hierarchy
1. Kernel (PID 0) - Ring 0
2. Sysman (PID 1) - Ring 3, creates other processes
3. UIManager (PID 2) - Ring 3, window server, owns framebuffer
4. Orbit (PID 3) - Ring 3, desktop shell, uses UIMan API

### Current Architecture
- **UIManager**: Window server, owns framebuffer, reads mouse, does hit testing, renders controls
- **Kernel**: Owns UI state arrays (g_kernel_windows[], g_kernel_controls[], g_kernel_event_queues[])
- **Syscalls**: All UI operations go through syscalls (40-47)
- **Orbit**: Pure client, uses syscalls to create UI controls, polls for events

### Key Architectural Principles
- **Ring 3 Protection is Sacred**: User-space code MUST use syscalls (INT 0x80) for kernel operations
- **NO Direct Hardware Access from Ring 3**: NEVER allow Ring 3 to touch hardware directly
- **Memory Safety**: ALWAYS use vmap_copy_from_user() and vmap_copy_to_user() for user/kernel transfers
- **Single Source of Truth**: Kernel owns all UI state, UIManager renders from kernel arrays

### Key Files
- Kernel: `c:\Maahi\MaahiOS\src\kernel.c`
- Sysman: `c:\Maahi\MaahiOS\src\sysman\sysman.c`
- UIManager: `c:\Maahi\MaahiOS\src\uimanager\uimanager.c`
- Orbit: `c:\Maahi\MaahiOS\src\orbit\orbit.c`
- Syscalls: `c:\Maahi\MaahiOS\src\syscalls\` (syscall_numbers.h, syscall_handler.c, user_syscalls.c/h)
- Interrupt handling: `c:\Maahi\MaahiOS\src\managers\interrupt\interrupt_stubs.s`
- Build script: `c:\Maahi\MaahiOS\build\build.sh`

---

## CODING STANDARDS AND PATTERNS

### Memory Management
- **CRITICAL**: ALWAYS validate user pointers before use in kernel
- **Use**: vmap_copy_from_user(), vmap_copy_to_user() for safe transfers
- **NEVER**: Directly dereference user-space pointers in kernel code

### System Calls
- **Interface**: INT 0x80 software interrupt
- **Parameters**: EAX=syscall_num, EBX=arg1, ECX=arg2, EDX=arg3, ESI=arg4, Stack for arg5+
- **Stack Args**: For >4 args, user pushes extra args on stack before INT 0x80
- **CRITICAL**: Kernel reads user stack via saved ESP in interrupt frame at [EBP+16]

### Rendering System (Professional approach)
- **Direct Framebuffer Rendering**: No back buffer initially (requires proper heap allocator)
- **Dirty Rectangle Tracking**: Linux/Windows style - only redraw changed regions
- **Cursor Handling**: Save background pixels, restore before moving cursor
- **VSync**: Future enhancement when VBE VSync interrupts are implemented
- **Memory Mapped Framebuffer**: 0xE0000000 (BGA mode, 800x600x32bpp)

---

## MISTAKES TO AVOID
1. ❌ Running QEMU from wrong directory (c:\Maahi\MaahiOS instead of build/)
2. ❌ Limiting QEMU output and missing important logs
3. ❌ Assuming Orbit crashed when debug messages don't appear
4. ❌ Forgetting that Orbit messages don't show (known issue)
5. ❌ Not changing directory before build/run commands

## CHECKLIST BEFORE RUNNING COMMANDS
- [ ] Am I in c:\Maahi\MaahiOS\build directory?
- [ ] Does boot.iso exist here? (Test-Path boot.iso)
- [ ] Am I running QEMU without output limits?
- [ ] Do I remember Orbit messages don't appear?

---
**REMEMBER**: User has told you about these mistakes 1000+ times over 9 months. 
READ THIS FILE BEFORE EVERY BUILD/RUN COMMAND.
