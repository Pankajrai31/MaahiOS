/* Linker script for Orbit (Ring 3 process) */

ENTRY(_start)

SECTIONS
{
    /* Start at 3MB mark (after sysman) */
    . = 0x00300000;
    
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
