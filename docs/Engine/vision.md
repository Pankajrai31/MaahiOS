# MaahiOS Vision & Roadmap

**Owner: User (Maahi) — only updated with explicit user approval**

## Mission

MaahiOS is a 32-bit x86 operating system built from scratch. The goal is to reach
a level where the OS is:

1. **Complete in its kernel layer** — all managers have proper capabilities and
   structured error reporting
2. **Clean in its syscall layer** — proper structure, consistent design, full coverage
3. **Robust in its system programs** — sysman manages executive health, a security
   manager oversees OS security, orbit and terminal provide full shell capabilities
   comparable to Windows CMD and Linux bash
4. **Mature in its executive layer** — well-architected, consistent patterns, with
   the right set of executives (not too few, not too many)
5. **Rich in user libraries** — comprehensive set enabling app developers to build
   both console and GUI applications
6. **Self-hosting** — a setup app on the desktop that can make a disk bootable,
   copy the OS to it, and boot from real disk. Then a Visual Basic-like IDE that
   runs on the OS itself for creating MEX apps (console and GUI)

## Current State (March 2026)

### What Works
- Full boot → desktop → windowed apps pipeline
- 16 kernel managers, 63+ syscalls across 11 domains
- 10 executives with SHM-based IPC
- 20 user-space libraries
- 15 applications (.mex format)
- Networking: E1000 driver, TCP/IP, TLS 1.2, HTTP client, web browser
- GUI: Compositor, windowed apps, AA fonts, icon system
- Filesystems: ISO 9660 (CD-ROM), MFS (native)

### What Needs Work
- Error reporting across managers is inconsistent (many return bare -1)
- No security model (all processes run with same privileges in Ring 3)
- Terminal lacks scripting, piping, redirection, environment variables
- No disk-boot setup capability yet
- No on-OS development environment
- Architecture violations accumulate without automated enforcement

## Milestones (User-defined priority order)

### Phase 1: Foundation Hardening
- [ ] Structured error codes across all managers
- [ ] Consistent error reporting in all syscall handlers
- [ ] Kernel logging improvements (severity filtering, structured output)
- [ ] Memory manager audit (BSS handling, page allocation edge cases)

### Phase 2: System Program Maturity
- [ ] Terminal: command piping, redirection, environment variables, scripting
- [ ] Sysman: executive health monitoring, restart on crash
- [ ] Security System Manager (future)

### Phase 3: Executive & Library Completeness
- [ ] Audit executives for coverage gaps
- [ ] Expand libraries based on app developer needs
- [ ] Standardize all executive SHM queue protocols

### Phase 4: Self-Hosting
- [ ] Setup app: format disk, install bootloader, copy OS files
- [ ] Boot from real disk (MFS)
- [ ] On-OS IDE for MEX development

## Architecture Principles (Immutable)

1. **6-layer model** — App → Library → Executive → Syscall → Manager → Driver
2. **Never skip a layer** — documented exceptions only (see architecture.md)
3. **Ring 0 is minimal** — kernel does dispatch, not business logic
4. **SHM queues for IPC** — all executive communication uses shared memory
5. **Executives own devices** — only executives make SYS_DEV_* syscalls
6. **Cell registry is global state** — publish/subscribe via key-value pairs
