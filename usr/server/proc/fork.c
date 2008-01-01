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

#include <prex/prex.h>
#include <server/proc.h>
#include <sys/list.h>

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "proc.h"

static int vfork_start(struct proc *proc);

/*
 * fork() support.
 *
 * It creates new process data and update all process relations.
 * The task creation and the thread creation are done by the fork()
 * library stub.
 */
int
proc_fork(struct msg *msg)
{
	struct proc *p;
	struct pgrp *pgrp;
	task_t child;
	pid_t pid;
	int vfork;

	if (curproc == NULL)
		return EINVAL;

	child = (task_t)msg->data[0];
	vfork = msg->data[1];

	dprintf("fork: parent=%x child=%x vfork=%d\n",
	    msg->hdr.task, child, vfork);

	if (task_to_proc(child) != NULL)
		return EINVAL;	/* Process already exists */

	if ((pid = pid_assign()) == 0)
		return EAGAIN;	/* Too many processes */

	if ((p = malloc(sizeof(struct proc))) == NULL)
		return ENOMEM;

	p->parent = curproc;
	p->pgrp = curproc->pgrp;
	p->stat = SRUN;
	p->exit_code = 0;
	p->pid = pid;
	p->task = child;
	list_init(&p->children);
	proc_add(p);

	list_insert(&curproc->children, &p->sibling);

	pgrp = p->pgrp;
	list_insert(&pgrp->members, &p->pgrp_link);

	list_insert(&allproc, &p->link);

	if (vfork)
		vfork_start(curproc);

	dprintf("fork: new pid=%d\n", p->pid);
	msg->data[0] = (int)p->pid;
	return 0;
}

/*
 * Clean up all resource created by fork().
 */
void
proc_cleanup(struct proc *proc)
{
	struct proc *pp;

	pp = proc->parent;
	list_remove(&proc->sibling);
	list_remove(&proc->pgrp_link);
	proc_remove(proc);
	list_remove(&proc->link);
	free(proc);
}

static int
vfork_start(struct proc *proc)
{
	void *stack;

	/*
	 * Save parent's stack
	 */
	if (vm_allocate(proc->task, &stack, USTACK_SIZE, 1) != 0)
		return ENOMEM;

	memcpy(stack, proc->stack_base, USTACK_SIZE);
	proc->stack_saved = stack;

	proc->wait_vfork = 1;
	dprintf("vfork_start: saved=%x org=%x\n", stack, proc->stack_base);

	return 0;
}

void
vfork_end(struct proc *proc)
{

	dprintf("vfork_end: org=%x saved=%x\n", proc->stack_base,
		proc->stack_saved);
	/*
	 * Restore parent's stack
	 */
	memcpy(proc->stack_base, proc->stack_saved, USTACK_SIZE);
	vm_free(proc->task, proc->stack_saved);

	/*
	 * Resume parent
	 */
	proc->wait_vfork = 0;
	task_resume(proc->task);
}
