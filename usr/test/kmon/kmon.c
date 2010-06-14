/*
 * Copyright (c) 2005, Kohsuke Ohtani
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
 * kmon.c - main routine for kernel monitor
 */

#include <sys/prex.h>
#include <sys/keycode.h>
#include <ctype.h>
#include <stdio.h>
#include <limits.h>

#define ARGMAX		32

extern int dispatch_cmd(int argc, char **args);

/*
 * Parse an entire given line.
 */
static int
parse_line(char *line)
{
	static char *args[ARGMAX];
	char *p, *word = NULL;
	int argc = 0;
	int rc = 0;

	if (line[0] != ' ' && line[0] != '\t')
		word = line;

	p = line;
	while (*p) {
		if (word == NULL) {
			/* Skip white space. */
			if (*p != ' ' && *p != '\t')
				word = p;
		} else {
			if (*p == ' ' || *p == '\t') {
				*p = '\0';
				args[argc++] = word;
				word = NULL;
				if (argc >= ARGMAX - 1) {
					printf("Too many args\n");
					return 0;
				}
			}
		}
		p++;
	}
	if (word)
		args[argc++] = word;
	args[argc] = NULL;

	if (argc) {
		if (dispatch_cmd(argc, args))
			rc = 1;
	}
	return rc;
}

static void
read_line(char *line)
{
	int c, len = 0;
	char *p = line;

	for (;;) {
		c = getchar();
		if (c == '\n' || len > LINE_MAX) {
			*p = '\0';
			return;
		}
		*p = (char)c;
		p++;
		len++;
	}
}

int
main(int argc, char *argv[])
{
	static char line[LINE_MAX];

	printf("Prex kernel monitor - type 'help' to list commands\n");

	for (;;) {
		printf("[kmon]$ ");
		read_line(line);
		if (parse_line(line))
			break;
	}
	return 0;
}
