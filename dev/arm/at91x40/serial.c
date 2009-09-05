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
 * serial.c - AT91x40 console driver. Works via usart0
 */

#include <driver.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <conf/config.h>
#include "pio.h"


#ifdef DEBUG_SERIAL
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

#define TERM_COLS	80
#define TERM_ROWS	25

#define UART_BASE	0xFFFD0000
#define PIO_BASE	0xFFFF0000

#define UART_IRQ	2

/* Usart pins */
#define TX_PIN		14
#define RX_PIN		15

/* UART Registers */
#define UART_CR		(*(volatile uint32_t *)(UART_BASE + 0x00))
#define UART_MR		(*(volatile uint32_t *)(UART_BASE + 0x04))
#define UART_IER	(*(volatile uint32_t *)(UART_BASE + 0x08))
#define UART_IDR	(*(volatile uint32_t *)(UART_BASE + 0x0c))
#define UART_IMR	(*(volatile uint32_t *)(UART_BASE + 0x10))
#define UART_CSR	(*(volatile uint32_t *)(UART_BASE + 0x14))
#define UART_RHR	(*(volatile uint32_t *)(UART_BASE + 0x18))
#define UART_THR	(*(volatile uint32_t *)(UART_BASE + 0x1c))
#define UART_BRGR	(*(volatile uint32_t *)(UART_BASE + 0x20))
#define UART_RTOR	(*(volatile uint32_t *)(UART_BASE + 0x24))
#define UART_TTGR	(*(volatile uint32_t *)(UART_BASE + 0x28))

/* UART_CR - control register */
#define CR_RSTRX	(1 << 2)	/* Reset tx */
#define CR_RSTTX	(1 << 3)	/* Reset rx */
#define CR_RXEN		(1 << 4)	/* rx enable */
#define CR_RXDIS	(1 << 5)	/* rx disable */
#define CR_TXEN		(1 << 6)	/* tx enable */
#define CR_TXDIS	(1 << 7)	/* tx disable */
#define CR_RSTSTA	(1 << 8)	/* reset status */

/* UART_MR - mode register */
#define MR_CLKS_CLOCK	(0 << 4)	/* mcu clock */
#define MR_CLKS_FDIV1	(1 << 4)
#define MR_CLKS_SLOW	(2 << 4)
#define MR_CLKS_EXT	(3 << 4)	/* external clock */
#define MR_CHRL_5_BITS	(0 << 6)	/* 5 bit */
#define MR_CHRL_6_BITS	(1 << 6)	/* 6 bit */
#define MR_CHRL_7_BITS	(2 << 6)	/* 7 bit */
#define MR_CHRL_8_BITS	(3 << 6)	/* 8 bit */
#define MR_PAR_EVEN	(0 << 9)	/* parity even */
#define MR_PAR_ODD	(1 << 9)	/* parity odd */
#define MR_PAR_SPACE	(2 << 9)	/* parity space */
#define MR_PAR_MARK	(3 << 9)	/* parity mark */
#define MR_PAR_NONE	(4 << 9)	/* parity none */
#define MR_NBSTOP_1_BIT		(0 << 12)	/* 1 stop bit */
#define MR_NBSTOP_15_BIT	(1 << 12)	/* 1.5 stop bit */
#define MR_NBSTOP_2_BIT		(2 << 12)	/* 2 stop bit */
#define MR_MODE9		(1 << 17)	/* 9 bit */

/* UART_IER, UART_IDR, UART_IMR, UART_CSR - interrupt registers */
#define IR_RXRDY	(1 <<  0)	/* rx ready */
#define IR_TXRDY	(1 <<  1)	/* tx ready */


/* Forward functions */
static int serial_init(void);
static int serial_read(device_t, char *, size_t *, int);
static int serial_write(device_t, char *, size_t *, int);
static int serial_ioctl(device_t, u_long, void *);

/*
 * Driver structure
 */
struct driver serial_drv = {
	/* name */	"Serial Console",
	/* order */	4,
	/* init */	serial_init,
};

/*
 * Device I/O table
 */
static struct devio serial_io = {
	/* open */	NULL,
	/* close */	NULL,
	/* read */	serial_read,
	/* write */	serial_write,
	/* ioctl */	serial_ioctl,
	/* event */	NULL,
};

static device_t serial_dev;	/* device object */
static struct tty serial_tty;	/* tty structure */
static irq_t serial_irq;	/* handle for irq */


static int
serial_read(device_t dev, char *buf, size_t *nbyte, int blkno)
{

	return tty_read(&serial_tty, buf, nbyte);
}

static int
serial_write(device_t dev, char *buf, size_t *nbyte, int blkno)
{

	return tty_write(&serial_tty, buf, nbyte);
}

static int
serial_ioctl(device_t dev, u_long cmd, void *arg)
{

	return tty_ioctl(&serial_tty, cmd, arg);
}

static void
serial_putc(char c)
{

	while (!(UART_CSR & IR_TXRDY))
		;

	UART_THR = (uint32_t)c;
}

/*
 * Start output operation.
 */
static void
serial_start(struct tty *tp)
{
	int c;

	while ((c = ttyq_getc(&tp->t_outq)) >= 0)  {
		if (c == '\n') {
			serial_putc('\r');
		}
		serial_putc(c);
	}

	/* Enable tx interrupt */
	UART_IER = IR_TXRDY;
}

/*
 * Interrupt service routine
 */
static int
serial_isr(int irq)
{
	int c;
	uint32_t status;

	status = UART_CSR;		/* Ack the interrupt */

	if (status & IR_RXRDY) {	/* Receive interrupt */
		c = UART_RHR;
		tty_input(c, &serial_tty);
	}
	if (status & IR_TXRDY) {	/* Transmit interrupt */
		UART_IDR = IR_TXRDY;	/* Disable tx interrupt */
		tty_done(&serial_tty);	/* Output is completed */
	}
	return 0;
}

#if defined(DEBUG) && defined(CONFIG_DIAG_SERIAL)
/*
 * Diag print handler
 */
static void
serial_puts(char *str)
{
	size_t count;
	char c;
	uint32_t old_int;

	irq_lock();

	/* Disable interrupts */
	old_int = UART_IMR;
	UART_IDR = IR_TXRDY | IR_RXRDY;

	for (count = 0; count < 128; count++) {
		c = *str;
		if (c == '\0')
			break;
		if (c == '\n')
			serial_putc('\r');
		serial_putc(c);
		str++;
	}

	/* Enable interrupts */
	UART_IER = old_int;
	irq_unlock();
}
#endif


/*
 * Init uart device to 8 bit, 1 stop, no parity, no flow control
 */
#ifndef CONFIG_DIAG_SERIAL
static void
init_port(void)
{
	pio_disable((1 << TX_PIN) | (1 << RX_PIN));

	UART_MR = MR_CLKS_CLOCK | MR_CHRL_8_BITS |
		MR_PAR_NONE | MR_NBSTOP_1_BIT;
	UART_RTOR = 0;
	UART_BRGR = CONFIG_MCU_FREQ / (16 * CONFIG_UART_BAUD);

	UART_CR = CR_RSTTX | CR_RSTRX | CR_RSTSTA;
	UART_CR = CR_RXEN | CR_TXEN;
}
#endif

/*
 * Init uart interrupts
 */
static void
init_int(void)
{
	serial_irq = irq_attach(UART_IRQ, IPL_COMM, 0, &serial_isr, NULL);

	UART_IER = IR_RXRDY;	/* enable RXReady interrupt */
}

/*
 * Initialize
 */
static int
serial_init(void)
{

	/* Initialize port */
#ifndef CONFIG_DIAG_SERIAL
	init_port();
#endif
	init_int();

#if defined(DEBUG) && defined(CONFIG_DIAG_SERIAL)
	debug_attach(serial_puts);
#endif

	/* Create device object */
	serial_dev = device_create(&serial_io, "console", DF_CHR);
	ASSERT(serial_dev);

	tty_attach(&serial_io, &serial_tty);

	serial_tty.t_oproc = serial_start;
	serial_tty.t_winsize.ws_row = (u_short)TERM_ROWS;
	serial_tty.t_winsize.ws_col = (u_short)TERM_COLS;

	return 0;
}
