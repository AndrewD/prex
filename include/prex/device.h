/*-
 * Copyright (c) 2009, Andrew Dennison
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

#ifndef _PREX_DEVICE_H
#define _PREX_DEVICE_H

#include <prex/types.h>
#include <sys/cdefs.h>

/*
 * Device file handle
 */
struct file {
	device_t dev;
	void *priv;
	u_long f_flags;
};
typedef struct file *file_t;

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
	int	(*open)	(file_t);
	int	(*close)(file_t);
	int	(*read)	(file_t, char *, size_t *, int);
	int	(*write)(file_t, char *, size_t *, int);
	int	(*ioctl)(file_t, u_long, void *);
	int	(*event)(int);
#ifdef __ppc__
	ret64_t (*iofn)	(file_t, int, arg_t, arg_t,
			 arg_t, arg_t, arg_t, arg_t);
#endif /* __ppc__ */
};

__BEGIN_DECLS
device_t device_create(const struct devio *, const char *, int, void *);
int	 device_destroy(device_t);
int	 device_broadcast(int, int);
__BEGIN_DECLS

#endif /* !_PREX_DEVICE_H */
