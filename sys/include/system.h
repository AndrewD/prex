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

#define STAT_KERNEL	1
#define STAT_MEMORY	2
#define STAT_SCHED	3

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

/*
 * Macro for kernel version
 */
#define MAKE_KVER(ver, patch, sub) (((ver) << 16) | ((patch) << 8) | (sub))
#define KVER_VER(ver)	((ver) >> 16)
#define KVER_PATCH(ver)	(((ver) >> 8) & 0xff)
#define KVER_SUB(ver)	((ver) & 0xff)

#define str2(x) #x
#define str(x) str2(x)

#define KERNEL_STAT(ks) \
{ \
	SYSNAME, \
	MAKE_KVER(VERSION, PATCHLEVEL, SUBLEVEL), \
	str(__ARCH__), \
	str(__PLATFORM__), \
	__DATE__, \
	HZ, \
}

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

extern int sys_log(char *str);
extern int sys_panic(char *str);
extern int sys_stat(int type, void *buf);
extern int sys_time(u_long *ticks);
extern int sys_debug(int cmd, int param);

#endif /* !_SYSTEM_H */
