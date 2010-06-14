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
 * null.c - null device
 */

#include <driver.h>

static int null_read(device_t, char *, size_t *, int);
static int null_write(device_t, char *, size_t *, int);
static int null_init(struct driver *);

static struct devops null_devops = {
	/* open */	no_open,
	/* close */	no_close,
	/* read */	null_read,
	/* write */	null_write,
	/* ioctl */	no_ioctl,
	/* devctl */	no_devctl,
};

struct driver null_driver = {
	/* name */	"null",
	/* devops */	&null_devops,
	/* devsz */	0,
	/* flags */	0,
	/* probe */	NULL,
	/* init */	null_init,
	/* shutdown */	NULL,
};

/*
 * Always returns 0 bytes as the read size.
 */
static int
null_read(device_t dev, char *buf, size_t *nbyte, int blkno)
{

	*nbyte = 0;
	return 0;
}

/*
 * Data written to this device is discarded.
 */
static int
null_write(device_t dev, char *buf, size_t *nbyte, int blkno)
{

	return 0;
}

static int
null_init(struct driver *self)
{

	device_create(self, "null", D_CHR);
	return 0;
}
