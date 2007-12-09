/*-
 * Copyright (c) 2005, Kohsuke Ohtani
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
 * syscall.c - system call table
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

#define SYSCALL_ENTRY(func) (syscall_func)(func)

const syscall_func syscall_table[] = {
	SYSCALL_ENTRY(object_create),		/* 0 */
	SYSCALL_ENTRY(object_delete),
	SYSCALL_ENTRY(object_lookup),
	SYSCALL_ENTRY(msg_send),
	SYSCALL_ENTRY(msg_receive),
	SYSCALL_ENTRY(msg_reply),
	SYSCALL_ENTRY(vm_allocate),
	SYSCALL_ENTRY(vm_free),
	SYSCALL_ENTRY(vm_attribute),
	SYSCALL_ENTRY(vm_map),
	SYSCALL_ENTRY(task_create),		/* 10 */
	SYSCALL_ENTRY(task_terminate),
	SYSCALL_ENTRY(task_self),
	SYSCALL_ENTRY(task_suspend),
	SYSCALL_ENTRY(task_resume),
	SYSCALL_ENTRY(task_name),
	SYSCALL_ENTRY(task_getcap),
	SYSCALL_ENTRY(task_setcap),
	SYSCALL_ENTRY(thread_create),
	SYSCALL_ENTRY(thread_terminate),
	SYSCALL_ENTRY(thread_load),		/* 20 */
	SYSCALL_ENTRY(thread_self),
	SYSCALL_ENTRY(thread_yield),
	SYSCALL_ENTRY(thread_suspend),
	SYSCALL_ENTRY(thread_resume),
	SYSCALL_ENTRY(thread_schedparam),
	SYSCALL_ENTRY(timer_sleep),
	SYSCALL_ENTRY(timer_alarm),
	SYSCALL_ENTRY(timer_periodic),
	SYSCALL_ENTRY(timer_waitperiod),
	SYSCALL_ENTRY(exception_setup),		/* 30 */
	SYSCALL_ENTRY(exception_return),
	SYSCALL_ENTRY(exception_raise),
	SYSCALL_ENTRY(exception_wait),
	SYSCALL_ENTRY(device_open),
	SYSCALL_ENTRY(device_close),
	SYSCALL_ENTRY(device_read),
	SYSCALL_ENTRY(device_write),
	SYSCALL_ENTRY(device_ioctl),
	SYSCALL_ENTRY(mutex_init),
	SYSCALL_ENTRY(mutex_destroy),		/* 40 */
	SYSCALL_ENTRY(mutex_lock),
	SYSCALL_ENTRY(mutex_trylock),
	SYSCALL_ENTRY(mutex_unlock),
	SYSCALL_ENTRY(cond_init),
	SYSCALL_ENTRY(cond_destroy),
	SYSCALL_ENTRY(cond_wait),
	SYSCALL_ENTRY(cond_signal),
	SYSCALL_ENTRY(cond_broadcast),
	SYSCALL_ENTRY(sem_init),
	SYSCALL_ENTRY(sem_destroy),		/* 50 */
	SYSCALL_ENTRY(sem_wait),
	SYSCALL_ENTRY(sem_trywait),
	SYSCALL_ENTRY(sem_post),
	SYSCALL_ENTRY(sem_getvalue),
	SYSCALL_ENTRY(sys_log),
	SYSCALL_ENTRY(sys_panic),
	SYSCALL_ENTRY(sys_stat),
	SYSCALL_ENTRY(sys_time),
	SYSCALL_ENTRY(sys_debug),
};
const int nr_syscalls = (int)(sizeof(syscall_table) / (sizeof(syscall_func)));
