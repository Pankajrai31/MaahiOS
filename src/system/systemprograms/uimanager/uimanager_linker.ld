/* Linker script for UIManager (Ring 3 process) */

ENTRY(_start)

SECTIONS
{
    /* Start at 2.5MB mark (between orbit at 3MB and sysman at 2MB) */
    . = 0x00280000;
    
    .text : {
        *(.text*)
    }
    
    .data : {
        *(.data*)
        *(.rodata*)
    }
    
    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }
}
