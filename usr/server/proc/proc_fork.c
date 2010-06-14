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
 * fork.c - fork() support
 */

#include <sys/prex.h>
#include <ipc/proc.h>
#include <sys/list.h>

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "proc.h"

static int vfork_start(struct proc *);

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
static pid_t
pid_alloc(void)
{
	pid_t pid;

	pid = last_pid + 1;
	if (pid >= PID_MAX)
		pid = 1;
	while (pid != last_pid) {
		if (p_find(pid) == NULL)
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
 * Create a new process.
 */
int
newproc(struct proc *p, pid_t pid, task_t task)
{
	struct pgrp *pg;

	pg = curproc->p_pgrp;

	if (pid == 0) {
		pid = pid_alloc();
		if (pid == 0) {
			/* Too many processes */
			return EAGAIN;
		}
	}
	/*
	 * make proc entry for new proc
	 */
	p->p_parent = curproc;
	p->p_pgrp = pg;
	p->p_stat = SRUN;
	p->p_exitcode = 0;
	p->p_pid = pid;
	p->p_task = task;
	p->p_vforked = 0;
	p->p_invfork = 0;

	list_init(&p->p_children);
	p_add(p);
	list_insert(&curproc->p_children, &p->p_sibling);
	list_insert(&pg->pg_members, &p->p_pgrp_link);
	list_insert(&allproc, &p->p_link);

	return 0;
}

/*
 * fork() support.
 *
 * It creates new process data and update all process relations.
 * The task creation and the thread creation are done by the
 * fork() library stub.
 */
int
sys_fork(task_t child, int vfork, pid_t *retval)
{
	struct proc *p;
	int error;

	DPRINTF(("proc: fork child=%x vfork=%d\n", child, vfork));

	if (vfork && curproc->p_invfork) {
		DPRINTF(("proc: vfork under vfork!\n"));
		return EINVAL;
	}

	if (task_to_proc(child) != NULL) {
		DPRINTF(("proc: process already exists\n"));
		return EINVAL;
	}

	if ((p = malloc(sizeof(struct proc))) == NULL)
		return ENOMEM;
	memset(p, 0, sizeof(*p));

	error = newproc(p, 0, child);
	if (error)
		return error;

	if (vfork) {
		vfork_start(curproc);
		p->p_invfork = 1;
	}

	DPRINTF(("proc: fork newpid=%d\n", p->p_pid));
	*retval = p->p_pid;
	return 0;
}

/*
 * Clean up all resource created by fork().
 */
void
cleanup(struct proc *p)
{
	struct proc *pp;

	DPRINTF(("proc: cleanup pid=%d\n", p->p_pid));
	pp = p->p_parent;
	list_remove(&p->p_sibling);
	list_remove(&p->p_pgrp_link);
	list_remove(&p->p_link);
	free(p);
}

static int
vfork_start(struct proc *p)
{
	void *stack;

	/*
	 * Save parent's stack
	 */
	DPRINTF(("proc: vfork_start stack=%x\n", p->p_stackbase));

	if (vm_allocate(p->p_task, &stack, DFLSTKSZ, 1) != 0) {
		DPRINTF(("proc: failed to allocate save stack\n"));
		return ENOMEM;
	}

	memcpy(stack, p->p_stackbase, DFLSTKSZ);
	p->p_stacksaved = stack;

	p->p_vforked = 1;
	return 0;
}

void
vfork_end(struct proc *p)
{

	DPRINTF(("proc: vfork_end org=%x saved=%x\n", p->p_stackbase,
		 p->p_stacksaved));
	/*
	 * Restore parent's stack
	 */
	memcpy(p->p_stackbase, p->p_stacksaved, DFLSTKSZ);
	vm_free(p->p_task, p->p_stacksaved);

	/*
	 * Resume parent
	 */
	p->p_vforked = 0;
	task_resume(p->p_task);
}
