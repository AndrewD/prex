/*
 * Copyright (c) 2006-2009, Kohsuke Ohtani
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
 * shutdown.c - test shutdown function.
 *
 * NOTE: This program requires CAP_PWRMGMT capability.
 */

#include <sys/prex.h>
#include <sys/ioctl.h>
#include <ipc/pow.h>
#include <ipc/ipc.h>

#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

static void usage(void);
static void shutdown(int);

int
main(int argc, char *argv[])
{
	int ch, checkch, rflag;

	rflag = 0;
	if (argc > 1) {
		if (!strcmp(argv[1], "-r"))
			rflag = 1;	/* reboot */
		else {
			usage();
			exit(1);
		}
	}
	/*
	 * User confirmation is required for security.
	 */
	printf("Do you want to shutdown the system now? (y/n) ");
	checkch = ch = getchar();
	while (ch != '\n' && ch != EOF)
		ch = getchar();
	if (checkch != 'y')
		exit(0);

	shutdown(rflag);

	fprintf(stderr, "Shutdown failed!\n");
	exit(1);
}

static void
usage(void)
{

	fprintf(stderr, "usage: shutdown [-r]\n");
	exit(1);
	/* NOTREACHED */
}

static void
shutdown(int reboot)
{
	object_t powobj;
	struct msg m;
	int error;

	if ((error = object_lookup("!pow", &powobj)) != 0)
		return;

	m.hdr.code = POW_SET_POWER;
	m.data[0] = reboot ? PWR_REBOOT : PWR_OFF;
	error = msg_send(powobj, &m, sizeof(m));
}
