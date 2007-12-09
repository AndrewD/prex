/*-
 * Copyright (c) 2005, Kohsuke Ohtani
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
 * trap.c - trap handling routine
 */

#include <kernel.h>
#include <except.h>
#include <thread.h>
#include <task.h>
#include "cpu.h"

/*
 * Known address which may cause page fault.
 */
extern void known_fault1(void);
extern void known_fault2(void);
extern void known_fault3(void);
extern void umem_fault(void);


#ifdef DEBUG
static void trap_dump(struct cpu_regs *);

static char *const trap_name[] = {
	"Divide error",		/*  0 */
	"Debug trap",		/*  1 */
	"NMI",			/*  2 */
	"Breakpoint",		/*  3 */
	"Overflow",		/*  4 */
	"Bounds check",		/*  5 */
	"Invalid opecode",	/*  6 */
	"Device not available",	/*  7 */
	"Double fault",		/*  8 */
	"Coprocessor overrun",	/*  9 */
	"Invalid TSS",		/* 10 */
	"Segment not present",	/* 11 */
	"Stack bounds",		/* 12 */
	"General Protection",	/* 13 */
	"Page fault",		/* 14 */
	"Reserved",		/* 15 */
	"Coprocessor error",	/* 16 */
	"Alignment check",	/* 17 */
	"Cache flush denied"	/* 18 */
};
#define MAX_TRAP (sizeof(trap_name) / sizeof(void *) - 1)
#endif	/* DEBUG */

/*
 * Trap/exception mapping table.
 * i386 trap code is translated to the architecture
 * independent exception code.
 */
static const int trap_map[] = {
	EXC_FPE,		/*  0: Divide error */
	EXC_TRAP,		/*  1: Debug trap */
	EXC_ILL,		/*  2: NMI */
	EXC_TRAP,		/*  3: Breakpoint */
	EXC_FPE,		/*  4: Overflow */
	EXC_ILL,		/*  5: Bounds check */
	EXC_ILL,		/*  6: Invalid opecode */
	EXC_FPE,		/*  7: Device not available */
	EXC_ILL,		/*  8: Double fault */
	EXC_FPE,		/*  9: Coprocessor overrun */
	EXC_SEGV,		/* 10: Invalid TSS */
	EXC_SEGV,		/* 11: Segment not present */
	EXC_SEGV,		/* 12: Stack bounds */
	EXC_ILL,		/* 13: General Protection fault */
	EXC_SEGV,		/* 14: Page fault */
	EXC_ILL,		/* 15: Reserved */
	EXC_FPE,		/* 16: Coprocessor error */
	EXC_ILL,		/* 17: Alignment check */
	EXC_ILL,		/* 18: Cache flush denied */
};

/*
 * Trap handler
 * Invoke the exception handler if it is needed.
 */
void trap_handler(struct cpu_regs *regs)
{
	u_long trap_no = regs->trap_no;

	if (trap_no > 18)
		panic("Unknown trap");
	else if (trap_no == 2)
		panic("NMI");

	/*
	 * Check whether this trap is kernel page fault caused
	 * by known access to user space.
	 */
	if (trap_no == 14 && regs->cs == KERNEL_CS &&
	    (regs->eip == (u_long)known_fault1 ||
	     regs->eip == (u_long)known_fault2 ||
	     regs->eip == (u_long)known_fault3)) {
		printk("*** Detect EFAULT ***\n");
		regs->eip = (u_long)umem_fault;
		return;
	}
#ifdef DEBUG
	printk("============================\n");
	printk("Trap %x: %s\n", trap_no, trap_name[trap_no]);
	if (trap_no == 14)
		printk(" Fault address=%x\n", get_cr2());
	printk("============================\n");
	trap_dump(regs);
	if (regs->cs == KERNEL_CS) {
		interrupt_mask(0);
		sti();
		while (1)
			cpu_idle();
	}
	for (;;);
#endif
	if (regs->cs == KERNEL_CS)
		panic("Kernel exception");

	exception_post(trap_map[trap_no]);
	exception_deliver();
}

#ifdef DEBUG
static void trap_dump(struct cpu_regs *r)
{
	u_long ss, esp, *fp;
	u_int i;

	if (r->cs & 3) {
		ss = r->ss;
		esp = r->esp;
	} else {
		ss = r->ds;
		esp = (u_long)r;
	}
	printk("Trap frame %x error %x\n", r, r->err_code);
	printk(" eax %08x ebx %08x ecx %08x edx %08x esi %08x edi %08x\n",
	       r->eax, r->ebx, r->ecx, r->edx, r->esi, r->edi);
	printk(" eip %08x esp %08x ebp %08x eflags %08x\n",
	       r->eip, esp, r->ebp, r->eflags);
	printk(" cs  %08x ss  %08x ds  %08x es  %08x esp0 %08x\n",
	       r->cs, ss, r->ds, r->es, tss_get());
	if (irq_nesting > 0)
		printk(" >> trap in isr (irq_nesting=%d)\n", irq_nesting);
	printk(" >> interrupt is %s\n",
	       (get_eflags() & EFL_IF) ? "enabled" : "disabled");
	printk(" >> task: id=%x \'%s\'\n", cur_task(), cur_task()->name);

	printk("Stack trace:\n");
	fp = (u_long *)r->ebp;
	for (i = 0; i < 16; i++) {
		fp = (u_long *)(*fp);	/* XXX: may cause fault */
		if (!(*(fp + 1) && *fp))
			break;
		printk(" %08x\n", *(fp + 1));
	}
}
#endif /* DEBUG */
