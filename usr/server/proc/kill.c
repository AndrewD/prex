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
 * kill.c - signal transfer.
 */

#include <prex/prex.h>
#include <prex/capability.h>
#include <server/proc.h>
#include <sys/list.h>

#include <unistd.h>
#include <errno.h>

#include "proc.h"

/*
 * Send a signal to the task.
 */
static int
send_sig(struct proc *proc, int sig)
{

	if (proc->pid == 0 || proc->pid == 1)
		return EPERM;
	return exception_raise(proc->task, sig);
}

/*
 * Send a signal to one process.
 */
static int
kill_one(pid_t pid, int sig)
{
	struct proc *p;

	dprintf("killone pid=%x sig=%d\n", pid, sig);
	if ((p = proc_find(pid)) == NULL)
		return ESRCH;
	return send_sig(p, sig);
}

/*
 * Send signal to all process in the process group.
 */
int
kill_pg(pid_t pgid, int sig)
{
	struct proc *p;
	struct pgrp *pgrp;
	list_t head, n;
	int err = 0;

	dprintf("killpg pgid=%x sig=%d\n", pgid, sig);

	if ((pgrp = pgrp_find(pgid)) == NULL)
		return ESRCH;

	head = &pgrp->members;
	for (n = list_first(head); n != head; n = list_next(n)) {
		p = list_entry(n, struct proc, pgrp_link);
		if ((err = send_sig(p, sig)) != 0)
			break;
	}
	return err;
}

/*
 * Send a signal.
 *
 * The behavior is different for the pid value.
 *
 *  if (pid > 0)
 *    Send a signal to specific process.
 *
 *  if (pid == 0)
 *    Send a signal to all processes in same process group.
 *
 *  if (pid == -1)
 *    Send a signal to all processes except init.
 *
 *  if (pid < -1)
 *     Send a signal to the process group.
 *
 * Note: Need CAP_KILL capability to send a signal to the different
 * process/group.
 */
int
proc_kill(struct msg *msg)
{
	pid_t pid;
	struct proc *p;
	list_t n;
	int sig, capable = 0;
	int err = 0;

	pid = (pid_t)msg->data[0];
	sig = msg->data[1];

	dprintf("kill pid=%x sig=%d\n", pid, sig);

	switch (sig) {
	case SIGFPE:
	case SIGILL:
	case SIGSEGV:
		return EINVAL;
	}

	if (curproc->cap & CAP_KILL)
		capable = 1;

	if (pid > 0) {
		if (pid != curproc->pid && !capable)
			return EPERM;
		err = kill_one(pid, sig);
	}
	else if (pid == -1) {
		if (!capable)
			return EPERM;
		for (n = list_first(&allproc); n != &allproc;
		     n = list_next(n)) {
			p = list_entry(n, struct proc, link);
			if (p->pid != 0 && p->pid != 1) {
				err = kill_one(p->pid, sig);
				if (err != 0)
					break;
			}
		}
	}
	else if (pid == 0) {
		if ((p = proc_find(pid)) == NULL)
			return ESRCH;
		err = kill_pg(p->pgrp->pgid, sig);
	}
	else {
		if (curproc->pgrp->pgid != -pid && !capable)
			return EPERM;
		err = kill_pg(-pid, sig);
	}
	return err;
}
