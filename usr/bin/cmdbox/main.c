/*
 * Copyright (c) 2005-2006, Kohsuke Ohtani
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

#include <limits.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <termios.h>

#include "cmdbox.h"

#ifdef CONFIG_CMD_SH
extern int sh_main(int argc, char *argv[]);
#endif

int
null_main(int argc, char *argv[])
{
	return -1;
}

int
help_main(int argc, char *argv[])
{
	struct cmdentry const *entry;
	struct winsize ws;
	int maxcol, col = 0;

	/* Get screen size */
	maxcol = 80;
	if (ioctl(fileno(stderr), TIOCGWINSZ, &ws) == 0)
		maxcol = (int)ws.ws_col;
	if (maxcol < 80)
		maxcol -= 15;
	else
		maxcol -= 25;

	fprintf(stderr, "usage: cmdbox [command] [arguments]...\n");
	fprintf(stderr, "builtin commands:\n");

	/*
	 * Show help for builtin commands.
	 */
	entry = builtin_cmds;
	while (entry->cmd != NULL) {
		col += fprintf(stderr, "%s%s", ((col == 0) ? "    " : ", "),
			       entry->cmd);
		entry++;
		if (col > maxcol && entry->cmd != NULL) {
			fprintf(stderr, ",\n");
			col = 0;
		}
	}

#ifdef CONFIG_CMD_SH
	/*
	 * Show help for shell commands.
	 */
	entry = shell_cmds;
	while (entry->cmd != NULL) {
		col += fprintf(stderr, "%s%s", ((col == 0) ? "    " : ", "),
			       entry->cmd);
		entry++;
		if (col > maxcol && entry->cmd != NULL) {
			fprintf(stderr, ",\n");
			col = 0;
		}
	}
#endif
	fprintf(stderr, "\nuse `-?` to find out more about each command.\n");
	return 0;
}

int
main(int argc, char *argv[])
{
	char *prog, *cmd;
	struct cmdentry const *entry;
	int shcmd = 0;

	prog = basename(argv[0]);
	cmd = prog;

	/*
	 * Alias:
	 * 'sh'            => sh
	 * 'cmdbox'        => sh
	 * 'cmdbox sh'     => sh
	 * 'cmdbox cmd'    => cmd
	 * 'cmd' (symlink) => cmd
	 */
	if (!strcmp(prog, "sh"))
		shcmd = 1;
	else if (!strcmp(prog, "cmdbox")) {
		if (argc == 1)
			shcmd = 1;
		else {
			if (!strcmp(argv[1], "sh"))
				shcmd = 1;
			else
				cmd = argv[1];
			argv++;
			argc--;
		}
	}

	if (shcmd) {
		task_setname(task_self(), "sh");
		exit(sh_main(argc, argv));
	}

	entry = builtin_cmds;
	while (entry->cmd != NULL) {
		if (!strcmp(cmd, entry->cmd)) {
			task_setname(task_self(), cmd);
			exit(entry->func(argc, argv));
		}
		entry++;
	}
	fprintf(stderr, "No such command: %s\n", cmd);
	return 0;
}
