/*
 * Copyright (c) 2005-2008, Kohsuke Ohtani
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
 * A process server is responsible to handle process ID, group
 * ID, signal and fork()/exec() state. Since Prex microkernel
 * does not have the concept about process or process group, the
 * process server will map each Prex task to POSIX process.
 *
 * Prex does not support uid (user ID) and gid (group ID) because
 * it runs only in a single user mode. The value of uid and gid is
 * always returned as 1 for all process. These are handled by the
 * library stubs, and it is out of scope in this server.
 *
 * Important Notice:
 * This server is made as a single thread program to reduce many
 * locks and to keep the code clean. So, we should not block in
 * the kernel for any service. If some service must wait an
 * event, it should wait within the library stub in the client
 * application.
 */

#include <sys/prex.h>
#include <sys/param.h>
#include <ipc/proc.h>
#include <ipc/ipc.h>
#include <ipc/exec.h>
#include <sys/list.h>

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "proc.h"

/* forward declarations */
static int proc_getpid(struct msg *);
static int proc_getppid(struct msg *);
static int proc_getpgid(struct msg *);
static int proc_setpgid(struct msg *);
static int proc_getsid(struct msg *);
static int proc_setsid(struct msg *);
static int proc_fork(struct msg *);
static int proc_exit(struct msg *);
static int proc_stop(struct msg *);
static int proc_waitpid(struct msg *);
static int proc_kill(struct msg *);
static int proc_exec(struct msg *);
static int proc_pstat(struct msg *);
static int proc_register(struct msg *);
static int proc_setinit(struct msg *);
static int proc_trace(struct msg *);
static int proc_boot(struct msg *);
static int proc_shutdown(struct msg *);
static int proc_noop(struct msg *);
static int proc_debug(struct msg *);

/*
 * Message mapping
 */
struct msg_map {
	int	code;
	int	(*func)(struct msg *);
};

static const struct msg_map procmsg_map[] = {
	{PS_GETPID,	proc_getpid},
	{PS_GETPPID,	proc_getppid},
	{PS_GETPGID,	proc_getpgid},
	{PS_SETPGID,	proc_setpgid},
	{PS_GETSID,	proc_getsid},
	{PS_SETSID,	proc_setsid},
	{PS_FORK,	proc_fork},
	{PS_EXIT,	proc_exit},
	{PS_STOP,	proc_stop},
	{PS_WAITPID,	proc_waitpid},
	{PS_KILL,	proc_kill},
	{PS_EXEC,	proc_exec},
	{PS_PSTAT,	proc_pstat},
	{PS_REGISTER,	proc_register},
	{PS_SETINIT,	proc_setinit},
	{PS_TRACE,	proc_trace},
	{STD_BOOT,	proc_boot},
	{STD_SHUTDOWN,	proc_shutdown},
	{STD_DEBUG,	proc_debug},
	{0,		proc_noop},
};

static struct proc proc0;	/* process data of this server (pid=0) */
static struct pgrp pgrp0;	/* process group for first process */
static struct session session0;	/* session for first process */

struct proc initproc;		/* process slot for init process (pid=1) */
struct proc *curproc;		/* current (caller) process */
struct list allproc;		/* list of all processes */

static int
proc_getpid(struct msg *msg)
{
	pid_t pid;

	pid = sys_getpid();

	msg->data[0] = (int)pid;
	return 0;
}

static int
proc_getppid(struct msg *msg)
{
	pid_t ppid;

	ppid = sys_getppid();

	msg->data[0] = (int)ppid;
	return 0;
}

static int
proc_getpgid(struct msg *msg)
{
	pid_t pid, pgid;
	int error;

	pid = (pid_t)msg->data[0];

	error = sys_getpgid(pid, &pgid);
	if (error)
		return error;

	msg->data[0] = (int)pgid;
	return 0;
}

static int
proc_setpgid(struct msg *msg)
{
	pid_t pid, pgid;

	pid = (pid_t)msg->data[0];
	pgid = (pid_t)msg->data[1];

	return sys_setpgid(pid, pgid);
}

static int
proc_getsid(struct msg *msg)
{
	pid_t pid, sid;
	int error;

	pid = (pid_t)msg->data[0];

	error = sys_getsid(pid, &sid);
	if (error)
		return error;

	msg->data[0] = (int)sid;
	return 0;
}

static int
proc_setsid(struct msg *msg)
{
	pid_t sid;
	int error;

	error = sys_setsid(&sid);
	if (error)
		return error;

	msg->data[0] = (int)sid;
	return 0;
}

static int
proc_fork(struct msg *msg)
{
	task_t child;
	int vfork;
	pid_t pid;
	int error;

	child = (task_t)msg->data[0];
	vfork = msg->data[1];

	error = sys_fork(child, vfork, &pid);
	if (error)
		return error;

	msg->data[0] = (int)pid;
	return 0;
}

static int
proc_exit(struct msg *msg)
{
	int exitcode;

	exitcode = msg->data[0];

	return sys_exit(exitcode);
}

static int
proc_stop(struct msg *msg)
{
	int exitcode;

	exitcode = msg->data[0];

	return stop(exitcode);
}

static int
proc_waitpid(struct msg *msg)
{
	pid_t pid, pid_child;
	int options, status, error;

	pid = (pid_t)msg->data[0];
	options = msg->data[1];

	error = sys_waitpid(pid, &status, options, &pid_child);
	if (error)
		return error;

	msg->data[0] = pid_child;
	msg->data[1] = status;

	return 0;
}

static int
proc_kill(struct msg *msg)
{
	pid_t pid;
	int sig;

	pid = (pid_t)msg->data[0];
	sig = msg->data[1];

	return sys_kill(pid, sig);
}

/*
 * exec() - Update pid to track the mapping with task id.
 * The almost all work is done by a exec server for exec()
 * emulation. So, there is not so many jobs here...
 */
static int
proc_exec(struct msg *msg)
{
	task_t orgtask, newtask;
	struct proc *p, *parent;

	DPRINTF(("proc: exec pid=%x\n", curproc->p_pid));

	orgtask = (task_t)msg->data[0];
	newtask = (task_t)msg->data[1];
	if ((p = task_to_proc(orgtask)) == NULL)
		return EINVAL;

	p_remove(p);
	p->p_task = newtask;
	p_add(p);
	p->p_invfork = 0;
	p->p_stackbase = (void *)msg->data[2];

	if (p->p_flag & P_TRACED) {
		DPRINTF(("proc: traced!\n"));
		sys_debug(DBGC_TRACE, (void *)newtask);
	}

	parent = p->p_parent;
	if (parent != NULL && parent->p_vforked)
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

	DPRINTF(("proc: pstat task=%x\n", msg->data[0]));

	task = (task_t)msg->data[0];
	if ((p = task_to_proc(task)) == NULL)
		return EINVAL;

	msg->data[0] = (int)p->p_pid;
	msg->data[2] = (int)p->p_stat;
	if (p->p_parent == NULL)
		msg->data[1] = (int)0;
	else
		msg->data[1] = (int)p->p_parent->p_pid;
	return 0;
}

/*
 * Set init process (pid=1).
 */
static int
proc_setinit(struct msg *msg)
{

	DPRINTF(("proc: setinit task=%x\n", msg->hdr.task));

	/* Check client's capability. */
	if (task_chkcap(msg->hdr.task, CAP_PROTSERV) != 0)
		return EPERM;

	if (initproc.p_stat == SRUN)
		return EPERM;

	curproc = &proc0;
	newproc(&initproc, 1, msg->hdr.task);

	return 0;
}

/*
 * Set trace flag
 */
static int
proc_trace(struct msg *msg)
{
	task_t task = msg->hdr.task;
	struct proc *p;

	DPRINTF(("proc: trace task=%x\n", task));

	if ((p = task_to_proc(task)) == NULL)
		return EINVAL;

	/* Toggle trace flag */
	p->p_flag ^= P_TRACED;
	return 0;
}

/*
 * Register boot task.
 */
static int
proc_register(struct msg *msg)
{
	struct proc *p;

	DPRINTF(("proc: register task=%x\n", msg->hdr.task));

	/* Check client's capability. */
	if (task_chkcap(msg->hdr.task, CAP_PROTSERV) != 0)
		return EPERM;

	if ((p = malloc(sizeof(struct proc))) == NULL)
		return ENOMEM;
	memset(p, 0, sizeof(struct proc));

	curproc = &proc0;
	if (newproc(p, 0, msg->hdr.task))
		sys_panic("proc: fail to register boot task");

	DPRINTF(("proc: register pid=%d\n", p->p_pid));
	return 0;
}

/*
 * Ready to boot
 */
static int
proc_boot(struct msg *msg)
{
	object_t obj;
	struct bind_msg m;

	DPRINTF(("proc: boot\n"));

	/* Check client's capability. */
	if (task_chkcap(msg->hdr.task, CAP_PROTSERV) != 0)
		return EPERM;

	/*
	 * Request exec server to bind an appropriate
	 * capability for us.
	 */
	if (object_lookup("!exec", &obj) != 0)
		sys_panic("proc: no exec found");
	m.hdr.code = EXEC_BINDCAP;
	strlcpy(m.path, "/boot/proc", sizeof(m.path));
	msg_send(obj, &m, sizeof(m));

	return 0;
}

static int
proc_shutdown(struct msg *msg)
{

	DPRINTF(("proc: shutdown\n"));
	return 0;
}

static int
proc_noop(struct msg *msg)
{

	return 0;
}

static int
proc_debug(struct msg *msg)
{
#ifdef DEBUG_PROC
	struct proc *p;
	list_t n;
	char stat[][5] = { "    ", "RUN ", "ZOMB", "STOP" };

	dprintf("<Process Server>\n");
	dprintf("Dump process\n");
	dprintf(" pid    ppid   pgid   sid    stat task\n");
	dprintf(" ------ ------ ------ ------ ---- --------\n");

	for (n = list_first(&allproc); n != &allproc;
	     n = list_next(n)) {
		p = list_entry(n, struct proc, p_link);
		dprintf(" %6d %6d %6d %6d %s %08x\n", p->p_pid,
			p->p_parent->p_pid, p->p_pgrp->pg_pgid,
			p->p_pgrp->pg_session->s_leader->p_pid,
			stat[p->p_stat], p->p_task);
	}
	dprintf("\n");
#endif
	return 0;
}

static void
proc_init(void)
{

	list_init(&allproc);
	tty_init();
	table_init();
}


/*
 * Initialize process 0.
 */
static void
proc0_init(void)
{
	struct proc *p;
	struct pgrp *pg;
	struct session *sess;

	p = &proc0;
	pg = &pgrp0;
	sess = &session0;

	pg->pg_pgid = 0;
	list_init(&pg->pg_members);
	pg_add(pg);

	pg->pg_session = sess;
	sess->s_refcnt = 1;
	sess->s_leader = p;
	sess->s_ttyhold = 0;

	p->p_parent = 0;
	p->p_pgrp = pg;
	p->p_stat = SRUN;
	p->p_exitcode = 0;
	p->p_pid = 0;
	p->p_task = task_self();
	p->p_vforked = 0;
	p->p_invfork = 0;

	list_init(&p->p_children);
	p_add(p);
	list_insert(&pg->pg_members, &p->p_pgrp_link);
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
	int error;

	sys_log("Starting process server\n");

	/* Boost thread priority. */
	thread_setpri(thread_self(), PRI_PROC);

	/* Initialize process and pgrp structures. */
	proc_init();

	/* Create process 0 (process server). */
	proc0_init();

	/* Create an object to expose our service. */
	if ((error = object_create("!proc", &obj)) != 0)
		sys_panic("proc: fail to create object");

	/*
	 * Message loop
	 */
	for (;;) {
		/*
		 * Wait for an incoming request.
		 */
		error = msg_receive(obj, &msg, sizeof(msg));
		if (error)
			continue;

		DPRINTF(("proc: msg code=%x task=%x\n",
			 msg.hdr.code, msg.hdr.task));

		error = EINVAL;
		map = &procmsg_map[0];
		while (map->code != 0) {
			if (map->code == msg.hdr.code) {

				/* Get current process */
				curproc = task_to_proc(msg.hdr.task);
				error = (*map->func)(&msg);
				break;
			}
			map++;
		}
		/*
		 * Reply to the client.
		 */
		msg.hdr.status = error;
		msg_reply(obj, &msg, sizeof(msg));
#ifdef DEBUG_PROC
		if (error) {
			DPRINTF(("proc: msg code=%x error=%d\n",
				 map->code, error));
		}
#endif
	}
}
