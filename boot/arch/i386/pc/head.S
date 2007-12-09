/*-
 * Copyright (c) 2005-2007, Kohsuke Ohtani
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors 
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * head.S - low level platform support
 */

/*-
 * Memory usage:
 *
 *  > 1000  - 1FFF  ... boot information
 *  > 2000  - 2FFF  ... stack for loader
 */
#define __ASSEMBLY__

#include <config.h>
#include <platform.h>

/* #define SCREEN_80x25 1 */
#define SCREEN_80x50 1

#define SEL_CODE32	0x10
#define SEL_DATA32	0x18

#define ENTRY(x) .global x; .align; x##:

	.text
	.code16

/*
 * boot_entry - Entry point for prex boot loader
 */
ENTRY(boot_entry)
	ljmp	$(LOADER_START >> 4), $(reset_cs - boot_entry)
reset_cs:
	movw	%cs, %ax		/* Reset segment registers */
	movw	%ax, %ds
	movw	%ax, %es

	xorw	%ax, %ax		/* Reset stack */
	movw	%ax, %ss
	movw	$BOOTSTACK_END, %sp

	call	setup_screen
	call	get_memsize
	cli				/* Disable all interrupts */
	call	enable_a20		/* Enable A20 line */

	lgdt	gdt_desc - boot_entry	/* Load GDT */

	movl	%cr0, %eax		/* Switch to protected mode */
	orl	$0x1, %eax
	movl	%eax, %cr0

	.byte	0x66			/* 32-bit long jump to reset CS */
	.byte	0xea
	.long	go_prot
	.word	SEL_CODE32
	.code32
go_prot:
	movw	$SEL_DATA32, %ax	/* Reset data segments */
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	movw	%ax, %fs
	movw	%ax, %gs

	jmp	loader_main		/* Jump to main routine in C */

/*
 * get_memsize - Get memory size
 */
	.code16
get_memsize:
	xorl	%eax, %eax
	int	$0x12			/* Get conventional memory size */
	movl	%eax, lo_mem - boot_entry	/* ax = K bytes */

	movl	$0x534d4150, %edx	/* 'SMAP' */
	xorl	%ebx, %ebx		/* Continuation value */
	movw	$(e820_buf - boot_entry), %di	/* Buffer */
mem_e820:
	movl	0x20, %ecx		/* Size of buffer */
	movl	$0xe820, %eax		/* Get extended memory */
	int	$0x15
	jc	mem_88
	cmpl	$0x534d4150, %eax	/* 'SMAP'*/
	jne	mem_88
	cmpl	$1, 16(%di)		/* Usable memory? */
	jne	try_next
	cmpl	$0x100000, (%di)	/* Extended? */
	jb	try_next
	movl	8(%di), %eax
	shrl	$10, %eax		/* Convert to K byte unit */
	jmp	mem_ok
try_next:
	cmpl	$0, %ebx		/* All done? */
	jne	mem_e820
	
mem_88:
	mov	$0x88, %ah
	int	$0x15
mem_ok:
	andb	$0xfc, %al		/* Adjust to page boundary */
	movl	%eax, hi_mem - boot_entry	/* ax = K bytes at 100000h */
	ret

/*
 * enable_a20 - Enable A20
 */
	.code16
enable_a20:
	call	empty_8042
	movb	$0xd1, %al
	outb	%al, $0x64
	call	empty_8042
	movb	$0xdf, %al
	outb	%al, $0x60
	call	empty_8042
wait_enable:
	ret
	
/*
 * empty_8042 - Empty 8042
 */
empty_8042:
	pushw	%ax
retry:
	call	io_delay
	inb	$0x64, %al
	testb	$1, %al
	jz	no_output

	call	io_delay
	inb	$0x60, %al
	jmp	retry

no_output:
	testb	$2, %al
	jnz	retry
	popw	%ax
	ret

/*
 * io_delay - I/O delay
 */
io_delay:
	pushw	%ax
	inb	$0x80, %al
	inb	$0x80, %al
	popw	%ax
	ret

/*
 * Setup screen
 */
setup_screen:
	pushaw
	pushw	%es
	pushw	%ds
	pushw	%bp
	movb	$0x2e, %al		/* print '.' for verify */
	movb	$0x0e, %ah
	movw	$0x07, %bx
	int	$0x10

	movw	$0x3, %ax		/* Use mode-3 */
	int	$0x10
	movw	$0x1202, %ax		/* 400 scan lines */
	movb	$0x30, %bl
	int	$0x10
#if SCREEN_80x50
	movw	$0x1112, %ax		/* Load 8x8 character set */
	movb	$0x0, %bl
	int	$0x10
	movw	$0x1201, %ax		/* Turn off cursor emulation */
	movb	$0x34, %bl
	int	$0x10
	movb	$0x01, %ah		/* Set cursor type */
	movw	$0x0607, %cx
	int	$0x10
#endif
	popw	%bp
	popw	%ds
	popw	%es
	popaw
	ret

	.code32
/*
 * Start kernel
 */
ENTRY(start_kernel)
	movl	4(%esp), %eax
	movl	8(%esp), %ebx		/* Store multiboot information in EBX */
	movl	%eax, kern_start
	jmp	code_flush
code_flush:

					/* Prepare registers for kernel */
	movw	$SEL_DATA32, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs

	xorl	%eax, %eax
	movl	%eax, %ecx
	movl	%eax, %edx
	movl	%eax, %esi
	movl	%eax, %edi

	movl	$0x2BADB002, %eax	/* Store multiboot magic in EAX */
	cli

	.byte	0xea
kern_start:
	.long	0
	.word	SEL_CODE32
	
/*
 * Data
 */
.align 16
gdt:
	.word 0x0,0x0,0x0,0x0		/* 0x00 - Null descritor */
	.word 0x0,0x0,0x0,0x0		/* 0x08 - Null descritor */
	.word 0xffff,0x0,0x9a00,0xcf	/* 0x10 - 32 bit code segment */
	.word 0xffff,0x0,0x9200,0xcf	/* 0x18 - 32 bit data segment */
	.word 0xffff,0x0,0x9a00,0x0	/* 0x20 - 16 bit code segment */
	.word 0xffff,0x0,0x9200,0x0	/* 0x28 - 16 bit data segment */

gdt_desc:
	.word	0x2F			/* limit */
	.long	gdt			/* address */

	.word	0x0			/* alignment */
idt_desc:
	.word	0x0
	.long	0x0

e820_buf:
	.space	20

	.align 16
	.global lo_mem, hi_mem
lo_mem:
	.long	0x0
hi_mem:
	.long	0x0

/*
 * Pad data
 */
	.section .tail,"ax"
dummy:
	.byte	0xff
