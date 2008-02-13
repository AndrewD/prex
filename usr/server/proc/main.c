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
 * Process server:
 *
 * A process server is responsible to handle process ID, group ID,
 * signal and fork()/exec() state. Since Prex microkernel does not
 * have the concept about process or process group, the process
 * server will map each Prex task to POSIX process.
 *
 * Prex does not support uid (user ID) and gid (group ID) because
 * it runs only in a single user mode. The value of uid and gid is
 * always returned as 1 for all process. These are handled by the
 * library stubs, and it is out of scope in this server.
 *
 * Important Notice:
 * This server is made as a single thread program to reduce many locks
 * and to keep the code clean. So, we should not block in the kernel
 * for any service. If some service must wait an event, it should wait
 * within the library stub in the client application.
 */

#include <prex/prex.h>
#include <server/proc.h>
#include <server/stdmsg.h>
#include <server/object.h>
#include <sys/list.h>

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "proc.h"

/* forward declarations */
static int proc_version(struct msg *msg);
static int proc_debug(struct msg *msg);
static int proc_shutdown(struct msg *msg);
static int proc_exec(struct msg *msg);
static int proc_pstat(struct msg *msg);
static int proc_register(struct msg *msg);
static int proc_setinit(struct msg *msg);

/*
 * Message mapping
 */
struct msg_map {
	int code;
	int (*func)(struct msg *);
};

static const struct msg_map procmsg_map[] = {
	{STD_VERSION,	proc_version},
	{STD_DEBUG,	proc_debug},
	{STD_SHUTDOWN,	proc_shutdown},
	{PS_GETPID,	proc_getpid},
	{PS_GETPPID,	proc_getppid},
	{PS_GETPGID,	proc_getpgid},
	{PS_SETPGID,	proc_setpgid},
	{PS_FORK,	proc_fork},
	{PS_EXIT,	proc_exit},
	{PS_STOP,	proc_stop},
	{PS_WAITPID,	proc_waitpid},
	{PS_KILL,	proc_kill},
	{PS_EXEC,	proc_exec},
	{PS_PSTAT,	proc_pstat},
	{PS_REGISTER,	proc_register},
	{PS_SETINIT,	proc_setinit},
	{0,		0},
};

static struct proc proc0;	/* process data of this server (pid=0) */
static struct pgrp pgrp0;	/* process group for process server */

struct proc initproc;		/* process slot for init process (pid=1) */
struct proc *curproc;		/* current (caller) process */
struct list allproc;		/* list of all processes */

static void
newproc(struct proc *p, pid_t pid, task_t task)
{

	p->parent = &proc0;
	p->pgrp = &pgrp0;
	p->stat = SRUN;
	p->exit_code = 0;
	p->wait_vfork = 0;
	p->pid = pid;
	p->task = task;
	list_init(&p->children);
	list_insert(&allproc, &p->link);
	proc_add(p);
	list_insert(&proc0.children, &p->sibling);
	list_insert(&pgrp0.members, &p->pgrp_link);
}

/*
 * exec() - Update pid to track the mapping with task id.
 *
 * The almost all work is done by a exec server for exec() emulation.
 * So, there is not so many jobs here...
 */
static int
proc_exec(struct msg *msg)
{
	task_t org_task, new_task;
	struct proc *p, *parent;

	dprintf("proc_exec: proc=%x\n", curproc);
	org_task = msg->data[0];
	new_task = msg->data[1];
	if ((p = task_to_proc(org_task)) == NULL)
		return EINVAL;

	proc_remove(p);
	p->task = new_task;
	proc_add(p);
	p->stack_base = (void *)msg->data[2];

	parent = p->parent;
	if (parent != NULL && parent->wait_vfork)
		vfork_end(parent);
	return 0;
}

/*
 * Get process status.
 */
static int
proc_pstat(struct msg *msg)
{
	task_t task;
	struct proc *p;

	dprintf("proc_pstat: task=%x\n", msg->data[0]);
	task = msg->data[0];
	if ((p = task_to_proc(task)) == NULL)
		return EINVAL;

	msg->data[0] = (int)p->pid;
	msg->data[2] = (int)p->stat;
	if (p->parent == NULL)
		msg->data[1] = (int)0;
	else
		msg->data[1] = (int)p->parent->pid;
	return 0;
}

/*
 * Set init process (pid=1).
 */
static int
proc_setinit(struct msg *msg)
{
	struct proc *p;

	p = &initproc;
	if (p->stat == SRUN)
		return EPERM;

	newproc(p, 1, msg->hdr.task);
	return 0;
}

/*
 * Register boot task.
 */
static int
proc_register(struct msg *msg)
{
	struct proc *p;
	pid_t pid;

	if ((p = malloc(sizeof(struct proc))) == NULL)
		return ENOMEM;

	if ((pid = pid_assign()) == 0)
		return EAGAIN;	/* Too many processes */

	newproc(p, pid, msg->hdr.task);
	return 0;
}

static int
proc_version(struct msg *msg)
{

	return 0;
}

static int
proc_shutdown(struct msg *msg)
{

	return 0;
}

static int
proc_debug(struct msg *msg)
{
#ifdef DEBUG_PROC
	struct proc *p;
	list_t n;
	char stat[][5] = { "RUN ", "ZOMB", "STOP" };

	printf("<Process Server>\n");
	printf("Dump process\n");
	printf(" pid    ppid   stat task\n");
	printf(" ------ ------ ---- --------\n");
	for (n = list_first(&allproc); n != &allproc;
	     n = list_next(n)) {
		p = list_entry(n, struct proc, link);
		printf(" %6d %6d %s %08x\n",
		       p->pid, p->parent->pid, stat[p->stat], p->task);
	}
	printf("\n");
#endif
	return 0;
}

static void
init(void)
{
	struct proc *p;

	tty_init();
	table_init();

	/*
	 * Setup a process for ourselves.
	 * pid=0 is always reserved by process server.
	 */
	p = &proc0;
	p->parent = 0;
	p->pgrp = &pgrp0;
	p->stat = SRUN;
	p->exit_code = 0;
	p->wait_vfork = 0;
	p->pid = 0;
	p->task = task_self();
	list_init(&p->children);
	list_init(&allproc);
	list_init(&pgrp0.members);
	proc_add(p);
	pgrp_add(&pgrp0);
	list_insert(&pgrp0.members, &p->pgrp_link);
}

/*
 * Main routine for process service.
 */
int
main(int argc, char *argv[])
{
	static struct msg msg;
	const struct msg_map *map;
	object_t obj;
	int err;

	sys_log("Starting Process Server\n");

	/*
	 * Boost current priority.
	 */
	thread_setprio(thread_self(), PRIO_PROC);

	/*
	 * Initialize everything.
	 */
	init();

	/*
	 * Create an object to expose our service.
	 */
	if ((err = object_create(OBJNAME_PROC, &obj)) != 0)
		sys_panic("proc: fail to create object");

	/*
	 * Message loop
	 */
	for (;;) {
		/*
		 * Wait for an incoming request.
		 */
		err = msg_receive(obj, &msg, sizeof(msg), 0);
		if (err)
			continue;

		err = EINVAL;
		map = &procmsg_map[0];
		while (map->code != 0) {
			if (map->code == msg.hdr.code) {

				/* Get current process */
				curproc = task_to_proc(msg.hdr.task);

				/* Update the capability of caller task. */
				if (curproc && task_getcap(msg.hdr.task,
							   &curproc->cap))
					break;

				err = map->func(&msg);
				break;
			}
			map++;
		}
		/*
		 * Reply to the client.
		 */
		msg.hdr.status = err;
		msg_reply(obj, &msg, sizeof(msg));
#ifdef DEBUG_PROC
		if (err)
			dprintf("msg code=%x error=%d\n", map->code, err);
#endif
	}
	return 0;
}
