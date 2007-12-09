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
 * main.c - file I/O test program
 */

#include <sys/syslog.h>
#include <sys/fcntl.h>

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "fslib.h"

#define IOBUFSZ	512

/*
 * Display file contents
 */
static void cat_file(char *filename)
{
	char iobuf[IOBUFSZ];
	int rd, fd;

	if ((fd = fslib_open(filename, O_RDONLY, 0)) < 0)
		panic("can not open file %s", filename);

	while ((rd = fslib_read(fd, iobuf, IOBUFSZ)) > 0)
		fslib_write(STDOUT_FILENO, iobuf, (size_t)rd);

	fslib_close(fd);
}

/*
 * Main routine
 */
int main(int argc, char *argv[])
{
	char test_str[] = "Reading file...\n\n";

	syslog(LOG_INFO, "fileio: fs test program\n");

	/*
	 * Initialize library
	 */
	fslib_init();

	/*
	 * Mount file systems
	 */
	fslib_mount("", "/", "ramfs", 0, NULL);

	fslib_mkdir("/dev", 0);
	fslib_mount("", "/dev", "devfs", 0, NULL);		/* device */

	fslib_mkdir("/boot", 0);
	fslib_mount("/dev/ram0", "/boot", "arfs", 0, NULL);	/* archive */

	/*
	 * Prepare stdio
	 */
	fslib_open("/dev/kbd", O_RDONLY);	/* stdin */
	fslib_open("/dev/console", O_WRONLY);	/* stdout */
	fslib_open("/dev/console", O_WRONLY);	/* stderr */

	/*
	 * Test read/write
	 */
	fslib_write(STDOUT_FILENO, test_str, strlen(test_str));
	cat_file("/boot/LICENSE");

	for (;;) ;
	return 0;
}
