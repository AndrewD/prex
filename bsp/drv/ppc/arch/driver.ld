/*
 * Linker script for driver
 */
INCLUDE config.ld
OUTPUT_FORMAT("elf32-powerpc")
OUTPUT_ARCH(powerpc)
ENTRY(driver_main)
SECTIONS
{
	. = SIZEOF_HEADERS ;

	.text . : {
		*(.text)
	}
	. = ALIGN(4);
	.rodata . : {
		*(.rodata)
		*(.rodata.*)
	}
	. = ALIGN(4);
	.data . : {
		*(.data)
		*(.sdata)
		*(.sdata2)
	}
	. = ALIGN(4);
	.bss . : {
		*(.sbss)
		*(.bss)
		*(COMMON)
	}

	/DISCARD/ : { *(.comment .note .debug_*) }
}
