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
 * wscons.c - "Workstation console" multiplexor driver.
 */

#include <driver.h>
#include <cons.h>
#include <tty.h>
#include <wscons.h>

/* #define DEBUG_WSCONS 1 */

#ifdef DEBUG_WSCONS
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

struct esc_state {
	int		index;
	int		arg1;
	int		arg2;
	int		argc;
	int		saved_col;
	int		saved_row;
};

struct wscons_softc {
	device_t	dev;		/* our device */
	struct tty	tty;		/* associated tty */
	int		row;		/* current row */
	int		col;		/* current col */
	int		nrows;		/* number of rows */
	int		ncols;		/* number of cols */
	int		attr;		/* current attribute */
	struct esc_state esc;		/* escape state */
	struct wscons_video_ops *vid_ops; /* video operations */
	struct wscons_kbd_ops   *kbd_ops; /* keyboard operations */
	void		*vid_aux;	/* video private data */
	void		*kbd_aux;	/* keyboard private data */
};

static int	wscons_init(struct driver *);
static int	wscons_read(device_t, char *, size_t *, int);
static int	wscons_write(device_t, char *, size_t *, int);
static int	wscons_ioctl(device_t, u_long, void *);

static int	wscons_cngetc(device_t);
static void	wscons_cnputc(device_t, int);
static void	wscons_cnpollc(device_t, int);


static struct devops wscons_devops = {
	/* open */	no_open,
	/* close */	no_close,
	/* read */	wscons_read,
	/* write */	wscons_write,
	/* ioctl */	wscons_ioctl,
	/* devctl */	no_devctl,
};

struct driver wscons_driver = {
	/* name */	"wscons",
	/* devops */	&wscons_devops,
	/* devsz */	sizeof(struct wscons_softc),
	/* flags */	0,
	/* probe */	NULL,
	/* init */	wscons_init,
	/* unload */	NULL,
};

static struct consdev wsconsdev = {
	/* dev */	NODEV,
	/* devops */	&wscons_devops,
	/* cngetc */	wscons_cngetc,
	/* cnputc */	wscons_cnputc,
	/* cnpollc */	wscons_cnpollc,
};

static const u_short ansi_colors[] = {0, 4, 2, 6, 1, 5, 3, 7};

/*
 * Pointer to the wscons state. There can be only one instance.
 */
static struct wscons_softc *wscons_softc;


static int
wscons_read(device_t dev, char *buf, size_t *nbyte, int blkno)
{
	struct tty *tty = &wscons_softc->tty;

	return tty_read(tty, buf, nbyte);
}

static int
wscons_write(device_t dev, char *buf, size_t *nbyte, int blkno)
{
	struct tty *tty = &wscons_softc->tty;

	return tty_write(tty, buf, nbyte);
}

static int
wscons_ioctl(device_t dev, u_long cmd, void *arg)
{
	struct tty *tty = &wscons_softc->tty;

	return tty_ioctl(tty, cmd, arg);
}

static void
wscons_move_cursor(struct wscons_softc *sc)
{
	struct wscons_video_ops *vops = sc->vid_ops;

	vops->cursor(sc->vid_aux, sc->row, sc->col);
}

static void
wscons_set_attr(struct wscons_softc *sc)
{
	struct wscons_video_ops *vops = sc->vid_ops;

	vops->set_attr(sc->vid_aux, sc->attr);
}

static void
wscons_clear(struct wscons_softc *sc)
{
	struct wscons_video_ops *vops = sc->vid_ops;

	vops->eraserows(sc->vid_aux, 0, sc->nrows);
	sc->col = 0;
	sc->row = 0;
	wscons_move_cursor(sc);
}

static void
wscons_scrollup(struct wscons_softc *sc)
{
	struct wscons_video_ops *vops = sc->vid_ops;

	vops->copyrows(sc->vid_aux, 1, 0, sc->nrows - 1);
	vops->eraserows(sc->vid_aux, sc->nrows - 1, 1);
}

static void
wscons_newline(struct wscons_softc *sc)
{

	sc->col = 0;
	sc->row++;
	if (sc->row >= sc->nrows) {
		sc->row = sc->nrows - 1;
		wscons_scrollup(sc);
	}
}

/*
 * Check for escape code sequence.
 * Rreturn true if escape
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
 *  ESC[#m	: attribute (0=attribure off, 4=underline, 5=blink)
 */
static int
wscons_check_escape(struct wscons_softc *sc, char c)
{
	struct esc_state *esc = &sc->esc;
	int move = 0;
	int val;
	u_short color;

	if (c == 033) {
		esc->index = 1;
		esc->argc = 0;
		return 1;
	}
	if (esc->index == 0)
		return 0;

	if (c >= '0' && c <= '9') {
		val = c - '0';
		switch (esc->argc) {
		case 0:
			esc->arg1 = val;
			esc->index++;
			break;
		case 1:
			esc->arg1 = esc->arg1 * 10 + val;
			break;
		case 2:
			esc->arg2 = val;
			esc->index++;
			break;
		case 3:
			esc->arg2 = esc->arg2 * 10 + val;
			break;
		default:
			goto reset;
		}
		esc->argc++;
		return 1;
	}

	esc->index++;

	switch (esc->index) {
        case 2:
		if (c != '[')
			goto reset;
		return 1;
	case 3:
		switch (c) {
		case 's':	/* Save cursor position */
			esc->saved_col = sc->col;
			esc->saved_row = sc->row;
			printf("TTY: save %d %d\n", sc->col, sc->row);
			break;
		case 'u':	/* Return to saved cursor position */
			sc->col = esc->saved_col;
			sc->row = esc->saved_row;
			printf("TTY: restore %d %d\n", sc->col, sc->row);
			wscons_move_cursor(sc);
			break;
		case 'K':	/* Clear to end of line */
			break;
		}
		goto reset;
	case 4:
		switch (c) {
		case 'A':	/* Move cursor up # lines */
			sc->row -= esc->arg1;
			if (sc->row < 0)
				sc->row = 0;
			move = 1;
			break;
		case 'B':	/* Move cursor down # lines */
			sc->row += esc->arg1;
			if (sc->row >= sc->nrows)
				sc->row = sc->nrows - 1;
			move = 1;
			break;
		case 'C':	/* Move cursor forward # spaces */
			sc->col += esc->arg1;
			if (sc->col >= sc->ncols)
				sc->col = sc->ncols - 1;
			move = 1;
			break;
		case 'D':	/* Move cursor back # spaces */
			sc->col -= esc->arg1;
			if (sc->col < 0)
				sc->col = 0;
			move = 1;
			break;
		case ';':
			if (esc->argc == 1)
				esc->argc = 2;
			return 1;
		case 'J':
			if (esc->arg1 == 2)	/* Clear screen */
				wscons_clear(sc);
			break;
		case 'm':	/* Change attribute */
			switch (esc->arg1) {
			case 0:		/* reset */
				sc->attr = 0x0F;
				break;
			case 1:		/* bold */
				sc->attr = 0x0F;
				break;
			case 4:		/* under line */
				break;
			case 5:		/* blink */
				sc->attr |= 0x80;
				break;
			case 30: case 31: case 32: case 33:
			case 34: case 35: case 36: case 37:
				color = ansi_colors[esc->arg1 - 30];
				sc->attr = (sc->attr & 0xf0) | color;
				break;
			case 40: case 41: case 42: case 43:
			case 44: case 45: case 46: case 47:
				color = ansi_colors[esc->arg1 - 40];
				sc->attr = (sc->attr & 0x0f) | (color << 4);
				break;
			}
			wscons_set_attr(sc);
			break;

		}
		if (move)
			wscons_move_cursor(sc);
		goto reset;
	case 6:
		switch (c) {
		case 'H':	/* Cursor position */
		case 'f':
			sc->row = esc->arg1;
			sc->col = esc->arg2;
			if (sc->row >= sc->nrows)
				sc->row = sc->nrows - 1;
			if (sc->col >= sc->ncols)
				sc->col = sc->ncols - 1;
			wscons_move_cursor(sc);
			break;
		case 'R':
			/* XXX */
			break;
		}
		goto reset;
	default:
		goto reset;
	}
	/* NOTREACHED */
 reset:
	esc->index = 0;
	esc->argc = 0;
	return 1;
}

static void
wscons_putc(int c)
{
	struct wscons_softc *sc = wscons_softc;
	struct wscons_video_ops *vops = sc->vid_ops;

	if (wscons_check_escape(sc, c))
		return;

	switch (c) {
	case '\n':
		wscons_newline(sc);
		return;
	case '\r':
		sc->col = 0;
		return;
	case '\b':
		if (sc->col == 0)
			return;
		sc->col--;
		return;
	}

	vops->putc(sc->vid_aux, sc->row, sc->col, c);

	sc->col++;
	if (sc->col >= sc->ncols) {
		sc->col = 0;
		sc->row++;
		if (sc->row >= sc->nrows) {
			sc->row = sc->nrows - 1;
			wscons_scrollup(sc);
		}
	}
}

/*
 * Start output operation.
 */
static void
wscons_start(struct tty *tp)
{
	struct wscons_softc *sc = wscons_softc;
	int c;

	while ((c = tty_getc(&tp->t_outq)) >= 0)
		wscons_putc(c);

	wscons_move_cursor(sc);
	tty_done(tp);
}

static int
wscons_cngetc(device_t dev)
{
	struct wscons_softc *sc = wscons_softc;
	struct wscons_kbd_ops *kops = sc->kbd_ops;

	return kops->getc(sc->kbd_aux);
}

static void
wscons_cnputc(device_t dev, int c)
{
	struct wscons_softc *sc = wscons_softc;

	wscons_putc(c);
	wscons_move_cursor(sc);
}

static void
wscons_cnpollc(device_t dev, int on)
{
	struct wscons_softc *sc = wscons_softc;
	struct wscons_kbd_ops *kops = sc->kbd_ops;

	kops->set_poll(sc->kbd_aux, on);
}

void
wscons_kbd_input(int c)
{
	struct wscons_softc *sc = wscons_softc;

	tty_input(c, &sc->tty);
}

void
wscons_attach_video(struct wscons_video_ops *ops, void *aux)
{
	struct wscons_softc *sc = wscons_softc;
	int diag = 0;

	sc->vid_ops = ops;
	sc->vid_aux = aux;
	ops->get_cursor(aux, &sc->col, &sc->row);

#ifdef CONFIG_DIAG_SCREEN
	diag = 1;
#endif
	wsconsdev.dev = sc->dev;
	cons_attach(&wsconsdev, diag);
}

void
wscons_attach_kbd(struct wscons_kbd_ops *ops, void *aux)
{
	struct wscons_softc *sc = wscons_softc;

	sc->kbd_ops = ops;
	sc->kbd_aux = aux;
}

static int
wscons_init(struct driver *self)
{
	struct bootinfo *bi;
	struct wscons_softc *sc;
	device_t dev;

	dev = device_create(self, "tty", D_CHR|D_TTY);

	sc = device_private(dev);
	sc->dev = dev;
	sc->esc.index = 0;
	sc->attr = 0x0f;
	wscons_softc = sc;

	tty_attach(&sc->tty);
	sc->tty.t_dev = dev;
	sc->tty.t_oproc = wscons_start;

	machine_bootinfo(&bi);
	sc->nrows = bi->video.text_y;
	sc->ncols = bi->video.text_x;
	return 0;
}
