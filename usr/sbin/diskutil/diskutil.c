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

/*
 * diskutil.c - disk management utility
 *
 * Required capabilities:
 *      CAP_DISKADMIN
 */

#include <sys/prex.h>
#include <sys/ioctl.h>
#include <ipc/ipc.h>

#include <sys/mount.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

static void disk_list(int, char **);
static void disk_info(int, char **);
static void disk_mount(int, char **);
static void disk_unmount(int, char **);
static void disk_eject(int, char **);
static void disk_rename(int, char **);
static void disk_verify(int, char **);
static void disk_partition(int, char **);
static void disk_help(int, char **);
static void disk_null(int, char **);

struct cmdtab {
	const char	*cmd;
	void		(*func)(int, char **);
	const char	*usage;
};

static const struct cmdtab cmdtab[] = {
	{ "list"	,disk_list	," list      - List the partitions of a disk.\n" },
	{ "info"	,disk_info	," info      - Display information on a disk or volume.\n" },
	{ "mount"	,disk_mount	," mount     - Mount a single volume.\n" },
	{ "unmount"	,disk_unmount	," unmount   - Unmount a single volume.\n" },
	{ "eject"	,disk_eject	," eject     - Eject a disk.\n" },
	{ "rename"	,disk_rename	," rename    - Rename a volume.\n" },
	{ "verify"	,disk_verify	," verify    - Verify the structure of a volume.\n" },
	{ "partition"	,disk_partition	," partition - Partitiona disk, removing all volume.\n" },
	{ "-?"		,disk_help	," -?        - This help.\n" },
	{ NULL		,disk_null	,NULL },
};

static void
disk_null(int argc, char **argv)
{
}

static void
disk_help(int argc, char **argv)
{
	int i = 0;

	fputs("usage: diskutil command\n", stderr);
	fputs("commands:\n", stderr);
	while (cmdtab[i].cmd != NULL) {
		if (cmdtab[i].usage)
			fputs(cmdtab[i].usage, stderr);
		i++;
	}
}

static void
disk_list(int argc, char **argv)
{
}

static void
disk_info(int argc, char **argv)
{
}

static void
disk_mount(int argc, char **argv)
{

	if (argc != 6) {
		fputs("usage: diskutil mount -t vfstype device dir\n", stderr);
		exit(1);
	}
	if (mount(argv[4], argv[5], argv[3], 0, NULL) < 0) {
		perror("mount");
		exit(1);
	}
}

static void
disk_unmount(int argc, char **argv)
{
}

static void
disk_eject(int argc, char **argv)
{
}

static void
disk_rename(int argc, char **argv)
{
}

static void
disk_verify(int argc, char **argv)
{
}

static void
disk_partition(int argc, char **argv)
{
}

int
main(int argc, char *argv[])
{
	int i = 0;
	int found = 0;

	if (argc < 2) {
		disk_help(1, NULL);
		exit(1);
	}

	while (cmdtab[i].cmd != NULL) {
		if (!strncmp(argv[1], cmdtab[i].cmd, LINE_MAX)) {
			(cmdtab[i].func)(argc, argv);
			found = 1;
			break;
		}
		i++;
	}
	if (!found)
		disk_help(1, NULL);
	exit(1);
}
