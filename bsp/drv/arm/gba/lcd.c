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
 * lcd.c - GBA LCD video driver
 */

#include <driver.h>
#include <wscons.h>
#include "lcd.h"
#include "font.h"

/* #define DEBUG_LCD 1 */

#ifdef DEBUG_LCD
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

struct lcd_softc {
	device_t	dev;
	uint16_t	*vram;
};

static int	lcd_init(struct driver *);
static void	lcd_cursor(void*, int, int);
static void	lcd_putc(void *, int, int, int);
static void	lcd_copyrows(void *,int, int, int);
static void	lcd_eraserows(void *,int, int);
static void	lcd_set_attr(void *, int);
static void	lcd_get_cursor(void *, int *, int *);

static struct devops lcd_devops = {
	/* open */	no_open,
	/* close */	no_close,
	/* read */	no_read,
	/* write */	no_write,
	/* ioctl */	no_ioctl,
	/* devctl */	no_devctl,
};

struct driver lcd_driver = {
	/* name */	"lcd",
	/* devops */	&lcd_devops,
	/* devsz */	sizeof(struct lcd_softc),
	/* flags */	0,
	/* probe */	NULL,
	/* init */	lcd_init,
	/* shutdown */	NULL,
};

static struct wscons_video_ops wscons_lcd_ops = {
	lcd_cursor,		/* cursor */
	lcd_putc,		/* putc */
	lcd_copyrows,		/* copyrows */
	lcd_eraserows,		/* eraserows */
	lcd_set_attr,		/* set_attr */
	lcd_get_cursor,		/* get_cursor */
};

static void
lcd_cursor(void *aux, int row, int col)
{

	/* DO NOTHING */
}

static void
lcd_putc(void *aux, int row, int col, int ch)
{
	struct lcd_softc *sc = aux;

	sc->vram[row * VSCR_COLS + col] = ch;
}

static void
lcd_copyrows(void *aux, int srcrow, int dstrow, int nrows)
{
	struct lcd_softc *sc = aux;
	int i;

	for (i = 0; i < nrows * VSCR_COLS; i++) {
		sc->vram[dstrow * VSCR_COLS + i] =
			sc->vram[srcrow * VSCR_COLS + i];
	}
}

static void
lcd_eraserows(void *aux, int row, int nrows)
{
	struct lcd_softc *sc = aux;
	int i, start, end;

	start = row * VSCR_COLS;
	end = start + nrows * VSCR_COLS;

	for (i = start; i < end; i++)
		sc->vram[i] = ' ';
}

static void
lcd_set_attr(void *aux, int attr)
{

	/* DO NOTHING */
}

static void
lcd_get_cursor(void *aux, int *col, int *row)
{

	*col = 0;
	*row = 0;
}

static void
lcd_init_font(void)
{
	int i, row, col, bit, val = 0;
	uint16_t *tile = CONSOLE_TILE;

	for (i = 0; i < 128; i++) {
		for (row = 0; row < 8; row++) {
			for (col = 7; col >= 0; col--) {
				bit = (font_bitmap[i][row] & (1 << col)) ? 2 : 1;
				if (col % 2)
					val = bit;
				else
					tile[(i * 32) + (row * 4) + ((7 - col) / 2)] =
						val + (bit << 8);
			}
		}
	}
}

static void
lcd_init_screen(void)
{
	uint16_t *pal = BG_PALETTE;

	/* Initialize palette */
	pal[0] = 0;		/* Transparent */
	pal[1] = RGB(0,0,0);	/* Black */
	pal[2] = RGB(31,31,31);	/* White */

	/* Setup lcd */
	REG_BG3CNT = 0x1080;	/* Size0, 256color, priority0 */
	REG_DISPCNT = 0x0800;	/* Mode0, BG3 */
}

static int
lcd_init(struct driver *self)
{
	device_t dev;
	struct lcd_softc *sc;

	dev = device_create(self, "lcd", D_CHR|D_TTY);

	sc = device_private(dev);
	sc->vram = CONSOLE_MAP;

	lcd_init_font();
	lcd_init_screen();

	wscons_attach_video(&wscons_lcd_ops, sc);
	return 0;
}
