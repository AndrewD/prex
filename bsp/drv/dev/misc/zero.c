/*-
 * Copyright (c) 2005-2006, Kohsuke Ohtani
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
 * zero.c - zero device
 */

#include <driver.h>

static int zero_read(device_t, char *, size_t *, int);
static int zero_write(device_t, char *, size_t *, int);
static int zero_init(struct driver *);

static struct devops zero_devops = {
	/* open */	no_open,
	/* close */	no_close,
	/* read */	zero_read,
	/* write */	zero_write,
	/* ioctl */	no_ioctl,
	/* devctl */	no_devctl,
};

struct driver zero_driver = {
	/* name */	"zero",
	/* devops */	&zero_devops,
	/* devsz */	0,
	/* flags */	0,
	/* probe */	NULL,
	/* init */	zero_init,
	/* shutdown */	NULL,
};

/*
 * Reading from this device will just fill the buffer with zero.
 */
static int
zero_read(device_t dev, char *buf, size_t *nbyte, int blkno)
{
	void *p;

	/* Translate buffer address to kernel address. */
	if ((p = kmem_map(buf, *nbyte)) == NULL)
		return EFAULT;

	memset(p, 0, *nbyte);
	return 0;
}

/*
 * Writing data to this device is ignored.
 */
static int
zero_write(device_t dev, char *buf, size_t *nbyte, int blkno)
{

	return 0;
}

static int
zero_init(struct driver *self)
{

	device_create(self, "zero", D_CHR);
	return 0;
}
