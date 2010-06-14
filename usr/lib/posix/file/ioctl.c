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

#include <sys/prex.h>
#include <sys/posix.h>
#include <ipc/fs.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <stdarg.h>
#include <string.h>

int
ioctl(int fd, unsigned long cmd, ...)
{
	struct ioctl_msg m;
	char *argp;
	va_list args;
	size_t size;
	int retval = 0;

	va_start(args, cmd);
	argp = va_arg(args, char *);
	va_end(args);

	/*
	 * Check the parameter size
	 */
	size = IOCPARM_LEN(cmd);
	if (size > IOCPARM_MAX) {
		errno = EINVAL;
		return -1;
	}

	/*
	 * Check fault
	 */
	if ((cmd & IOC_IN && (cmd & IOC_IVAL) == 0 && argp == NULL) ||
	    (cmd & IOC_OUT && (cmd & IOC_OVAL) == 0 && argp == NULL)) {
		errno = EFAULT;
		return -1;
	}

	/*
	 * Copy in
	 */
	if (cmd & IOC_IN) {
		if (cmd & IOC_IVAL)
			*((int *)m.buf) = (int)argp;
		else
			memcpy(&m.buf, argp, size);
	}

	m.hdr.code = FS_IOCTL;
	m.fd = fd;
	m.request = cmd;
	if (__posix_call(__fs_obj, &m, sizeof(m), 0) != 0)
		return -1;

	/*
	 * Copy out
	 */
	if (cmd & IOC_OUT) {
		if (cmd & IOC_OVAL)
			retval = *((int *)m.buf);
		else
			memcpy(argp, &m.buf, size);
	}

	return retval;
}
