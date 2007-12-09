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
 * diag.c - diagnostic message support
 */

/*
 * Piority for output device:
 *
 * 1. Bochs debug port (if it detects)
 * 2. Screen
 */

#include <kernel.h>
#include <page.h>
#include "../i386/cpu.h"

#ifdef DEBUG

typedef void (*print_func)(char);

static print_func print_handler;	/* Pointer to print handler */

#ifdef CONFIG_DIAG_SCREEN
/*
 * Screen
 */

#define VID_ATTR	0x0F00
#define VID_PORT	0x03d4
#define VID_RAM		0xB8000
#define SCR_WIDTH	80
#define SCR_HIGHT	25

/* Screen info */
static short *vram;
static int pos_x;
static int pos_y;

static void screen_scroll_up(void)
{
	int i;

	memcpy(vram, vram + SCR_WIDTH, SCR_WIDTH * (SCR_HIGHT - 1) * 2);
	for (i = 0; i < SCR_WIDTH; i++)
		vram[SCR_WIDTH * (SCR_HIGHT - 1) + i] = ' ';
}

static void screen_move_cursor(void)
{
	int pos = pos_y * SCR_WIDTH + pos_x;

	outb(0x0e, VID_PORT);
	outb(pos >> 8, VID_PORT + 1);

	outb(0x0f, VID_PORT);
	outb(pos & 0xff, VID_PORT + 1);
}

static void screen_new_line(void)
{
	pos_x = 0;
	pos_y++;
	if (pos_y >= SCR_HIGHT) {
		pos_y = SCR_HIGHT - 1;
		screen_scroll_up();
	}
	screen_move_cursor();
}

static void screen_putchar(char ch)
{
	switch (ch) {
	case '\n':
		screen_new_line();
		return;
	case '\r':
		pos_x = 0;
		screen_move_cursor();
		return;
	case '\b':
		if (pos_x == 0)
			return;
		pos_x--;
		screen_move_cursor();
		return;
	}
	vram[pos_y * SCR_WIDTH + pos_x] = ch | VID_ATTR;
	pos_x++;
	if (pos_x >= SCR_WIDTH) {
		pos_x = 0;
		pos_y++;
		if (pos_y >= SCR_HIGHT) {
			pos_y = SCR_HIGHT - 1;
			screen_scroll_up();
		}
	}
	screen_move_cursor();
}

static int screen_init(void)
{
	vram = (short *)phys_to_virt(VID_RAM);
	pos_x = 0;
	pos_y = 0;
	return 0;
}
#endif /* CONFIG_DIAG_SCREEN */


#ifdef CONFIG_DIAG_BOCHS
/*
 * Bochs PC emulator
 */

static void bochs_putchar(char buf)
{
	outb(buf, 0xe9);
}

static int bochs_init(void)
{
	if (inb(0xe9) != 0xe9)
		return -1;
	return 0;
}
#endif /* CONFIG_DIAG_BOCHS */


void diag_print(char *buf)
{
	if (print_handler == NULL)
		return;

	while (*buf)
		print_handler(*buf++);
}
#endif	/* DEBUG */

void diag_init(void)
{
#ifdef DEBUG
	print_handler = NULL;
#ifdef CONFIG_DIAG_BOCHS
	if (bochs_init() == 0) {
		print_handler = bochs_putchar;
		return;
	}
#endif
#ifdef CONFIG_DIAG_SCREEN
	screen_init();
	print_handler = screen_putchar;
#endif
#endif	/* DEBUG */
}
