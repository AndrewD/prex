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

#include <prex/prex.h>
#include <prex/keycode.h>
#include <ctype.h>
#include <stdio.h>
#include <limits.h>

extern int dispatch_cmd(int argc, char **arg_list);

static char *arg_list[LINE_MAX];

void get_line(char *line)
{
	int c;
	char *p = line;
	int len = 0;

	while (1) {
		c = getchar();
		switch (c) {
		case K_ENTR:
			putchar('\r');
			putchar('\n');
			*p = 0;
			return;
		case K_BKSP:
			if (len == 0)
				continue;
			putchar('\b');
			putchar(' ');
			putchar('\b');
			len--;
			p--;
			*p = 0;
			continue;
		}
		if (c >= 0x80)
			continue;

		if (len >= LINE_MAX)
			continue;

		putchar(c);
		*p = (char)c;
		p++;
		len++;
	}
}

int parse_line(char *line)
{
	char *p = line;
	int argc = 0;

	while (1) {
		/* Skip space */
		while (*p && *p == ' ')
			p++;
		if (*p == 0)
			break;
		arg_list[argc++] = p;

		/* Skip word */
		while (*p && *p != ' ')
			p++;
		if (*p == 0)
			break;
		*p++ = 0;
	}
	return argc;
}

int main(int argc, char *argv[])
{
	char line[LINE_MAX];
	int cnt;

	printf("Prex kernel monitor - type 'help' to list commands\n");

	for (;;) {
		printf("[kmon]$ ");
		get_line(line);
		cnt = parse_line(line);
		if (cnt) {
			if (dispatch_cmd(cnt, arg_list))
				break;
		}
		printf("\n");
	}
	return 0;
}
