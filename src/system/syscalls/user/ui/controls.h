#ifndef USER_CONTROLS_SYSCALLS_H
#define USER_CONTROLS_SYSCALLS_H

#include "../../syscall_numbers.h"
#include <stdint.h>

/**
 * UI Controls Syscalls - Ring 3 User Mode
 */

int syscall_ui_create_button(int window_id, int x, int y, int w, int h, const char *text);
int syscall_ui_create_label(int window_id, int x, int y, const char *text);
int syscall_ui_poll_event(void *event_ptr);
int syscall_ui_create_icon(int window_id, int x, int y, const char *text);
int syscall_ui_update_control_text(int control_id, const char *text);
int syscall_ui_register_button(int owner_pid, int x, int y, int w, int h, const char *label);

#endif // USER_CONTROLS_SYSCALLS_H
