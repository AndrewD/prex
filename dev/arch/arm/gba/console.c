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
 * console.c - GBA console driver
 */
#include <driver.h>
#include <string.h>
#include "lcd.h"
#include "font.h"

static int console_init();
static int console_write();

#ifdef CONFIG_CONSOLE
/*
 * Driver structure
 */
struct driver console_drv __driver_entry = {
	/* name */ "GBA Console",
	/* order */ 10,
	/* init */  console_init,
};

static struct devio console_io = {
	/* open */  NULL,
	/* close */ NULL,
	/* read */  NULL,
	/* write */ console_write,
	/* ioctl */ NULL,
	/* event */ NULL,
};
#endif

static device_t console_dev;
static u_int16_t *vram = CONSOLE_MAP;
static int pos_x;
static int pos_y;
static u_short attrib;

static int esc_index;
static int esc_arg1;
static int esc_arg2;
static int esc_argc;
static int esc_saved_x;
static int esc_saved_y;

static u_short ansi_colors[] = {0, 4, 2, 6, 1, 5, 3, 7};

static void scroll_up(void)
{
	int i;

	for (i = 0; i < VSCR_WIDTH * (SCR_HIGHT - 1); i++)
		vram[i] = vram[i + VSCR_WIDTH];
	for (i = 0; i < VSCR_WIDTH; i++)
		vram[VSCR_WIDTH * (SCR_HIGHT - 1) + i] = ' ';
}

static void move_cursor(void)
{
}


static void new_line(void)
{
	pos_x = 0;
	pos_y++;
	if (pos_y >= SCR_HIGHT) {
		pos_y = SCR_HIGHT - 1;
		scroll_up();
	}
}

static void clear_screen(void)
{
	int i;

	for (i = 0; i < VSCR_WIDTH * SCR_HIGHT; i++)
		vram[i] = ' ';

	pos_x = 0;
	pos_y = 0;
	move_cursor();
}

/*
 * Check for escape code sequence.
 * Return true if escape
 *
 * <Support list>
 *  ESC[#;#H or	: moves cursor to line #, column #
 *  ESC[#;#f
 *  ESC[#A	: moves cursor up # lines
 *  ESC[#B	: moves cursor down # lines
 *  ESC[#C	: moves cursor right # spaces
 *  ESC[#D	: moves cursor left # spaces
 *  ESC[#;#R	: reports current cursor line & column
 *  ESC[s	: save cursor position for recall later
 *  ESC[u	: return to saved cursor position
 *  ESC[2J	: clear screen and home cursor
 *  ESC[K	: clear to end of line
 *
 * <Not support>
 *  ESC[#m	: attribute (0=attribure off, 4=underline, 5=blink)
 */
static int check_escape(char ch)
{
	int move = 0;
	int val;
	u_short color;

	if (ch == 033) {
		esc_index = 1;
		esc_argc = 0;
		return 1;
	}
	if (esc_index == 0)
		return 0;

	if (ch >= '0' && ch <= '9') {
		val = ch - '0';
		switch (esc_argc) {
		case 0:
			esc_arg1 = val;
			esc_index++;
			break;
		case 1:
			esc_arg1 = esc_arg1 * 10 + val;
			break;
		case 2:
			esc_arg2 = val;
			esc_index++;
			break;
		case 3:
			esc_arg2 = esc_arg2 * 10 + val;
			break;
		default:
			goto reset;
		}
		esc_argc++;
		return 1;
	}

	esc_index++;

	switch (esc_index) {
        case 2:
		if (ch != '[')
			goto reset;
		return 1;
	case 3:
		switch (ch) {
		case 's':	/* Save cursor position */
			esc_saved_x = pos_x;
			esc_saved_y = pos_y;
			break;
		case 'u':	/* Return to saved cursor position */
			pos_x = esc_saved_x;
			pos_y = esc_saved_y;
			move_cursor();
			break;
		case 'K':	/* Clear to end of line */
			break;
		}
		goto reset;
	case 4:
		switch (ch) {
		case 'A':	/* Move cursor up # lines */
			pos_y -= esc_arg1;
			if (pos_y < 0)
				pos_y = 0;
			move = 1;
			break;
		case 'B':	/* Move cursor down # lines */
			pos_y += esc_arg1;
			if (pos_y >= SCR_HIGHT)
				pos_y = SCR_HIGHT - 1;
			move = 1;
			break;
		case 'C':	/* Move cursor forward # spaces */
			pos_x += esc_arg1;
			if (pos_x >= SCR_WIDTH)
				pos_x = SCR_WIDTH - 1;
			move = 1;
			break;
		case 'D':	/* Move cursor back # spaces */
			pos_x -= esc_arg1;
			if (pos_x < 0)
				pos_x = 0;
			move = 1;
			break;
		case ';':
			return 1;
		case 'J':
			if (esc_arg1 == 2)	/* Clear screen */
				clear_screen();
			break;
		case 'm':	/* Change attribute */
			switch (esc_arg1) {
			case 0:		/* reset */
				attrib = 0x0F;
				break;
			case 1:		/* bold */
				attrib = 0x0F;
				break;
			case 4:		/* under line */
				break;
			case 5:		/* blink */
				attrib |= 0x80;
				break;
			case 30: case 31: case 32: case 33:
			case 34: case 35: case 36: case 37:
				color = ansi_colors[esc_arg1 - 30];
				attrib = (attrib & 0xf0) | color;
				break;
			case 40: case 41: case 42: case 43:
			case 44: case 45: case 46: case 47:
				color = ansi_colors[esc_arg1 - 40];
				attrib = (attrib & 0x0f) | (color << 4);
				break;
			}
			break;

		}
		if (move)
			move_cursor();
		goto reset;
	case 6:
		switch (ch) {
		case 'H':
		case 'f':
			pos_y = esc_arg1;
			pos_x = esc_arg2;
			if (pos_y >= SCR_HIGHT)
				pos_y = SCR_HIGHT - 1;
			if (pos_x >= SCR_WIDTH)
				pos_x = SCR_WIDTH - 1;
			move_cursor();
			break;
		case 'R':
			/* XXX */
			break;
		}
		goto reset;
	default:
		goto reset;
	}
	return 1;
 reset:
	esc_index = 0;
	esc_argc = 0;
	return 1;
}

void put_char(char ch)
{
	if (check_escape(ch))
		return;

	switch (ch) {
	case '\n':
		new_line();
		return;
	case '\r':
		pos_x = 0;
		return;
	case '\b':
		if (pos_x == 0)
			return;
		pos_x--;
		return;
	}

	vram[pos_y * VSCR_WIDTH + pos_x] = ch;
	pos_x++;
	if (pos_x >= SCR_WIDTH) {
		pos_x = 0;
		pos_y++;
		if (pos_y >= SCR_HIGHT) {
			pos_y = SCR_HIGHT - 1;
			scroll_up();
		}
	}
}

/*
 * Debug print handler
 */
void console_print(char *str)
{
	size_t size = 128;

	console_write(0, str, &size, 0);
}

/*
 * Write
 */
int console_write(device_t dev, char *buf, u_long *nbyte, int blkno)
{
	u_long count;
	char ch;

	for (count = 0; count < *nbyte; count++) {
		ch = *buf;
		if (ch == '\0')
			break;
		put_char(ch);
		buf++;
	}
	move_cursor();
	*nbyte = count;
	esc_index = 0;
	return 0;
}

/*
 * Init font
 */
void init_font(void)
{
	int i, row, col, bit, val = 0;
	u_int16_t *tile = CONSOLE_TILE;

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


/*
 * Init screen
 */
void init_screen(void)
{
	u_int16_t *pal = BG_PALETTE;

	/* Initialize palette */
	pal[1] = RGB(0,0,0);	/* Black */
	pal[2] = RGB(31,31,31);	/* White */

	/* Setup video */
	REG_BG3CNT = 0x1080;	/* Size0, 256color, priority0 */
	REG_DISPCNT = 0x0800;	/* Mode0, BG3 */
}

/*
 * Init
 */
int console_init(void)
{
	esc_index = 0;
	pos_x = 0;
	pos_y = 19;
	console_dev = device_create(&console_io, "console");
	init_font();
	init_screen();
	/* debug_attach(console_print); */
	return 0;
}
