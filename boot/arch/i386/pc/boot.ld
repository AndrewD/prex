/*
 * Linker script for boot loader
 */
SECTIONS {
	. = 0x40000;
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
	. = 0x41fff;
	.tail : {
		*(.tail)
	}
	__end = . ;
}
