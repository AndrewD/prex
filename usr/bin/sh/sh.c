/*
 * Copyright (c) 2005-2009, Kohsuke Ohtani
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
#include <sys/keycode.h>
#include <sys/syslog.h>
#include <sys/wait.h>
#include <sys/fcntl.h>

#include <limits.h>
#include <dirent.h>
#include <termios.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <setjmp.h>
#include <libgen.h>	/* for basename() */

#include "sh.h"

#define ARGMAX		32

#define	CMD_PIPE	1
#define	CMD_BACKGND	2
#define	CMD_BUILTIN	4

extern const struct cmdentry shell_cmds[];
#ifdef CMDBOX
extern const struct cmdentry builtin_cmds[];
#define main(argc, argv)	sh_main(argc, argv)
#endif

static pid_t	shpid = 0;	/* pid of shell */
char		retval;		/* return value for shell */
unsigned	interact;	/* if shell reads from stdin */

jmp_buf	jmpbuf;

static void
error(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	if (msg != NULL) {
		vfprintf(stderr, msg, ap);
		fprintf(stderr, "\n");
	}
	va_end(ap);
	if (getpid() != shpid)
		_exit(1);

	if (!interact)
		_exit(1);
	retval = 1;
}

static void
showsignal(int pid, int s)
{
	int signo = WTERMSIG(s);

	signo &= 0x7f;
	if (signo < NSIG && sys_siglist[signo])
		error(" %d: %s", pid, sys_siglist[signo]);
	else
		error(" %d: Signal %d", pid, signo);

	retval = signo + 0200;
}

static void
showprompt(void)
{
	static char cwd[PATH_MAX];
	static char prompt[PATH_MAX+20];

	getcwd(cwd, PATH_MAX);
	sprintf(prompt, "\033[32m[prex:%s]\033[0m# ", cwd);
	write(1, prompt, strlen(prompt));
}

static void
execute(int argc, char *argv[], int *redir, int flags, cmdfn_t cmdfn)
{
	int pid, i;
	int status;
	static char **arg;
	char *file;
	char spid[20];

	arg = argc > 1 ? &argv[1] : NULL;
	file = argv[0];
	pid = vfork();
	if (pid == -1) {
		for (i = 0; i < 2; i++)
			if (redir[i] != -1)
				close(redir[i]);
		error("Cannot fork");
		return;
	}
	if (pid == 0) {
		/* Child only */
		setpgid(0, 0);
		tcsetpgrp(2, getpgrp());

		for (i = 0; i < 2; i++) {
			if (redir[i] != -1) {
				if (dup2(redir[i], i) == -1)
					error("Cannot redirect %d", i);
				close(redir[i]);
			}
		}
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);

		if (flags & CMD_BACKGND) {
			signal(SIGINT, SIG_IGN);
			signal(SIGQUIT, SIG_IGN);
			if (redir[0] == -1) {
				close(0);
				open("/dev/null", O_RDWR);
			}
		}
		errno = 0;
		if (cmdfn) {
			task_setname(task_self(), basename(file));
			if (cmdfn(argc, argv) != 0)
				fprintf(stderr, "%s: %s\n", argv[0],
					strerror(errno));
		} else {
			execv(file, arg);
			/* Try $PATH */
			if (errno == ENOENT)
				execvp(file, arg);
			if (errno == ENOENT || errno == ENOTDIR)
				error("%s: command not found", argv[0]);
			else if (errno == EACCES)
				error("Permission denied");
			else
				error("%s cannot execute", argv[0]);
		}
		exit(1);
		/* NOTREACHED */
	}
	/* Parent */
	for (i = 0; i < 2; i++) {
		if (redir[i] != -1)
			close(redir[i]);
	}
	if (flags & CMD_PIPE)
		return;
	if (flags & CMD_BACKGND) {
		sprintf(spid, "%u\n", pid);
		write(1, spid, strlen(spid));
		return;
	}

	while (wait(&status) != pid);
	if (status) {
		if (WIFSIGNALED(status))
			showsignal(pid, status);
		else if (WIFEXITED(status))
			retval = WEXITSTATUS(status);
	} else
		retval = 0;
	return;
}

static int
redirect(char **args, int *redir)
{
	unsigned int i, io, append = 0;
	int fd, argc;
	char *p, *file;

	for (i = 0; args[i] != NULL; i++) {
		p = args[i];
		switch (*p) {
		case '<':
			io = 0;
 			break;
		case '>':
			io = 1;
			if (*(p + 1) == '>') {
				append = 1;
				p++;
			}
 			break;
		default:
			continue;
		}

		/* get file name */
		args[i] = (char *)-1;
		if (*(p + 1) == '\0') {
			file = args[++i];
			args[i] = (char *)-1;
		} else
			file = p + 1;

		/* if redirected from pipe, ignore */
		if (redir[io] == -1) {
			if (io == 1) {
				if (append)
					fd = open(file, O_WRONLY | O_APPEND);
				else
					fd = creat(file, 0666);
			} else
				fd = open(file, O_RDONLY);

			if (fd == -1) {
				error("%s: cannot open", file);
				return -1;
			}
			redir[io] = fd;
		}
	}

	/* strip redirection info */
	argc = 0;
	for (i = 0; args[i]; i++) {
		if (args[i] != (char *)-1)
			args[argc++] = args[i];
	}
	args[argc] = NULL;
	return argc;
}

static cmdfn_t
findcmd(const struct cmdentry cmds[], char *cmd)
{
	int i = 0;

	while (cmds[i].cmd != NULL) {
		if (!strcmp(cmd, cmds[i].cmd))
			return cmds[i].func;
		i++;
	}
	return 0;
}

static void
parsecmd(char *cmds, int *redir, int flags)
{
	static char cmdbox[] = "cmdbox";
	static char *args[ARGMAX];
	char *p, *word = NULL;
	cmdfn_t fn;
	int i, argc = 0;

	optind = 1;	/* for nommu */

	if (cmds[0] != ' ' && cmds[0] != '\t')
		word = cmds;

	p = cmds;
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
					error("Too many args");
					return;
				}
			}
		}
		p++;
	}
	if (argc == 0 && word == NULL)
		return;

	if (word)
		args[argc++] = word;
	args[argc] = NULL;

	/* Handle variable */
	if ((p = strchr(args[0], '=')) != NULL) {
		*p++ = '\0';
		if (*p == '\0')
			unsetvar(args[0]);
		else
			setvar(args[0], p);
		return;
	}

	fn = findcmd(shell_cmds, args[0]);
	if (fn) {
		/* Run as shell internal command */
		if ((*fn)(argc, args) != 0)
			error("%s: %s", args[0], strerror(errno));
		return;
	}
	argc = redirect(args, redir);
	if (argc == -1)
		return;

	fn = findcmd(builtin_cmds, args[0]);

	/*
	 * Alias: 'sh' => 'cmdbox sh'
	 */
	if (fn == NULL && !strcmp(args[0], "sh")) {
		for (i = argc; i >= 0; i--)
			args[i + 1] = args[i];
		args[0] = cmdbox;
		argc++;
	}
	execute(argc, args, redir, flags, fn);
}

static void
parsepipe(char *str, int flags)
{
	int pip[2] = { -1, -1 };
	int redir[2] = { -1, -1 };
	char *p, *cmds;

	p = cmds = str;
	while (*cmds) {
		switch (*p) {
		case '|':
			*p = '\0';
			redir[0] = pip[0];
			if (pipe(pip) == -1) {
				error("Cannot pipe");
				return;
			}
			redir[1] = pip[1];
			parsecmd(cmds, redir, flags | CMD_PIPE);
			cmds = p + 1;
			break;
		case '\0':
			redir[0] = pip[0];
			redir[1] = -1;
			parsecmd(cmds, redir, flags);
			return;
		}
		p++;
	}
}

static void
parseline(char *line)
{
	char *p, *cmds;

	p = cmds = line;
	while (*cmds) {
		switch (*p) {
		case ';':
			*p = '\0';
			parsepipe(cmds, 0);
			cmds = p + 1;
			break;
		case '&':
			*p = '\0';
			parsepipe(cmds, CMD_BACKGND);
			cmds = p + 1;
			break;
		case '\0':
		case '\n':
		case '#':
			*p = '\0';
			parsepipe(cmds, 0);
			return;
		}
		p++;
	}
}

static char *
readline(int fd, char *line, int len)
{
	char *p = line;
	int nleft = len;
	int cnt;

	while (--nleft > 0) {
		cnt = read(fd, p, 1);
		if (cnt == -1)
			return (char *)-1;	/* error */
		if (cnt == 0) {
			if (p == line)
				return NULL;	/* EOF */
			break;
		}
		if (*p == '\n')
			break;
		p++;
	}
	*p = '\0';
	return line;
}

static void
cmdloop(int fd)
{
	static char line[LINE_MAX];
	char *p;

	for (;;) {
		if (interact)
			showprompt();

		line[0] = '\0';
		p = readline(fd, line, sizeof(line));
		if (p == (char *)-1)
			continue;
		if (p == NULL)
			break;
		parseline(line);
		tcsetpgrp(2, shpid);
	}
}

int
main(int argc, char **argv)
{
	int input;

	if (shpid == 0)
		shpid = getpid();

	if (setjmp(jmpbuf)) {
		argv = (char **)0;
		argc = 1;
	}
	interact = 1;
	initvar();

	if (argc == 1) {
		input = 0;
		if (isatty(0) && isatty(1)) {
			interact = 1;
			signal(SIGINT, SIG_IGN);
			signal(SIGQUIT, SIG_IGN);
			signal(SIGTERM, SIG_IGN);
		}
	} else {
		interact = 0;
		close(0);
		input = open(argv[1], O_RDONLY);
		if (input < 0) {
			fprintf(stderr, "%s: cannot open\n", argv[1]);
			interact = 1;
			exit(1);
		}
	}
	cmdloop(input);
	exit(retval);
	return 0;
}
