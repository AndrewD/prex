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
 * ns16550.c - NS16550 serial driver
 */

#include <driver.h>
#include <tty.h>
#include <serial.h>

/* #define DEBUG_NS16550 1 */

#ifdef DEBUG_NS16550
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

#define COM_BASE	CONFIG_NS16550_BASE
#define COM_IRQ		CONFIG_NS16550_IRQ

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

/* Interrupt enable register */
#define	IER_RDA		0x01	/* enable receive data available */
#define	IER_THRE	0x02	/* enable transmitter holding register empty */
#define	IER_RLS		0x04	/* enable recieve line status */
#define	IER_RMS		0x08	/* enable receive modem status */

/* Interrupt identification register */
#define	IIR_MSR		0x00	/* modem status change */
#define	IIR_IP		0x01	/* 0 when interrupt pending */
#define	IIR_TXB		0x02	/* transmitter holding register empty */
#define	IIR_RXB		0x04	/* received data available */
#define	IIR_LSR		0x06	/* line status change */
#define	IIR_MASK	0x07	/* mask off just the meaningful bits */

/* line status register */
#define	LSR_RCV_FIFO	0x80
#define	LSR_TSRE	0x40	/* Transmitter empty: byte sent */
#define	LSR_TXRDY	0x20	/* Transmitter buffer empty */
#define	LSR_BI		0x10	/* Break detected */
#define	LSR_FE		0x08	/* Framing error: bad stop bit */
#define	LSR_PE		0x04	/* Parity error */
#define	LSR_OE		0x02	/* Overrun, lost incoming byte */
#define	LSR_RXRDY	0x01	/* Byte ready in Receive Buffer */
#define	LSR_RCV_MASK	0x1f	/* Mask for incoming data or error */

/* Forward functions */
static int	ns16550_probe(struct driver *);
static int	ns16550_init(struct driver *);

static void	ns16550_xmt_char(struct serial_port *, char);
static char	ns16550_rcv_char(struct serial_port *);
static void	ns16550_set_poll(struct serial_port *, int);
static void	ns16550_start(struct serial_port *);
static void	ns16550_stop(struct serial_port *);


struct driver ns16550_driver = {
	/* name */	"ns16550",
	/* devops */	NULL,
	/* devsz */	0,
	/* flags */	0,
	/* probe */	ns16550_probe,
	/* init */	ns16550_init,
	/* unload */	NULL,
};

static struct serial_ops ns16550_ops = {
	/* xmt_char */	ns16550_xmt_char,
	/* rcv_char */	ns16550_rcv_char,
	/* set_poll */	ns16550_set_poll,
	/* start */	ns16550_start,
	/* stop */	ns16550_stop,
};


static struct serial_port ns16550_port;


static void
ns16550_xmt_char(struct serial_port *sp, char c)
{

	while (!(bus_read_8(COM_LSR) & LSR_TXRDY))
		;
	bus_write_8(COM_THR, c);
}

static char
ns16550_rcv_char(struct serial_port *sp)
{

	while (!(bus_read_8(COM_LSR) & LSR_RXRDY))
		;
	return bus_read_8(COM_RBR);
}

static void
ns16550_set_poll(struct serial_port *sp, int on)
{

	if (on) {
		/* Disable interrupt for polling mode. */
		bus_write_8(COM_IER, 0x00);
	} else {
		/* enable interrupt again */
		bus_write_8(COM_IER, IER_RDA|IER_THRE|IER_RLS);
	}
}

static int
ns16550_isr(void *arg)
{
	struct serial_port *sp = arg;

	switch (bus_read_8(COM_IIR) & IIR_MASK) {
	case IIR_MSR:		/* Modem status change */
		break;
	case IIR_LSR:		/* Line status change */
		bus_read_8(COM_LSR);
		break;
	case IIR_TXB:		/* Transmitter holding register empty */
		serial_xmt_done(sp);
		break;
	case IIR_RXB:		/* Received data available */
		bus_read_8(COM_LSR);
		serial_rcv_char(sp, bus_read_8(COM_RBR));
		break;
	}
	return 0;
}

static void
ns16550_start(struct serial_port *sp)
{
	int s;

	bus_write_8(COM_IER, 0x00);	/* Disable interrupt */
	bus_write_8(COM_LCR, 0x80);	/* Access baud rate */
	bus_write_8(COM_DLL, 0x01);	/* 115200 baud */
	bus_write_8(COM_DLM, 0x00);
	bus_write_8(COM_LCR, 0x03);	/* N, 8, 1 */
	bus_write_8(COM_FCR, 0x06);	/* Disable & clear FIFO */

	sp->irq = irq_attach(COM_IRQ, IPL_COMM, 0, ns16550_isr,
			     IST_NONE, sp);

	s = splhigh();
	bus_write_8(COM_MCR, 0x0b);  /* Enable OUT2 interrupt */
	bus_write_8(COM_IER, IER_RDA|IER_THRE|IER_RLS);  /* Enable interrupt */
	bus_read_8(COM_IIR);
	splx(s);
}

static void
ns16550_stop(struct serial_port *sp)
{

	/* Disable all interrupts */
	bus_write_8(COM_IER, 0x00);
}


static int
ns16550_probe(struct driver *self)
{

	if (bus_read_8(COM_LSR) == 0xff)
		return ENXIO;	/* Port is disabled */
	return 0;
}

static int
ns16550_init(struct driver *self)
{

	serial_attach(&ns16550_ops, &ns16550_port);
	return 0;
}
