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
 * diag.c - diagnostic message support
 */

#include <sys/bootinfo.h>
#include <kernel.h>
#include <cpufunc.h>
#include <io.h>

#ifdef CONFIG_DIAG_SERIAL

#define COM_BASE	CONFIG_NS16550_BASE
#define COM_THR		(COM_BASE + 0x00)	/* transmit holding register */
#define COM_LSR		(COM_BASE + 0x05)	/* line status register */

static void
serial_putc(char c)
{

	while (!(inb(COM_LSR) & 0x20))
		;
	outb(COM_THR, c);
}

void
diag_puts(char *str)
{

	while (*str) {
		if (*str == '\n')
			serial_putc('\r');
		serial_putc(*str++);
	}
}

/*
 * We assume the serial port has already been initialized by
 * the boot loader.
 */
void
diag_init(void)
{
	/* DO NOTHING */
}
#endif	/* !CONFIG_DIAG_SERIAL */

#ifdef CONFIG_DIAG_QEMU
void
diag_puts(char *str)
{

	while (*str) {
		/* Write to the qemu debug port. */
		outb(0xf00, (u_char)*str++);
	}
}

void
diag_init(void)
{
}
#endif	/* !CONFIG_DIAG_QEMU */
