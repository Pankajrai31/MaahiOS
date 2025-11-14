.section .text
.global ring3_switch_asm
.type ring3_switch_asm, @function

/* ring3_switch_asm(entry_point, stack_pointer)
 * Switch to Ring 3 using IRET
 * Parameters (cdecl calling convention):
 *   4(%esp) = entry_point (EIP for Ring 3)  - after return address
 *   8(%esp) = stack_pointer (ESP for Ring 3)
 */
ring3_switch_asm:
    /* Get parameters from stack (after return address) */
    mov 4(%esp), %eax          /* entry_point */
    mov 8(%esp), %ebx          /* stack_pointer */
    
    /* Build IRET frame on current stack */
    push $0x23                  /* SS (Ring 3 data segment) */
    push %ebx                   /* ESP (Ring 3 stack) */
    
    /* Push EFLAGS with IF set */
    pushf
    pop %ecx
    or $0x200, %ecx            /* Set IF bit */
    push %ecx
    
    push $0x1B                  /* CS (Ring 3 code segment) */
    push %eax                   /* EIP (entry point) */
    
    /* Clear registers before entering Ring 3 */
    xor %eax, %eax
    xor %ebx, %ebx
    xor %ecx, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    xor %ebp, %ebp
    
    /* IRET to Ring 3 */
    iretl
