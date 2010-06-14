/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kevin Fall.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* modified by Kohsuke Ohtani for Prex. */

#include <sys/stat.h>
#include <sys/fcntl.h>

#include <err.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CMDBOX
#define main(argc, argv)	cat_main(argc, argv)
#endif

static void do_cat(int rfd);

static char *stdbuf;
static char *filename;
static int rval;

int
main(int argc, char *argv[])
{
	int ch, fd;

	while ((ch = getopt(argc, argv, "")) != -1)
		switch(ch) {
		case '?':
		default:
			fprintf(stderr, "usage: cat [-] [file ...]\n");
			exit(1);
			/* NOTREACHED */
		}
	argv += optind;

	if ((stdbuf = malloc(BUFSIZ)) == NULL)
		err(1, NULL);

	fd = fileno(stdin);
	filename = "stdin";
	do {
		if (*argv) {
			if (!strcmp(*argv, "-"))
				fd = fileno(stdin);
			else if ((fd = open(*argv, O_RDONLY, 0)) < 0) {
				warn("%s", *argv);
				++argv;
				rval = 1;
				continue;
			}
			filename = *argv++;
		}
		do_cat(fd);
		if (fd != fileno(stdin))
			(void)close(fd);
	} while (*argv);
	free(stdbuf);
	exit(rval);
}

static void
do_cat(int rfd)
{
	int nr, nw, off, wfd;
	struct stat sbuf;

	wfd = fileno(stdout);
	if (fstat(wfd, &sbuf))
		err(1, "%s", filename);
	while ((nr = read(rfd, stdbuf, BUFSIZ)) > 0)
		for (off = 0; nr; nr -= nw, off += nw)
			if ((nw = write(wfd, stdbuf + off, (size_t)nr)) < 0)
				err(1, "stdout");
	if (nr < 0) {
		warn("%s @%d", filename, errno);
		rval = 1;
	}
}
