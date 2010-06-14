/*-
 * Copyright (c) 2008-2009, Kohsuke Ohtani
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
 * pl011.c - ARM PrimeCell PL011 UART
 */

#include <driver.h>
#include <tty.h>
#include <serial.h>

/* #define DEBUG_PL011 1 */

#ifdef DEBUG_PL011
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

#define UART_BASE	CONFIG_PL011_BASE
#define UART_IRQ	CONFIG_PL011_IRQ
#define UART_CLK	14745600
#define BAUD_RATE	115200

/* UART Registers */
#define UART_DR		(UART_BASE + 0x00)
#define UART_RSR	(UART_BASE + 0x04)
#define UART_ECR	(UART_BASE + 0x04)
#define UART_FR		(UART_BASE + 0x18)
#define UART_IBRD	(UART_BASE + 0x24)
#define UART_FBRD	(UART_BASE + 0x28)
#define UART_LCRH	(UART_BASE + 0x2c)
#define UART_CR		(UART_BASE + 0x30)
#define UART_IMSC	(UART_BASE + 0x38)
#define UART_MIS	(UART_BASE + 0x40)
#define UART_ICR	(UART_BASE + 0x44)

/* Flag register */
#define FR_RXFE		0x10	/* Receive FIFO empty */
#define FR_TXFF		0x20	/* Transmit FIFO full */

/* Masked interrupt status register */
#define MIS_RX		0x10	/* Receive interrupt */
#define MIS_TX		0x20	/* Transmit interrupt */

/* Interrupt clear register */
#define ICR_RX		0x10	/* Clear receive interrupt */
#define ICR_TX		0x20	/* Clear transmit interrupt */

/* Line control register (High) */
#define LCRH_WLEN8	0x60	/* 8 bits */
#define LCRH_FEN	0x10	/* Enable FIFO */

/* Control register */
#define CR_UARTEN	0x0001	/* UART enable */
#define CR_TXE		0x0100	/* Transmit enable */
#define CR_RXE		0x0200	/* Receive enable */

/* Interrupt mask set/clear register */
#define IMSC_RX		0x10	/* Receive interrupt mask */
#define IMSC_TX		0x20	/* Transmit interrupt mask */

/* Forward functions */
static int	pl011_init(struct driver *);
static void	pl011_xmt_char(struct serial_port *, char);
static char	pl011_rcv_char(struct serial_port *);
static void	pl011_set_poll(struct serial_port *, int);
static void	pl011_start(struct serial_port *);
static void	pl011_stop(struct serial_port *);


struct driver pl011_driver = {
	/* name */	"pl011",
	/* devops */	NULL,
	/* devsz */	0,
	/* flags */	0,
	/* probe */	NULL,
	/* init */	pl011_init,
	/* detach */	NULL,
};

static struct serial_ops pl011_ops = {
	/* xmt_char */	pl011_xmt_char,
	/* rcv_char */	pl011_rcv_char,
	/* set_poll */	pl011_set_poll,
	/* start */	pl011_start,
	/* stop */	pl011_stop,
};

static struct serial_port pl011_port;


static void
pl011_xmt_char(struct serial_port *sp, char c)
{

	while (bus_read_32(UART_FR) & FR_TXFF)
		;
	bus_write_32(UART_DR, (uint32_t)c);
}

static char
pl011_rcv_char(struct serial_port *sp)
{
	char c;

	while (bus_read_32(UART_FR) & FR_RXFE)
		;
	c = bus_read_32(UART_DR) & 0xff;
	return c;
}

static void
pl011_set_poll(struct serial_port *sp, int on)
{

	if (on) {
		/*
		 * Disable interrupt for polling mode.
		 */
		bus_write_32(UART_IMSC, 0);
	} else
		bus_write_32(UART_IMSC, (IMSC_RX | IMSC_TX));
}

static int
pl011_isr(void *arg)
{
	struct serial_port *sp = arg;
	int c;
	uint32_t mis;

	mis = bus_read_32(UART_MIS);

	if (mis & MIS_RX) {
		/*
		 * Receive interrupt
		 */
		while (bus_read_32(UART_FR) & FR_RXFE)
			;
		do {
			c = bus_read_32(UART_DR);
			serial_rcv_char(sp, c);
		} while ((bus_read_32(UART_FR) & FR_RXFE) == 0);

		/* Clear interrupt status */
		bus_write_32(UART_ICR, ICR_RX);
	}
	if (mis & MIS_TX) {
		/*
		 * Transmit interrupt
		 */
		serial_xmt_done(sp);

		/* Clear interrupt status */
		bus_write_32(UART_ICR, ICR_TX);
	}
	return 0;
}

static void
pl011_start(struct serial_port *sp)
{
	uint32_t divider, remainder, fraction;

	bus_write_32(UART_CR, 0);	/* Disable everything */
	bus_write_32(UART_ICR, 0x07ff);	/* Clear all interrupt status */

	/*
	 * Set baud rate:
	 * IBRD = UART_CLK / (16 * BAUD_RATE)
	 * FBRD = ROUND((64 * MOD(UART_CLK,(16 * BAUD_RATE))) / (16 * BAUD_RATE))
	 */
	divider = UART_CLK / (16 * BAUD_RATE);
	remainder = UART_CLK % (16 * BAUD_RATE);
	fraction = (8 * remainder / BAUD_RATE) >> 1;
	fraction += (8 * remainder / BAUD_RATE) & 1;
	bus_write_32(UART_IBRD, divider);
	bus_write_32(UART_FBRD, fraction);

	/* Set N, 8, 1, FIFO enable */
	bus_write_32(UART_LCRH, (LCRH_WLEN8 | LCRH_FEN));

	/* Enable UART */
	bus_write_32(UART_CR, (CR_RXE | CR_TXE | CR_UARTEN));

	/* Install interrupt handler */
	sp->irq = irq_attach(UART_IRQ, IPL_COMM, 0, pl011_isr, IST_NONE, sp);

	/* Enable TX/RX interrupt */
	bus_write_32(UART_IMSC, (IMSC_RX | IMSC_TX));
}

static void
pl011_stop(struct serial_port *sp)
{

	bus_write_32(UART_IMSC, 0);	/* Disable all interrupts */
	bus_write_32(UART_CR, 0);	/* Disable everything */
}

static int
pl011_init(struct driver *self)
{

	serial_attach(&pl011_ops, &pl011_port);
	return 0;
}
