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

#ifndef _PREX_H
#define _PREX_H

/*
 * Prex kernel interface
 */

#include <config.h>
#include <sys/types.h>
#include <sys/param.h>

typedef int object_t;
typedef int task_t;
typedef int thread_t;
typedef int device_t;
typedef int mutex_t;
typedef int cond_t;
typedef int sem_t;
typedef unsigned int cap_t;

extern int object_create(char *name, object_t *obj);
extern int object_delete(object_t obj);
extern int object_lookup(char *name, object_t *obj);

extern int msg_send(object_t obj, void *msg, size_t size);
extern int msg_receive(object_t obj, void *msg, size_t size);
extern int msg_reply(object_t obj, void *msg, size_t size);

extern int vm_allocate(task_t task, void **addr, size_t size, int anywhere);
extern int vm_free(task_t task, void *addr);
extern int vm_attribute(task_t task, void *addr, int attr);
extern int vm_map(task_t target, void  *addr, size_t size, void **alloc);

extern int task_create(task_t parent, int vm_inherit, task_t *child);
extern int task_terminate(task_t task);
extern task_t task_self(void);
extern int task_suspend(task_t task);
extern int task_resume(task_t task);
extern int task_name(task_t task, char *name);
extern int task_getcap(task_t task, cap_t *cap);
extern int task_setcap(task_t task, cap_t *cap);

extern int thread_create(task_t task, thread_t *th);
extern int thread_terminate(thread_t th);
extern int thread_load(thread_t th, void *entry, void *stack);
extern thread_t thread_self(void);
extern void thread_yield(void);
extern int thread_suspend(thread_t th);
extern int thread_resume(thread_t th);
extern int thread_getprio(thread_t th, int *prio);
extern int thread_setprio(thread_t th, int prio);
extern int thread_getpolicy(thread_t th, int *policy);
extern int thread_setpolicy(thread_t th, int policy);
extern int thread_getinterval(thread_t th, int *ticks);
extern int thread_setinterval(thread_t th, int ticks);

extern int timer_sleep(u_long delay, u_long *remain);
extern int timer_alarm(u_long delay, u_long *remain);
extern int timer_periodic(thread_t th, u_long start, u_long period);
extern int timer_waitperiod(void);

extern int exception_setup(void (*handler)(int, void *));
extern int exception_return(void *regs);
extern int exception_raise(task_t task, int excpt);
extern int exception_wait(int *excpt);

extern int device_open(char *name, int mode, device_t *dev);
extern int device_close(device_t dev);
extern int device_read(device_t dev, void *buf, size_t *nbyte, int blkno);
extern int device_write(device_t dev, void *buf, size_t *nbyte, int blkno);
extern int device_ioctl(device_t dev, int cmd, u_long arg);

extern int mutex_init(mutex_t *mu);
extern int mutex_destroy(mutex_t *mu);
extern int mutex_trylock(mutex_t *mu);
extern int mutex_lock(mutex_t *mu);
extern int mutex_unlock(mutex_t *mu);

extern int cond_init(cond_t *cond);
extern int cond_destroy(cond_t *cond);
extern int cond_wait(cond_t *cond, mutex_t *mu);
extern int cond_signal(cond_t *cond);
extern int cond_broadcast(cond_t *cond);

extern int sem_init(sem_t *sem, u_int value);
extern int sem_destroy(sem_t *sem);
extern int sem_wait(sem_t *sem, u_long timeout);
extern int sem_trywait(sem_t *sem);
extern int sem_post(sem_t *sem);
extern int sem_getvalue(sem_t *sem, u_int *value);

extern int sys_stat(int type, void *buf);
extern int sys_log(const char *);
extern void sys_panic(const char *);
extern int sys_time(u_long *ticks);
extern int sys_debug(int cmd, int param);

/* wrapper for sys_panic */
#ifdef DEBUG
extern void panic(const char *, ...);
#else
#define panic(fmt...) sys_panic(NULL);
#endif

/*
 * vm_inherit options for task_crate()
 */
#define VM_NONE		0
#define VM_SHARE	1
#define VM_COPY		2

/*
 * Task capabilities
 */
#define CAP_SETPCAP	0	/* Allow setting capability */
#define CAP_TASK	1	/* Allow controlling another task's execution */
#define CAP_MEMORY	2	/* Allow touching another task's memory  */
#define CAP_KILL	3	/* Allow raising exception to another task */
#define CAP_SEMAPHORE	4	/* Allow accessing another task's semaphore */
#define CAP_NICE	5	/* Allow changing scheduling parameter */
#define CAP_IPC		6	/* Allow accessing another task's IPC object */
#define CAP_DEVIO	7	/* Allow device I/O operation */
#define CAP_POWER	8	/* Allow power control including shutdown */
#define CAP_TIME	9	/* Allow setting system time */
#define CAP_RAWIO	10	/* Allow direct I/O access */
#define CAP_MOUNT	15	/* Allow mounting file system */
#define CAP_CHROOT	16	/* Allow changing root directory */

/*
 * attr flags for vm_attribute()
 */
#define ATTR_READ	0x01
#define ATTR_WRITE	0x02
#define ATTR_EXEC	0x04

/*
 * Device open mode for device_open()
 */
#define DO_RDONLY	0x0
#define DO_WRONLY	0x1
#define DO_RDWR		0x2
#define DO_RWMASK	0x3

/*
 * Scheduling policy
 */
#define SCHED_FIFO	0
#define SCHED_RR	1
#define SCHED_OTHER	2

/*
 * Stastics type for sys_stat()
 */
#define STAT_KERNEL	1
#define STAT_MEMORY	2
#define STAT_SCHED	3

/*
 * Exception code
 */
#define EXC_ILL		4	/* Illegal instruction (SIGILL) */
#define EXC_TRAP	5	/* Break point (SIGTRAP) */
#define EXC_FPE		8	/* Math error (SIGFPE) */
#define EXC_SEGV	11	/* Invalid memory access (SIGSEGV) */
#define EXC_ALRM	14	/* Alarm clock (SIGALRM) */

/*
 * Synch initializer
 */
#define MUTEX_INITIALIZER	(mutex_t)0x4d496e69
#define COND_INITIALIZER	(cond_t)0x43496e69

/*
 * Kernel stastics
 */
struct stat_kernel {
	char	name[16];	/* Kernel name string */
	u_int	version;	/* Kernel version */
	char	arch[16];	/* Architecture name string */
	char	platform[16];	/* Platform name string */
	char	release[16];	/* Release name */
	u_long	timer_hz;	/* Timer tick rate - HZ */
};

#define MAKE_KVER(ver, patch, sub) (((ver) << 16) | ((patch) << 8) | (sub))
#define KVER_VER(ver)	((ver) >> 16)
#define KVER_PATCH(ver)	(((ver) >> 8) & 0xff)
#define KVER_SUB(ver)	((ver) & 0xff)

/*
 * Memory stastics
 */
struct stat_memory {
	size_t total;		/* Total memory size in bytes */
	size_t free;		/* Current free memory size in bytes */
	size_t kernel;		/* Memory size used by kernel in bytes */
};

/*
 * Scheduler statstics
 */
struct stat_sched {
	u_long system_ticks;	/* Ticks since boot time */
	u_long idle_ticks;	/* Total tick count for idle */
};

/*
 * System debug service
 */
#define DBGCMD_DUMP	0	/* Kernel dump */


/*
 * Parameter for DBGCMD_DUMP
 */
#define DUMP_THREAD	1
#define DUMP_TASK	2
#define DUMP_OBJECT	3
#define DUMP_TIMER	4
#define DUMP_IRQ	5
#define DUMP_DEVICE	6
#define DUMP_VM		7
#define DUMP_TRACE	8

#endif /* !_PREX_H */
