/*
 * Copyright (c) 2007, Kohsuke Ohtani
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
 * fslib.c - file system access libarary.
 */

#include <prex/prex.h>
#include <server/fs.h>

#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "fslib.h"

static object_t __file_server;

/*
 * Init
 */
int fslib_init(void)
{
	struct msg m;
	int i, err;

	__file_server = NULL;

	thread_yield();

	/*
	 * Wait for server loading. timeout is 2 sec.
	 */
	for (i = 0; i < 200; i++) {
		/* Look up file system server */
		err = object_lookup(OBJNAME_FS, &__file_server);
		if (err == 0)
			break;
		/* Wait 10msec */
		timer_sleep(10, 0);
		thread_yield();
	}
	if (err)
		panic("fslib: can not find object");

	/*
	 * Register this task to fs server.
	 */
	m.hdr.code = FS_BOOT;
	msg_send(__file_server, &m, sizeof(m));
	if (m.hdr.status)
		panic("fslib: can not register to fs");

	return 0;
}

/*
 * Mount file system.
 */
int fslib_mount(const char *dev, const char *dir, const char *fs,
	  int flags, const void *data)
{
	struct mount_msg m;
	int err;

	/*
	 * Build mount message. No length check...
	 */
	strlcpy(m.dev, (char *)dev, PATH_MAX);
	strlcpy(m.dir, (char *)dir, PATH_MAX);
	strlcpy(m.fs, (char *)fs, 16);
	if (data != NULL)
		strlcpy(m.data, (char *)data, 64);
	m.flags = flags;

	/*
	 * Request mount() for file server.
	 */
	do {
		m.hdr.code = FS_MOUNT;
		err = msg_send(__file_server, &m, sizeof(m));
	} while (err == EINTR);
	if (m.hdr.status) {
		errno = m.hdr.status;
		return -1;
	}
	return 0;
}

/*
 * File open
 */
int fslib_open(char *path, int flags, ...)
{
	struct open_msg m;
	int err;
	va_list args;
	mode_t mode;

	va_start(args, flags);
	mode = va_arg(args, int);
	va_end(args);

	m.flags = flags;
	m.mode = mode;
	strlcpy(m.path, (char *)path, PATH_MAX);
	do {
		m.hdr.code = FS_OPEN;
		err = msg_send(__file_server, &m, sizeof(m));
	} while (err == EINTR);
	if (err) {
		errno = err;
		return -1;
	}
	if (m.hdr.status) {
		errno = m.hdr.status;
		return -1;
	}
	return m.fd;
}

/*
 * File close
 */
int fslib_close(int fd)
{
	struct msg m;
	int err;

	m.hdr.code = FS_CLOSE;
	m.data[0] = fd;
	err = msg_send(__file_server, &m, sizeof(m));
	if (err == EINTR)
		errno = EINTR;
	else if (err)
		errno = ENOSYS;
	else if (m.hdr.status)
		errno = m.hdr.status;
	else
		return 0;
	return -1;
}

/*
 * File stat
 */
int fslib_fstat(int fd, struct stat *st)
{
	struct stat_msg m;
	int err;

	do {
		m.hdr.code = FS_FSTAT;
		m.fd = fd;
		err = msg_send(__file_server, &m, sizeof(m));
	} while (err == EINTR);
	if (err) {
		errno = ENOSYS;
		return -1;
	} else if (m.hdr.status) {
		errno = m.hdr.status;
		return -1;
	}
	memcpy(st, &m.st, sizeof(struct stat));
	return 0;
}

/*
 * File read
 */
int fslib_read(int fd, void *buf, size_t len)
{
	struct io_msg m;
	int err;

	m.hdr.code = FS_READ;
	m.fd = fd;
	m.buf = buf;
	m.size = len;
	err = msg_send(__file_server, &m, sizeof(m));
	if (err) {
		errno = err;
		return -1;
	}
	if (m.hdr.status) {
		errno = m.hdr.status;
		return -1;
	}
	return m.size;
}

/*
 * File write
 */
int fslib_write(int fd, void *buf, size_t len)
{
	struct io_msg m;
	int err;

	m.hdr.code = FS_WRITE;
	m.fd = fd;
	m.buf = buf;
	m.size = len;
	err = msg_send(__file_server, &m, sizeof(m));
	if (err) {
		errno = err;
		return -1;
	}
	if (m.hdr.status) {
		errno = m.hdr.status;
		return -1;
	}
	return m.size;
}

/*
 * Lseek
 */
int fslib_lseek(int fd, off_t offset, int whence)
{
	struct msg m;
	int err;

	do {
		m.hdr.code = FS_LSEEK;
		m.data[0] = fd;
		m.data[1] = offset;
		m.data[2] = whence;
		err = msg_send(__file_server, &m, sizeof(m));
	} while (err == EINTR);
	if (err) {
		errno = ENOSYS;
		return -1;
	} else if (m.hdr.status) {
		errno = m.hdr.status;
		return -1;
	}
	return m.data[0];
}

/*
 * Create directory
 */
int fslib_mkdir(const char *path, mode_t mode)
{
	struct open_msg m;
	int err;

	m.flags = 0;
	m.mode = mode;
	strlcpy(m.path, (char *)path, PATH_MAX);
	do {
		m.hdr.code = FS_MKDIR;
		err = msg_send(__file_server, &m, sizeof(m));
	} while (err == EINTR);
	if (err) {
		errno = err;
		return -1;
	}
	if (m.hdr.status) {
		errno = m.hdr.status;
		return -1;
	}
	return 0;
}
