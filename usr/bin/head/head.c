/*
 * Copyright (c) 1980, 1987, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * head - give the first few lines of a stream or of each of a set of files
 *
 * Bill Joy UCB August 24, 1977
 */

#ifdef CMDBOX
#define main(argc, argv)	head_main(argc, argv)
#endif

void head(FILE *, int);
static void usage(void);

int eval;

int
main(int argc, char *argv[])
{
	int ch;
	FILE *fp;
	int first, linecnt;
	char *ep;

	linecnt = 10;
	while ((ch = getopt(argc, argv, "n:")) != EOF)
		switch(ch) {
		case 'n':
			linecnt = strtol(optarg, &ep, 10);
			if (*ep || linecnt <= 0)
				err(1, "illegal line count -- %s", optarg);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (*argv)
		for (first = 1; *argv; ++argv) {
			if ((fp = fopen(*argv, "r")) == NULL) {
				err(0, "%s: %s", *argv, strerror(errno));
				continue;
			}
			if (argc > 1) {
				(void)printf("%s==> %s <==\n",
				    first ? "" : "\n", *argv);
				first = 0;
			}
			head(fp, linecnt);
			(void)fclose(fp);
		}
	else
		head(stdin, linecnt);
	exit(eval);
}

void
head(FILE *fp, int cnt)
{
	int ch;

	while (cnt--)
		while ((ch = getc(fp)) != EOF) {
			if (putchar(ch) == EOF)
				err(1, "stdout: %s", strerror(errno));
			if (ch == '\n')
				break;
		}
}

static void
usage()
{
	(void)fprintf(stderr, "usage: head [-n lines] [file ...]\n");
	exit(1);
}
