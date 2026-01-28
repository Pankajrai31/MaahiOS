/**
 * UI Syscall Handlers - Split by Component
 * Header file declaring all UI syscall handler functions
 */

#ifndef UI_HANDLERS_H
#define UI_HANDLERS_H

/**
 * Text-mode VGA syscall handler
 * Handles: PUTCHAR, PUTS, PUTINT, CLEAR, SET_COLOR, SET_CURSOR, DRAW_RECT, PRINT_AT, DRAW_BOX
 */
unsigned int syscall_handle_text_vga(unsigned int syscall_num,
                                      unsigned int arg1,
                                      unsigned int arg2,
                                      unsigned int arg3);

/**
 * Graphics-mode syscall handler
 * Handles: GRAPHICS_MODE, PUT_PIXEL, CLEAR_GFX, GFX_SET_COLOR, GFX_FILL_RECT, GFX_PRINT_AT, GFX_CLEAR_COLOR
 */
unsigned int syscall_handle_graphics(unsigned int syscall_num,
                                      unsigned int arg1,
                                      unsigned int arg2,
                                      unsigned int arg3,
                                      unsigned int arg4_esi);

/**
 * Window management syscall handler
 * Handles: UI_CREATE_WINDOW, FIND_WINDOW_BY_TITLE, GET_WINDOW_STATE, RESTORE_WINDOW, FOCUS_WINDOW, SET_WINDOW_ICON
 */
unsigned int syscall_handle_window(unsigned int syscall_num,
                                    unsigned int arg1,
                                    unsigned int arg2,
                                    unsigned int arg3,
                                    unsigned int arg4_esi,
                                    unsigned int arg5,
                                    unsigned int arg6);

/**
 * UI controls syscall handler
 * Handles: UI_CREATE_BUTTON, UI_CREATE_ICON, UI_CREATE_LABEL, UI_CREATE_LIST, UI_POLL_EVENT, UI_GET_*_PTR
 */
unsigned int syscall_handle_controls(unsigned int syscall_num,
                                      unsigned int arg1,
                                      unsigned int arg2,
                                      unsigned int arg3,
                                      unsigned int arg4_esi,
                                      unsigned int arg5,
                                      unsigned int arg6);

/**
 * Advanced control framework syscall handler
 * Handles: CONTROL_CREATE, CONTROL_SET_*, PANEL_*, TABLE_*, TEXTBOX_*
 */
unsigned int syscall_handle_control_framework(unsigned int syscall_num,
                                                unsigned int arg1,
                                                unsigned int arg2,
                                                unsigned int arg3,
                                                unsigned int arg4_esi,
                                                unsigned int arg5);

/**
 * Mouse/input syscall handler
 * Handles: MOUSE_GET_X/Y/BUTTONS/IRQ_TOTAL, GET_PIC_MASK, RE_ENABLE_MOUSE, POLL_MOUSE, BGA_CURSOR_IS_SUPPORTED
 */
unsigned int syscall_handle_mouse_input(unsigned int syscall_num);

#endif // UI_HANDLERS_H
