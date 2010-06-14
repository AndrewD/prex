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

#include <sys/fcntl.h>
#include <sys/prex.h>

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#include "sh.h"

extern char **environ;

static int cmd_null(int argc, char *argv[]);
static int cmd_cd(int argc, char *argv[]);
static int cmd_mem(int argc, char *argv[]);
static int cmd_exec(int argc, char *argv[]);
static int cmd_exit(int argc, char *argv[]);

/*
 * Internal shell commands
 */
const struct cmdentry shell_cmds[] = {
    { "cd"         ,cmd_cd           },
    { "exec"       ,cmd_exec         },
    { "exit"       ,cmd_exit         },
    { "export"     ,cmd_export       },
    { "mem"        ,cmd_mem          },
    { "set"        ,cmd_showvars     },
    { "unset"      ,cmd_unsetvar     },
    { NULL         ,cmd_null         },
};


static int
cmd_null(int argc, char *argv[])
{
	return 0;
}

static int
cmd_cd(int argc, char *argv[])
{
	char *p;

	if (argc > 2) {
		fprintf(stderr, "usage: cd [dir]\n");
		return 0;
	}
	if (argc == 1) {
		p = getenv("HOME");
		if (p == NULL)
			p = "/";
	} else
		p = argv[1];

	if (chdir(p) < 0)
		return errno;
	return 0;
}

static int
cmd_exec(int argc, char *argv[])
{
	char **envp = environ;

	if (argc < 2) {
		fprintf(stderr, "usage: exec command\n");
		return 0;
	}

	close(0);
	open("/dev/tty", O_RDONLY);

	/*
	 * Memory size optimization for 'exec sh'.
	 */
	if (!strcmp(argv[1], "sh")) {
		longjmp(jmpbuf, 1);
		/* NOTREACHED */
	}
	return execve(argv[1], &argv[2], envp);
}

static int
cmd_mem(int argc, char *argv[])
{
	struct meminfo info;

	sys_info(INFO_MEMORY, &info);

	/* UNIX v7 style... */
	printf("mem = %d\n", (u_int)info.total);
	return 0;
}

static int
cmd_exit(int argc, char *argv[])
{

	exit(0);
	/* NOTREACHED */
}
