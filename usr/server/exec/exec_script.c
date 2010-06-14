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
 * exec_script.c - Script file loader
 */

#include <sys/prex.h>
#include <ipc/fs.h>
#include <ipc/proc.h>
#include <sys/param.h>

#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "exec.h"

static char interp[PATH_MAX];		/* interpreter name */
static char intarg[LINE_MAX];		/* argument to interpreter */
static char script[LINE_MAX];		/* script name */

/*
 * Load script file
 */
int
script_load(struct exec *exec)
{

	return 0;
}

/*
 * Probe script file
 */
int
script_probe(struct exec *exec)
{
	char *hdrstr = exec->header;
	char *p, *name;

	/* Check magic header */
	if ((hdrstr[0] != '#') || (hdrstr[1] != '!'))
		return PROBE_ERROR;

	/* Strip spaces before the interpriter name */
	for (p = hdrstr + 2; *p == ' ' || *p == '\t'; p++)
		;
	if (*p == '\0')
		return PROBE_ERROR;

	DPRINTF(("script_probe: found\n"));

	/* Pick up interpreter name */
	name = p;
	for (; *p != '\0' && *p != ' ' && *p != '\t'; p++)
		;
	*p++ = '\0';

	if (!strncmp(name, "/bin/sh", PATH_MAX)) {
		strlcpy(interp, "/boot/cmdbox", sizeof(interp));
		strlcpy(intarg, "sh", sizeof(intarg));
		exec->xarg1 = intarg;
		exec->xarg2 = script;
	} else {
		strlcpy(interp, name, sizeof(interp));
		exec->xarg1 = intarg;
		exec->xarg2 = NULL;
	}
	strlcpy(script, exec->path, sizeof(script));
	exec->path = interp;

	DPRINTF(("script_probe: interpreter=%s arg=%s script=%s\n",
		 interp, intarg, script));

	return PROBE_INDIRECT;
}

void
script_init(void)
{
}
