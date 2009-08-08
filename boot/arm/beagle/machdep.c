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

#include <boot.h>

#define L4_Per		0x49000000
#define L4_UART3	(L4_Per  + 0x20000)
#define UART_BASE	L4_UART3
#define UART_THR	(*(volatile uint32_t *)(UART_BASE + 0x00))
#define UART_DLL	(*(volatile uint32_t *)(UART_BASE + 0x00))
#define UART_IER	(*(volatile uint32_t *)(UART_BASE + 0x04))
#define UART_DLH	(*(volatile uint32_t *)(UART_BASE + 0x04))
#define UART_FCR	(*(volatile uint32_t *)(UART_BASE + 0x08))
#define UART_LCR	(*(volatile uint32_t *)(UART_BASE + 0x0C))
#define UART_MCR	(*(volatile uint32_t *)(UART_BASE + 0x10))
#define UART_LSR	(*(volatile uint32_t *)(UART_BASE + 0x14))
#define UART_MDR1	(*(volatile uint32_t *)(UART_BASE + 0x20))

#define THRE		0x20

#define UART_CLK	48000000
#define BAUD_RATE	115200

#define MCR_DTR		0x01
#define MCR_RTS		0x02
#define LCR_8N1		0x03
#define LCR_BKSE	0x80					/* Bank select enable        */

#define FCR_FIFO_EN	0x01					/* Fifo enable               */
#define FCR_RXSR	0x02					/* Receiver soft reset       */
#define FCR_TXSR	0x04					/* Transmitter soft reset    */

#define LCRVAL		LCR_8N1					/* 8 data, 1 stop, no parity */
#define MCRVAL		(MCR_DTR | MCR_RTS)			/* RTS/DTR                   */
#define FCRVAL		(FCR_FIFO_EN | FCR_RXSR | FCR_TXSR)	/* Clear & enable FIFOs      */

/*
 * Setup boot information.
 */
static void
bootinfo_setup(void)
{

	bootinfo->video.text_x = 80;
	bootinfo->video.text_y = 25;

	/*
	 * SDRAM - 128 MB
	 */

	bootinfo->ram[0].base = 0x80000000;
	bootinfo->ram[0].size = 0x8000000;
	bootinfo->ram[0].type = MT_USABLE;
	bootinfo->nr_rams = 1;
}

#ifdef DEBUG
#ifdef CONFIG_DIAG_SERIAL
/*
 * Put chracter to serial port
 */
static void
serial_putc(int c)
{

	while ((UART_LSR & THRE) == 0) ;
	UART_THR = c;
}

/*
 * Setup serial port
 */
static void
serial_setup(void)
{

	int baud_divisor = UART_CLK / 16 / BAUD_RATE;

	UART_IER = 0x00;
	UART_MDR1 = 0x7;
	UART_LCR = LCR_BKSE | LCRVAL;
	UART_DLL = baud_divisor & 0xff;
	UART_DLH = (baud_divisor >> 8) & 0xff;
	UART_LCR = LCRVAL;
	UART_MCR = MCRVAL;
	UART_FCR = FCRVAL;
	UART_MDR1 = 0;
}
#endif /* !CONFIG_DIAG_SERIAL */

/*
 * Print one chracter
 */
void
machine_putc(int c)
{

#ifdef CONFIG_DIAG_SERIAL
	if (c == '\n')
		serial_putc('\r');
	serial_putc(c);
#endif /* !CONFIG_DIAG_SERIAL */
}
#endif /* !DEBUG */

/*
 * Panic handler
 */
void
machine_panic(void)
{

	for (;;) ;
}

/*
 * Setup machine state
 */
void
machine_setup(void)
{

#if defined(DEBUG) && defined(CONFIG_DIAG_SERIAL)
	serial_setup();
#endif
	bootinfo_setup();
}
