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
 * serial.c - machine independent driver for serial port.
 */

#include <driver.h>
#include <sys/ioctl.h>
#include <tty.h>
#include <cons.h>
#include <serial.h>

/* #define DEBUG_SERIAL 1 */

#ifdef DEBUG_SERIAL
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

struct serial_softc {
	device_t	dev;		/* device object */
	struct tty	tty;		/* tty structure */
	struct serial_port *port;	/* port setting */
	struct serial_ops  *ops;	/* h/w operation */
};

/* Forward functions */
static int	serial_init(struct driver *);
static int	serial_read(device_t, char *, size_t *, int);
static int	serial_write(device_t, char *, size_t *, int);
static int	serial_ioctl(device_t, u_long, void *);

static int	serial_cngetc(device_t);
static void	serial_cnputc(device_t, int);
static void	serial_cnpollc(device_t, int);


static struct devops serial_devops = {
	/* open */	no_open,
	/* close */	no_close,
	/* read */	serial_read,
	/* write */	serial_write,
	/* ioctl */	serial_ioctl,
	/* devctl */	no_devctl,
};

struct driver serial_driver = {
	/* name */	"serial",
	/* devops */	&serial_devops,
	/* devsz */	sizeof(struct serial_softc),
	/* flags */	0,
	/* probe */	NULL,
	/* init */	serial_init,
	/* unload */	NULL,
};

static struct consdev serial_consdev = {
	/* dev */	NODEV,
	/* devops */	&serial_devops,
	/* cngetc */	serial_cngetc,
	/* cnputc */	serial_cnputc,
	/* cnpollc */	serial_cnpollc,
};


static int
serial_read(device_t dev, char *buf, size_t *nbyte, int blkno)
{
	struct serial_softc *sc = device_private(dev);

	return tty_read(&sc->tty, buf, nbyte);
}

static int
serial_write(device_t dev, char *buf, size_t *nbyte, int blkno)
{
	struct serial_softc *sc = device_private(dev);

	return tty_write(&sc->tty, buf, nbyte);
}

static int
serial_ioctl(device_t dev, u_long cmd, void *arg)
{
	struct serial_softc *sc = device_private(dev);

	return tty_ioctl(&sc->tty, cmd, arg);
}

/*
 * Start TTY output operation.
 */
static void
serial_start(struct tty *tp)
{
	struct serial_softc *sc = device_private(tp->t_dev);
	struct serial_port *port = sc->port;
	int c;

	while ((c = tty_getc(&tp->t_outq)) >= 0)
		sc->ops->xmt_char(port, c);
}

/*
 * Output completed.
 */
void
serial_xmt_done(struct serial_port *port)
{

	tty_done(port->tty);
}

/*
 * Character input.
 */
void
serial_rcv_char(struct serial_port *port, char c)
{

	tty_input(c, port->tty);
}

static int
serial_cngetc(device_t dev)
{
	struct serial_softc *sc = device_private(dev);
	struct serial_port *port = sc->port;

	return sc->ops->rcv_char(port);
}

static void
serial_cnputc(device_t dev, int c)
{
	struct serial_softc *sc = device_private(dev);
	struct serial_port *port = sc->port;

	sc->ops->xmt_char(port, c);
}

static void
serial_cnpollc(device_t dev, int on)
{
	struct serial_softc *sc = device_private(dev);
	struct serial_port *port = sc->port;

	sc->ops->set_poll(port, on);
}

void
serial_attach(struct serial_ops *ops, struct serial_port *port)
{
	struct serial_softc *sc;
	device_t dev;
	int diag = 0;

	dev = device_create(&serial_driver, "tty", D_CHR|D_TTY);

	sc = device_private(dev);
	sc->dev = dev;
	sc->ops = ops;
	sc->port = port;

	tty_attach(&sc->tty);
	sc->tty.t_dev = dev;
	sc->tty.t_oproc = serial_start;

	/* Start device */
	port->tty = &sc->tty;
	ops->start(port);

#ifdef CONFIG_DIAG_SERIAL
	diag = 1;
#endif
	serial_consdev.dev = dev;
	cons_attach(&serial_consdev, diag);
}

static int
serial_init(struct driver *self)
{

	return 0;
}
