/*
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * David Hitz of Auspex Systems Inc.
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

/* This code is derived from cp.c */
/* modified for Prex by Kohsuke Ohtani. */

/*
 * install.c - software installer.
 *
 * Required capabilities:
 *      CAP_SYSFILES
 */

#include <sys/stat.h>
#include <sys/fcntl.h>

#include <unistd.h>
#include <err.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <stdio.h>

static void usage(void);
static void error(void);
static int copy(char *from, char *to);


static char iobuf[BUFSIZ];

int
main(int argc, char *argv[])
{
	int r, ch, checkch, i;
	char target[] = "/bin";
	char *src;
	struct stat to_stat;

	if (argc < 2)
		usage();
	if (!strcmp(argv[1], "-?"))
		usage();

	r = stat(target, &to_stat);
	if (r == -1 && errno != ENOENT)
		error();
	if (r == -1 || !S_ISDIR(to_stat.st_mode)) {
		error();
	} else {
		r = 0;
		for (i = 1; i < argc; i++) {
			src = argv[i];
			/*
			 * User confirmation is required for security.
			 */
			fputs("Are you sure you want to install ", stdout);
			fputs(basename(src), stdout);
			fputs("? (y/n) ", stdout);
			checkch = ch = getchar();
			while (ch != '\n' && ch != EOF)
				ch = getchar();
			if (checkch != 'y')
				exit(0);

			r = copy(src, target);
			if (r)
				error();
		}
	}
	exit(r);
}

static void
usage(void)
{
	fputs("usage: install file\n", stderr);
	exit(1);
	/* NOTREACHED */
}

static int
copy(char *from, char *to)
{
	char path[PATH_MAX];
	int fold, fnew, n;
	struct stat stbuf;
	mode_t mode;
	char *p;

	p = strrchr(from, '/');
	p = p ? p + 1 : from;
	strlcpy(path, to, sizeof(path));
	strlcat(path, "/", sizeof(path));
	strlcat(path, p, sizeof(path));
	to = path;

	if ((fold = open(from, O_RDONLY)) == -1)
		return 1;
	fstat(fold, &stbuf);
	mode = stbuf.st_mode;

	if ((fnew = creat(to, mode)) == -1) {
		close(fold);
		return 1;
	}
	while ((n = read(fold, iobuf, BUFSIZ)) > 0) {
		if (write(fnew, iobuf, (size_t)n) != n) {
			close(fold);
			close(fnew);
			return 1;
		}
	}
	close(fold);
	close(fnew);
	return 0;
}

static void
error(void)
{
	perror("install");
}
