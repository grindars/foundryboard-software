ENTRY(Reset_Handler)

MEMORY {
    sram  : ORIGIN = 0x20000000, LENGTH = 20K
    flash : ORIGIN = 0x08000000, LENGTH = 128K
}

SECTIONS {
    .vectors : {
        *(.isr_vector)
    } > sram

    .text : {
        *(.text*)
        *(.rodata*)

        . = ALIGN(4);
    } > sram

    .data : {
        _sdata = ABSOLUTE(.);
        _sidata = ABSOLUTE(.);

        *(.data)

        . = ALIGN(4);

        _edata = ABSOLUTE(.);
    } > sram

    .bss : {
        _sbss = ABSOLUTE(.);
        *(.bss)

        . = ALIGN(4);

        _ebss = ABSOLUTE(.);
    } > sram

    .stack : {
        _sstack = ABSOLUTE(.);

        . = . + 2K;

        _estack = ABSOLUTE(.);
    } > sram

    . = ALIGN(8);

    _sheap = ABSOLUTE(.);
    _eheap = ABSOLUTE(0x20000000 + 20K);
    
    /DISCARD/ : {
        *(.comment)
    }
}