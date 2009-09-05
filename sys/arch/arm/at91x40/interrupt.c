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
 * interrupt.c - interrupt handling routines for AT91x40
 */

#include <kernel.h>
#include <irq.h>
#include <locore.h>
#include <cpu.h>

/* Advanced interrupt controller registers */

/* Blocks of registers for all 32 interrupt sources */
#define AIC_SMR		((volatile uint32_t *)(AIC_BASE + 0x00))
#define AIC_SVR		((volatile uint32_t *)(AIC_BASE + 0x80))

#define AIC_IVR		(*(volatile uint32_t *)(AIC_BASE + 0x100))
#define AIC_FVR		(*(volatile uint32_t *)(AIC_BASE + 0x104))
#define AIC_ISR		(*(volatile uint32_t *)(AIC_BASE + 0x108))
#define AIC_IPR		(*(volatile uint32_t *)(AIC_BASE + 0x10c))
#define AIC_IMR		(*(volatile uint32_t *)(AIC_BASE + 0x110))
#define AIC_CISR	(*(volatile uint32_t *)(AIC_BASE + 0x114))
#define AIC_IECR	(*(volatile uint32_t *)(AIC_BASE + 0x120))
#define AIC_IDCR	(*(volatile uint32_t *)(AIC_BASE + 0x124))
#define AIC_ICCR	(*(volatile uint32_t *)(AIC_BASE + 0x128))
#define AIC_ISCR	(*(volatile uint32_t *)(AIC_BASE + 0x12c))
#define AIC_EOICR	(*(volatile uint32_t *)(AIC_BASE + 0x130))
#define AIC_SPU		(*(volatile uint32_t *)(AIC_BASE + 0x134))

/* AIC_SMR - Source mode register */
/* IRQ priority */
#define SMR_PRIOR	(7 << 0)
#define SMR_LOWEST	0
#define SMR_HIGHEST	7

/* IRQ Source Type  */
#define SMR_TYPE	(3 << 5)
#define SMR_LOW_LEVEL	(0 << 5)
#define SMR_NEG_EDGE	(1 << 5)
#define SMR_HIGH_LEVEL	(2 << 5)
#define SMR_POS_EDGE	(3 << 5)

/* Special function register - Protect mode register */
#define SF_PMR		(*(volatile uint32_t *)(SF_BASE + 0x18))
#define PMR_AIC		(1 << 5)
#define PMR_KEY		(0x27A8 << 16)

/*
 * Current IPL
 */
volatile int irq_level;

/*
 * Table to map Prex interrupt priority to
 * interrupt controller H/W priority
 */
static const uint32_t ipl_to_prio[NIPLS] = {
	0,		/* IPL_NONE */
	0,		/* IPL_COMM */
	0,		/* IPL_BLOCK */
	1,		/* IPL_NET */
	1,		/* IPL_DISPLAY */
	2,		/* IPL_INPUT */
	2,		/* IPL_AUDIO */
	3,		/* IPL_BUS */
	4,		/* IPL_RTC */
	5,		/* IPL_PROFILE */
	6,		/* IPL_CLOCK */
	7		/* IPL_HIGH */
};

/*
 * Table to save IPL value for each interrupt vector
 */
static int vector_to_ipl[NIRQS];

/*
 * Unmask interrupt for specified irq.
 */
void
interrupt_unmask(int vector, int level)
{
	uint32_t prio;

	/* Save IPL for future use */
	vector_to_ipl[vector] = level;

	/* Get H/W interrupt level */
	prio = ipl_to_prio[level];

	/* Config irq line priority */
	AIC_SMR[vector] = (prio & SMR_PRIOR);

	/* Enable irq line */
	AIC_IECR = (1 << vector);
}

/*
 * Mask interrupt for specified irq.
 */
void
interrupt_mask(int vector)
{
	AIC_IDCR = (1 << vector);
}

/*
 * Setup interrupt mode.
 */
void
interrupt_setup(int vector, int mode)
{
	uint32_t old_smr;
	uint32_t type;

	old_smr = AIC_SMR[vector];
	if (vector < 16) {
		/* All internal sources as low level */
		type = SMR_LOW_LEVEL;
	} else {
		if (mode == IMODE_LEVEL) {
			type = SMR_LOW_LEVEL;
		} else {
			type = SMR_NEG_EDGE;
		}
	}
	AIC_SMR[vector] = (old_smr & (~SMR_TYPE)) | type;
}

void
interrupt_dispatch(int vector)
{
	int  old_ipl;

	/* Recalculate current IPL */
	old_ipl = irq_level;

	/* Get IPL for current vector */
	irq_level = vector_to_ipl[vector];

	/* Allow another interrupt that has higher priority */
	interrupt_enable();

	/* Dispatch interrupt */
	irq_handler(vector);

	interrupt_disable();

	irq_level = old_ipl;
}

/*
 * Common interrupt handler.
 */
void
interrupt_handler(void)
{
	int vector, dummy;

	/* Ack interrupt */
	dummy = AIC_IVR;

#ifdef DEBUG
	/* Ack interrupt in protect mode */
	AIC_IVR = dummy;
#endif

	/* Get highest priority vector */
	vector = AIC_ISR;

	if (vector != 0) {
		interrupt_dispatch(vector);
	} else {
		/* Detected spurious interrupt */
		/* printf("Spurious interrupt detected!") */;
	}

	/* End of interrupt */
	AIC_EOICR = 0;

	return;
}

/*
 * Initialize interrupt controllers.
 * All interrupts will be masked off.
 */
void
interrupt_init(void)
{
	int irq;

	/* Disable all irq */
	AIC_IDCR = 0xFFFFFFFF;

	for (irq = 0; irq < NIRQS; ++irq) {
		AIC_SVR[irq] = 0x00000000;
	}
	AIC_SPU = 0x00000000;

#ifdef DEBUG
	/* Put AIC in protected mode */
	SF_PMR = PMR_KEY | PMR_AIC;
#endif
}
