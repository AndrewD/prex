/*
 * Linker script for kernel
 */

INCLUDE config.ld

/*
 * We use ELF as an output format. So that we can
 * debug the code in some form.
 */
OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)

ENTRY(kernel_start)

PHDRS
{
	text PT_LOAD FILEHDR PHDRS;
	data PT_LOAD;
}

SECTIONS
{
	. = KERNEL_BASE + SIZEOF_HEADERS;

	.text : {
		*(.text)
		*(.glue_7)
		*(.glue_7t)
	} : text = 0xff

	. = ALIGN(4);
	.rodata : {
		*(.rodata)
		*(.rodata.*)
	} : text

	.kstrtab : {
		*(.kstrtab)
	} : text

	.ksymtab ALIGN(4) : {
		*(.ksymtab)
	} : text

	.driver_table ALIGN(4) : {
		__driver_table = . ;
		*(.driver_table)
		__driver_table_end = . ;
	} : text

	.data ALIGN(4) : {
		*(.data)
	} : data = 0xff

	. = ALIGN(4);
	__bss = . ;
	.bss .    : {
		*(.bss)
		*(COMMON)
	}
	. = ALIGN(4);
	__end = . ;

	/DISCARD/ : { *(.comment .note) }
}
