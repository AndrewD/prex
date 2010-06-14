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
 * 3. Neither the name of the author nor the names of any co-contibutors
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
 * lock.c - terminal lock utility.
 *
 * Required capabilities:
 *      CAP_USRFILES
 */

#include <sys/stat.h>
#include <sys/fcntl.h>

#include <unistd.h>
#include <ctype.h>
#include <err.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <paths.h>

static void usage(void);
static void setpass(void);

int
main(int argc, char *argv[])
{
	char passcode[PASSWORD_LEN + 1];
	FILE *fp;
	int update = 0;
	char *p;

	if (argc == 2) {
		if (!strcmp(argv[1], "-u"))
			update = 1;
		else
			usage();
	}

	/*
	 * Load current passcode.
	 */
	if ((fp = fopen(_PATH_PASSWD, "r")) == NULL) {
		printf("Passcode is not set.\n");
		setpass();
	}
	if (((p = fgets(passcode, sizeof(passcode), fp)) == NULL) ||
	    (strlen(passcode) != 4)) {
		printf("Invalid passcode is set.\n");
		fclose(fp);
		setpass();
	}
	fclose(fp);

	if (update) {
		/*
		 * Update current passcode.
		 */
		if (strcmp(getpass("Old passcode:"), passcode)) {
			printf("Mismatch.\n");
			exit(0);
		}
		setpass();
	}

	/*
	 * Lock keyboard until correct passcode input.
	 */
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);

	printf("\33[2J");	/* clear screen */
	printf("Device is locked.\n");
	for (;;) {
		if (!strcmp(getpass("Enter passcode:"), passcode))
			break;
	}
	exit(0);
}

static void
usage(void)
{
	fputs("usage: lock [-u]\n", stderr);
	exit(1);
	/* NOTREACHED */
}

/*
 * We don't need to encrypt passcode file because it is stored
 * secure area in the file system.
 */
static void
setpass(void)
{
	FILE *fp;
	char *p, *t;
	char buf[PASSWORD_LEN + 1];

	for (buf[0] = '\0';;) {
		p = getpass("New passcode:");
		if (!*p) {
			printf("Passcode unchanged.\n");
			exit(0);
		}
		for (t = p; *t && isdigit(*t); ++t);
		if (strlen(p) != 4 || *t) {
			printf("Please enter 4 digit number for passcode.\n");
			continue;
		}
		strlcpy(buf, p, sizeof(buf));
		if (!strcmp(buf, getpass("Retype new passcode:")))
			break;
		printf("Mismatch; try again, EOF to quit.\n");
	}
	if ((fp = fopen(_PATH_PASSWD, "w+")) == NULL) {
		err(1, NULL);
		exit(0);
	}
	fputs(buf, fp);
	fclose(fp);
	exit(0);
}
