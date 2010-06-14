/*
 * Copyright (c) 2005-2009, Kohsuke Ohtani
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
 * syscall.h - system call number
 */

#ifndef _SYSCALL_H
#define _SYSCALL_H

#define SYS_exception_return	 0
#define SYS_exception_setup	 1
#define SYS_exception_raise	 2
#define SYS_exception_wait	 3
#define SYS_task_create		 4
#define SYS_task_terminate	 5
#define SYS_task_self		 6
#define SYS_task_suspend	 7
#define SYS_task_resume		 8
#define SYS_task_setname	 9
#define SYS_task_setcap		10
#define SYS_task_chkcap		11
#define SYS_thread_create	12
#define SYS_thread_terminate	13
#define SYS_thread_load		14
#define SYS_thread_self		15
#define SYS_thread_yield	16
#define SYS_thread_suspend	17
#define SYS_thread_resume	18
#define SYS_thread_schedparam	19
#define SYS_vm_allocate		20
#define SYS_vm_free		21
#define SYS_vm_attribute	22
#define SYS_vm_map		23
#define SYS_object_create	24
#define SYS_object_destroy	25
#define SYS_object_lookup	26
#define SYS_msg_send		27
#define SYS_msg_receive		28
#define SYS_msg_reply		29
#define SYS_timer_sleep		30
#define SYS_timer_alarm		31
#define SYS_timer_periodic	32
#define SYS_timer_waitperiod	33
#define SYS_device_open		34
#define SYS_device_close	35
#define SYS_device_read		36
#define SYS_device_write	37
#define SYS_device_ioctl	38
#define SYS_mutex_init		39
#define SYS_mutex_destroy	40
#define SYS_mutex_lock		41
#define SYS_mutex_trylock	42
#define SYS_mutex_unlock	43
#define SYS_cond_init		44
#define SYS_cond_destroy	45
#define SYS_cond_wait		46
#define SYS_cond_signal		47
#define SYS_cond_broadcast	48
#define SYS_sem_init		49
#define SYS_sem_destroy		50
#define SYS_sem_wait		51
#define SYS_sem_trywait		52
#define SYS_sem_post		53
#define SYS_sem_getvalue	54
#define SYS_sys_log		55
#define SYS_sys_panic		56
#define SYS_sys_info		57
#define SYS_sys_time		58
#define SYS_sys_debug		59

#endif /* _SYSCALL_H */
