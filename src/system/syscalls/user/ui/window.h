#ifndef USER_WINDOW_SYSCALLS_H
#define USER_WINDOW_SYSCALLS_H

#include "../../syscall_numbers.h"

/**
 * Window Management Syscalls - Ring 3 User Mode
 */

int syscall_find_window_by_title(const char *title);
int syscall_get_window_state(int window_id);
int syscall_restore_window(int window_id);
int syscall_focus_window(int window_id);
void syscall_set_window_icon(int window_id, const char *icon_name);

#endif // USER_WINDOW_SYSCALLS_H
