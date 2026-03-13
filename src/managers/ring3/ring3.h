/**
 * MaahiOS Ring 3 (User Mode) Switcher
 * 
 * Provides the mechanism to jump from Ring 0 to Ring 3.
 */

#ifndef RING3_H
#define RING3_H

/**
 * Switch to Ring 3 (user mode) with the given entry point and stack.
 * This function never returns.
 * @param entry_point User-mode entry address
 * @param stack_top   Top of user-mode stack
 */
void ring3_switch_with_stack(unsigned int entry_point,
                             unsigned int stack_top) __attribute__((noreturn));

#endif /* RING3_H */
