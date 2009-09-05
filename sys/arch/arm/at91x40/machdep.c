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
 * machdep.c - machine-dependent routines for AT91x40
 */

#include <kernel.h>
#include <cpu.h>
#include <locore.h>
#include <irq.h>
#include <conf/config.h>
#include "platform.h"

/* Watchdog registers */
#define WD_OMR		(*(volatile uint32_t *)(WD_BASE + 0x00))
#define WD_CMR		(*(volatile uint32_t *)(WD_BASE + 0x04))
#define WD_CR		(*(volatile uint32_t *)(WD_BASE + 0x08))
#define WD_SR		(*(volatile uint32_t *)(WD_BASE + 0x0c))

/* Power save registers */
#define PS_CR		(*(volatile uint32_t *)(PS_BASE + 0x00))

/* WD_OMR - Overflow mode register */
#define OMR_WDEN	(1 << 0)
#define OMR_RSTEN	(1 << 1)
#define OMR_IRQEN	(1 << 2)
#define OMR_EXTEN	(1 << 3)
#define OMR_OKEY	(0x234 << 4)

/* WD_CMR - Clock mode register */
#define CMR_MCK8	(0x0)
#define CMR_MCK32	(0x1)
#define CMR_MCK128	(0x2)
#define CMR_MCK1024	(0x3)
#define CMR_HPCV	(0xF << 2)
#define CMR_CKEY	(0x06E << 7)

/* WD_CR - Control register */
#define CR_RSTKEY	(0xC071 << 0)

/*
 * Machine-dependent startup code
 */
void
machine_init(void)
{

	/*
	 * Setup vector page.
	 */
	vector_copy((vaddr_t)phys_to_virt(ARM_VECTORS_LOW));
}

/*
 * Disable the watchdog timer
 */
#ifdef CONFIG_WATCHDOG
static void
wdt_stop(void)
{

	WD_OMR = OMR_OKEY;
}
#endif

/*
 * Stop the MCU
 */
void
machine_stop(void)
{
	irq_lock();

#ifdef CONFIG_WATCHDOG
	wdt_stop();
#endif

	for (;;) {
		machine_idle();
	}
}

/*
 * Set system power
 */
void
machine_setpower(int state)
{
	irq_lock();
#ifdef DEBUG
	printf("The system is halted. You can turn off power.");
#endif
#ifdef CONFIG_WATCHDOG
	wdt_stop();
#endif
	for (;;) {
		machine_idle();
	}
}

/*
 * Reset the MCU
 */
void
machine_reset(void)
{
	irq_lock();

#ifdef CONFIG_WATCHDOG
	wdt_stop();
#endif
	/* Set minimum restart time */
	WD_CMR = CMR_CKEY | CMR_MCK8 | (0 & CMR_HPCV);

	/* Enable watchdog & enable reset MCU */
	WD_OMR = OMR_OKEY | OMR_WDEN | OMR_RSTEN;

	/* Watchdog will reset the MCU */
	for (;;) {
		machine_idle();
	}
}

/*
 * Idle loop
 */
void
machine_idle(void)
{

	/* Halt the CPU core */
	PS_CR = 1;
}
