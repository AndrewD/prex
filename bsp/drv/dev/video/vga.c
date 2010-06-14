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
 * vga.c - vga driver
 */

#include <driver.h>
#include <wscons.h>
#include <devctl.h>
#include <pm.h>

/* #define DEBUG_VGA 1 */

#ifdef DEBUG_VGA
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

#define CRTC_INDEX	0x3d4
#define CRTC_DATA	0x3d5
#define SEQ_INDEX	0x3c4
#define SEQ_DATA	0x3c5

#define VID_RAM		0xB8000

struct vga_softc {
	device_t	dev;
	short		*vram;
	int		cols;
	int		attr;
	int		blank;
};

static int	vga_devctl(device_t, u_long, void *);
static int	vga_init(struct driver *);
static void	vga_cursor(void*, int, int);
static void	vga_putc(void *, int, int, int);
static void	vga_copyrows(void *,int, int, int);
static void	vga_eraserows(void *,int, int);
static void	vga_set_attr(void *, int);
static void	vga_get_cursor(void *, int *, int *);

static struct devops vga_devops = {
	/* open */	no_open,
	/* close */	no_close,
	/* read */	no_read,
	/* write */	no_write,
	/* ioctl */	no_ioctl,
	/* devctl */	vga_devctl,
};

struct driver vga_driver = {
	/* name */	"vga",
	/* devops */	&vga_devops,
	/* devsz */	sizeof(struct vga_softc),
	/* flags */	0,
	/* probe */	NULL,
	/* init */	vga_init,
	/* shutdown */	NULL,
};

static struct wscons_video_ops wscons_vga_ops = {
	vga_cursor,	/* cursor */
	vga_putc,	/* putc */
	vga_copyrows,	/* copyrows */
	vga_eraserows,	/* eraserows */
	vga_set_attr,	/* set_attr */
	vga_get_cursor,	/* set_cursor */
};


static u_char
crtc_read(u_char index)
{

	bus_write_8(CRTC_INDEX, index);
	return bus_read_8(CRTC_DATA);
}

static void
crtc_write(u_char index, u_char val)
{

	bus_write_8(CRTC_INDEX, index);
	bus_write_8(CRTC_DATA, val);
}

static void
vga_on(void)
{
	u_char val;

	bus_write_8(SEQ_INDEX, 1);
	val = bus_read_8(SEQ_DATA);
	bus_write_8(SEQ_DATA, val & ~0x20);
}

static void
vga_off(void)
{
	u_char val;

	bus_write_8(SEQ_INDEX, 1);
	val = bus_read_8(SEQ_DATA);
	bus_write_8(SEQ_DATA, val | 0x20);
}

static void
vga_cursor(void *aux, int row, int col)
{
	struct vga_softc *sc = aux;
	int pos, s;

	pos = row * sc->cols + col;

	s = splhigh();
	crtc_write(0x0e, (u_char)((pos >> 8) & 0xff));
	crtc_write(0x0f, (u_char)(pos & 0xff));
	splx(s);
}

static void
vga_putc(void *aux, int row, int col, int ch)
{
	struct vga_softc *sc = aux;

	sc->vram[row * sc->cols + col] = ch | (sc->attr << 8);
}

static void
vga_copyrows(void *aux, int srcrow, int dstrow, int nrows)
{
	struct vga_softc *sc = aux;

	memcpy(sc->vram + dstrow * sc->cols,
	       sc->vram + srcrow * sc->cols,
	       (size_t)nrows * sc->cols * 2);
}

static void
vga_eraserows(void *aux, int row, int nrows)
{
	struct vga_softc *sc = aux;
	int i, start, end;

	start = row * sc->cols;
	end = start + nrows * sc->cols;

	for (i = start; i < end; i++)
		sc->vram[i] = ' ' | (sc->attr << 8);
}

static void
vga_set_attr(void *aux, int attr)
{
	struct vga_softc *sc = aux;

	sc->attr = attr;
}


static void
vga_get_cursor(void *aux, int *col, int *row)
{
	struct vga_softc *sc = aux;
	u_int offset;
	int s;

	s = splhigh();
	offset = crtc_read(0x0e);
	offset <<= 8;
	offset += crtc_read(0x0f);
	*col = (int)offset % sc->cols;
	*row = (int)offset / sc->cols;
	splx(s);
}

static int
vga_devctl(device_t dev, u_long cmd, void *arg)
{
	struct vga_softc *sc = device_private(dev);
	int s;

	DPRINTF(("vga: devctl cmd=%x\n", cmd));

	s = splhigh();
	switch (cmd) {
	case DEVCTL_PM_LCDOFF:
		if (!sc->blank) {
			DPRINTF(("vga: LCD off\n"));
			vga_off();
			sc->blank = 1;
		}
		break;
	case DEVCTL_PM_LCDON:
		if (sc->blank) {
			vga_on();
			sc->blank = 0;
			DPRINTF(("vga: LCD on\n"));
		}
		break;
	}
	splx(s);
	return 0;
}

static int
vga_init(struct driver *self)
{
	device_t dev;
	struct bootinfo *bi;
	struct vga_softc *sc;

	machine_bootinfo(&bi);

	dev = device_create(self, "vga", D_CHR);

	sc = device_private(dev);
	sc->vram = ptokv(VID_RAM);
	sc->cols = bi->video.text_x;
	sc->attr = 0x0f;
	sc->blank = 0;

	wscons_attach_video(&wscons_vga_ops, sc);

	pm_attach_lcd(dev);
	return 0;
}
