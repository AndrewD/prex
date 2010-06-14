/*-
 * Copyright (c) 2009, Kohsuke Ohtani
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
 * interrupt.c - interrupt management routines for intel 8259 chip
 */

#include <kernel.h>
#include <hal.h>
#include <irq.h>
#include <io.h>
#include <cpufunc.h>
#include <context.h>
#include <trap.h>
#include <clock.h>
#include <locore.h>
#include <sys/ipl.h>

/* Number of IRQ lines */
#define NIRQS		16

/* I/O address for master/slave programmable interrupt controller */
#define PIC_M           0x20
#define PIC_S           0xa0

/* Edge/level control register */
#define ELCR            0x4d0

/*
 * Interrupt priority level
 *
 * Each interrupt has its logical priority level, with 0 being
 * the highest priority. While some ISR is running, all lower
 * priority interrupts are masked off.
 */
static volatile int irq_level;

/*
 * Interrupt mapping table
 */
static int	ipl_table[NIRQS];	/* Vector -> level */
static u_int	mask_table[NIPLS];	/* Level -> mask */

/*
 * Set mask for current ipl
 */
static void
update_mask(void)
{
	u_int mask = mask_table[irq_level];

	outb(PIC_M + 1, mask & 0xff);
	outb(PIC_S + 1, mask >> 8);
}

/*
 * Unmask interrupt in PIC for specified irq.
 * The interrupt mask table is also updated.
 * Assumed CPU interrupt is disabled in caller.
 */
void
interrupt_unmask(int vector, int level)
{
	u_int unmask = (u_int)~(1 << vector);
	int i, s;

	s = splhigh();
	ipl_table[vector] = level;
	/*
	 * Unmask target interrupt for all
	 * lower interrupt levels.
	 */
	for (i = 0; i < level; i++)
		mask_table[i] &= unmask;
	update_mask();
	splx(s);
}

/*
 * Mask interrupt in PIC for specified irq.
 * Interrupt must be disabled when this routine is called.
 */
void
interrupt_mask(int vector)
{
	u_int mask = (u_int)(1 << vector);
	int i, level, s;

	s = splhigh();
	level = ipl_table[vector];
	for (i = 0; i < level; i++)
		mask_table[i] |= mask;
	ipl_table[vector] = IPL_NONE;
	update_mask();
	splx(s);
}

/*
 * Setup interrupt mode.
 * Select whether an interrupt trigger is edge or level.
 */
void
interrupt_setup(int vector, int mode)
{
	int port, s;
	u_int bit;
	u_char val;

	s = splhigh();
	port = vector < 8 ? ELCR : ELCR + 1;
	bit = (u_int)(1 << (vector & 7));

	val = inb(port);
	if (mode == IMODE_LEVEL)
		val |= bit;
	else
		val &= ~bit;
	outb(port, val);
	splx(s);
}

/*
 * Get interrupt source.
 */
static int
interrupt_lookup(void)
{
	int irq;

	outb(PIC_M, 0x0c);	/* poll and ack */
	irq = inb(PIC_M) & 7;
	if (irq == 2) {
		outb(PIC_S, 0x0c);
		irq = (inb(PIC_M) & 7) + 8;
	}
	return irq;
}

/*
 * Common interrupt handler.
 *
 * This routine is called from the low level interrupt routine
 * written in assemble code. The interrupt flag is automatically
 * disabled by h/w in CPU when the interrupt is occurred. The
 * target interrupt will be masked in ICU while the irq handler
 * is called.
 */
void
interrupt_handler(struct cpu_regs *regs)
{
	int vector;
	int old_ipl, new_ipl;

	/* Handle decrementer interrupt */
	if (regs->trap_no == TRAP_DECREMENTER) {
		clock_isr(NULL);
		return;
	}

	/* Find pending interrupt */
	vector = interrupt_lookup();

	/* Adjust interrupt level */
	old_ipl = irq_level;
	new_ipl = ipl_table[vector];
	if (new_ipl > old_ipl)		/* Ignore spurious interrupt */
		irq_level = new_ipl;
	update_mask();

	/* Dispatch interrupt */
	splon();
	irq_handler(vector);
	sploff();

	/* Restore interrupt level */
	irq_level = old_ipl;
	update_mask();
}

/*
 * Initialize 8259 interrupt controllers.
 * All interrupts will be masked off in ICU.
 */
void
interrupt_init(void)
{
	int i;

	irq_level = IPL_NONE;

	for (i = 0; i < NIRQS; i++)
		ipl_table[i] = IPL_NONE;

	for (i = 0; i < NIPLS; i++)
		mask_table[i] = 0xfffb;

	outb(PIC_M, 0x11);	/* Start initialization edge, master */
	outb(PIC_M + 1, 0x00);	/* Set h/w vector = 0x0 */
	outb(PIC_M + 1, 0x04);	/* Chain to slave (IRQ2) */
	outb(PIC_M + 1, 0x01);	/* 8086 mode */

	outb(PIC_S, 0x11);	/* Start initialization edge, master */
	outb(PIC_S + 1, 0x08);	/* Set h/w vector = 0x8 */
	outb(PIC_S + 1, 0x02);	/* Slave (cascade) */
	outb(PIC_S + 1, 0x01);	/* 8086 mode */

	outb(PIC_S, 0x0b);	/* Read ISR by default */
	outb(PIC_M, 0x0b);	/* Read ISR by default */

	outb(PIC_S + 1, 0xff);	/* Mask all */
	outb(PIC_M + 1, 0xfb);	/* Mask all except IRQ2 (cascade) */
}
