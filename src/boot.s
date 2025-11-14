.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .bss
.align 16
stack_bottom:
.space 65536
stack_top:

.section .text
.global _start
.type _start, @function

_start:
    mov $stack_top, %esp
    
    /* EBX = multiboot_info pointer (from GRUB)
     * EAX = magic number (should be 0x2BADB002)
     * 
     * SAVE THESE BEFORE CALLING load_sysman_module (it destroys them!)
     */
    push %eax    /* Save magic on stack */
    push %ebx    /* Save mbi on stack */
    
    /* Load sysman.bin from module into memory at 0x00110000 */
    call load_sysman_module
    
    /* Restore and push in correct order for kernel_main(magic, mbi) */
    pop %ebx     /* Restore mbi */
    pop %eax     /* Restore magic */
    push %ebx    /* Push mbi (2nd arg) */
    push %eax    /* Push magic (1st arg) */
    
    call kernel_main
    
    cli
    hlt
    jmp .

/* Load sysman.bin module into memory at 0x00110000
 * Expects: EBX = multiboot_info pointer
 * Uses: EAX, ECX, EDI, ESI, EDX
 */
load_sysman_module:
    push %ebp
    mov %esp, %ebp
    sub $8, %esp          /* Local space for source, size */
    
    /* Get multiboot_info pointer from EBX (it was passed by GRUB) */
    mov %ebx, %eax
    
    /* Check flags: bit 3 = modules_count valid? */
    mov 0(%eax), %ecx     /* mbi->flags */
    test $0x08, %ecx
    jz load_sysman_done   /* No modules - skip */
    
    /* Get modules_count */
    mov 20(%eax), %ecx    /* mbi->mods_count (offset 20, not 24) */
    test %ecx, %ecx
    jz load_sysman_done   /* No modules */
    
    /* Get mods_addr (pointer to module table) */
    mov 24(%eax), %eax    /* mbi->mods_addr -> EAX (offset 24, not 20) */
    
    /* First module entry:
     * offset +0: mod_start (physical address in memory)
     * offset +4: mod_end (physical address in memory)
     * offset +8: string pointer (module name/cmdline)
     */
    mov 0(%eax), %esi     /* mod_start -> ESI (source) */
    mov 4(%eax), %edx     /* mod_end -> EDX */
    
    /* Calculate size: mod_end - mod_start */
    mov %edx, %ecx
    sub %esi, %ecx        /* ECX = size */
    
    /* Destination: 0x00110000 */
    mov $0x00110000, %edi /* EDI = destination */
    
    /* Copy with rep movsb */
    cld                   /* Clear direction flag (forward copy) */
    rep movsb             /* Copy ECX bytes from ESI to EDI */
    
load_sysman_done:
    add $8, %esp
    pop %ebp
    ret

.size _start, . - _start
