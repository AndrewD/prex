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
 * gdb_blue.c - serial data transter routine for gdb stub
 */

#include <kernel.h>
#include <cpu.h>

#ifdef CONFIG_GDB

/* Serial port registers */
#define COM_PORT  0x3F8
#define RBR	0		/* 3F8 Receive buffer register. */
#define THR	0		/* 3F8 Transmit holding register. */
#define IER	1		/* 3F9 Interrupt enable register. */
#define FCR	2		/* 3FA FIFO control register. */
#define IIR	2		/* 3FA Interrupt identification register. */
#define LCR	3		/* 3FB Line control register. */
#define MCR	4		/* 3FC Modem control register. */
#define LSR	5		/* 3FD Line status register. */
#define MSR	6		/* 3FE Modem status register. */

#define DLL	0		/* 3F8 Divisor latch LSB (LCR[7] = 1). */
#define DLM	1		/* 3F9 Divisor latch MSB (LCR[7] = 1). */

int
serial_getchar(void)
{

	while (!(inb(COM_PORT + LSR) & 0x01));
	return inb(COM_PORT + RBR);
}

void
serial_putchar(int ch)
{

	while (!(inb(COM_PORT + LSR) & 0x20));
	outb((char)ch, COM_PORT + THR);
}

int
serial_init(void)
{

	if (inb(COM_PORT + LSR) == 0xFF)
		return -1;	/* Serial port is disabled */

	outb(0x00, COM_PORT + IER);	/* Disable all interrupt */
	outb(0x80, COM_PORT + LCR);	/* Access baud rate */
	outb(0x01, COM_PORT + DLL);	/* Baud rate = 115200 */
	outb(0x00, COM_PORT + DLM);
	outb(0x03, COM_PORT + LCR);	/* N, 8, 1 */
	outb(0x03, COM_PORT + MCR);	/* Ready */
	outb(0x00, COM_PORT + FCR);	/* Disable FIFO */
	inb(COM_PORT);
	inb(COM_PORT);
	return 0;
}

#endif /* CONFIG_GDB */
