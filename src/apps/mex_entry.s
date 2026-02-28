/*
 * mex_entry.s - Standard entry point for MaahiOS .mex applications
 *
 * Every .mex app uses this entry stub. It sets up Ring 3 data segments
 * and calls mex_main() which the app must define.
 *
 * The app implements:   void mex_main(void);
 * Build with:           i686-elf-gcc -c mex_entry.s -o mex_entry.o
 */

.section .text._start
.global _start
.extern mex_main

_start:
    /* Set up Ring 3 data segments (selector 0x23 = GDT entry 4, RPL 3) */
    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    /* Zero BSS section */
    movl $__bss_start, %edi
    movl $__bss_end, %ecx
    subl %edi, %ecx
    jz .bss_done
    xorl %eax, %eax
    rep stosb
.bss_done:

    /* Call application entry point */
    call mex_main

    /* If mex_main returns, halt via syscall (SYS_EXIT = 17) */
    movl $17, %eax
    movl $0, %ebx
    int $0x80

    /* Fallback hang if exit syscall fails */
.hang:
    hlt
    jmp .hang
