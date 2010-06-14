/*-
 * Copyright (c) 2009, Kohsuke Ohtani
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
 * creat.c - file creation test.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

#define FILE_NUM 100

static char path[128];

int
main(int argc, char *argv[])
{
	int fd, i;
	char file[128];
	pid_t pid = getpid();

	sprintf(path, "t%05d", pid);
	if (mkdir(path, 0770) < 0)
		err(1, "mkdir(%s)", path);
	if (chdir(path) == -1)
		err(1, "chdir(%s)", path);

	for (i = 0; i < FILE_NUM; i++) {
		sprintf(file, "p%05d.%03d", pid, i);
		if ((fd = creat(file, 0660)) == -1) {
			if (errno != EINTR) {
				warn("creat(%s)", file);
				break;
			}
		}
		if (fd != -1 && close(fd) == -1)
			err(2, "close(%d)", i);
	}

	for (i--; i >= 0; i--) {
		sprintf(file, "p%05d.%03d", pid, i);
		if (unlink(file) == -1)
			err(3, "unlink(%s)", file);
	}

	return (0);
}
