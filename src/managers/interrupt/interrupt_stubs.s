.section .text
.globl exception_stub_0
.globl exception_stub_1
.globl exception_stub_2
.globl exception_stub_3
.globl exception_stub_4
.globl exception_stub_5
.globl exception_stub_6
.globl exception_stub_7
.globl exception_stub_8
.globl exception_stub_9
.globl exception_stub_10
.globl exception_stub_11
.globl exception_stub_12
.globl exception_stub_13
.globl exception_stub_14
.globl exception_stub_15
.globl exception_stub_16
.globl exception_stub_17
.globl exception_stub_18
.globl exception_stub_19
.globl syscall_int
.globl irq0_stub

/* Extern C handler functions */
.extern exception_handler
.extern syscall_dispatcher
.extern pit_handler

/* Exception stub for exceptions WITHOUT error code (e.g., exception 0 - divide by zero) */
.macro exception_no_error_code exception_num
.align 4
exception_stub_\exception_num:
    push $\exception_num        /* Push exception number (2nd param) */
    push $0                     /* Push dummy error code (1st param) */
    jmp exception_common
.endm

/* Exception stub for exceptions WITH error code (e.g., exception 13 - GPF) */
.macro exception_with_error_code exception_num
.align 4
exception_stub_\exception_num:
    /* Error code already on stack, push by CPU */
    push $\exception_num        /* Push exception number (2nd param) */
    jmp exception_common
.endm

/* Common handler for all exceptions */
.align 4
exception_common:
    /* Save all general purpose registers */
    push %eax
    push %ebx
    push %ecx
    push %edx
    push %esi
    push %edi
    push %ebp
    
    /* Call C exception handler */
    /* Stack: [ESP] = ebp, [ESP+4] = edi, ..., [ESP+32] = exception_num, [ESP+36] = error_code */
    call exception_handler
    
    /* Restore all registers */
    pop %ebp
    pop %edi
    pop %esi
    pop %edx
    pop %ecx
    pop %ebx
    pop %eax
    
    /* Remove exception number and error code from stack */
    add $8, %esp
    
    /* Return from exception */
    iret

/* Define exception stubs */
exception_no_error_code 0       /* Divide by zero */
exception_no_error_code 1       /* Debug */
exception_no_error_code 2       /* NMI */
exception_no_error_code 3       /* Breakpoint */
exception_no_error_code 4       /* Overflow */
exception_no_error_code 5       /* Bound range exceeded */
exception_no_error_code 6       /* Invalid opcode */
exception_no_error_code 7       /* Device not available */
exception_with_error_code 8     /* Double fault */
exception_no_error_code 9       /* Coprocessor segment overrun */
exception_with_error_code 10    /* Invalid TSS */
exception_with_error_code 11    /* Segment not present */
exception_with_error_code 12    /* Stack segment fault */
exception_with_error_code 13    /* General protection fault */
exception_with_error_code 14    /* Page fault - CPU pushes error code */
exception_no_error_code 15      /* Reserved */
exception_no_error_code 16      /* FPU exception */
exception_with_error_code 17    /* Alignment check */
exception_no_error_code 18      /* Machine check */
exception_no_error_code 19      /* SIMD exception */

/* ============================================================ */
/* SYSCALL HANDLER - INT 0x80 */
/* ============================================================ */
/* 
 * Syscall stub for INT 0x80 (software interrupt)
 * 
 * Called from Ring 3 user code when syscall_putchar(), syscall_puts(), etc. are called
 * 
 * Register conventions on entry:
 *   EAX = syscall number (SYSCALL_PUTCHAR, SYSCALL_PUTS, etc.)
 *   EBX = first argument
 *   ECX = second argument (if needed)
 *   EDX = third argument (if needed)
 * 
 * INT 0x80 is a trap gate, CPU automatically:
 *   - Saves EFLAGS, CS, EIP to stack
 *   - Switches from Ring 3 to Ring 0
 *   - Jumps here
 * 
 * Note: NO error code is pushed by CPU for INT 0x80
 * Only EFLAGS, CS, EIP are on the stack
 */
.align 4
syscall_int:
    /* Software interrupt handler for INT 0x80 (syscalls) - OSDev approach */
    /* When INT 0x80 is executed from Ring 3:
     * - CPU pushes: [EFLAGS] [CS] [EIP]
     * - Registers contain: EAX=syscall#, EBX=arg1, ECX=arg2, EDX=arg3
     * 
     * The OSDev approach: Pass registers directly to C via registers, not stack
     * This avoids any stack offset confusion
     */
    
    /* Save callee-saved registers (we might clobber them) */
    push %ebp
    mov %esp, %ebp
    push %ebx
    push %edi
    push %esi
    
    /* Now we have clean register access:
     * EAX = syscall number (don't touch it yet)
     * EBX = arg1
     * ECX = arg2
     * EDX = arg3
     */
    
    /* Call dispatcher: syscall_dispatcher(eax, ebx, ecx, edx)
     * In cdecl, we need to push args right-to-left */
    push %edx                   /* arg3 -> 3rd parameter */
    push %ecx                   /* arg2 -> 2nd parameter */
    push %ebx                   /* arg1 -> 1st parameter */
    push %eax                   /* syscall_num -> 0th parameter */
    
    call syscall_dispatcher
    
    /* Pop arguments */
    add $16, %esp
    
    /* Restore callee-saved registers */
    pop %esi
    pop %edi
    pop %ebx
    pop %ebp
    
    /* Return to Ring 3 - IRET restores CS:EIP and EFLAGS */
    iret

/* ============================================================ */
/* IRQ 0 HANDLER - PIT Timer (with preemptive multitasking) */
/* ============================================================ */
.align 4
irq0_stub:
    /* Save all general purpose registers (part of context) */
    pusha
    
    /* Call simple PIT handler (no context switching yet) */
    call pit_handler
    
    /* Send EOI to PIC (End of Interrupt) */
    movb $0x20, %al
    outb %al, $0x20
    
    /* Restore registers */
    popa
    
    /* Return from interrupt */
    iret
