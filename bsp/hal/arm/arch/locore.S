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

#include <conf/config.h>
#include <machine/asm.h>
#include <machine/syspage.h>
#include <machine/memory.h>
#include <sys/errno.h>
#include <context.h>
#include <trap.h>
#include <cpu.h>

	.section ".text","ax"
	.code 32

/*
 * Kernel start point
 */
ENTRY(kernel_start)
#ifdef CONFIG_MMU
	b	reset_entry		/* Relative jump */
#endif
vector_start:
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

vector_end:

	.global bootinfo
bootinfo:		.word	BOOTINFO
boot_stack:		.word	BOOTSTKTOP
int_stack:		.word	INTSTKTOP - 0x100
irq_mode_stack:		.word	INTSTKTOP
sys_mode_stack:		.word	SYSSTKTOP
abort_mode_stack:	.word	ABTSTKTOP
irq_nesting:	 	.word	irq_nesting_value
curspl:	 		.word	curspl_value
init_done:	 	.word	init_done_value
#ifdef CONFIG_MMU
reload_pc_target:	.word	reload_pc
#endif

ENTRY(reset_entry)
	/*
	 * Setup stack pointer for each processor mode
	 */
	mov	r0, #(PSR_IRQ_MODE|PSR_FIQ_DIS|PSR_IRQ_DIS)
	msr	cpsr, r0
	ldr	sp, irq_mode_stack	/* Set IRQ mode stack */

	mov	r0, #(PSR_UND_MODE|PSR_FIQ_DIS|PSR_IRQ_DIS)
	msr	cpsr, r0
	ldr	sp, abort_mode_stack	/* Set Undefined mode stack */

	mov	r0, #(PSR_ABT_MODE|PSR_FIQ_DIS|PSR_IRQ_DIS)
	msr	cpsr, r0
	ldr	sp, abort_mode_stack	/* Set Abort mode stack */

	mov	r0, #(PSR_SYS_MODE|PSR_FIQ_DIS|PSR_IRQ_DIS)
	msr	cpsr, r0
	ldr	sp, sys_mode_stack	/* Set SYS mode stack */

	mov	r0, #(PSR_SVC_MODE|PSR_FIQ_DIS|PSR_IRQ_DIS)
	msr	cpsr, r0
	ldr	sp, boot_stack		/* Set SVC mode stack */

	/* It's svc mode now. */

#ifdef CONFIG_MMU
	/*
	 * Setup control register
	 */
	mov	r0, #CTL_DEFAULT
	mcr	p15, 0, r0, c1, c0, 0

	/*
	 * Initialize page table
	 * The physical address 0-4M is mapped on virtual address 2G.
	 */
	mov	r1, #BOOT_PGD_PHYS		/* Clear page directory */
	mov	r2, #(BOOT_PGD_PHYS + 0x4000)	/* +16k */
	mov	r0, #0
1:
	str	r0, [r1], #4
	teq	r1, r2
	bne	1b

	mov	r1, #(BOOT_PGD_PHYS + 0x2000) /* Set PTE0 address in pgd */
	mov	r0, #BOOT_PTE0_PHYS	/* WBUF/CACHE/SYSTEM attribute */
	orr	r0, r0, #0x03
	str	r0, [r1]

	mov	r1, #BOOT_PTE0_PHYS	/* Fill boot page table entry */
	add	r2, r1, #0x1000
	mov	r0, #0x1e
1:
	str	r0, [r1], #4
	add	r0, r0, #0x1000
	teq	r1, r2
	bne	1b

	/*
	 * Enable paging
	 * The physical address 0-4M is temporarily mapped to virtial
	 * address 0-4M. This is needed to enable paging.
	 */
	mov	r1, #BOOT_PGD_PHYS	/* Set PTE0 address in pgd */
	mov	r0, #BOOT_PTE0_PHYS	/* WBUF/CACHE/SYSTEM attribute */
	orr	r0, r0, #0x03
	str	r0, [r1]

	mov	r0, #0
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mcr	p15, 0, r0, c8, c7, 0	/* flush I,D TLBs */
	mov	r1, #BOOT_PGD_PHYS
	mcr	p15, 0, r1, c2, c0, 0	/* load page table pointer */
	mov	r0, #-1
	mcr	p15, 0, r0, c3, c0	/* load domain access register */
	mrc	p15, 0, r0, c1, c0, 0
	orr	r0, r0, #0x1000		/* I-cache enable */
	orr	r0, r0, #0x003d		/* Write buffer, mmu */
	mcr	p15, 0, r0, c1, c0, 0

	/*
	 * Reload PC register for virutal address.
	 */
	ldr	pc, reload_pc_target	/* Reset pc here */
reload_pc:

	/*
	 * Unmap 0-4M.
	 * Since the first page must be accessible for exception
	 * vector, we have to map it later.
	 */
	mov	r1, #BOOT_PGD_PHYS	/* Set PTE0 address in pgd */
	add	r1, r1, #KERNBASE
	mov	r0, #0
	str	r0, [r1]
	mcr	p15, 0, r0, c8, c7, 0	/* flush I,D TLBs */

#endif /* !CONFIG_MMU */

	/*
	 * Clear kernel BSS
	 */
	ldr	r1, =__bss
	ldr	r2, =__end
	mov	r0, #0
	cmp	r1, r2
	beq	2f
1:
	str	r0, [r1], #4
	cmp	r1, r2
	bls	1b
2:

	/*
	 * Initilize spl.
	 */
	ldr	r1, curspl		/* curspl = 15 */
	mov	r2, #15
	str	r2, [r1]

	/*
	 * Jump to kernel main routine
	 */
	b	main

/*
 * Relocate exception vector
 *
 * void vector_copy(vaddr_t dest);
 */
ENTRY(vector_copy)
	ldr	r1, =vector_start
	ldr	r2, =vector_end
1:
	ldmia	r1!, {r3}
	stmia	r0!, {r3}
	teq	r1, r2
	bne	1b
	mov	pc, lr

#ifdef CONFIG_CACHE
/*
 * Enable cache
 */
ENTRY(cache_init)
	mov	pc, lr
#endif

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
	sub	r4, sp, #(4*5)		/* r4: Pointer to saved registers */
	mrs	r0, spsr		/* r0: cpsr */
	sub	r3, lr, #4		/* r3: original pc */

	mrs	r1, cpsr		/* Set processor to SVC mode */
	bic	r1, r1, #PSR_MODE
	orr	r1, r1, #PSR_SVC_MODE
	msr	cpsr_c, r1

	mov	r1, sp			/* r1: svc_sp */
	mov	r2, lr			/* r2: svc_lr */
	stmfd	sp!, {r0-r3}		/* Push cpsr, svc_sp, svc_lr, pc */
	ldmfd	r4, {r0-r4}		/* Restore work registers */
	sub	sp, sp, #(4*15)
	stmia	sp, {r0-r14}^		/* Push r0-r14 */
	nop				/* Instruction gap for stm^ */

	ldr	r4, irq_nesting   	/* Increment IRQ nesting level */
	ldr	r5, [r4]		/* r5: Previous nesting level */
	add	r0, r5, #1
 	str	r0, [r4]

	mov	r7, sp			/* Save stack */
	ldr	r3, int_stack		/* Adjust stack for IRQ */
	cmp	r5, #0			/* Outermost interrupt? */
	moveq	sp, r3			/* If outermost, switch stack */
	bleq	sched_lock		/* If outermost, lock scheduler */
	bl	interrupt_handler	/* Call main interrupt handler */

	mov	sp, r7			/* Restore stack */
	str	r5, [r4]		/* Restore IRQ nesting level */
	cmp	r5, #0			/* Outermost interrupt? */
	bne	interrupt_ret
	bl	sched_unlock		/* Try to preempt */

	ldr	r0, [sp, #REG_CPSR]	/* Get previous mode */
	and	r0, r0, #PSR_MODE
	cmp	r0, #PSR_APP_MODE	/* Return to application mode? */
	bne	interrupt_ret

	mrs	r5, cpsr		/* Enable IRQ */
	bic	r4, r5, #0xc0
	msr	cpsr_c, r4
	bl	exception_deliver	/* Check exception */
	msr	cpsr_c, r5		/* Restore IRQ */
interrupt_ret:
	mov	r0, sp
 	ldr	r1, [r0, #REG_CPSR]	/* Restore spsr */
 	msr	spsr_all, r1
 	ldmfd	r0, {r0-r14}^		/* Restore user mode registers */
	nop				/* Instruction gap for ldm^ */
	add	sp, sp, #(4*16)
	ldmia	sp, {sp, lr, pc}^	/* Restore lr and exit */

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
	sub	sp, sp, #CTXREGS	/* Adjust stack */
	stmia	sp, {r0-r14}^		/* Push r0-r14 */
	nop				/* Instruction gap for stm^ */
	mrs	r5, spsr		/* Push cpsr */
	str	r5, [sp, #REG_CPSR]
	add	r5, sp, #CTXREGS
	str	r5, [sp, #REG_SVCSP]	/* Push svc_sp */
	str	lr, [sp, #REG_SVCLR]	/* Push svc_lr */
	str	lr, [sp, #REG_PC]	/* Push pc */
#ifndef __gba__
	ldr	r4, [lr, #-4]		/* Get SWI number */
	bic	r4, r4, #0xff000000

	mrs	r5, cpsr		/* Enable IRQ */
	bic	r5, r5, #0xc0
	msr	cpsr_c, r5
#endif

	stmfd	sp!, {r4}
	bl	syscall_handler		/* System call dispatcher */
	ldmfd	sp!, {r4}

	cmp	r4, #0			/* Skip storing error if exception_return */
	strne	r0, [sp]		/* Set return value to r0 */
	bl	exception_deliver	/* Check exception */
syscall_ret:
	mrs	r5, cpsr		/* Disable IRQ */
	orr	r5, r5, #0xc0
	msr	cpsr_c, r5
	mov	r5, sp
	ldr	r1, [r5, #REG_CPSR]	/* Restore spsr */
	msr	spsr_all, r1
 	ldmfd	r5, {r0-r14}^		/* Restore user mode registers */
	nop				/* Instruction gap for ldm^ */
	add	sp, sp, #REG_SVCSP
	ldmia	sp, {sp, lr, pc}^	/* Restore lr and exit */

/*
 * Undefined instruction
 */
ENTRY(undefined_entry)
	sub	sp, sp, #CTXREGS	/* Adjust stack */
	stmia	sp, {r0-r14}^		/* Push r0-r14 */
	nop				/* Instruction gap for stm^ */
	mov	r0, #TRAP_UNDEFINED
	b	trap_common

/*
 * Prefetch abort
 */
ENTRY(prefetch_entry)
	sub	lr, lr, #8		/* Adjust the lr */
	sub	sp, sp, #CTXREGS	/* Adjust stack */
	stmia	sp, {r0-r14}^		/* Push r0-r14 */
	nop				/* Instruction gap for stm^ */
	mov	r0, #TRAP_PREFETCH_ABORT
	b	trap_common

/*
 * Data abort
 */
ENTRY(abort_entry)
	sub	lr, lr, #4		/* Adjust the lr */
	sub	sp, sp, #CTXREGS	/* Adjust stack */
	stmia	sp, {r0-r14}^		/* Push r0-r14 */
	nop				/* Instruction gap for stm^ */
	mov	r0, #TRAP_DATA_ABORT
	b	trap_common

/*
 * Common entry for all traps
 * r0 - trap type
 */
ENTRY(trap_common)
	add	r5, sp, #CTXREGS
	str	r5, [sp, #REG_SVCSP]	/* Push svc_sp */
	str	lr, [sp, #REG_PC]	/* Push pc */
	mrs	r5, spsr		/* Push cpsr */
	str	r5, [sp, #REG_CPSR]

	str	r0, [sp, #REG_R0]	/* Set trap type to R0 */
	mov	r0, sp
	bl	trap_handler

	mov	r5, sp
	ldr	r1, [r5, #REG_CPSR]	/* Restore cpsr */
	msr	spsr_all, r1
	ldr	lr, [r5, #REG_PC]	/* Restore pc (lr) */
	ldr	sp, [r5, #REG_SVCSP]	/* Restore svc_sp */
	ldmfd	r5, {r0-r14}^		/* Restore user mode registers */
	nop				/* Instruction gap for ldm^ */
	movs	pc, lr			/* Exit, with restoring cpsr */

ENTRY(fiq_entry)
	b	fiq_entry		/* Not support... */

/*
 * Switch register context.
 * r0 = previous kern_regs, r1 = next kern_regs
 * Interrupts must be disabled by caller.
 *
 * syntax - void cpu_switch(kern_regs *prev, kern_regs *next)
 *
 * Note: GCC uses r0-r3,r12 as scratch registers
 */
ENTRY(cpu_switch)
	stmia	r0, {r4-r11, sp, lr}	/* Save previous register context */
	ldmia	r1, {r4-r11, sp, pc}	/* Restore next register context */

/*
 * Entry point for kernel thread
 */
ENTRY(kernel_thread_entry)
	mov	r0, r5			/* Set argument */
	mov	pc, r4			/* Jump to kernel thread */
1:
	b	1b


/*
 * Copy data from user to kernel space.
 * Returns 0 on success, or EFAULT on page fault.
 *
 *  syntax - int copyin(const void *uaddr, void *kaddr, size_t len)
 */
	.global known_fault1
ENTRY(copyin)
 	mov	r12, sp
 	stmdb	sp!, {r4, r11, r12, lr, pc}
 	sub	r11, r12, #4
	cmp	r0, #(USERLIMIT)
	bhi	copy_fault
 	mov	r12, #0
 	b	2f
1:
	ldrb	r3, [r12, r0]
known_fault1:				/* May be fault here */
 	strb	r3, [r12, r1]
 	add	r12, r12, #1
2:
	subs	r2, r2, #1
 	bcs	1b
	mov	r0, #0			/* Set no error */
 	ldmia	sp, {r4, r11, sp, pc}

/*
 * Copy data to user from kernel space.
 * Returns 0 on success, or EFAULT on page fault.
 *
 *  syntax - int copyout(const void *kaddr, void *uaddr, size_t len)
 */
	.global known_fault2
ENTRY(copyout)
 	mov	r12, sp
 	stmdb	sp!, {r4, r11, r12, lr, pc}
 	sub	r11, r12, #4
	cmp	r1, #(USERLIMIT)
	bhi	copy_fault
 	mov	r12, #0
 	b	2f
1:
	ldrb	r3, [r12, r0]
known_fault2:				/* May be fault here */
 	strb	r3, [r12, r1]
 	add	r12, r12, #1
2:
	subs	r2, r2, #1
 	bcs	1b
	mov	r0, #0			/* Set no error */
 	ldmia	sp, {r4, r11, sp, pc}

/*
 * copyinstr - Copy string from user space.
 * Returns 0 on success, or EFAULT on page fault, or ENAMETOOLONG.
 *
 *  syntax - int copyinstr(const char *uaddr, void *kaddr, size_t len);
 */
	.global known_fault3
ENTRY(copyinstr)
 	mov	r12, sp
 	stmdb	sp!, {r4, r11, r12, lr, pc}
 	sub	r11, r12, #4
	cmp	r0, #(USERLIMIT)
	bhi	copy_fault
 	mov	r12, #0
 	b	2f
1:
	ldrb	r3, [r12, r0]
known_fault3:				/* May be fault here */
 	strb	r3, [r12, r1]
	cmp	r3, #0
	beq	3f
 	add	r12, r12, #1
2:
	subs	r2, r2, #1
 	bcs	1b
	mov	r0, #(ENAMETOOLONG)
	b	4f
3:
	mov	r0, #0			/* Set no error */
4:
 	ldmia	sp, {r4, r11, sp, pc}

/*
 * Fault entry for user access
 */
ENTRY(copy_fault)
	mov	r0, #(EFAULT)
 	ldmia	sp, {r4, r11, sp, pc}


/*
 * int spl0(void);
 */
ENTRY(spl0)
	ldr	r1, curspl		/* curspl = 0 */
	ldr	r0, [r1]
	mov	r2, #0
	str	r2, [r1]

	mrs	r1, cpsr		/* Enable interrupt */
	bic	r1, r1, #0xc0
	msr	cpsr_c, r1
	mov	pc, lr

/*
 * int splhigh(void);
 */
ENTRY(splhigh)
	mrs	r1, cpsr		/* Disable interrupt */
	orr	r1, r1, #0xc0
	msr	cpsr_c, r1

	ldr	r1, curspl		/* curspl = 15 */
	ldr	r0, [r1]
	mov	r2, #15
	str	r2, [r1]
	mov	pc, lr

/*
 * void splx(int s);
 */
ENTRY(splx)
	mov	r3, r0			/* r3: new spl */
	ldr	r1, curspl
	ldr	r0, [r1]
	cmp	r3, r0
	moveq	pc, lr			/* Return if equal */
	str	r3, [r1]

	cmp	r3, #0
	mrs	r1, cpsr
	bic	r1, r1, #0xc0
	orrne	r1, r1, #0xc0		/* Disable interrupt if curspl != 0 */
	msr	cpsr_c, r1
	mov	pc, lr


/*
 * void sploff(void);
 */
ENTRY(sploff)
	mrs	r0, cpsr
	orr	r0, r0, #0xc0
	msr	cpsr_c, r0
	mov	pc, lr

/*
 * void splon(void);
 */
ENTRY(splon)
	mrs	r0, cpsr
	bic	r0, r0, #0xc0
	msr	cpsr_c, r0
	mov	pc, lr

/*
 * Interrupt nest counter.
 *
 * This counter is incremented in the entry of interrupt handler
 * to switch the interrupt stack. Since all interrupt handlers
 * share same one interrupt stack, each handler must pay attention
 * to the stack overflow.
 */
	.section ".bss"
irq_nesting_value:
	.long	0

/*
 * Current spl
 */
curspl_value:
	.long	0

/*
 * Init flag for debug
 */
init_done_value:
	.long	0

	.end
