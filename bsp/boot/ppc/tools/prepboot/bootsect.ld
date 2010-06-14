SECTIONS {
	. = 0;
	.text . : {
		*(.text)
		__etext = . ;
	}
}
