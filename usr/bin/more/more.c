/*
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

#include <sys/termios.h>
#include <sys/ioctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

#ifdef CMDBOX
#define main(argc, argv)	more_main(argc, argv)
#endif

static void do_more(FILE *, int);

int
main(int argc, char *argv[])
{
	struct winsize ws;
	int ch;
	FILE *fp = NULL;
	int height = 25;
	int rval = 0;

	while ((ch = getopt(argc, argv, "")) != -1)
		switch(ch) {
		case '?':
		default:
			fprintf(stderr, "usage: more [file ...]\n");
			exit(1);
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;

	if (ioctl(0, TIOCGWINSZ, &ws) == 0)
		height = (int)ws.ws_row;

	if (*argv) {
		for (; *argv; ++argv) {
			if ((fp = fopen(*argv, "r")) == NULL) {
				warn("%s", *argv);
				++argv;
				rval = 1;
				continue;
			}
			do_more(fp, height);
			(void)fclose(fp);
		}
	} else
		do_more(stdin, height);

	exit(rval);
}

static void
do_more(FILE *fp, int maxlines)
{
	int c, len, lines = 0;

	while ((c = getc(fp)) != EOF) {
		if (c == '\n' && ++lines >= maxlines) {
			len = printf("\n--More-- ");
			fflush(stdout);
			read(2, &c, 1);

			printf("\033[1A"); /* up cursor */
			putc('\r', stdout);
			while (--len >= 0)
				putc(' ', stdout);
			putc('\r', stdout);
			len = 0;
			lines = 0;
		} else
			putchar(c);
	}
	putchar('\n');
}
