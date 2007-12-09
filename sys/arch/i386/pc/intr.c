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
 * intr.c - interrupt handling routines for intel 8259 chip
 */

#include <kernel.h>
#include <irq.h>
#include "../i386/cpu.h"

/*
 * Interrupt priority level
 */
#define NR_IPLS		8	/* Number of interrupt priority levels */
#define IPL_NORMAL	7	/* Default interrupt priority level */

/* I/O address for master/slave programmable interrupt controller */
#define PIC_M           0x20
#define PIC_S           0xa0

/* Edge/level control register */
#define ELCR            0x4d0

/*
 * Interrupt nest counter.
 *
 * This counter is incremented in the entry of interrupt handler
 * to switch the interrupt stack. Since all interrupt handlers
 * share same one interrupt stack, each handler must pay attention
 * to the stack overflow.
 * This counter is also used by IRQ_ASSERT(). This macro detects the 
 * illegal function calls that are not allowed during interrupt level.
 */
volatile int irq_nesting = 0;

/*
 * Interrupt priority level (IPL)
 *
 * Each interrupt has its logical priority level, with 0 being the highest
 * priority. While some ISR is running, all lower priority interrupts
 * are masked off.
 */
static volatile int cur_ipl;

/*
 * Interrupt mapping table
 */
static int irq_level[NR_IRQS];	/* Vector -> level */
static u_int irq_mask[NR_IPLS];	/* Level -> mask */

/*
 * Set mask for current ipl
 */
static void update_mask(void)
{
	u_int mask = irq_mask[cur_ipl];

	outb(mask & 0xff, PIC_M + 1);
	outb(mask >> 8, PIC_S + 1);
}

/*
 * Unmask interrupt in PIC for specified irq.
 * The interrupt mask table is also updated.
 * Assumed CPU interrupt is disabled in caller.
 */
void interrupt_unmask(int vector, int level)
{
	int i;
	u_int unmask = (u_int)~(1 << vector);

	irq_level[vector] = level;
	for (i = level + 1; i <= IPL_NORMAL; i++)
		irq_mask[i] &= unmask;
	update_mask();
}

/*
 * Mask interrupt in PIC for specified irq.
 * Interrupt must be disabled when this routine is called.
 */
void interrupt_mask(int vector)
{
	int i;
	u_int mask = (u_int)(1 << vector);

	for (i = irq_level[vector] + 1; i <= IPL_NORMAL; i++)
		irq_mask[i] |= mask;
	irq_level[vector] = IPL_NORMAL;
	update_mask();
}

/*
 * Setup interrupt mode.
 * Select whether an interrupt trigger is edge or level.
 */
void interrupt_setup(int vector, int mode)
{
	int port;
	u_int bit;
	u_char val;

	port = vector < 8 ? ELCR : ELCR + 1;
	bit = (u_int)(1 << (vector & 7));

	val = inb(port);
	if (mode == IMODE_LEVEL)
		val |= bit;
	else
		val &= ~bit;
	outb(val, port);
}

/*
 * Common interrupt handler.
 * This routine is called from low level interrupt routine written
 * in assemble code. The interrupt flag is automatically disabled
 * by h/w in CPU when the interrupt is occurred.
 * The target interrupt will be masked in ICU while the irq handler
 * is called.
 */
void interrupt_handler(struct cpu_regs *regs)
{
	int vector = (int)regs->trap_no;
	int old_ipl, new_ipl;

	/* Adjust interrupt level */
	old_ipl = cur_ipl;
	new_ipl = irq_level[vector];
	if (new_ipl < old_ipl)		/* Ignore spurious interrupt */
		cur_ipl = new_ipl;
	update_mask();

	/* Send acknowledge to PIC for specified irq */
	if (vector & 8)			/* Slave ? */
		outb(0x20, PIC_S);	/* Non specific EOI to slave */
	outb(0x20, PIC_M);		/* Non specific EOI to master */

	/* Dispatch interrupt */
	sti();
	irq_handler(vector);
	cli();

	/* Restore interrupt level */
	cur_ipl = old_ipl;
	update_mask();
}

/*
 * Initialize 8259 interrupt controllers.
 * All interrupts will be masked off in ICU.
 */
void interrupt_init(void)
{
	int i;

	cur_ipl = IPL_NORMAL;

	for (i = 0; i < NR_IRQS; i++)
		irq_level[i] = IPL_NORMAL;

	for (i = 0; i < NR_IPLS; i++)
		irq_mask[i] = 0xfffb;

	outb_p(0x11, PIC_M);		/* Start initialization edge, master */
	outb_p(0x20, PIC_M + 1);	/* Set h/w vector = 0x20 */
	outb_p(0x04, PIC_M + 1);	/* Chain to slave (IRQ2) */
	outb_p(0x01, PIC_M + 1);	/* 8086 mode */

	outb_p(0x11, PIC_S);		/* Start initialization edge, master */
	outb_p(0x28, PIC_S + 1);	/* Set h/w vector = 0x28 */
	outb_p(0x02, PIC_S + 1);	/* Slave (cascade) */
	outb_p(0x01, PIC_S + 1);	/* 8086 mode */

	outb(0xff, PIC_S + 1);		/* Mask all */
	outb(0xfb, PIC_M + 1);		/* Mask all except IRQ2 (cascade) */
}
