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

/*
 * cmd.c - command processor
 */

#include <sys/prex.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

static void cmd_help(int argc, char **argv);
static void cmd_ver(int argc, char **argv);
static void cmd_mem(int argc, char **argv);
static void cmd_clear(int argc, char **argv);
static void cmd_kill(int argc, char **argv);
static void cmd_reboot(int argc, char **argv);
static void cmd_shutdown(int argc, char **argv);

struct cmd_entry {
	const char *cmd;
	void	   (*func)(int, char **);
	const char *usage;
};

static const struct cmd_entry cmd_table[] = {
	{ "ver"	 	,cmd_ver	,"Version information" },
	{ "mem"	 	,cmd_mem	,"Show memory usage" },
	{ "clear"	,cmd_clear	,"Clear screen" },
	{ "kill"	,cmd_kill	,"Terminate thread" },
	{ "reboot"	,cmd_reboot	,"Reboot system" },
	{ "shutdown"	,cmd_shutdown	,"Shutdown system" },
	{ "help"	,cmd_help	,"This help" },
	{ NULL		,NULL		,NULL },
};

static void
cmd_help(int argc, char **argv)
{
	int i = 0;

	while (cmd_table[i].cmd != NULL) {
		printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].usage);
		i++;
	}
}

static void
cmd_ver(int argc, char **argv)
{
	struct kerninfo info;

	sys_info(INFO_KERNEL, &info);

	printf("Kernel version:\n");
	printf("%s version %s for %s\n",
	       info.sysname, info.version, info.machine);
}

static void
cmd_mem(int argc, char **argv)
{
	struct meminfo info;

	sys_info(INFO_MEMORY, &info);

	printf("Memory usage:\n");
	printf(" Used     : %8d KB\n",
	       (u_int)((info.total - info.free) / 1024));
	printf(" Free     : %8d KB\n", (u_int)(info.free / 1024));
	printf(" Total    : %8d KB\n", (u_int)(info.total / 1024));
	printf(" Bootdisk : %8d KB\n", (u_int)(info.bootdisk / 1024));
}

static void
cmd_clear(int argc, char **argv)
{

	printf("\33[2J");
}

static void
cmd_kill(int argc, char **argv)
{
	thread_t t;
	char *ep;

	if (argc < 2) {
		puts("Usage: kill thread");
		return;
	}
	t = (thread_t)strtoul(argv[1], &ep, 16);
	printf("Kill thread id:%x\n", (u_int)t);

	if (thread_terminate(t))
		printf("Thread %x does not exist\n", (u_int)t);
}

static void
cmd_reboot(int argc, char **argv)
{
	device_t pm_dev;
	int error, state = PWR_REBOOT;

	if ((error = device_open("pm", 0, &pm_dev)) == 0) {
		error = device_ioctl(pm_dev, PMIOC_SET_POWER, &state);
		device_close(pm_dev);
	}
	if (error)
		printf("Error %d\n", error);
}

static void
cmd_shutdown(int argc, char **argv)
{
	device_t pm_dev;
	int error, state = PWR_OFF;

	if ((error = device_open("pm", 0, &pm_dev)) == 0) {
		error = device_ioctl(pm_dev, PMIOC_SET_POWER, &state);
		device_close(pm_dev);
	}
	if (error)
		printf("Error %d\n", error);
}

int
dispatch_cmd(int argc, char **argv)
{
	int i = 0;

	while (cmd_table[i].cmd != NULL) {
		if (!strcmp(argv[0], cmd_table[i].cmd)) {
			(cmd_table[i].func)(argc, argv);
			break;
		}
		i++;
	}
	if (cmd_table[i].cmd == NULL)
		printf("%s: command not found\n", argv[0]);

	return 0;
}
