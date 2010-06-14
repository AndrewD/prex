/*-
 * Copyright (c) 2005, Kohsuke Ohtani
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
 * swkbd.c - GBA software keyboard driver
 */

/**
 * This driver emulates a generic keyboard by using GBA keypad.
 *
 * <Key assign>
 *
 * On-screen keyboard:
 *
 *  A        : Select pointed key
 *  B        : Enter key
 *  Select   : Hide virtual keyboard
 *  Start    :
 *  Right    : Move keyboard cursor right
 *  Left     : Move keyboard cursor left
 *  Up       : Move keyboard cursor up
 *  Down     : Move keyboard cursor down
 *  Button R : Toggle shift state
 *  Button L : Toggle shift state
 *
 * No on-screen keyboard:
 *
 *  A        : A key
 *  B        : B key
 *  Select   : Show virtual keyboard
 *  Start    : Enter key
 *  Right    : Right key
 *  Left     : Left key
 *  Up       : Up key
 *  Down     : Down key
 *  Button R : R key
 *  Button L : L Key
 *
 */

#include <driver.h>
#include <sys/keycode.h>
#include <wscons.h>
#include "lcd.h"
#include "kbd_img.h"
#include "keymap.h"

/*
 * Since GBA does not kick interrupt for the button release, we have
 * to wait for a while after button is pressed. Otherwise, many key
 * events are queued by one button press.
 */
#define CURSOR_WAIT	100	/* 100 msec */
#define BUTTON_WAIT	200	/* 200 msec */

struct swkbd_softc {
	device_t	dev;		/* device object */
	irq_t		irq;		/* irq handle */
	timer_t		timer;		/* timer to prevent chattering */

	int		kbd_on;		/* 0: direct input, 1: virtual KBD */
	int		kbd_page;	/* video page to display keyboard */
	int		ignore_key;	/* ignore key input if true */
	u_int		pos_x;		/* cursor x position */
	u_int		pos_y;		/* cursor y position */
	int		cursor_type;	/* current cursor type */

	int		shift;		/* shift key state */
	int		alt;		/* alt key state */
	int		ctrl;		/* control key state */
	int		capslk;		/* caps lock key staet */
};

static int	swkbd_init(struct driver *);
static void	swkbd_move_cursor(void);

static struct devops swkbd_devops = {
	/* open */	no_open,
	/* close */	no_close,
	/* read */	no_read,
	/* write */	no_write,
	/* ioctl */	no_ioctl,
	/* devctl */	no_devctl,
};

struct driver swkbd_driver = {
	/* name */	"swkbd",
	/* devops */	&swkbd_devops,
	/* devsz */	sizeof(struct swkbd_softc),
	/* flags */	0,
	/* probe */	NULL,
	/* init */	swkbd_init,
	/* shutdown */	NULL,
};


static struct swkbd_softc *swkbd_softc;


/*
 * Select keyboard page.
 *
 *   Page0 ... Text only
 *   Page1 ... Text & Normal keyboard
 *   Page2 ... Text & Shifted keyboard
 */
static void
swkbd_select_page(int page)
{

	if (page == 0)
		REG_DISPCNT = 0x0840;	/* only BG3 */
	else if (page == 1) {
		REG_DISPCNT = 0x1A40;	/* use BG1&3 */
		swkbd_move_cursor();
	} else {
		REG_DISPCNT = 0x1C40;	/* use BG2&3 */
		swkbd_move_cursor();
	}
	swkbd_softc->kbd_page = page;
}

/*
 * Toggle keyboard type: normal or shift.
 */
static void
swkbd_toggle_shift(void)
{
	struct swkbd_softc *sc = swkbd_softc;
	int page;

	if (sc->kbd_page == 0)
		return;
	if (sc->capslk)
		page = sc->shift ? 1 : 2;
	else
		page = sc->shift ? 2 : 1;
	swkbd_select_page(page);
}

/*
 * Timer call back handler.
 * Just clear ignoring flag.
 */
static void
swkbd_timeout(void *arg)
{
	struct swkbd_softc *sc = arg;

	sc->ignore_key = 0;
}

/*
 * Move cursor to point key.
 */
static void
swkbd_move_cursor(void)
{
	struct swkbd_softc *sc = swkbd_softc;
	uint16_t *oam = OAM;
	struct _key_info *ki;
	int x, y;
	int curcur;	/* current cursor */
	int newcur;	/* new cursor */

	curcur = sc->cursor_type;

	ki = (struct _key_info *)&key_info[sc->pos_y][sc->pos_x];
	x = ki->pos_x + 108;
	y = sc->pos_y * 8 + 11;

	newcur = 0;
	switch (ki->width) {
	case 9: newcur = 0; break;
	case 11: newcur = 1; break;
	case 12: newcur = 2; break;
	case 13: newcur = 3; break;
	case 15: newcur = 4; break;
	case 17: newcur = 5; break;
	case 19: newcur = 6; break;
	case 53: newcur = 7; break;
	}
	if (newcur != curcur) {
		oam[curcur * 4] = (uint16_t)((oam[curcur * 4] & 0xff00) | 160);
		oam[curcur * 4 + 1] = (uint16_t)((oam[curcur * 4 + 1] & 0xfe00) | 240);
		sc->cursor_type = newcur;
	}
	oam[newcur * 4] = (uint16_t)((oam[newcur * 4] & 0xff00) | y);
	oam[newcur * 4 + 1] = (uint16_t)((oam[newcur * 4 + 1] & 0xfe00) | x);
}

/*
 * Process key press
 */
static void
swkbd_key_press(void)
{
	struct swkbd_softc *sc = swkbd_softc;
	struct _key_info *ki;
	u_char ac;

	ki = (struct _key_info *)&key_info[sc->pos_y][sc->pos_x];
	ac = ki->normal;

	/* Check meta key */
	switch (ac) {
	case K_SHFT:
		sc->shift = !sc->shift;
		swkbd_toggle_shift();
		return;
	case K_CTRL:
		sc->ctrl = !sc->ctrl;
		return;
	case K_ALT:
		sc->alt = !sc->alt;
		return;
	case K_CAPS:
		sc->capslk = !sc->capslk;
		swkbd_toggle_shift();
		return;
	}
	/* Check ctrl & shift state */
	if (sc->ctrl) {
		if (ac >= 'a' && ac <= 'z')
			ac = ac - 'a' + 0x01;
		else if (ac == '\\')
			ac = 0x1c;
		else
			ac = 0;
	} else if (sc->kbd_page == 2)
		ac = ki->shifted;

	if (ac == 0)
		return;

	/* Check caps lock state */
	if (sc->capslk) {
		if (ac >= 'A' && ac <= 'Z')
			ac += 'a' - 'A';
		else if (ac >= 'a' && ac <= 'z')
			ac -= 'a' - 'A';
	}

	/* Check alt key */
	if (sc->alt)
		ac |= 0x80;

	wscons_kbd_input(ac);

	/*
	 * Reset meta key status
	 */
	if (sc->shift) {
		sc->shift = 0;
		swkbd_toggle_shift();
	}
	if (sc->ctrl)
		sc->ctrl = 0;
	if (sc->alt)
		sc->alt = 0;
}

/*
 * Input handler
 * This routine will be called by gamepad ISR.
 */
void
swkbd_input(u_char c)
{
	struct swkbd_softc *sc = swkbd_softc;
	int move = 0;
	int timeout = BUTTON_WAIT;

	if (sc->ignore_key)
		return;

	/* Select key */
	if (c == '\t') {
		sc->kbd_on = !sc->kbd_on;
		swkbd_select_page(sc->kbd_on);

		/* Reset meta status */
		sc->shift = 0;
		sc->alt = 0;
		sc->ctrl = 0;
		sc->capslk = 0;
		goto out;
	}

	/* Direct input */
	if (!sc->kbd_on) {
		wscons_kbd_input(c);
		goto out;
	}

	switch (c) {
	case K_LEFT:
		if (sc->pos_x > 0) {
			if (sc->pos_y == 4 && sc->pos_x >=4 && sc->pos_x <= 8)
				sc->pos_x = 3;
			sc->pos_x--;
			move = 1;
		}
		break;
	case K_RGHT:
		if (sc->pos_x < max_x[sc->pos_y]) {
			if (sc->pos_y == 4 && sc->pos_x > 3 && sc->pos_x <= 7)
				sc->pos_x = 8;
			sc->pos_x++;
			move = 1;
		}
		break;
	case K_UP:
		if (sc->pos_y > 0 ) {
			sc->pos_y--;
			move = 1;
			if (sc->pos_x > max_x[sc->pos_y])
				sc->pos_x = max_x[sc->pos_y];
		}
		break;
	case K_DOWN:
		if (sc->pos_y < 4) {
			sc->pos_y++;
			move = 1;
			if (sc->pos_x > max_x[sc->pos_y])
				sc->pos_x = max_x[sc->pos_y];
		}
		break;
	case 'A':
		swkbd_key_press();
		break;
	case 'B':
		wscons_kbd_input('\n');
		break;
	case 'R':
	case 'L':
		sc->shift = sc->shift ? 0 : 1;
		swkbd_toggle_shift();
		break;
	}
	if (move) {
		timeout = CURSOR_WAIT;
		swkbd_move_cursor();
	}
out:
	sc->ignore_key = 1;
	timer_callout(&sc->timer, timeout, &swkbd_timeout, sc);
	return;
}

/*
 * Init keyboard image
 */
static void
swkbd_init_image(void)
{
	uint8_t bit;
	uint16_t val1, val2;
	uint16_t *pal = BG_PALETTE;
	uint16_t *tile1 = KBD1_TILE;
	uint16_t *tile2 = KBD2_TILE;
	uint16_t *map1 = KBD1_MAP;
	uint16_t *map2 = KBD2_MAP;
	int i, j, row, col;

	/* Load tiles for keyboard image */
	for (i = 0; i < 32; i++)
		tile1[i] = 0;

	for (i = 0; i < 64 * 12; i++) {
		bit = 0x01;
		for (j = 0; j < 4; j++) {
			val1 = kbd1_bitmap[i] & bit ? 0xff : 0x03;
			val2 = kbd2_bitmap[i] & bit ? 0xff : 0x03;
			bit = bit << 1;
			val1 |= kbd1_bitmap[i] & bit ? 0xff00 : 0x0300;
			val2 |= kbd2_bitmap[i] & bit ? 0xff00 : 0x0300;
			bit = bit << 1;
			tile1[i * 4 + 32 + j] = val1;
			tile2[i * 4 + j] = val2;
		}
	}


	/* Setup map */
	i = 1;
	for (row = 1; row < 7; row++) {
		for (col = 13; col < 29; col++) {
			map1[row * 32 + col] = (uint16_t)i;
			map2[row * 32 + col] = (uint16_t)(i + 127);
			i++;
		}
	}

	pal[3] = RGB(0,0,31);	/* Kbd bg color */
	pal[255] = RGB(28,28,28);	/* Kbd color */

	/* Setup video */
	REG_BG1CNT = 0x1284;	/* Size0, 256color, priority0 */
	REG_BG2CNT = 0x1484;	/* Size0, 256color, priority0 */

	swkbd_select_page(1);
}

/*
 * Initialize keyboard cursor
 */
static void
swkbd_init_cursor(void)
{
	int i, j;
	uint8_t bit;
	uint16_t val;
	uint16_t *oam = OAM;
	uint16_t *cur = CURSOR_DATA;
	uint16_t *pal = SPL_PALETTE;

	/* Move out all objects */
	for (i = 0; i < 128; i++) {
		oam[0] = 160;
		oam[1] = 240;
		oam += 4;
	}
	/* Load cursor splite */
	for (i = 0; i < 64 * 7 + 64 * 8; i++) {
		bit = 0x01;
		for (j = 0; j < 4; j++) {
			val = cursor_bitmap[i] & bit ? 0xff : 0;
			bit = bit << 1;
			val |= cursor_bitmap[i] & bit ? 0xff00 : 0;
			bit = bit << 1;
			cur[i * 4 + j] = val;
		}
	}

	/* Setup cursors */
	oam = OAM;
	for (i = 0; i < 7; i++) {
		oam[0] = (uint16_t)(0x6000 + 160); /* 256 color, Horizontal */
		oam[1] = (uint16_t)(0x8000 + 240); /* 32x16 */
		oam[2] = (uint16_t)(i * 16); /* Tile number */
		oam += 4;
	}
	/* for space key */
	oam[0] = 0x6000 + 160; /* 256 color, Horizontal */
	oam[1] = 0xC000 + 240; /* 64x32 */
	oam[2] = 112;

	pal[255] = RGB(31,0,0);	/* cursor color */
}

/*
 * Initialize
 */
static int
swkbd_init(struct driver *self)
{
	struct swkbd_softc *sc;
	device_t dev;

	dev = device_create(self, "swkbd", D_CHR);

	sc = device_private(dev);
	sc->dev = dev;
	sc->kbd_on = 1;

	swkbd_softc = sc;

	swkbd_init_cursor();
	swkbd_init_image();
	swkbd_move_cursor();

	return 0;
}
