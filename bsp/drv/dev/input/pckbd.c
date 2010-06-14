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
 * pckbd.c - PC/AT keyboard driver
 */

#include <driver.h>
#include <sys/keycode.h>
#include <wscons.h>
#include <pm.h>

#include "i8042.h"

/* Parameters */
#define KBD_IRQ		1

struct pckbd_softc {
	device_t	dev;		/* device object */
	irq_t		irq;		/* irq handle */
	int		polling;	/* true if polling mode */
	u_char		led_sts;	/* keyboard LED status */
	int		shift;		/* shift key state */
	int		alt;		/* alt key state */
	int		ctrl;		/* control key state */
	int		capslk;		/* caps lock key staet */
};

static int	pckbd_init(struct driver *);
static int	pckbd_getc(void *);
static void	pckbd_set_poll(void *, int);

struct driver pckbd_driver = {
	/* name */	"pckbd",
	/* devops */	NULL,
	/* devsz */	sizeof(struct pckbd_softc),
	/* flags */	0,
	/* probe */	NULL,
	/* init */	pckbd_init,
	/* shutdown */	NULL,
};

static struct wscons_kbd_ops wscons_pckbd_ops = {
	/* getc */	pckbd_getc,
	/* set_poll */	pckbd_set_poll,
};

/*
 * Key map
 */
static const u_char key_map[] = {
	0,      0x1b,   '1',    '2',    '3',    '4',    '5',    '6',
	'7',    '8',    '9',    '0',    '-',    '=',    '\b',   '\t',
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
	'o',    'p',    '[',    ']',    '\n',   K_CTRL, 'a',    's',
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';',
	'\'',   '`',    K_SHFT, '\\',   'z',    'x',    'c',    'v',
	'b',    'n',    'm',    ',',    '.',    '/',    K_SHFT, '*',
	K_ALT,  ' ',    K_CAPS, K_F1,   K_F2,   K_F3,   K_F4,   K_F5,
	K_F6,   K_F7,   K_F8,   K_F9,   K_F10,  0,      0,      K_HOME,
	K_UP,   K_PGUP, 0,      K_LEFT, 0,      K_RGHT, 0,      K_END,
	K_DOWN, K_PGDN, K_INS,  0x7f,   K_F11,  K_F12
};

#define KEY_MAX (sizeof(key_map) / sizeof(char))

static const u_char shift_map[] = {
	0,      0x1b,   '!',    '@',    '#',    '$',    '%',    '^',
	'&',    '*',    '(',    ')',    '_',    '+',    '\b',   '\t',
	'Q',    'W',    'E',    'R',    'T',    'Y',    'U',    'I',
	'O',    'P',    '{',    '}',    '\n',   K_CTRL, 'A',    'S',
	'D',    'F',    'G',    'H',    'J',    'K',    'L',    ':',
	'"',    '~',    0,      '|',    'Z',    'X',    'C',    'V',
	'B',    'N',    'M',    '<',    '>',    '?',    0,      '*',
	K_ALT,  ' ',    0,      0,      0,      0,      0,      0,
	0,      0,      0,      0,      0,      0,      0,      K_HOME,
	K_UP,   K_PGUP, 0,      K_LEFT, 0,      K_RGHT, 0,      K_END,
	K_DOWN, K_PGDN, K_INS,  0x7f,   0,      0
};

/*
 * Send command to keyboard controller
 */
static void
kmc_send_cmd(u_char cmd)
{

	kmc_wait_ibe();
	bus_write_8(KMC_CMD, cmd);
}

/*
 * Update LEDs for current modifier state.
 */
static void
pckbd_set_leds(struct pckbd_softc *sc)
{
	u_char val = 0;

	/* Update LEDs */
	if (sc->capslk)
		val |= 0x04;
	if (sc->led_sts != val) {
		sc->led_sts = val;
		bus_write_8(KMC_DATA, 0xed);
		while (bus_read_8(KMC_STS) & 2);
		bus_write_8(KMC_DATA, val);
		while (bus_read_8(KMC_STS) & 2);
	}
}

/*
 * Scan key input. Returns ascii code.
 */
static int
pckbd_scan_key(struct pckbd_softc *sc)
{
	u_char scan, ascii, val;
	int press;

 again:
	/* Get scan code */
	kmc_wait_obf();
	scan = bus_read_8(KMC_DATA);

	/* Send ack to the controller */
	val = bus_read_8(KMC_PORTB);
	bus_write_8(KMC_PORTB, val | 0x80);
	bus_write_8(KMC_PORTB, val);

	/* Convert scan code to ascii */
	press = scan & 0x80 ? 0 : 1;
	scan = scan & 0x7f;
	if (scan >= KEY_MAX)
		goto again;
	ascii = key_map[scan];

	/* Check meta key */
	switch (ascii) {
	case K_SHFT:
		sc->shift = press;
		return 0;
	case K_CTRL:
		sc->ctrl = press;
		return 0;
	case K_ALT:
		sc->alt = press;
		return 0;
	case K_CAPS:
		sc->capslk = !sc->capslk;
		pckbd_set_leds(sc);
		return 0;
	}

	/* Ignore key release */
	if (!press)
		return 0;

	if (ascii >= 0x80)
		return ascii;

	/* Check Alt+Ctrl+Del */
	if (sc->alt && sc->ctrl && ascii == 0x7f) {
#ifdef CONFIG_PM
		pm_set_power(PWR_REBOOT);
#else
		machine_powerdown(PWR_REBOOT);
#endif
	}

	/* Check ctrl & shift state */
	if (sc->ctrl) {
		if (ascii >= 'a' && ascii <= 'z')
			ascii = ascii - 'a' + 0x01;
		else if (ascii == '\\')
			ascii = 0x1c;
		else
			ascii = 0;
	} else if (sc->shift)
		ascii = shift_map[scan];

	if (ascii == 0)
		return 0;

	/* Check caps lock state */
	if (sc->capslk) {
		if (ascii >= 'A' && ascii <= 'Z')
			ascii += 'a' - 'A';
		else if (ascii >= 'a' && ascii <= 'z')
			ascii -= 'a' - 'A';
	}

	/* Check alt key */
	if (sc->alt)
		ascii |= 0x80;

	/* Insert key to queue */
	return ascii;
}

/*
 * Interrupt service routine
 */
static int
pckbd_isr(void *arg)
{
	struct pckbd_softc *sc = arg;
	int c;

	c = pckbd_scan_key(sc);
	if (c != 0)
		wscons_kbd_input(c);
	return 0;
}

static int
pckbd_getc(void *aux)
{
	struct pckbd_softc *sc = aux;
	int c;
	int s;

	sc->alt = 0;
	sc->ctrl = 0;
	sc->shift = 0;

	s = splhigh();
	while ((c = pckbd_scan_key(sc)) == 0) ;
	splx(s);
	return c;
}

static void
pckbd_set_poll(void *aux, int on)
{
	struct pckbd_softc *sc = aux;

	sc->polling = on;
}

static int
pckbd_init(struct driver *self)
{
	struct pckbd_softc *sc;
	device_t dev;

	dev = device_create(self, "kbd", D_CHR);

	sc = device_private(dev);
	sc->dev = dev;
	sc->polling = 0;
	sc->led_sts = 0;

	/* Disable keyboard controller */
	kmc_send_cmd(CMD_KBD_DIS);

	sc->irq = irq_attach(KBD_IRQ, IPL_INPUT, 0, pckbd_isr, IST_NONE, sc);

	/* Discard garbage data */
	while (bus_read_8(KMC_STS) & STS_OBF)
		bus_read_8(KMC_DATA);

	/* Enable keyboard controller */
	kmc_send_cmd(CMD_KBD_EN);

	wscons_attach_kbd(&wscons_pckbd_ops, sc);
	return 0;
}
