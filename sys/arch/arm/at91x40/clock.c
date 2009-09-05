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
 * clock.c - clock driver for AT91x40
 */

#include <kernel.h>
#include <timer.h>
#include <irq.h>
#include "platform.h"


/*Clock interrupt vector */
#define TC_IRQ		4

#define TC_CCR		(*(volatile uint32_t *)(TC_BASE + 0x00))
#define TC_CMR		(*(volatile uint32_t *)(TC_BASE + 0x04))
#define TC_RC		(*(volatile uint32_t *)(TC_BASE + 0x1c))
#define TC_SR		(*(volatile uint32_t *)(TC_BASE + 0x20))
#define TC_IER		(*(volatile uint32_t *)(TC_BASE + 0x24))
#define TC_IDR		(*(volatile uint32_t *)(TC_BASE + 0x28))
#define TC_IMR		(*(volatile uint32_t *)(TC_BASE + 0x2c))

/* TC_CCR - Clock control register */
#define CCR_CLKEN	(1 << 0)
#define CCR_SWTRG	(1 << 2)

/* TC_CMR - Clock mode register */
#define CMR_CPCTRG	(1 << 14)
#define CMR_MCK1024	(4 << 0)

/* TC_SR, TC_IER, TC_IDR, TC_IMR - TC interrupt types */
#define IR_CPCS		(1 << 4)

/*
 * Clock ISR
 */
static int
clock_isr(int irq)
{
	uint32_t dummy;

	/* Ack Timer interrupt */
	dummy = TC_SR;

	irq_lock();
	timer_tick();
	irq_unlock();

	return INT_DONE;
}

/*
 * Initialize clock timer
 */
void
clock_init(void)
{
	irq_t clock_irq;

	/* RC compare */
	TC_CMR = CMR_CPCTRG | CMR_MCK1024;

	/* Setting up prescaler */
	TC_RC = (CONFIG_MCU_FREQ / (1024 * HZ));

	/* Enable interrupt on RC compare */
	TC_IER = IR_CPCS;

	clock_irq = irq_attach(TC_IRQ, IPL_CLOCK, 0, &clock_isr, NULL);

	/* Enable timer clock */
	TC_CCR = CCR_CLKEN | CCR_SWTRG;
	TC_CCR = CCR_CLKEN;

	ASSERT(clock_irq != NULL);
}
