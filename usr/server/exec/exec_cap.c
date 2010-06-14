/*-
 * Copyright (c) 2008, Kohsuke Ohtani
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
 * exec_cap.c - file capability management routines.
 */

#include <sys/prex.h>
#include <sys/capability.h>
#include <string.h>
#include <errno.h>

#include "exec.h"

/*
 * Bind capabilities for the known file.
 */
void
bind_cap(char *path, task_t task)
{
	const struct cap_map *map;
	cap_t cap = 0;
	int error;

	/*
	 * Set capabilities to the known applications.
	 */
	map = &cap_table[0];
	while (map->c_path != NULL) {
		if (!strncmp(path, map->c_path, PATH_MAX)) {
			cap = map->c_capset;
			break;
		}
		map++;
	}
	if (cap != 0) {
		DPRINTF(("exec: set capability:%08x to %s\n", cap, path));
		error = task_setcap(task, cap);
		if (error)
			sys_panic("exec: no SETPCAP capability");
	}
}

/*
 * Bind capability for server
 */
int
exec_bindcap(struct bind_msg *msg)
{
	task_t task;
	int error;

	task = msg->hdr.task;

	if (msg->path == NULL)
		return EFAULT;

	/*
	 * Check capability of caller task.
	 */
	error = task_chkcap(task, CAP_PROTSERV);
	if (error != 0)
		return EPERM;

	/*
	 * Set capability
	 */
	bind_cap(msg->path, task);

	return 0;
}
