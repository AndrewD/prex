/*-
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
 * environ.c - test POSIX environment variable
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern char **environ;

static void
oops(void)
{
	printf("oops!\n");
	exit(1);
}

static void
show_env(void)
{
	char **envp = environ;
	int i;

	for (i = 0; envp[i] != NULL; i++) {
		printf("envp[%d]: %s\n", i, envp[i]);
	}
}

int
main(int argc, char *argv[])
{
	char *val;

	printf("test environment variables\n");

	/* Show environment passed by shell */
	printf("show env\n");
	show_env();

	/* Set new viriable */
	printf("setenv-1\n");
	setenv("PATH", "/boot", 1);
	show_env();

	/* Overwirte existing viriable */
	printf("setenv-2\n");
	setenv("PATH", "/boot:/bin", 1);
	show_env();

	/* No overwirte please... */
	printf("setenv-3\n");
	setenv("PATH", "/boot", 0);
	show_env();

	/* No overwirte please... */
	printf("setenv-4\n");
	setenv("PATH", "/boot", 0);
	show_env();

	/* Get non existing variable */
	printf("getenv-1\n");
	val = getenv("TMP");
	if (val != NULL)
		oops();

	/* Get existing variable */
	printf("getenv-2\n");
	val = getenv("PATH");
	if (val == NULL)
		oops();
	printf("PATH=%s\n", val);

	/* putenv */
	printf("putenv-1\n");
	putenv("TMP=/tmp");
	show_env();

	printf("putenv-2\n");
	putenv("ABC=/abc");
	show_env();

	exit(0);
}
