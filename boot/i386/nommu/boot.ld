/*
 * Linker script for boot loader
 */
SECTIONS {
	. = 0x4000;
	.text . : {
		*(.text)
	}
	.rodata . : {
		*(.rodata)
	}
	.data . : {
		*(.data)
	}
	.bss . : {
		*(.bss)
		*(COMMON)
	}
	. = 0x5fff;
	.tail : {
		*(.tail)
	}
}
