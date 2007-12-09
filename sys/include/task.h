/*-
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

#ifndef _TASK_H
#define _TASK_H

#include <list.h>
#include <timer.h>

#define TASK_MAGIC	0x54736b3f	/* 'Tsk?' */

typedef uint32_t cap_t;

/*
 * Task struct
 */
struct task {
	int		magic;		/* Magic number */
	char		name[MAX_TASKNAME]; /* Task name */
	struct list	link;		/* Link for all tasks in system */
	struct list	objects;	/* Objects owned by this task */
	struct list	threads;	/* Threads in this task */
	struct vm_map	*map;		/* Virtual memory map */
	int		sus_count;	/* Suspend counter */
	cap_t		capability;	/* Security permission flag */
	struct timer	alarm;		/* Alarm timer */
	void (*exc_handler)(int, u_long); /* Pointer to excepion handler */
};

typedef struct task *task_t;

#define cur_task()	(cur_thread->task)
#define task_valid(tsk) (kern_area(tsk) && ((tsk)->magic == TASK_MAGIC))
#define task_capable(cap) ((int)(cur_task()->capability & (1U << (cap))))

/*
 * Task capability
 */
#define CAP_SETPCAP	0	/* Allow setting capability */
#define CAP_TASK	1	/* Allow controlling another task's execution */
#define CAP_MEMORY	2	/* Allow touching another task's memory  */
#define CAP_KILL	3	/* Allow raising exception to another task */
#define CAP_SEMAPHORE	4	/* Allow accessing another task's semaphore */
#define CAP_NICE	5	/* Allow changing scheduling parameter */
#define CAP_IPC		6	/* Allow accessing another task's IPC object */
#define CAP_DEVIO	7	/* Allow device I/O operations */
#define CAP_POWER	8	/* Allow power control including shutdown */
#define CAP_TIME	9	/* Allow setting system time */
#define CAP_RAWIO	10	/* Allow direct I/O access */
#define CAP_DEBUG	11	/* Allow debugging reqeusts */

/*
 * vm_inherit options for task_create()
 */
#define VM_NONE		0	/* Create new memory map */
#define VM_SHARE	1	/* Share parent's memory map */
#define VM_COPY		2	/* Duplicate parent's memory map */

#define KERN_TASK(task) \
{ \
	.magic = TASK_MAGIC, \
	.name = "kernel", \
	.link = LIST_INIT(task.link), \
	.objects = LIST_INIT(task.objects), \
	.threads = LIST_INIT(task.threads), \
	.sus_count = 0, \
	.capability = 0xffffffff, \
	.exc_handler = NULL, \
}

extern void task_init(void);
extern void task_boot(void);
extern int __task_create(task_t parent, int vm_inherit, task_t *child);
extern int task_create(task_t parent, int vm_inherit, task_t *child);
extern int task_terminate(task_t task);
extern task_t task_self(void);
extern int task_suspend(task_t task);
extern int task_resume(task_t task);
extern int task_name(task_t task, char *name);
extern int task_getcap(task_t task, cap_t *cap);
extern int task_setcap(task_t task, cap_t *cap);
extern int __task_capable(cap_t cap);

#endif /* !_TASK_H */
