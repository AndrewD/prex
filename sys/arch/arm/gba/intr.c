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
 * intr.c - interrupt handling routines for GBA
 */

#include <kernel.h>
#include <irq.h>

extern void interrupt_entry(void);

/*
 * Interrupt priority level
 */
#define NR_IPLS		8	/* Number of interrupt priority levels */
#define IPL_NORMAL	7	/* Default interrupt priority level */

/* Interrupt hook vector */
#define IRQ_VECTOR	*(uint32_t *)0x3007ffc

/* Registers for interrupt control unit - enable/flag/master */
#define ICU_IE		(*(volatile uint16_t *)0x4000200)
#define ICU_IF		(*(volatile uint16_t *)0x4000202)
#define ICU_IME		(*(volatile uint16_t *)0x4000208)

/* ICU_IE */
#define IRQ_VALID	0x3fff

/* ICU_IME */
#define IRQ_OFF		0
#define IRQ_ON		1

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
volatile int irq_nesting;

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
static int irq_level[NR_IRQS];		/* Vector -> level */
static uint16_t irq_mask[NR_IPLS];	/* Level -> mask */

/*
 * Set mask for current ipl
 */
#define update_mask()	ICU_IE = irq_mask[cur_ipl]

/*
 * Unmask interrupt in PIC for specified irq.
 * The interrupt mask table is also updated.
 * Assumed CPU interrupt is disabled in caller.
 */
void interrupt_unmask(int vector, int level)
{
	int i;
	uint16_t unmask = (uint16_t)1 << vector;

	irq_level[vector] = level;
	for (i = level + 1; i <= IPL_NORMAL; i++)
		irq_mask[i] |= unmask;
	update_mask();
}

/*
 * Mask interrupt in PIC for specified irq.
 * Interrupt must be disabled when this routine is called.
 */
void interrupt_mask(int vector)
{
	int i;
	u_int mask = (uint16_t)~(1 << vector);

	for (i = irq_level[vector] + 1; i <= IPL_NORMAL; i++)
		irq_mask[i] &= mask;
	irq_level[vector] = IPL_NORMAL;
	update_mask();
}

/*
 * Setup interrupt mode.
 * Select whether an interrupt trigger is edge or level.
 */
void interrupt_setup(int vector, int mode)
{
	/* nop */
}

/*
 * Dispatch interrupt
 *
 */
void interrupt_dispatch(int vector)
{
	int old_ipl;

	/* Save & update interrupt level */
	old_ipl = cur_ipl;
	cur_ipl = irq_level[vector];
	update_mask();

	/* Send acknowledge to ICU for this irq */
	ICU_IF = (uint16_t)(1 << vector);

	/* Allow another interrupt that has higher priority */
	interrupt_enable();

	/* Dispatch interrupt */
	irq_handler(vector);

	interrupt_disable();

	/* Restore interrupt level */
	cur_ipl = old_ipl;
	update_mask();
}

/*
 * Common interrupt handler.
 */
void interrupt_handler(void)
{
	uint16_t bits;
	int vector;

	bits = ICU_IF;
retry:
	for (vector = 0; vector < NR_IRQS; vector++) {
		if (bits & (uint16_t)(1 << vector))
			break;
	}
	if (vector == NR_IRQS)
		goto out;

	interrupt_dispatch(vector);

	/*
	 * Multiple interrupts can be fired in case of GBA.
	 * So, we have to check the interrupt status, again.
	 */
	bits = ICU_IF;
	if (bits & IRQ_VALID)
		goto retry;
out:
	return;
}

/*
 * Initialize interrupt controllers.
 * All interrupts will be masked off.
 */
void interrupt_init(void)
{
	int i;

	irq_nesting = 0;
	cur_ipl = IPL_NORMAL;

	for (i = 0; i < NR_IRQS; i++)
		irq_level[i] = IPL_NORMAL;

	for (i = 0; i < NR_IPLS; i++)
		irq_mask[i] = 0;

	ICU_IME = IRQ_OFF;
	IRQ_VECTOR = (uint32_t)interrupt_entry; /* Interrupt hook address */
	ICU_IE = 0;			/* Mask all interrupts */
	ICU_IME = IRQ_ON;
}
