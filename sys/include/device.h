/*-
 * Copyright (c) 2005-2007, Kohsuke Ohtani
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

#ifndef _DEVICE_H
#define _DEVICE_H

#include <list.h>
#include <system.h>

#define DEVICE_MAGIC	0x4465763f	/* 'Dev?' */

/*
 * Driver structure
 *
 * "order" is initialize order which must be between 0 and 15.
 * The driver with order 0 is called first.
 */
struct driver {
	const char	*name;		/* Name of device driver */
	const int	order;		/* Initialize order */
	int		(*init)();	/* Initialize routine */
};
typedef struct driver *driver_t;

/*
 * Device I/O table
 */
struct devio {
	int (*open)();
	int (*close)();
	int (*read)();
	int (*write)();
	int (*ioctl)();
	int (*event)();
};
typedef struct devio *devio_t;

/*
 * Device structure
 */
struct device {
	int	    magic;		/* Magic number */
	int	    ref_count;		/* Reference count */
	struct list link;		/* Link for device list */
	devio_t	    devio;		/* Device I/O table */
	char	    name[MAX_DEVNAME];	/* Name of device */
};
typedef struct device *device_t;

#define device_valid(dev) (kern_area(dev) && ((dev)->magic == DEVICE_MAGIC))

extern void device_init(void);
extern int device_open(char *name, int mode, device_t *pdev);
extern int device_close(device_t dev);
extern int device_read(device_t dev, void *buf, size_t *nbyte, int blkno);
extern int device_write(device_t dev, void *buf, size_t *nbyte, int blkno);
extern int device_ioctl(device_t dev, int cmd, u_long arg);
extern int device_info(struct info_device *info);


#endif /* !_DEVICE_H */
