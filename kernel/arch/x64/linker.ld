ENTRY(_start)

SECTIONS
{
    /* Link everything at 1MB Physical (Standard for Multiboot) */
    . = 0x00100000;

    /* Multiboot Header */
    .multiboot ALIGN(4) :
    {
        KEEP(*(.multiboot))
    }

    .text ALIGN(4K) :
    {
        *(.text .stub .text.*)
    }

    .rodata ALIGN(4K) :
    {
        *(.rodata .rodata.*)
    }

    .data ALIGN(4K) :
    {
        *(.data .data.*)
    }

    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss .bss.*)
    }

    _kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}