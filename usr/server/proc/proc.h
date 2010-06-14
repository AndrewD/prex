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

#ifndef _PROC_H
#define _PROC_H

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/list.h>
#include <ipc/proc.h>
#include <ipc/ipc.h>
#include <sys/capability.h>

#include <unistd.h>
#include <stdio.h>
#include <assert.h>

/* #define DEBUG_PROC 1 */

#ifdef DEBUG_PROC
#define DPRINTF(a)	dprintf a
#define ASSERT(e)	dassert(e)
#else
#define DPRINTF(a)
#define ASSERT(e)
#endif

#define PID_MAX		0x8000		/* max number for PID */

#define ID_MAXBUCKETS	32
#define IDHASH(x)	((x) & (ID_MAXBUCKETS - 1))

struct proc;

/*
 * Session
 */
struct session {
	int		s_refcnt;	/* reference count */
	struct proc	*s_leader;	/* session leader */
	int		s_ttyhold;	/* true if hold tty */
};


/*
 * Process group
 */
struct pgrp {
	struct list	pg_link;	/* link for pgid hash */
	struct list	pg_members;	/* list head of processes */
	struct session	*pg_session;	/* pointer to session */
	pid_t		pg_pgid;	/* pgrp id */
};

/*
 * Description of a process
 */
struct proc {
	struct list	p_link;		/* link for all processes */
	struct proc 	*p_parent;	/* pointer to parent process */
	struct list 	p_children;	/* list head of child processes */
	struct list 	p_sibling;	/* link for sibling processes */
	struct list 	p_pid_link;	/* link for pid hash */
	struct list 	p_task_link;	/* link for task hash */
	struct list 	p_pgrp_link;	/* link for process group */
	struct pgrp 	*p_pgrp;	/* pointer to process group */
	int		p_stat;		/* process status S* */
	int		p_flag;		/* P_* flags */
	int		p_exitcode;	/* exit code to send to parrent */
	int		p_vforked;	/* true while processing vfork() */
	int		p_invfork;	/* true if child of vfork() */
	pid_t		p_pid;		/* process id */
	task_t		p_task;		/* task id */
	void		*p_stackbase;	/* pointer to stack */
	void		*p_stacksaved;	/* pointer to saved stack */
};

/* Status values. */
#define SRUN	1	/* running */
#define SZOMB	2	/* process terminated but not waited for */
#define SSTOP	3	/* proces stopped */

/* These flags are kept in p_flags. */
#define	P_TRACED	0x00001		/* debugged process being traced. */

/*
 * Global variables.
 */
extern struct proc initproc;		/* process slot for init */
extern struct list allproc;		/* list of all processes */
extern struct proc *curproc;		/* current (caller) process */
extern int perrno;			/* errno */

__BEGIN_DECLS

/* pid.c */
pid_t	sys_getpid(void);
pid_t	sys_getppid(void);
int	sys_getpgid(pid_t, pid_t *);
int	sys_setpgid(pid_t, pid_t);
int	sys_getsid(pid_t, pid_t *);
int	sys_setsid(pid_t *);

/* fork.c */
int	newproc(struct proc *, pid_t, task_t);
int	sys_fork(task_t, int, pid_t *);
void	cleanup(struct proc *);
void	vfork_end(struct proc *);

/* exit.c */
int	sys_exit(int);
int	stop(int);
int	sys_waitpid(pid_t, int *, int, pid_t *);

/* kill.c */
int	sys_kill(pid_t pid, int sig);
int	kill_pg(pid_t, int);

/* hash.c */
struct proc *p_find(pid_t);
struct pgrp *pg_find(pid_t);
struct proc *task_to_proc(task_t);
void	p_add(struct proc *);
void	p_remove(struct proc *);
void	pg_add(struct pgrp *);
void	pg_remove(struct pgrp *);
void	table_init(void);

/* tty.c */
void	tty_init(void);

__END_DECLS

#endif /* !_PROC_H */
