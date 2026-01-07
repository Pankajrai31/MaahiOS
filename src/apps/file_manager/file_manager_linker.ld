/* Linker script for File Manager (Ring 3 process) */

ENTRY(_start)

SECTIONS
{
    /* Start at 4MB mark (after orbit) */
    . = 0x00400000;
    
    .text : {
        *(.text)
    }
    
    .data : {
        *(.data)
        *(.rodata)
    }
    
    .bss : {
        *(.bss)
    }
}
