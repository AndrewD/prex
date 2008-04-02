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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/list.h>
#include <sys/syslog.h>
#include <server/proc.h>
#include <server/stdmsg.h>

#include <unistd.h>
#include <stdio.h>

#ifdef DEBUG
/* #define DEBUG_PROC 1 */
#endif

#ifdef DEBUG_PROC
#define dprintf(fmt, ...)	syslog(LOG_DEBUG, "proc: " fmt, ## __VA_ARGS__)
#else
#define dprintf(fmt, ...)	do {} while (0)
#endif

#define PRIO_PROC	130		/* priority of process server */
#define PID_MAX		0x8000		/* max number for PID */

#define ID_MAXBUCKETS	32
#define IDHASH(x)	((x) & (ID_MAXBUCKETS - 1))

/*
 * Process group
 */
struct pgrp {
	struct list	pgid_link;	/* link for pgid hash */
	struct list	members;	/* list head of processes */
	pid_t		pgid;		/* pgrp id */
};

/*
 * Description of a process.
 */
struct proc {
	struct list	link;		/* link for all processes */
	struct proc 	*parent;	/* pointer to parent process */
	struct list 	children;	/* list head of child processes */
	struct list 	sibling;	/* link for sibling processes */
	struct list 	pid_link;	/* link for pid hash */
	struct list 	task_link;	/* link for task hash */
	struct list 	pgrp_link;	/* link for process group */
	struct pgrp 	*pgrp;		/* pointer to process group */
	int		stat;		/* process status S* */
	int		exit_code;	/* exit code to send to parrent */
	int		wait_vfork;	/* true while processing vfork() */
	pid_t		pid;		/* process id */
	task_t		task;		/* task id */
	cap_t		cap;		/* capability of the task */
	void		*stack_base;	/* pointer to stack */
	void		*stack_saved;	/* pointer to saved stack */
};

/*
 * stat codes
 */
#define SRUN	1	/* running */
#define SZOMB	2	/* process terminated but not waited for */
#define SSTOP	3	/* proces stopped */

/*
 * Global variables.
 */
extern struct proc initproc;		/* process slot for init */
extern struct list allproc;		/* list of all processes */
extern struct proc *curproc;		/* current (caller) process */

extern pid_t pid_assign(void);
extern struct proc *proc_find(pid_t);
extern struct pgrp *pgrp_find(pid_t);
extern struct proc *task_to_proc(task_t);
extern void proc_add(struct proc *);
extern void proc_remove(struct proc *);
extern void pgrp_add(struct pgrp *);
extern void pgrp_remove(struct pgrp *);
extern void table_init(void);
extern void pid_init(void);
extern void proc_cleanup(struct proc *);
extern void vfork_end(struct proc *);
extern int kill_pg(pid_t, int);
extern void tty_init(void);

extern int proc_getpid(struct msg *);
extern int proc_getppid(struct msg *);
extern int proc_getpgid(struct msg *);
extern int proc_setpgid(struct msg *);
extern int proc_fork(struct msg *);
extern int proc_exit(struct msg *);
extern int proc_stop(struct msg *);
extern int proc_waitpid(struct msg *);
extern int proc_kill(struct msg *);
extern int proc_gettask(struct msg *);

#endif /* !_PROC_H */
