/*-
 * Copyright (c) 2009, Richard Pandion
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
 * interrupt.c - interrupt handling routines
 */

#include <kernel.h>
#include <irq.h>
#include <locore.h>
#include <cpu.h>
#include <cpufunc.h>

/* MPU INTC Registers */
#define INTCPS_SYSCONFIG	(*(volatile uint32_t *)(MPU_INTC_BASE + 0x010))
#define INTCPS_SYSSTATUS	(*(volatile uint32_t *)(MPU_INTC_BASE + 0x014))
#define INTCPS_SIR_IRQ		(*(volatile uint32_t *)(MPU_INTC_BASE + 0x040))
#define INTCPS_SIR_FIQ		(*(volatile uint32_t *)(MPU_INTC_BASE + 0x044))
#define INTCPS_CONTROL		(*(volatile uint32_t *)(MPU_INTC_BASE + 0x048))
#define INTCPS_PROTECTION	(*(volatile uint32_t *)(MPU_INTC_BASE + 0x04C))
#define INTCPS_IDLE		(*(volatile uint32_t *)(MPU_INTC_BASE + 0x050))
#define INTCPS_IRQ_PRIORITY	(*(volatile uint32_t *)(MPU_INTC_BASE + 0x060))
#define INTCPS_FIQ_PRIORITY	(*(volatile uint32_t *)(MPU_INTC_BASE + 0x064))
#define INTCPS_THRESHOLD	(*(volatile uint32_t *)(MPU_INTC_BASE + 0x068))
#define INTCPS_ITR(a)		(*(volatile uint32_t *)(MPU_INTC_BASE + 0x080 + (0x20*a)))
#define INTCPS_MIR(a)		(*(volatile uint32_t *)(MPU_INTC_BASE + 0x084 + (0x20*a)))
#define INTCPS_MIR_CLEAR(a)	(*(volatile uint32_t *)(MPU_INTC_BASE + 0x088 + (0x20*a)))
#define INTCPS_MIR_SET(a)	(*(volatile uint32_t *)(MPU_INTC_BASE + 0x08C + (0x20*a)))
#define INTCPS_ISR_SET(a)	(*(volatile uint32_t *)(MPU_INTC_BASE + 0x090 + (0x20*a)))
#define INTCPS_ISR_CLEAR(a)	(*(volatile uint32_t *)(MPU_INTC_BASE + 0x094 + (0x20*a)))
#define INTCPS_PENDING_IRQ(a)	(*(volatile uint32_t *)(MPU_INTC_BASE + 0x098 + (0x20*a)))
#define INTCPS_PENDING_FIQ(a)	(*(volatile uint32_t *)(MPU_INTC_BASE + 0x09C + (0x20*a)))


/*
 * Interrupt Priority Level
 *
 * Each interrupt has its logical priority level, with 0 being
 * the lowest priority. While some ISR is running, all lower
 * priority interrupts are masked off.
 */
volatile int irq_level;

/*
 * Interrupt mapping table. As the number of interrupts on the
 * Beagle is > 32, we need a 2 dimensions array for the mask table
 */
static int ipl_table[NIRQS];			/* vector -> level */
static uint32_t mask_table[NIPLS][NIRQS>>5];	/* level -> mask   */

/*
 * Set mask for current ipl
 */
static void
update_mask(void)
{
	int i;
	uint32_t mask;

	for (i = 0; i < (NIRQS>>5); i++) {
		mask = mask_table[irq_level][i];
		INTCPS_MIR(i) = ~mask;
	}
}

/*
 * Unmask interrupt in INTC for specified irq.
 * The interrupt mask table is also updated.
 * Assumes CPU interrupt is disabled in caller.
 */
void
interrupt_unmask(int vector, int level)
{
	int i;
	uint32_t unmask = (uint32_t)1 << (vector & 0x1f);

	/* Save level mapping */
	ipl_table[vector] = level;

	/*
	 * Unmask the target interrupt for all
	 * lower interrupt levels.
	 */
	for (i = 0; i < level; i++)
		mask_table[i][vector>>5] |= unmask;

	update_mask();
}

/*
 * Mask interrupt in INTC for specified irq.
 * Interrupt must be disabled when this routine is called.
 */
void
interrupt_mask(int vector)
{
	int i, level;
	uint32_t mask = (uint32_t)~(1 << (vector & 0x1f));

	level = ipl_table[vector];

	for (i = 0; i < level; i++)
		mask_table[i][vector>>5] &= mask;

	ipl_table[vector] = IPL_NONE;

	update_mask();
}

/*
 * Setup interrupt mode.
 * Select whether an interrupt trigger is edge or level.
 */
void
interrupt_setup(int vector, int mode)
{
	/* nop */
}

/*
 * Common interrupt handler.
 */
void
interrupt_handler(void)
{
	uint32_t bits;
	int vector, old_ipl, new_ipl;

	bits = INTCPS_SIR_IRQ;		/* Get interrupt source */
	if (bits >= NIRQS)		/* Ignore spurious interrupts */
		goto out;
	vector = bits & 0x7f;		/* Get device firing the interrupt */

	/* Adjust interrupt level */
	old_ipl = irq_level;
	new_ipl = ipl_table[vector];
	if (new_ipl > old_ipl)		/* Ignore spurious interrupt */
		irq_level = new_ipl;
	update_mask();

	INTCPS_CONTROL = 0x01;		/* Allow new IRQ on INTC side */
	mpu_intc_sync();			/* Data synchronization barrier */

	/* Dispatch interrupt */
	interrupt_enable();
	irq_handler(vector);
	interrupt_disable();

	/* Restore interrupt level */
	irq_level = old_ipl;
	update_mask();
out:
	return;
}

/*
 * Initialize interrupt controllers.
 * All interrupts will be masked off.
 */
void
interrupt_init(void)
{
	int i,j;

	irq_level = IPL_NONE;

	for (i = 0; i < NIRQS; i++)
		ipl_table[i] = IPL_NONE;

	for (i = 0; i < NIPLS; i++)
		for (j = 0; j < (NIRQS>>5); j++)
			mask_table[i][j] = 0;

	INTCPS_SYSCONFIG = 0x02;	/* Reset interrupt controller. This also masks all interrupts */

	while (INTCPS_SYSSTATUS != 0x01) ;

}
