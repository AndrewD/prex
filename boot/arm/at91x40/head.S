/*-
 * Copyright (c) 2008, Lazarenko Andrew
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
 * head.S - low level startup code for AT91x40
 */

/*
 * Memory map after remap:
 *
 * 0x00000000 - 0x0003ffff : On-chip SRAM (256K)
 * 0x80000000 - 0x801fffff : External FLASH (2M)
 * 0xffc00000 - 0xffffffff : I/O Register (4M)
 */

#include <conf/config.h>
#include <syspage.h>
#include "ebi.h"

#define EBI_REGION(base, cs, bat, tdf, pages, wse, nws, dbw) \
	((base) & EBI_BA_MASK) | (cs) | (bat) | (tdf) | \
	(pages) | (wse) | (nws) | (dbw)


#define ENTRY(x) .global x; .align; x##:

/*
 * Bootloader start point at 0x00000000
 *
 */
	.section ".text","ax"
	.code 32
	.org 0x00
ENTRY(cpu_reset)
	b boot_entry			/* Reset */
	ldr pc, machine_panic		/* Undefined instruction */
	ldr pc, machine_panic		/* Software interrupt */
	ldr pc, machine_panic		/* Prefetch abort */
	ldr pc, machine_panic		/* Data abort */
	nop				/* Reserved */
	ldr pc, machine_panic		/* IRQ */
	ldr pc, machine_panic		/* FIQ */

stack_end:     .word   (BOOT_STACKTOP - PAGE_OFFSET)

boot_entry:
	/*
	 * Relocate the loader code
	 */
	mov	r0, #0x0
	mov	r1, #0x00300000
	mov	r2, #256		/* 8192 (loader size) / (8 * 4) */
loop:
	ldmia	r0!, {r3-r10}
	stmia	r1!, {r3-r10}
	subs	r2, r2, #1
	bne	loop

	/*
	 * Perform remap command
	 */
	ldr	r10, =ebi_init_table
	ldmia	r10!, {r0-r9,r11}
	stmia	r11!, {r0-r9}		/* Write EBI regs with remap command */

	/* Config stack for svc mode */
	ldr	sp, stack_end

#if defined(DEBUG) && defined(CONFIG_DIAG_SERIAL)
	bl	serial_setup
#endif

	b	loader_main

/*
 * Start kernel
 */
ENTRY(start_kernel)
	bx	r0

/*
 * Panic handler
 */
ENTRY(machine_panic)
stop:
	b	stop

/*
 * External Bus Interface init table
 */
	.align
ebi_init_table:
/*
 * CS0 FLASH
 */
	.word	EBI_REGION(	0x80000000,
				EBI_CSEN,
				EBI_BAT_BYTE_SELECT,
				EBI_TDF_3,
				EBI_PAGES_4M,
				EBI_WSE,
				EBI_NWS_7,
				EBI_DBW_16)
/*
 * CS1 Ext SRAM
 */
	.word	EBI_REGION(	0x10000000,
				EBI_CSEN,
				EBI_BAT_BYTE_SELECT,
				EBI_TDF_1,
				EBI_PAGES_4M,
				EBI_WSE,
				EBI_NWS_2,
				EBI_DBW_16)

	.word	0x20000000		/* CS2 Off */
	.word	0x30000000		/* CS3 Off */
	.word	0x40000000		/* CS4 Off */
	.word	0x50000000		/* CS5 Off */
	.word	0x60000000		/* CS6 Off */
	.word	0x70000000		/* CS7 Off */

	.word	EBI_RCB			/* Remap command */

	/* Standard Read and 2M address line */
	.word	EBI_ALE_2M | EBI_DRP_STANDARD

	/* EBI Base Address */
	.word	0xffe00000


	.section .tail,"ax"
dummy:
	.byte	0xff

.end
