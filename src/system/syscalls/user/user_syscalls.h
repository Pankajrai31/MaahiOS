#ifndef USER_SYSCALLS_H
#define USER_SYSCALLS_H

/* Include syscall number definitions */
#include "../syscall_numbers.h"

/**
 * Ring 3 Syscall Interface - Unified Header
 * 
 * This header includes all split syscall component headers
 * for backward compatibility
 */

/* Include component-specific syscall headers */
#include "ui/io.h"
#include "ui/memory.h"
#include "ui/process.h"
#include "ui/graphics.h"
#include "ui/mouse.h"
#include "ui/window.h"
#include "ui/controls.h"
#include "ui/iso_fs.h"

#endif // USER_SYSCALLS_H
