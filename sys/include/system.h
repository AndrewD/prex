/*-
 * Copyright (c) 2007, Kohsuke Ohtani
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

#ifndef _SYSTEM_H
#define _SYSTEM_H

/*
 * Infomation type for sys_info()
 */
#define INFO_KERNEL	1
#define INFO_MEMORY	2
#define INFO_SCHED	3
#define INFO_THREAD	4
#define INFO_DEVICE	5

/*
 * Kernel infomation
 */
#define _SYS_NMLN	32

struct info_kernel {
	char	sysname[_SYS_NMLN];	/* Kernel name */
	char	nodename[_SYS_NMLN];	/* Obsolete data */
	char	release[_SYS_NMLN];	/* Release level */
	char	version[_SYS_NMLN];	/* Version level */
	char	machine[_SYS_NMLN];	/* Architecture/platform */
};

#define str2(x) #x
#define str(x) str2(x)

#define KERNEL_INFO(ki) \
{ \
	SYSNAME, \
	"Unknown", \
	str(VERSION) "." str(PATCHLEVEL) "." str(SUBLEVEL), \
	__DATE__ " " __TIME__, \
	str(__ARCH__) "-" str(__PLATFORM__) \
}

/*
 * Memory information
 */
struct info_memory {
	size_t	total;		/* Total memory size in bytes */
	size_t	free;		/* Current free memory size in bytes */
	size_t	kernel;		/* Memory size used by kernel in bytes */
};

/*
 * Scheduler infomation
 */
struct info_sched {
	u_long	system_ticks;	/* Ticks since boot time */
	u_long	idle_ticks;	/* Total tick count for idle */
	u_long	timer_hz;	/* Timer tick rate - HZ */
};

/*
 * Thread information
 */
struct info_thread {
	u_long	cookie;		/* Index cookie - 0 for first thread */
	int	state;		/* Thread state */
	int	policy;		/* Scheduling policy */
	int	prio;		/* Current priority */
	int	base_prio;	/* Base priority */
	int	sus_count;	/* Suspend counter */
	u_int	total_ticks;	/* Total running ticks */
	task_t	task;		/* Task ID */
	char	task_name[MAX_TASKNAME];	/* Task name */
};

/*
 * Device information
 */
struct info_device {
	u_long	cookie;		/* Index cookie - 0 for first thread */
	char	name[MAX_DEVNAME];
};

extern int sys_log(char *str);
extern int sys_panic(char *str);
extern int sys_info(int type, void *buf);
extern int sys_time(u_long *ticks);
extern int sys_debug(int cmd, int param);

#endif /* !_SYSTEM_H */
