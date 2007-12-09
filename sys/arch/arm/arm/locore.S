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
 * locore.S - low level platform support
 */

#define __ASSEMBLY__

#include <config.h>
#include <platform.h>
#include "cpu.h"

#define ENTRY(x) .global x; .align; x##:

	.section ".text","ax"
	.code 32
/*
 * Kernel start point
 */
ENTRY(kernel_start)

	/*
	 * Exception vector
	 *
	 * This table will be copied to an appropriate location.
	 * (the location is platform specific.)
	 */
	ldr	pc, reset_target	/* 0x00 mode: svc */
	ldr	pc, undefined_target	/* 0x04 mode: ? */
	ldr	pc, swi_target		/* 0x08 mode: svc */
	ldr	pc, prefetch_target	/* 0x0c mode: abort */
	ldr	pc, abort_target	/* 0x10 mode: abort */
	nop				/* 0x14 reserved */
	ldr	pc, irq_target		/* 0x18 mode: irq */
	ldr	pc, fiq_target		/* 0x1c mode: fiq */
	
reset_target:		.word	reset_entry
undefined_target:	.word	undefined_entry
swi_target:		.word	syscall_entry
prefetch_target:	.word	prefetch_entry
abort_target:		.word	abort_entry
irq_target:		.word	interrupt_entry
fiq_target:		.word	fiq_entry

	.global boot_info
boot_info:		.word	BOOT_INFO
boot_stack_top:		.word	BOOT_STACK + 0x800
int_stack_top:		.word	INT_STACK + 0x500
irq_mode_stack:		.word	INT_STACK
sys_mode_stack:		.word	SYS_STACK
irq_nest_count:		.word	irq_nesting
		
reset_entry:
	/*
	 * Clear kernel BSS
	 */
	ldr	r1, =__bss
	ldr	r2, =__end
	mov	r0, #0
	cmp	r1, r2
	beq	2f
1:	str	r0, [r1], #4
	cmp	r1, r2
	bls	1b
2:
	/*
	 * Setup stack pointer for each processor mode
	 */
	mov	r0, #(PSR_IRQ_MODE|PSR_FIQ_DIS|PSR_IRQ_DIS)
	msr	cpsr_c, r0
	ldr	sp, irq_mode_stack	/* Set IRQ mode stack */

	mov	r0, #(PSR_SYS_MODE|PSR_FIQ_DIS|PSR_IRQ_DIS)
	msr	cpsr, r0
	ldr	sp, sys_mode_stack	/* Set SYS mode stack */
				
	mov	r0, #(PSR_SVC_MODE|PSR_FIQ_DIS|PSR_IRQ_DIS)
	msr	cpsr, r0
	ldr	sp, boot_stack_top	/* Set SVC mode stack */

	/*
	 * Initialize basic h/w
	 */
	bl	diag_init
	bl	interrupt_init

	/*
	 * Jump to kernel main routine
	 */	
	b	kernel_main

/*
 * Interrupt entry point
 */
/*	
 * Memo: GBA BIOS interrupt handler.
 *
 *	stmfd	sp!, {r0-r3,r12,lr}
 *	mov	r0, #0x4000000
 *	adr	lr, IntRet
 *	ldr	pc, [r0,#-4] @ pc = [0x3007ffc]
 *IntRet:
 *	ldmfd	sp!, {r0-r3,r12,lr}
 *	subs	pc, lr, #4
 */
ENTRY(interrupt_entry)
#ifdef __gba__
	ldmfd	sp!, {r0-r3,r12,lr}	/* Discard GBA BIOS's stack */
#endif
	stmfd	sp, {r0-r4}		/* Save work registers */
	sub	r2, lr, #4		/* r2: pc */
	mrs	r3, spsr		/* r3: cpsr */
	sub	r4, sp, #(4*5)

	mrs	r0, cpsr		/* Set processor to SVC mode */
	bic	r0, r0, #PSR_MODE
	orr	r0, r0, #PSR_SVC_MODE
	msr	cpsr_c, r0

	mov	r0, sp
	mov	r1, lr
	stmfd	sp!, {r0-r3}		/* Push svc_sp, svc_lr, pc, cpsr */
	ldmfd	r4, {r0-r4}		/* Restore work registers */
	sub	sp, sp, #(4*15)
	stmia	sp, {r0-r14}^		/* Push r0-r14 */
	nop				/* Instruction gap for stm^ */

	ldr	r4, irq_nest_count	/* Increment IRQ nesting level */
	ldr	r5, [r4]		/* r5: Previous nesting level */
	add	r0, r5, #1
	str	r0, [r4]

	mov	r7, sp			/* Save stack */
	ldr	r3, int_stack_top	/* Adjust stack for IRQ */
	cmp	r5, #0			/* Outermost interrupt? */
	moveq	sp, r3			/* If outermost, switch stack */
	bleq	sched_lock		/* If outermost, lock scheduler */
	bl	interrupt_handler	/* Call main interrupt handler */
	mov	sp, r7			/* Restore stack */

	str	r5, [r4]		/* Restore IRQ nesting level */
	cmp	r5, #0			/* Outermost interrupt? */
	bne	nested_irq

	bl	sched_unlock		/* Try to preempt */
	ldr	r0, [sp, #(4*18)]	/* Get previous mode */
	and	r0, r0, #PSR_MODE
	cmp	r0, #PSR_APP_MODE	/* Return to application mode? */
	bleq	exception_deliver	/* If so, check exception */
	
nested_irq:
	mov	r0, sp
 	ldr	sp, [r0, #(4*15)]	/* Restore svc_sp */
 	ldr	lr, [r0, #(4*16)]	/* Restore svc_lr */

	mrs	r1, cpsr		/* Set processor to IRQ mode */
	bic	r1, r1, #PSR_MODE
	orr	r1, r1, #PSR_IRQ_MODE
	msr	cpsr_c, r1
	
 	ldr	lr, [r0, #(4*17)]	/* Restore lr */
 	ldr	r1, [r0, #(4*18)]	/* Restore spsr */
 	msr	spsr_all, r1
 	ldmfd	r0, {r0-r14}^		/* Restore user mode registers */
	nop				/* Instruction gap for ldm^ */
	movs	pc, lr			/* Exit, with restoring cpsr */
	
/*
 * System call entry
 */
	.global syscall_ret
ENTRY(syscall_entry)
#ifdef __gba__
	mov	r5, lr			/* Syscall stub already saved r5 */
	mrs	r12, cpsr		/* Set processor to SVC mode */
	bic	r12, r12, #PSR_MODE
	orr	r12, r12, #PSR_SVC_MODE
	msr	cpsr_c, r12
	mov	lr, r5
#endif
	sub	sp, sp, #(4*19)		/* Adjust stack */
	stmia	sp, {r0-r14}^		/* Push r0-r14 */
	nop				/* Instruction gap for stm^ */
	add	r5, sp, #(4*19)
	str	r5, [sp, #(4*15)]	/* Push svc_sp */
	str	lr, [sp, #(4*17)]	/* Push pc */
	mrs	r5, spsr		/* Push cpsr */
	str	r5, [sp, #(4*18)]
#ifndef __gba__
	ldr	r4, [lr, #4]		/* Get SWI number */
	bic	r4, r4, #0xff000000
#endif
	ldr	r5, =nr_syscalls	/* Check SWI number */
	ldr	r5, [r5]
	cmp	r4, r5
	bge	bad_syscall

	ldr	r5,=syscall_table
	ldr	r4, [r5, r4, lsl #2]
	mov	lr, pc
	mov	pc, r4			/* Dispatch functions */
	str	r0, [sp]		/* Set return value to r0 */
	bl	exception_deliver	/* Check exception */
syscall_ret:
	mov	r5, sp
	ldr	r1, [r5, #(4*18)]	/* Restore cpsr */
	msr	spsr_all, r1
	ldr	lr, [r5, #(4*17)]	/* Restore pc (lr) */
	ldr	sp, [r5, #(4*15)]	/* Restore svc_sp */
	ldmfd	r5, {r0-r14}^		/* Restore user mode registers */
	nop				/* Instruction gap for ldm^ */
	movs	pc, lr			/* Exit, with restoring cpsr */
bad_syscall:
	mov	r0, #22			/* Set EINVAL to r0 */
	str	r0, [sp, #(4*0)]
	b	syscall_ret
		
		
/*
 * TODO: Need to handle following exceptions.
 */
ENTRY(undefined_entry)
ENTRY(prefetch_entry)
ENTRY(abort_entry)
ENTRY(fiq_entry)
	b	fiq_entry		/* hang... */
			
/*
 * Switch register context.
 * r0 = previous kern_regs, r1 = next kern_regs
 * Interrupts must be disabled by caller.
 *
 * syntax - void __context_switch(kern_regs *prev, kern_regs *next)
 *
 * Note: GCC uses r0-r3,r12 as scratch registers
 */
ENTRY(__context_switch)
	stmia	r0, {r4-r11, sp, lr}	/* Save previous register context */
	ldmia	r1, {r4-r11, sp, pc}	/* Restore next register context */
	
/*
 * Entry point for kernel thread
 */
ENTRY(kernel_thread_entry)
	mov	r0, r5			/* Set argument */
	mov	pc, r4			/* Jump to kernel thread */

	.end
