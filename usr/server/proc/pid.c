/*
 * Copyright (c) 2005-2007, Kohsuke Ohtani
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
 * pid.c - service for process id.
 */

#include <prex/prex.h>
#include <server/proc.h>
#include <server/stdmsg.h>
#include <sys/list.h>

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "proc.h"

/*
 * PID previously allocated.
 *
 * Note: The following pids are reserved by default.
 *   pid = 0: Process server
 *   pid = 1: Init process
 */
static pid_t last_pid = 1;

/*
 * Assign new pid.
 * Returns pid on sucess, or 0 on failure.
 */
pid_t
pid_assign(void)
{
	pid_t pid;

	pid = last_pid + 1;
	if (pid >= PID_MAX)
		pid = 1;

	while (pid != last_pid) {
		if (!proc_find(pid))
			break;
		if (++pid >= PID_MAX)
			pid = 1;
	}
	if (pid == last_pid)
		return 0;
	last_pid = pid;
	return pid;
}

/*
 * Get pid for specified task
 */
int
proc_getpid(struct msg *msg)
{

	if (curproc == NULL)
		return ESRCH;

	msg->data[0] = (int)curproc->pid;
	return 0;
}

/*
 * Get ppid for specified task
 */
int
proc_getppid(struct msg *msg)
{

	if (curproc == NULL)
		return ESRCH;

	msg->data[0] = (int)curproc->parent->pid;
	return 0;
}

/*
 * Get pgid for specified process.
 */
int
proc_getpgid(struct msg *msg)
{
	pid_t pid;
	struct proc *p;

	pid = (pid_t)msg->data[0];
	if ((p = proc_find(pid)) == NULL)
		return ESRCH;

	msg->data[0] = (int)p->pgrp->pgid;
	dprintf("proc getpgid=%x\n", msg->data[0]);
	return 0;
}

/*
 * Set process group for specified process.
 */
int
proc_setpgid(struct msg *msg)
{
	pid_t pid, pgid;
	struct proc *p;
	struct pgrp *pgrp;

	pid = (pid_t)msg->data[0];
	pgid = (pid_t)msg->data[1];

	if (pgid < 0)
		return EINVAL;

	if ((p = proc_find(pid)) == NULL)
		return ESRCH;

	if (p->pgrp->pgid == pgid)
		return 0;

	pgrp = pgrp_find(pgid);
	if (pgrp == NULL) {
		/*
		 * Create new process group
		 */
		pgrp = malloc(sizeof(struct pgrp));
		if (pgrp == NULL)
			return ENOMEM;
		list_init(&pgrp->members);
		pgrp->pgid = pgid;
		pgrp_add(pgrp);
	}
	else {
		/*
		 * Remove from old process group
		 */
		list_remove(&p->pgrp_link);
	}
	list_insert(&pgrp->members, &p->pgrp_link);
	p->pgrp = pgrp;
	return 0;
}
