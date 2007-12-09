/*-
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
 * syscalls.c - system call table
 */
#include <kernel.h>
#include <thread.h>
#include <timer.h>
#include <vm.h>
#include <task.h>
#include <except.h>
#include <ipc.h>
#include <device.h>
#include <sync.h>
#include <system.h>

typedef void (*syscall_func)(void);

#define SYSENT(func)	(syscall_func)(func)

const syscall_func syscall_table[] = {
	SYSENT(object_create),		/* 0 */
	SYSENT(object_delete),
	SYSENT(object_lookup),
	SYSENT(msg_send),
	SYSENT(msg_receive),
	SYSENT(msg_reply),
	SYSENT(vm_allocate),
	SYSENT(vm_free),
	SYSENT(vm_attribute),
	SYSENT(vm_map),
	SYSENT(task_create),		/* 10 */
	SYSENT(task_terminate),
	SYSENT(task_self),
	SYSENT(task_suspend),
	SYSENT(task_resume),
	SYSENT(task_name),
	SYSENT(task_getcap),
	SYSENT(task_setcap),
	SYSENT(thread_create),
	SYSENT(thread_terminate),
	SYSENT(thread_load),		/* 20 */
	SYSENT(thread_self),
	SYSENT(thread_yield),
	SYSENT(thread_suspend),
	SYSENT(thread_resume),
	SYSENT(thread_schedparam),
	SYSENT(timer_sleep),
	SYSENT(timer_alarm),
	SYSENT(timer_periodic),
	SYSENT(timer_waitperiod),
	SYSENT(exception_setup),	/* 30 */
	SYSENT(exception_return),
	SYSENT(exception_raise),
	SYSENT(exception_wait),
	SYSENT(device_open),
	SYSENT(device_close),
	SYSENT(device_read),
	SYSENT(device_write),
	SYSENT(device_ioctl),
	SYSENT(mutex_init),
	SYSENT(mutex_destroy),		/* 40 */
	SYSENT(mutex_lock),
	SYSENT(mutex_trylock),
	SYSENT(mutex_unlock),
	SYSENT(cond_init),
	SYSENT(cond_destroy),
	SYSENT(cond_wait),
	SYSENT(cond_signal),
	SYSENT(cond_broadcast),
	SYSENT(sem_init),
	SYSENT(sem_destroy),		/* 50 */
	SYSENT(sem_wait),
	SYSENT(sem_trywait),
	SYSENT(sem_post),
	SYSENT(sem_getvalue),
	SYSENT(sys_log),
	SYSENT(sys_panic),
	SYSENT(sys_info),
	SYSENT(sys_time),
	SYSENT(sys_debug),
};
const int nr_syscalls = (int)(sizeof(syscall_table) / (sizeof(syscall_func)));
