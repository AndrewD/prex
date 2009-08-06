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
 * clock.c - clock driver
 */

#include <kernel.h>
#include <timer.h>
#include <irq.h>

/* Interrupt vector for timer (GPTIMER2) */
#define CLOCK_IRQ	38

/* The clock rate per second - 32Khz */
/* We are using the 32 Khz clock in order to get accurate 1 ms tick rate */
#define CLOCK_RATE	32768L

/* The initial counter value */
#define TIMER_COUNT	(CLOCK_RATE / HZ)

/* PRCM registers for timer */
#define CM_CLKSEL_PER	(*(volatile uint32_t *)(0x48005040))
#define CM_FCLKEN_PER	(*(volatile uint32_t *)(0x48005000))
#define CM_ICLKEN_PER	(*(volatile uint32_t *)(0x48005010))

/* Timer 2 registers (32 bit regs) */
#define TIDR		(*(volatile uint32_t *)(TIMER_BASE + 0x00))	/* r */
#define TIOCP_CFG	(*(volatile uint32_t *)(TIMER_BASE + 0x10))	/* rw */
#define TISTAT		(*(volatile uint32_t *)(TIMER_BASE + 0x14))	/* r */
#define TISR		(*(volatile uint32_t *)(TIMER_BASE + 0x18))	/* rw */
#define TIER		(*(volatile uint32_t *)(TIMER_BASE + 0x1C))	/* rw */
#define TWER		(*(volatile uint32_t *)(TIMER_BASE + 0x20))	/* rw */
#define TCLR		(*(volatile uint32_t *)(TIMER_BASE + 0x24))	/* rw */
#define TCRR		(*(volatile uint32_t *)(TIMER_BASE + 0x28))	/* rw */
#define TLDR		(*(volatile uint32_t *)(TIMER_BASE + 0x2C))	/* rw */
#define TTGR		(*(volatile uint32_t *)(TIMER_BASE + 0x30))	/* rw */
#define TWPS		(*(volatile uint32_t *)(TIMER_BASE + 0x34))	/* r */
#define TMAR		(*(volatile uint32_t *)(TIMER_BASE + 0x38))	/* rw */
#define TCAR1		(*(volatile uint32_t *)(TIMER_BASE + 0x3c))	/* r */
#define TSICR		(*(volatile uint32_t *)(TIMER_BASE + 0x40))	/* rw */
#define TCAR2		(*(volatile uint32_t *)(TIMER_BASE + 0x44))	/* r */
#define TPIR		(*(volatile uint32_t *)(TIMER_BASE + 0x48))	/* rw */
#define TNIR		(*(volatile uint32_t *)(TIMER_BASE + 0x4C))	/* rw */

#define INTCPS_ILR(a)	(*(volatile uint32_t *)(MPU_INTC_BASE + 0x100 + (0x04*a)))

/*
 * Clock interrupt service routine.
 * No H/W reprogram is required.
 */
static int
clock_isr(int irq)
{

	irq_lock();
	timer_tick();
	TISR = 0x02;	/* Clear GPT2 interrupt pending flag */
	irq_unlock();

	return INT_DONE;
}

/*
 * Initialize clock H/W chip.
 * Setup clock tick rate and install clock ISR.
 */
void
clock_init(void)
{
	irq_t clock_irq;

	/* Setup PRCM so that GPT2 uses 32Khz clock now */
	CM_CLKSEL_PER &= 0xFE;	/* GPT2 clock = 32K_FCLK */
	CM_ICLKEN_PER |= 0x10;	/* Enable Interface clock on GPT2 */
	CM_FCLKEN_PER |= 0x10;	/* Enable Functional clock on GPT2 */

	TIOCP_CFG = 0x02;	/* Now, reset GPT2 */
	while (TISTAT != 0x01) ;

	TCLR &= 0xFFFFFF00;	/* Stop GPT2 and disable all timing modes */
	TPIR = 232000;		/* Positive increment value for accurate 1 ms tick */
	TNIR = -768000;		/* Negative increment value for accurate 1 ms tick */
	TLDR = 0xFFFFFFE0;	/* Load value for 1 ms tick */
	TCRR = 0xFFFFFFE0;	/* Current value = Load value */

	/* Install ISR */
	clock_irq = irq_attach(CLOCK_IRQ, IPL_CLOCK, 0, &clock_isr, NULL);
	ASSERT(clock_irq != NULL);

	/* Enable overflow interrupt in auto-reload mode */
	INTCPS_ILR(CLOCK_IRQ) = ((NIPLS-IPL_CLOCK)<<2);
	TIER = 0x02;		/* Enable overflow interrupt */
	TCLR |= 0x03;		/* Start timer in auto-reload mode */
}
