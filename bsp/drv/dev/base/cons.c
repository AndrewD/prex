/*-
 * Copyright (c) 2008-2009, Kohsuke Ohtani
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
 * cons.c - console redirection driver
 */

#include <driver.h>
#include <cons.h>
#include <sys/dbgctl.h>

static int cons_open(device_t, int);
static int cons_close(device_t);
static int cons_read(device_t, char *, size_t *, int);
static int cons_write(device_t, char *, size_t *, int);
static int cons_ioctl(device_t, u_long, void *);
static int cons_devctl(device_t, u_long, void *);
static int cons_init(struct driver *);

static struct devops cons_devops = {
	/* open */	cons_open,
	/* close */	cons_close,
	/* read */	cons_read,
	/* write */	cons_write,
	/* ioctl */	cons_ioctl,
	/* devctl */	cons_devctl,
};

struct driver cons_driver = {
	/* name */	"cons",
	/* devops */	&cons_devops,
	/* devsz */	0,
	/* flags */	0,
	/* probe */	NULL,
	/* init */	cons_init,
	/* unload */	NULL,
};

static struct diag_ops cons_diag_ops = {
	/* puts */	cons_puts,
};

static struct consdev *consdev;		/* console device info */

static int
cons_open(device_t dev, int mode)
{

	ASSERT(consdev != NULL);
	dev = consdev->dev;
	return consdev->devops->open(dev, mode);
}

static int
cons_close(device_t dev)
{

	ASSERT(consdev != NULL);
	dev = consdev->dev;
	return consdev->devops->close(dev);
}

static int
cons_read(device_t dev, char *buf, size_t *nbyte, int blkno)
{

	ASSERT(consdev != NULL);
	dev = consdev->dev;
	return consdev->devops->read(dev, buf, nbyte, blkno);
}

static int
cons_write(device_t dev, char *buf, size_t *nbyte, int blkno)
{

	ASSERT(consdev != NULL);
	dev = consdev->dev;
	return consdev->devops->write(dev, buf, nbyte, blkno);
}

static int
cons_ioctl(device_t dev, u_long cmd, void *arg)
{

	ASSERT(consdev != NULL);
	dev = consdev->dev;
	return consdev->devops->ioctl(dev, cmd, arg);
}

static int
cons_devctl(device_t dev, u_long cmd, void *arg)
{

	ASSERT(consdev != NULL);
	dev = consdev->dev;
	return consdev->devops->devctl(dev, cmd, arg);
}

/*
 * Poll (busy wait) for a input and return the input key.
 * cons_pollc() must be called before cons_getc() could be used.
 * cons_getc() should be used only for kernel deugger.
 */
int
cons_getc(void)
{

	ASSERT(consdev != NULL);
	return consdev->cngetc(consdev->dev);
}

/*
 * Switch the console driver to polling mode if on is non-zero, or
 * back to interrupt driven mode if on is zero.
 * cons_pollc() should be used only for kernel deugger.
 */
void
cons_pollc(int on)
{

	ASSERT(consdev != NULL);
	consdev->cnpollc(consdev->dev, on);
}

/*
 * Console character output routine.
 */
void
cons_putc(int c)
{

	ASSERT(consdev != NULL);
	if (c) {
		consdev->cnputc(consdev->dev, c);
		if (c == '\n')
			consdev->cnputc(consdev->dev, '\r');
	}
}

void
cons_puts(char *str)
{

	ASSERT(consdev != NULL);

	consdev->cnpollc(consdev->dev, 1);
	while (*str) {
		consdev->cnputc(consdev->dev, *str);
		if (*str == '\n')
			consdev->cnputc(consdev->dev, '\r');
		str++;
	}
	consdev->cnpollc(consdev->dev, 0);
}

void
cons_attach(struct consdev *cdev, int diag)
{

	if (consdev != NULL)
		return;
	consdev = cdev;

	/*
	 * Set console port as diag port.
	 */
	if (diag)
		dbgctl(DBGC_SETDIAG, &cons_diag_ops);
}

int
cons_init(struct driver *self)
{

	consdev = NULL;
	device_create(self, "console", D_CHR|D_TTY);
	return 0;
}
