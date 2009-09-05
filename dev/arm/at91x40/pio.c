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

#include "pio.h"

#define PIO_BASE	0xFFFF0000

#define PIO_PER		(*(volatile uint32_t *)(PIO_BASE + 0x00))
#define PIO_PDR		(*(volatile uint32_t *)(PIO_BASE + 0x04))
#define PIO_PSR		(*(volatile uint32_t *)(PIO_BASE + 0x08))

#define PIO_OER		(*(volatile uint32_t *)(PIO_BASE + 0x10))
#define PIO_ODR		(*(volatile uint32_t *)(PIO_BASE + 0x14))
#define PIO_OSR		(*(volatile uint32_t *)(PIO_BASE + 0x18))
#define PIO_SODR	(*(volatile uint32_t *)(PIO_BASE + 0x30))
#define PIO_CODR	(*(volatile uint32_t *)(PIO_BASE + 0x34))
#define PIO_ODSR	(*(volatile uint32_t *)(PIO_BASE + 0x38))
#define PIO_PDSR	(*(volatile uint32_t *)(PIO_BASE + 0x3c))

#define PIO_IER		(*(volatile uint32_t *)(PIO_BASE + 0x40))
#define PIO_IDR		(*(volatile uint32_t *)(PIO_BASE + 0x44))
#define PIO_IMR		(*(volatile uint32_t *)(PIO_BASE + 0x48))
#define PIO_ISR		(*(volatile uint32_t *)(PIO_BASE + 0x4c))


void
pio_enable(uint32_t mask)
{
	PIO_PER = mask;
}

void
pio_disable(uint32_t mask)
{
	PIO_PDR = mask;
}

uint32_t
pio_status(void)
{
	return PIO_PSR;
}

void
pio_setin(uint32_t mask)
{
	PIO_ODR = mask;
}

void
pio_setout(uint32_t mask)
{
	PIO_OER = mask;
}

uint32_t
pio_getout(void)
{
	return PIO_OSR;
}

void
pio_set(uint32_t mask)
{
	PIO_SODR = mask;
}

void
pio_clear(uint32_t mask)
{
	PIO_CODR = mask;
}

uint32_t
pio_get(void)
{
	return PIO_ODSR;
}

uint32_t
pio_state(void)
{
	return PIO_PDSR;
}
