/*-
 * Copyright (c) 2005-2009 Kohsuke Ohtani
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
 * sysent.c - system call switch table.
 */

#include <kernel.h>
#include <thread.h>
#include <timer.h>
#include <vm.h>
#include <task.h>
#include <exception.h>
#include <ipc.h>
#include <device.h>
#include <sync.h>
#include <system.h>

typedef register_t (*sysfn_t)(register_t, register_t, register_t, register_t);

#ifdef DEBUG
static void strace_entry(register_t, register_t, register_t, register_t,
			 register_t);
static void strace_return(register_t, register_t);
#endif

struct sysent {
#ifdef DEBUG
	int	sy_narg;	/* number of arguments */
	char	*sy_name;	/* name string */
#endif
	sysfn_t	sy_call;	/* handler */
};

/*
 * Sysent initialization macros.
 *
 * Initialization macro for system calls which take their args
 * in the C style. In order to reduce the memory space, we
 * store the syscall name and its argument count only when
 * DEBUG is defined.
 *
 */
#ifdef DEBUG
#define SYSENT(n, fn)	{n, __STRING(fn), (sysfn_t)(fn)}
#else
#define SYSENT(n, fn)	{(sysfn_t)(fn)}
#endif

/*
 * This table is the switch used to transfer to the
 * appropriate routine for processing a system call.
 * The first element must be exception_return because
 * it requires special handling in HAL code.
 */
static const struct sysent sysent[] = {
	/*  0 */ SYSENT(0, exception_return),
	/*  1 */ SYSENT(1, exception_setup),
	/*  2 */ SYSENT(2, exception_raise),
	/*  3 */ SYSENT(1, exception_wait),
	/*  4 */ SYSENT(3, task_create),
	/*  5 */ SYSENT(1, task_terminate),
	/*  6 */ SYSENT(0, task_self),
	/*  7 */ SYSENT(1, task_suspend),
	/*  8 */ SYSENT(1, task_resume),
	/*  9 */ SYSENT(2, task_setname),
	/* 10 */ SYSENT(2, task_setcap),
	/* 11 */ SYSENT(2, task_chkcap),
	/* 12 */ SYSENT(2, thread_create),
	/* 13 */ SYSENT(1, thread_terminate),
	/* 14 */ SYSENT(3, thread_load),
	/* 15 */ SYSENT(0, thread_self),
	/* 16 */ SYSENT(0, thread_yield),
	/* 17 */ SYSENT(1, thread_suspend),
	/* 18 */ SYSENT(1, thread_resume),
	/* 19 */ SYSENT(3, thread_schedparam),
	/* 20 */ SYSENT(4, vm_allocate),
	/* 21 */ SYSENT(2, vm_free),
	/* 22 */ SYSENT(3, vm_attribute),
	/* 23 */ SYSENT(4, vm_map),
	/* 24 */ SYSENT(2, object_create),
	/* 25 */ SYSENT(1, object_destroy),
	/* 26 */ SYSENT(2, object_lookup),
	/* 27 */ SYSENT(3, msg_send),
	/* 28 */ SYSENT(3, msg_receive),
	/* 29 */ SYSENT(3, msg_reply),
	/* 30 */ SYSENT(2, timer_sleep),
	/* 31 */ SYSENT(2, timer_alarm),
	/* 32 */ SYSENT(3, timer_periodic),
	/* 33 */ SYSENT(0, timer_waitperiod),
	/* 34 */ SYSENT(3, device_open),
	/* 35 */ SYSENT(1, device_close),
	/* 36 */ SYSENT(4, device_read),
	/* 37 */ SYSENT(4, device_write),
	/* 38 */ SYSENT(3, device_ioctl),
	/* 39 */ SYSENT(1, mutex_init),
	/* 40 */ SYSENT(1, mutex_destroy),
	/* 41 */ SYSENT(1, mutex_lock),
	/* 42 */ SYSENT(1, mutex_trylock),
	/* 43 */ SYSENT(1, mutex_unlock),
	/* 44 */ SYSENT(1, cond_init),
	/* 45 */ SYSENT(1, cond_destroy),
	/* 46 */ SYSENT(2, cond_wait),
	/* 47 */ SYSENT(1, cond_signal),
	/* 48 */ SYSENT(1, cond_broadcast),
	/* 49 */ SYSENT(2, sem_init),
	/* 50 */ SYSENT(1, sem_destroy),
	/* 51 */ SYSENT(2, sem_wait),
	/* 52 */ SYSENT(1, sem_trywait),
	/* 53 */ SYSENT(1, sem_post),
	/* 54 */ SYSENT(2, sem_getvalue),
	/* 55 */ SYSENT(1, sys_log),
	/* 56 */ SYSENT(1, sys_panic),
	/* 57 */ SYSENT(2, sys_info),
	/* 58 */ SYSENT(1, sys_time),
	/* 59 */ SYSENT(2, sys_debug),
};

#define NSYSCALL	(int)(sizeof(sysent) / sizeof(sysent[0]))


/*
 * System call dispatcher.
 */
register_t
syscall_handler(register_t a1, register_t a2, register_t a3, register_t a4,
		register_t id)
{
	register_t retval = EINVAL;
	const struct sysent *callp;

#ifdef DEBUG
	strace_entry(a1, a2, a3, a4, id);
#endif

	if (id < NSYSCALL) {
		callp = &sysent[id];
		retval = (*callp->sy_call)(a1, a2, a3, a4);
	}

#ifdef DEBUG
	strace_return(retval, id);
#endif
	return retval;
}

#ifdef DEBUG
/*
 * Show syscall info if the task is being traced.
 */
static void
strace_entry(register_t a1, register_t a2, register_t a3, register_t a4,
	     register_t id)
{
	const struct sysent *callp;

	if (curtask->flags & TF_TRACE) {
		if (id >= NSYSCALL) {
			printf("%s: OUT OF RANGE (%d)\n",
			       curtask->name, id);
			return;
		}

		callp = &sysent[id];

		printf("%s: %s(", curtask->name, callp->sy_name);
		switch (callp->sy_narg) {
		case 0:
			printf(")\n");
			break;
		case 1:
			printf("0x%08x)\n", a1);
			break;
		case 2:
			printf("0x%08x, 0x%08x)\n", a1, a2);
			break;
		case 3:
			printf("0x%08x, 0x%08x, 0x%08x)\n",
			       a1, a2, a3);
			break;
		case 4:
			printf("0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			       a1, a2, a3, a4);
			break;
		}
	}
}

/*
 * Show status if syscall is failed.
 *
 * We ignore the return code for the function which does
 * not have any arguments, although timer_waitperiod()
 * has valid return code...
 */
static void
strace_return(register_t retval, register_t id)
{
	const struct sysent *callp;

	if (curtask->flags & TF_TRACE) {
		if (id >= NSYSCALL)
			return;
		callp = &sysent[id];
		if (callp->sy_narg != 0 && retval != 0)
			printf("%s: !!! %s() = 0x%08x\n",
				curtask->name, callp->sy_name, retval);
	}
}
#endif /* !DEBUG */
