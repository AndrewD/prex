/*-
 * Copyright (c) 2005-2009, Kohsuke Ohtani
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
 * psaux.c - ps2 mouse support
 */

/*
 * PS/2 mouse packet
 *
 *         Bit7   Bit6   Bit5   Bit4   Bit3  Bit2   Bit1   Bit0
 *  ------ ------ ------ ------ ------ ----- ------ ------ ------
 *  Byte 1 Yovf   Xovf   Ysign  Xsign    1   MidBtn RgtBtn LftBtn
 *  Byte 2 X movement
 *  Byte 3 Y movement
 */

#include <driver.h>

#include "i8042.h"

/* #define DEBUG_MOUSE 1 */

#ifdef DEBUG_MOUSE
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

#define MOUSE_IRQ	12

struct psaux_softc {
	device_t	dev;		/* device object */
	irq_t		irq;		/* handle for mouse irq */
	u_char		packet[3];	/* mouse packet */
	int		index;
};

static int psaux_init(struct driver *);
static int psaux_open(device_t, int);
static int psaux_close(device_t);
static int psaux_read(device_t, char *, size_t *, int);

static struct devops psaux_devops = {
	/* open */	psaux_open,
	/* close */	psaux_close,
	/* read */	psaux_read,
	/* write */	no_write,
	/* ioctl */	no_ioctl,
	/* devctl */	no_devctl,
};

struct driver psaux_driver = {
	/* name */	"psaux",
	/* devops */	&psaux_devops,
	/* devsz */	sizeof(struct psaux_softc),
	/* flags */	0,
	/* probe */	NULL,
	/* init */	psaux_init,
	/* shutdown */	NULL,
};

/*
 * Write aux device command
 */
static void
kmc_send_auxcmd(u_char val)
{

	DPRINTF(("kmc_send_auxcmd: %x\n", val));
	kmc_wait_ibe();
	bus_write_8(KMC_CMD, 0x60);
	kmc_wait_ibe();
	bus_write_8(KMC_DATA, val);
}

/*
 * Returns 0 on success, -1 on failure.
 */
static int
kmc_write_aux(u_char val)
{
	int rc = -1;
	int s;

	DPRINTF(("kmc_write_aux: val=%x\n", val));
	s = splhigh();

	/* Write the value to the device */
	kmc_wait_ibe();
	bus_write_8(KMC_CMD, 0xd4);
	kmc_wait_ibe();
	bus_write_8(KMC_DATA, val);

	/* Get the ack */
	kmc_wait_obf();
	if ((bus_read_8(KMC_STS) & 0x20) == 0x20) {
		if (bus_read_8(KMC_DATA) == 0xfa)
			rc = 0;
	}
	splx(s);
#ifdef DEBUG_MOUSE
	if (rc)
		printf("kmc_write_aux: error val=%x\n", val);
#endif
	return rc;
}

/*
 * Interrupt handler
 */
static int
psaux_isr(void *arg)
{
	struct psaux_softc *sc = arg;
	u_char dat, id;

	if ((bus_read_8(KMC_STS) & 0x21) != 0x21)
		return 0;

	dat = bus_read_8(KMC_DATA);
	if (dat == 0xaa) {	/* BAT comp (reconnect) ? */
		DPRINTF(("BAT comp"));
		sc->index = 0;
		kmc_wait_obf();
		if ((bus_read_8(KMC_STS) & 0x20) == 0x20) {
			id = bus_read_8(KMC_DATA);
			DPRINTF(("Mouse ID=%x\n", id));
		}
		kmc_write_aux(0xf4);	/* Enable aux device */
		return 0;
	}

	sc->packet[sc->index++] = dat;
	if (sc->index < 3)
		return 0;
	sc->index = 0;
	DPRINTF(("mouse packet %x:%d:%d\n", sc->packet[0],
		 sc->packet[1], sc->packet[2]));
	return 0;
}

/*
 * Open
 */
static int
psaux_open(device_t dev, int mode)
{

	DPRINTF(("psaux_open: dev=%x\n", dev));
	return 0;
}

/*
 * Close
 */
static int
psaux_close(device_t dev)
{
	DPRINTF(("psaux_close: dev=%x\n", dev));
	return 0;
}

/*
 * Read
 */
static int
psaux_read(device_t dev, char *buf, size_t *nbyte, int blkno)
{

	return 0;
}

static int
psaux_init(struct driver *self)
{
	struct psaux_softc *sc;
	device_t dev;

#ifdef DEBUG
	printf("Mouse sampling rate=100 samples/sec\n");
#endif
	dev = device_create(self, "mouse", D_CHR);

	sc = device_private(dev);
	sc->dev = dev;
	sc->index = 0;

	/* Allocate IRQ */
	sc->irq = irq_attach(MOUSE_IRQ, IPL_INPUT, 0, psaux_isr,
			     IST_NONE, sc);

	kmc_wait_ibe();
	bus_write_8(KMC_CMD, 0xa8);	/* Enable aux */

	kmc_write_aux(0xf3);	/* Set sample rate */
	kmc_write_aux(100);	/* 100 samples/sec */

	kmc_write_aux(0xe8);	/* Set resolution */
	kmc_write_aux(3);	/* 8 counts per mm */
	kmc_write_aux(0xe7);	/* 2:1 scaling */

	kmc_write_aux(0xf4);	/* Enable aux device */
	kmc_send_auxcmd(0x47);	/* Enable controller ints */
	return 0;
}
