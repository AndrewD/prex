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
 * diag.c - diagnostic message support.
 */

#include <kernel.h>
#include <sys/bootinfo.h>
#include <cpufunc.h>

#ifdef CONFIG_DIAG_SCREEN

#define VID_ATTR	0x0F00
#define VID_PORT	0x03d4
#define VID_RAM		0xB8000

/* Screen info */
static short	*vram;
static int	pos_x;
static int	pos_y;
static int	screen_x;
static int	screen_y;

static void
screen_scroll_up(void)
{
	int i;

	memcpy(vram, vram + screen_x, (size_t)(screen_x * (screen_y - 1) * 2));
	for (i = 0; i < screen_x; i++)
		vram[screen_x * (screen_y - 1) + i] = ' ';
}

static void
screen_move_cursor(void)
{
	int pos = pos_y * screen_x + pos_x;

	outb(VID_PORT, 0x0e);
	outb(VID_PORT + 1, (u_int)pos >> 8);

	outb(VID_PORT, 0x0f);
	outb(VID_PORT + 1, (u_int)pos & 0xff);
}

static void
screen_newline(void)
{

	pos_x = 0;
	pos_y++;
	if (pos_y >= screen_y) {
		pos_y = screen_y - 1;
		screen_scroll_up();
	}
}

static void
screen_putc(char c)
{

	switch (c) {
	case '\n':
		screen_newline();
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
	vram[pos_y * screen_x + pos_x] = c | VID_ATTR;
	pos_x++;
	if (pos_x >= screen_x) {
		pos_x = 0;
		pos_y++;
		if (pos_y >= screen_y) {
			pos_y = screen_y - 1;
			screen_scroll_up();
		}
	}
}

void
diag_puts(char *str)
{

	while (*str)
		screen_putc(*str++);

	screen_move_cursor();
}

void
diag_init(void)
{
	struct bootinfo *bi;

	machine_bootinfo(&bi);

	vram = ptokv(VID_RAM);
	pos_x = 0;
	pos_y = 0;
	screen_x = bi->video.text_x;
	screen_y = bi->video.text_y;
}
#endif	/* !CONFIG_DIAG_SCREEN */


#ifdef CONFIG_DIAG_BOCHS
static void
bochs_putc(char c)
{
	/* Write to the bochs debug port. */
	outb(0xe9, (u_char)c);
}

void
diag_puts(char *str)
{

	/* Skip if Bochs is not running. */
	if (inb(0xe9) != 0xe9)
		return;

	while (*str)
		bochs_putc(*str++);
}

void
diag_init(void)
{
}
#endif	/* !CONFIG_DIAG_BOCHS */


#ifdef CONFIG_DIAG_SERIAL

#define COM_BASE	CONFIG_NS16550_BASE
#define COM_THR		(COM_BASE + 0x00)	/* transmit holding register */
#define COM_LSR		(COM_BASE + 0x05)	/* line status register */

static void
serial_putc(char c)
{

	while (!(inb(COM_LSR) & 0x20))
		;
	outb(COM_THR, c);
}

void
diag_puts(char *str)
{

	while (*str) {
		if (*str == '\n')
			serial_putc('\r');
		serial_putc(*str++);
	}
}

/*
 * We assume the serial port has already been initialized by
 * the boot loader.
 */
void
diag_init(void)
{
	/* DO NOTHING */
}

#endif	/* !CONFIG_DIAG_SERIAL */
