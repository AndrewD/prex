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

/*
 * Driver structure
 *
 * "order" is initialize order which must be between 0 and 15.
 * The driver with order 0 is called first.
 */
struct driver {
	const char	*name;		/* Name of device driver */
	const int	order;		/* Initialize order */
	int		(*init)(void);	/* Initialize routine */
};
typedef struct driver *driver_t;

/*
 * Device I/O table
 */
struct devio {
	int	(*open)	(device_t, int);
	int	(*close)(device_t);
	int	(*read)	(device_t, char *, size_t *, int);
	int	(*write)(device_t, char *, size_t *, int);
	int	(*ioctl)(device_t, int, u_long);
	int	(*event)(int);
};

/*
 * Device structure
 */
struct device {
	int		magic;		/* magic number */
	int		ref_count;	/* reference count */
	int		flags;		/* device characteristics */
	struct list	link;		/* linkage on device list */
	const struct devio *devio;		/* device i/o table */
	char		name[MAXDEVNAME]; /* name of device */
};

#define device_valid(dev) (kern_area(dev) && ((dev)->magic == DEVICE_MAGIC))

extern int	 device_open(const char *, int, device_t *);
extern int	 device_close(device_t);
extern int	 device_read(device_t, void *, size_t *, int);
extern int	 device_write(device_t, void *, size_t *, int);
extern int	 device_ioctl(device_t, int, u_long);
extern int	 device_info(struct info_device *);
extern void	 device_dump(void);
extern void	 device_init(void);

#endif /* !_DEVICE_H */
