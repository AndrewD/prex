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

/*
 * serial.c - Serial console driver for TI OMAP
 */

#include <driver.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include "platform.h"

/* #define DEBUG_SERIAL 1 */

#ifdef DEBUG_SERIAL
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

#define TERM_COLS	80
#define TERM_ROWS	25
#define FIFO_SIZE	64  /* size of the rx and tx fifo's */

#define UART_IRQ	74
#define UART_CLK	48000000
#define BAUD_RATE	115200

#define INTCPS_ILR(a)	(*(volatile uint32_t *)(MPU_INTC_BASE + 0x100 + (0x04*a)))

#define UART_THR	(*(volatile uint32_t *)(UART_BASE + 0x00))
#define UART_RHR	(*(volatile uint32_t *)(UART_BASE + 0x00))
#define UART_DLL	(*(volatile uint32_t *)(UART_BASE + 0x00))
#define UART_IER	(*(volatile uint32_t *)(UART_BASE + 0x04))
#define UART_DLH	(*(volatile uint32_t *)(UART_BASE + 0x04))
#define UART_FCR	(*(volatile uint32_t *)(UART_BASE + 0x08))
#define UART_IIR	(*(volatile uint32_t *)(UART_BASE + 0x08))
#define UART_EFR	(*(volatile uint32_t *)(UART_BASE + 0x08))
#define UART_LCR	(*(volatile uint32_t *)(UART_BASE + 0x0C))
#define UART_MCR	(*(volatile uint32_t *)(UART_BASE + 0x10))
#define UART_LSR	(*(volatile uint32_t *)(UART_BASE + 0x14))
#define UART_MDR1	(*(volatile uint32_t *)(UART_BASE + 0x20))
#define UART_SCR	(*(volatile uint32_t *)(UART_BASE + 0x40))
#define UART_SSR	(*(volatile uint32_t *)(UART_BASE + 0x44))
#define UART_SYSC	(*(volatile uint32_t *)(UART_BASE + 0x54))

/* Bit definitions for interrupt identification */
#define II_INTR		0x01
#define II_MS		0x00
#define II_TX		0x02
#define II_RX		0x04
#define II_RXTO		0x0C
#define II_LS		0x06
#define II_MASK		0x0E
#define II_FIFO		0x80

/* Bit definitions for line control */
#define LCR_BITS_MASK	0x03
#define LCR_STB2	0x04
#define LCR_PEN		0x08
#define LCR_EPS		0x10
#define LCR_SPS		0x20
#define LCR_BREAK	0x40
#define LCR_DLAB	0x80

/* Bit definitions for modem control */
#define MCR_DTR		0x01
#define MCR_RTS		0x02
#define MCR_CDSTSCH	0x08
#define MCR_LOOPBACK	0x10
#define MCR_XON		0x20
#define MCR_TCRTLR	0x40
#define MCR_CLKSEL	0x80

/* Bit definitions for line status */
#define LSR_RXRDY	0x01
#define LSR_OE		0x02
#define LSR_PE		0x04
#define LSR_FE		0x08
#define LSR_BI		0x10
#define LSR_TXRDY	0x20
#define LSR_TSRE	0x40
#define LSR_RCV_FIFO	0x80

/* Bit definitions for modem status  */
#define MSR_DCTS	0x01
#define MSR_DDSR	0x02
#define MSR_DRING	0x04
#define MSR_DDCD	0x08
#define MSR_CTS		0x10
#define MSR_DSR		0x20
#define MSR_RING	0x40
#define MSR_DCD		0x80

/* Bit definitions for interrupt enable register  */
#define IER_RHR		0x01
#define IER_THR		0x02
#define IER_LS		0x04
#define IER_MS		0x08
#define IER_SLEEP	0x10
#define IER_XOFF	0x20
#define IER_RTS		0x40
#define IER_CTS		0x80

/* Bit definitions for fifo control register  */
#define FCR_ENABLE	0x01
#define FCR_RXCLR	0x02
#define FCR_TXCLR	0x04
#define FCR_DMA		0x08

/* Bit definitions for supplementary status register  */
#define SSR_TXFULL	0x01
#define SSR_WU_STS	0x02

/* Bit definitions for enhanced feature register  */
#define EFR_ENHANCED	0x10
#define EFR_AUTO_RTS	0x40
#define EFR_AUTO_CTS	0x80

/* Mode settings for mode definition register 1  */
#define MDR1_ENABLE	0x00
#define MDR1_AUTOBAUD	0x02
#define MDR1_DISABLE	0x07

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

	while ((UART_LSR & LSR_TXRDY) == 0) ;
	UART_THR = (uint32_t)c;
}

/*
 * Start output operation.
  */
static void
serial_start(struct tty *tp)
{
	int c;

	sched_lock();
	while ((c = ttyq_getc(&tp->t_outq)) >= 0) {
		if (c == '\n')
			serial_putc('\r');
		serial_putc(c);
	}

	tty_done(&serial_tty);

	sched_unlock();

}

/*
 * Interrupt service routine
 */
static int
serial_isr(int irq)
{
	int lsr, iir, c, dummy;

	iir = UART_IIR;
	lsr = UART_LSR;

	switch(iir & II_MASK) {
		case II_LS:		  	/* Line status change */
			if( lsr & (LSR_BI|LSR_FE|LSR_PE|LSR_OE) ) {
			/*
			 * Error character
			 * Read whatever input data happens to be in the buffer to "eat" the
			 * spurious data associated with break, parity error, etc.
			 */
			dummy = UART_RHR;
			}
			/* Read LSR again... */
			dummy = UART_LSR;
			break;

		case II_RXTO:		/* Receive data timeout*/
			dummy = UART_RHR; /* "Eat" the spurious data (same as above) */
			break;

		case II_RX:			/* Receive data */
			c = UART_RHR;
			tty_input(c, &serial_tty);
			break;

		case II_TX:			/* Transmit buffer empty */
			tty_done(&serial_tty);
			break;

		default:
			break;
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

	sched_lock();
	for (count = 0; count < 128; count++) {
		c = *str;
		if (c == '\0')
			break;
		if (c == '\n')
			serial_putc('\r');
		serial_putc(c);
		str++;
	}
	sched_unlock();
}
#endif

static int
init_port(void)
{
	int baud_divisor = UART_CLK / 16 / BAUD_RATE;
	int dummy;

	UART_MDR1 = MDR1_DISABLE;
	while ((UART_LSR & LSR_TXRDY) == 0) ;	/* Wait for transmit FIFO to be empty */
	UART_IER  = 0x00;					/* Mask all interrupt */
	dummy = UART_LSR;					/* Clear LS interrupt */
	dummy = UART_RHR;					/* Clear RX interrupt */
	dummy = UART_THR;					/* Clear TX interrupt */
	UART_LCR  = LCR_DLAB;
	UART_DLL  = baud_divisor & 0xff;
	UART_DLH  = (baud_divisor >> 8) & 0xff;
	UART_LCR  = LCR_BITS_MASK;
	UART_MCR  = MCR_DTR|MCR_RTS;
	UART_FCR  = FCR_RXCLR|FCR_TXCLR;
	UART_MDR1 = MDR1_ENABLE;

	/* Install interrupt handler */
	serial_irq = irq_attach(UART_IRQ, IPL_COMM, 0, serial_isr, NULL);

	/* Enable interrupt */
	INTCPS_ILR(UART_IRQ) = ((NIPL-IPL_COMM)<<2);
	UART_IER = IER_RHR|IER_LS;

	return 0;
}

/*
 * Initialize
 */
static int
serial_init(void)
{

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

	/* Initialize port */
	init_port();

	return 0;
}
