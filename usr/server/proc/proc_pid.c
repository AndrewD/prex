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

#include <sys/prex.h>
#include <ipc/proc.h>
#include <ipc/ipc.h>
#include <sys/list.h>

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "proc.h"

/*
 * getpid - get the process ID.
 */
pid_t
sys_getpid(void)
{

	ASSERT(curproc);
	return curproc->p_pid;
}

/*
 * getppid - get the parent process ID.
 */
pid_t
sys_getppid(void)
{

	ASSERT(curproc);
	return curproc->p_parent->p_pid;
}

/*
 * getpgid - get the process group ID for a process.
 *
 * If the specified pid is equal to 0, it returns the process
 * group ID of the calling process.
 */
int
sys_getpgid(pid_t pid, pid_t *retval)
{
	struct proc *p;

	ASSERT(curproc);

	if (pid == 0)
		p = curproc;
	else {
		if ((p = p_find(pid)) == NULL)
			return ESRCH;
	}
	*retval = p->p_pgrp->pg_pgid;
	return 0;
}

/*
 * getsid - get the process group ID of a session leader.
 */
int
sys_getsid(pid_t pid, pid_t *retval)
{
	pid_t sid;
	struct proc *p, *leader;

	ASSERT(curproc);

	if (pid == 0)
		p = curproc;
	else {
		if ((p = p_find(pid)) == NULL)
			return ESRCH;
	}
	leader = p->p_pgrp->pg_session->s_leader;
	sid = leader->p_pid;

	DPRINTF(("proc: getsid sid=%d\n", sid));
	*retval = sid;
	return 0;
}

/*
 * Move process to a new or existing process group.
 */
int
enterpgrp(struct proc *p, pid_t pgid)
{
	struct pgrp *pgrp;

	DPRINTF(("proc: enter pgrp pid=%d pgid=%d\n", p->p_pid, pgid));
	if ((pgrp = pg_find(pgid)) == NULL) {
		/*
		 * Create a new process group.
		 */
		DPRINTF(("proc: create new pgrp\n"));
		if ((pgrp = malloc(sizeof(struct pgrp))) == NULL)
			return ENOMEM;
		memset(pgrp, 0, sizeof(*pgrp));
		list_init(&pgrp->pg_members);
		pgrp->pg_pgid = pgid;
		pg_add(pgrp);
	}
	/*
	 * Remove from the old process group.
	 * And, join an existing process group.
	 */
	list_remove(&p->p_pgrp_link);
	list_insert(&pgrp->pg_members, &p->p_pgrp_link);
	pgrp->pg_session = curproc->p_pgrp->pg_session;
	p->p_pgrp = pgrp;
	return 0;
}

/*
 * Remove process from process group.
 */
int
leavepgrp(struct proc *p)
{
	struct pgrp *pgrp = p->p_pgrp;

	list_remove(&p->p_pgrp_link);
	if (list_empty(&pgrp->pg_members)) {

		/* XXX: do some work for session */

		pg_remove(pgrp);
		free(pgrp);
	}
	p->p_pgrp = 0;
	return (0);
}


/*
 * setpgid - set process group ID for job control.
 *
 * If the specified pid is equal to 0, the process ID of
 * the calling process is used. Also, if pgid is 0, the process
 * ID of the indicated process is used.
 */
int
sys_setpgid(pid_t pid, pid_t pgid)
{
	struct proc *p;

	DPRINTF(("proc: setpgid pid=%d pgid=%d\n", pid, pgid));

	if (pid == 0)
		p = curproc;
	else {
		if ((p = p_find(pid)) == NULL)
			return ESRCH;
	}
	if (pgid < 0)
		return EINVAL;
	if (pgid == 0)
		pgid = p->p_pid;
	if (p->p_pgrp->pg_pgid == pgid)	/* already leader */
		return 0;
	return (enterpgrp(p, pgid));
}

/*
 * setsid - create session and set process group ID.
 */
int
sys_setsid(pid_t *retval)
{
	struct proc *p;
	struct pgrp *pgrp;
	struct session *sess;
	int error;

	DPRINTF(("proc: setsid sid=%d\n", curproc->p_pid));

	p = curproc;
	if (p->p_pid == p->p_pgrp->pg_pgid)	/* already leader */
		return EPERM;

	if ((sess = malloc(sizeof(struct session))) == NULL)
		return ENOMEM;
	memset(sess, 0, sizeof(*sess));

	/*
	 * Create a new process group.
	 */
	if ((error = enterpgrp(p, p->p_pid)) != 0) {
		free(sess);
		return error;
	}
	pgrp = p->p_pgrp;

	/*
	 * Create a new session.
	 */
	sess->s_refcnt = 1;
	sess->s_leader = p;
	sess->s_ttyhold = 0;
	pgrp->pg_session = sess;

	*retval = p->p_pid;
	return 0;
}
