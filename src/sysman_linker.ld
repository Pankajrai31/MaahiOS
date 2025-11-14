ENTRY(sysman_main)

SECTIONS
{
    /* User mode code - GRUB loads modules at 0x00115000 (observed from boot) */
    . = 0x00115000;

    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.text*)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data*)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss*)
    }
}
