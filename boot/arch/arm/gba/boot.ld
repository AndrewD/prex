SECTIONS {
	. = 0x3000000;
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
	. = 0x3001fff;
	.tail : {
		*(.tail)
	}
	__end = . ;
}
