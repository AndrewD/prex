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

#include <sys/param.h>
#include <boot.h>


#define SCREEN_80x25 1
/* #define SCREEN_80x50 1 */

#define COM_BASE	CONFIG_NS16550_BASE

/* Register offsets */
#define COM_RBR		(COM_BASE + 0x00)	/* receive buffer register */
#define COM_THR		(COM_BASE + 0x00)	/* transmit holding register */
#define COM_IER		(COM_BASE + 0x01)	/* interrupt enable register */
#define COM_FCR		(COM_BASE + 0x02)	/* FIFO control register */
#define COM_IIR		(COM_BASE + 0x02)	/* interrupt identification register */
#define COM_LCR		(COM_BASE + 0x03)	/* line control register */
#define COM_MCR		(COM_BASE + 0x04)	/* modem control register */
#define COM_LSR		(COM_BASE + 0x05)	/* line status register */
#define COM_MSR		(COM_BASE + 0x06)	/* modem status register */
#define COM_DLL		(COM_BASE + 0x00)	/* divisor latch LSB (LCR[7] = 1) */
#define COM_DLM		(COM_BASE + 0x01)	/* divisor latch MSB (LCR[7] = 1) */

#ifdef DEBUG
volatile u_char *ISA_io  = (u_char *)0x80000000;

static void
outb(int port, u_char val)
{

	ISA_io[port] = val;
}

static u_char
inb(int port)
{

	return (ISA_io[port]);
}
#endif

/*
 * Print one chracter
 */
void
debug_putc(int c)
{

#if defined(DEBUG) && defined(CONFIG_DIAG_SERIAL)
	/*
	 * output to serial port.
	 */
	while (!(inb(COM_LSR) & 0x20))
		;
	outb(COM_THR, (u_char)c);
#endif

#if defined(DEBUG) && defined(CONFIG_DIAG_QEMU)
	inb(0xf00); /* dummy */
	outb(0xf00, (u_char)c);
#endif
}

/*
 * Initialize debug port.
 */
void
debug_init(void)
{

#if defined(DEBUG) && defined(CONFIG_DIAG_SERIAL)
	/*
	 * Initialize serial port.
	 */
	if (inb(COM_LSR) == 0xff)
		return;		/* Serial port is disabled */

	outb(COM_IER, 0x00);	/* Disable interrupt */
	outb(COM_LCR, 0x80);	/* Access baud rate */
	outb(COM_DLL, 0x01);	/* 115200 baud */
	outb(COM_DLM, 0x00);
	outb(COM_LCR, 0x03);	/* N, 8, 1 */
	outb(COM_MCR, 0x03);	/* Ready */
	outb(COM_FCR, 0x00);	/* Disable FIFO */
	inb(COM_RBR);
	inb(COM_RBR);
#endif
}
