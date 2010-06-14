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
 * keypad.c - GBA gamepad driver
 */

#include <driver.h>
#include <sys/keycode.h>
#include "swkbd.h"

/* Parameters */
#define KEYPAD_IRQ	12

/* Registers for gamepad control */
#define REG_KEYSTS	(*(volatile uint16_t *)0x4000130)
#define REG_KEYCNT	(*(volatile uint16_t *)0x4000132)

/* KEY_STS/KEY_CNT */
#define KEY_A		0x0001
#define KEY_B		0x0002
#define KEY_SELECT	0x0004
#define KEY_START	0x0008
#define KEY_RIGHT	0x0010
#define KEY_LEFT	0x0020
#define KEY_UP		0x0040
#define KEY_DOWN	0x0080
#define KEY_R		0x0100
#define KEY_L		0x0200

#define KEY_ALL		0x03ff

/* KEY_CNT value */
#define KEYIRQ_EN	0x4000	/* 0=Disable, 1=Enable */
#define KEYIRQ_COND	0x8000  /* 0=Logical OR, 1=Logical AND */


struct keypad_softc {
	device_t	dev;
	irq_t		irq;
};

static int keypad_init(struct driver *);

static struct devops keypad_devops = {
	/* open */	no_open,
	/* close */	no_close,
	/* read */	no_read,
	/* write */	no_write,
	/* ioctl */	no_ioctl,
	/* devctl */	no_devctl,
};

struct driver keypad_driver = {
	/* name */	"keypad",
	/* devops */	&keypad_devops,
	/* devsz */	sizeof(struct keypad_softc),
	/* flags */	0,
	/* probe */	NULL,
	/* init */	keypad_init,
	/* shutdown */	NULL,
};

/*
 * Interrupt service routine
 */
static int
keypad_isr(void *arg)
{
	uint16_t sts;

	sts = ~REG_KEYSTS & KEY_ALL;

	if (sts == (KEY_SELECT|KEY_START))
		machine_powerdown(PWR_REBOOT);

	if (sts & KEY_A)
		swkbd_input('A');
	if (sts & KEY_B)
		swkbd_input('B');
	if (sts & KEY_SELECT)
		swkbd_input('\t');
	if (sts & KEY_START)
		swkbd_input('\n');
	if (sts & KEY_RIGHT)
		swkbd_input(K_RGHT);
	if (sts & KEY_LEFT)
		swkbd_input(K_LEFT);
	if (sts & KEY_UP)
		swkbd_input(K_UP);
	if (sts & KEY_DOWN)
		swkbd_input(K_DOWN);
	if (sts & KEY_R)
		swkbd_input('R');
	if (sts & KEY_L)
		swkbd_input('L');

	return 0;
}

int
keypad_init(struct driver *self)
{
	struct keypad_softc *sc;
	device_t dev;

	dev = device_create(self, "keypad", D_CHR);
	sc = device_private(dev);

	/*
	 * Setup isr
	 */
	REG_KEYCNT = 0;	/* disable irq */
	sc->irq = irq_attach(KEYPAD_IRQ, IPL_INPUT, 0, keypad_isr,
			     IST_NONE, sc);
	REG_KEYCNT = KEY_ALL | KEYIRQ_EN;

	return 0;
}

