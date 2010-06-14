/*
 * Copyright (c) 2008, Kohsuke Ohtani
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

#ifndef _SYS_DEVICE_H
#define _SYS_DEVICE_H

#include <sys/cdefs.h>
#include <sys/types.h>

/*
 * Device flags
 *
 * If D_PROT is set, the device can not be opened via devfs.
 */
#define D_CHR		0x00000001	/* character device */
#define D_BLK		0x00000002	/* block device */
#define D_REM		0x00000004	/* removable device */
#define D_PROT		0x00000008	/* protected device */
#define D_TTY		0x00000010	/* tty device */


#ifdef KERNEL

/*
 * Device operations
 */
struct devops {
	int (*open)	(device_t, int);
	int (*close)	(device_t);
	int (*read)	(device_t, char *, size_t *, int);
	int (*write)	(device_t, char *, size_t *, int);
	int (*ioctl)	(device_t, u_long, void *);
	int (*devctl)	(device_t, u_long, void *);
};

typedef int (*devop_open_t)   (device_t, int);
typedef int (*devop_close_t)  (device_t);
typedef int (*devop_read_t)   (device_t, char *, size_t *, int);
typedef int (*devop_write_t)  (device_t, char *, size_t *, int);
typedef int (*devop_ioctl_t)  (device_t, u_long, void *);
typedef int (*devop_devctl_t) (device_t, u_long, void *);

#define	no_open		((devop_open_t)nullop)
#define	no_close	((devop_close_t)nullop)
#define	no_read		((devop_read_t)enodev)
#define	no_write	((devop_write_t)enodev)
#define	no_ioctl	((devop_ioctl_t)enodev)
#define	no_devctl	((devop_devctl_t)nullop)

/*
 * Driver object
 */
struct driver {
	const char	*name;		/* name of device driver */
	struct devops	*devops;	/* device operations */
	size_t		devsz;		/* size of private data */
	int		flags;		/* state of driver */
	int (*probe)	(struct driver *);
	int (*init)	(struct driver *);
	int (*unload)	(struct driver *);
};

/*
 * flags for the driver.
 */
#define	DS_INACTIVE	0x00		/* driver is inactive */
#define DS_ALIVE	0x01		/* probe succeded */
#define DS_ACTIVE	0x02		/* intialized */
#define DS_DEBUG	0x04		/* debug */

#endif /* !KERNEL */
#endif /* !_SYS_DEVICE_H */
