/*-
 * Copyright (c) 2008, Kohsuke Ohtani
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

#include <kernel.h>
#include <syspage.h>
#include <cpu.h>
#include "platform.h"

#ifdef DEBUG

#ifdef CONFIG_DIAG_SERIAL

#define UART_CSR	(*(volatile uint32_t *)(UART_BASE + 0x14))
#define UART_THR	(*(volatile uint32_t *)(UART_BASE + 0x1c))

/* UART_CSR - channel status register */
#define IR_TXRDY	(1 << 1)	/* rx ready */

static void
serial_putc(char c)
{

	while (!(UART_CSR & IR_TXRDY))
		;

	UART_THR = (uint32_t)c;
}
#endif /* CONFIG_DIAG_SERIAL */

void
diag_print(char *buf)
{

#ifdef CONFIG_DIAG_SERIAL
	while (*buf) {
		if (*buf == '\n')
			serial_putc('\r');
		serial_putc(*buf);
		++buf;
	}
#endif
}

#endif /* DEBUG */

/*
 * Init
 */
void
diag_init(void)
{

#ifdef CONFIG_DIAG_SERIAL
	/*
	 * No init needed...
	 * Usart already has been initialized
	 * by bootloader
	 */
#endif
}
