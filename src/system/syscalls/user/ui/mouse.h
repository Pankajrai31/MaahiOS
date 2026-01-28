#ifndef USER_MOUSE_SYSCALLS_H
#define USER_MOUSE_SYSCALLS_H

#include "../../syscall_numbers.h"

/**
 * Mouse and Input Syscalls - Ring 3 User Mode
 */

int syscall_mouse_get_x(void);
int syscall_mouse_get_y(void);
unsigned int syscall_mouse_get_buttons(void);
int syscall_mouse_get_irq_total(void);
int syscall_poll_mouse(void);
unsigned int syscall_get_pic_mask(void);
void syscall_re_enable_mouse(void);

#endif // USER_MOUSE_SYSCALLS_H
