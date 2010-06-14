/*
 * Linker script for boot loader
 */
INCLUDE config.ld
OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
SECTIONS {
	. = CONFIG_LOADER_TEXT ;

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
	}
	. = ALIGN(4);
	.bss . : {
		*(.bss)
		*(COMMON)
	}
	. = CONFIG_LOADER_TEXT + 0x1fff;
	.tail : {
		*(.tail)
	}
}
