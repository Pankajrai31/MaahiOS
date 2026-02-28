/* Memory Executive Linker Script */
OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS
{
    /* All user processes load at this virtual address.
     * Per-process page directories ensure each process has its own mapping. */
    . = 0x10000000;
    
    .text : {
        *(.text*)
    }
    
    .rodata : {
        *(.rodata*)
    }
    
    .data : {
        *(.data*)
    }
    
    /* PIC-generated sections - must be placed BEFORE .bss
     * so that .bss zeroing doesn't destroy the GOT */
    .got : {
        *(.got)
    }
    
    .got.plt : {
        *(.got.plt)
    }
    
    .bss : {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
    }
}
