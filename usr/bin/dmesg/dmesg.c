/*-
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

#include <prex/prex.h>
#include <sys/ioctl.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef CMDBOX
#define main(argc, argv)	dmesg_main(argc, argv)
#endif

static int print_msg(char *buf, int size);

int
main(int argc, char *argv[])
{
	int size;
	char *buf;

	if (sys_debug(DCMD_LOGSIZE, &size) != 0)
		exit(1);

	if ((buf = malloc(size)) == NULL)
		exit(1);

	if (sys_debug(DCMD_GETLOG, buf) != 0) {
		free(buf);
		exit(1);
	}
	print_msg(buf, size);
	free(buf);
	return 0;
}

static int
print_msg(char *buf, int size)
{
	struct winsize ws;
	int i, cnt, ignore;
	int row, maxrow;

	/* Get screen size */
	maxrow = 79;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, (u_long)&ws) == 0)
		maxrow = (int)ws.ws_row - 1;

	/* Find message start point */
	for (i = 0; i < size; i++) {
		if (buf[i] == -1)
			break;
	}
	if (i == size)
		return 1;

	row = 0;
	ignore = 1;
	for (cnt = 0; cnt < size; cnt++) {
		if (buf[i] == '\n') {
			row++;
			ignore = 0;
		}
		if (!ignore)		/* skip first line */
			putc(buf[i], stdout);
		if (++i >= size)
			i = 0;
		if (row >= maxrow) {
			printf("--More-- ");
			getc(stdin);
			putchar('\n');
			row = 0;
		}
	}
	return 0;
}
