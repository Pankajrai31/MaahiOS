.set ALIGN,      1<<0
.set MEMINFO,    1<<1
.set VIDEO_MODE, 1<<2       /* Request video mode */
.set FLAGS,      ALIGN | MEMINFO    /* DON'T request video mode - let GRUB use text mode */
.set MAGIC,      0x1BADB002
.set CHECKSUM,   -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM
.long 0  /* header_addr (not used with ELF) */
.long 0  /* load_addr */
.long 0  /* load_end_addr */
.long 0  /* bss_end_addr */
.long 0  /* entry_addr */
.long 0  /* mode_type: 0 = linear graphics mode */
.long 1024  /* width: 1024 pixels */
.long 768   /* height: 768 pixels */
.long 32    /* depth: 32 bits per pixel (RGBA) */

.section .bss
.align 16
stack_bottom:
.space 65536
stack_top:

/* VBE information storage (will be passed to kernel) */
.section .data
.global vbe_mode_info
.align 4
vbe_mode_info:
    .long 0     /* framebuffer physical address */
    .long 0     /* width */
    .long 0     /* height */
    .long 0     /* pitch (bytes per line) */
    .long 0     /* bpp (bits per pixel) */

.section .text
.global _start
.type _start, @function

_start:
    /* We're in 32-bit protected mode already (GRUB loaded us)
     * GRUB has already set up the video mode we requested
     * Extract framebuffer info from multiboot structure
     */
    
    mov $stack_top, %esp
    
    /* EBX = multiboot_info pointer (from GRUB)
     * EAX = magic number (should be 0x2BADB002)
     */
    
    /* Extract VBE info from multiboot structure before we lose EBX */
    mov %ebx, %esi              /* Save mbi pointer */
    
    /* Check if framebuffer info is available (bit 12 in flags) */
    mov 0(%esi), %eax           /* Load flags */
    test $0x1000, %eax          /* Test bit 12 */
    jz skip_vbe_setup
    
    /* Extract framebuffer info from multiboot (offset 88+) */
    mov 88(%esi), %eax          /* framebuffer_addr (lower 32 bits) */
    mov %eax, vbe_mode_info+0   /* Store framebuffer address */
    
    mov 100(%esi), %eax         /* framebuffer_width */
    mov %eax, vbe_mode_info+4   /* Store width */
    
    mov 104(%esi), %eax         /* framebuffer_height */
    mov %eax, vbe_mode_info+8   /* Store height */
    
    mov 108(%esi), %eax         /* framebuffer_pitch */
    mov %eax, vbe_mode_info+12  /* Store pitch */
    
    mov 112(%esi), %eax         /* framebuffer_bpp */
    mov %eax, vbe_mode_info+16  /* Store bpp */
    
skip_vbe_setup:
    mov %esi, %ebx              /* Restore mbi pointer to EBX */
    mov $0x2BADB002, %eax       /* Restore magic */
    
    /* SAVE THESE BEFORE CALLING load_sysman_module (it destroys them!) */
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
