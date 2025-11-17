ENTRY(sysman_main)

SECTIONS
{
    /* Position-independent - works at any address GRUB loads it */
    . = 0;

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
