/*
 * Copyright (c) 2007-2009, Kohsuke Ohtani
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

#ifndef _SYS_SYSINFO_H
#define _SYS_SYSINFO_H

#include <sys/param.h>
#include <sys/utsname.h>
#include <sys/capability.h>
#include <sys/device.h>

/*
 * Max size of info buffer.
 *
 * Please make sure MAXINFOSZ is still correct if you change
 * the information structure below.
 */
#define MAXINFOSZ	sizeof(struct kerninfo)

/*
 * Data type for sys_info()
 */
#define INFO_KERNEL	1
#define INFO_MEMORY	2
#define INFO_TIMER	3
#define INFO_THREAD	4
#define INFO_TASK	5
#define INFO_VM		6
#define INFO_DEVICE	7
#define INFO_IRQ	8

/*
 * Kernel information
 * Note: must be same with struct utsname.
 */
struct kerninfo {
	char	sysname[_SYS_NMLN];	/* name of this OS. */
	char	nodename[_SYS_NMLN];	/* name of this network node. */
	char	release[_SYS_NMLN];	/* release level. */
	char	version[_SYS_NMLN];	/* version level. */
	char	machine[_SYS_NMLN];	/* hardware type. */
};

/*
 * Memory information
 */
struct meminfo {
	psize_t		total;		/* total memory size in bytes */
	psize_t		free;		/* current free memory in bytes */
	psize_t		bootdisk;	/* total size of boot disk */
};

/*
 * Thread information
 */
struct threadinfo {
	u_long		cookie;		/* index cookie */
	thread_t	id;		/* thread id */
	int		state;		/* thread state */
	int		policy;		/* scheduling policy */
	int		priority;	/* current priority */
	int		basepri;	/* base priority */
	u_int		time;		/* total running time */
	int		suscnt;		/* suspend count */
	task_t		task;		/* task id */
	int		active;		/* true if active thread */
	char		taskname[MAXTASKNAME];	/* task name */
	char		slpevt[MAXEVTNAME];	/* sleep event */
};

/*
 * Task information
 */
struct taskinfo {
	u_long		cookie;		/* index cookie */
	task_t		id;		/* task id */
	int		flags;		/* task flags */
	int		suscnt;		/* suspend count */
	cap_t		capability;	/* security permission flag */
	size_t		vmsize;		/* used memory size */
	int		nthreads;	/* number of threads */
	int		active;		/* true if active task */
	char		taskname[MAXTASKNAME];	/* task name */
};

/*
 * VM information
 */
struct vminfo {
	u_long		cookie;		/* index cookie */
	task_t		task;		/* task id */
	vaddr_t		virt;		/* virtual address */
	size_t		size;		/* size */
	int		flags;		/* region flag */
	paddr_t		phys;		/* physical address */
};

/* Flags for vm */
#define VF_READ		0x00000001
#define VF_WRITE	0x00000002
#define VF_EXEC		0x00000004
#define VF_SHARED	0x00000008
#define VF_MAPPED	0x00000010
#define VF_FREE		0x00000080

/*
 * Device information
 */
struct devinfo {
	u_long		cookie;		/* index cookie */
	device_t	id;		/* device id */
	int	 	flags;		/* device characteristics flags */
	char	 	name[MAXDEVNAME]; /* device name */
};

/*
 * Timer informations
 */
struct timerinfo {
	int		hz;		/* clock frequency */
	u_long		cputicks;	/* total cpu ticks since boot */
	u_long		idleticks;	/* total idle ticks */
};

/*
 * IRQ information
 */
struct irqinfo {
	int		cookie;		/* index cookie */
	int		vector;		/* vector number */
	u_int		count;		/* interrupt count */
	int		priority;	/* interrupt priority */
	int		istreq;		/* pending ist request */
	thread_t	thread;		/* thread id of ist */
};

#endif /* !_SYS_SYSINFO_H */
