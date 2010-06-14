/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)param.h	8.3 (Berkeley) 4/4/95
 */

#ifndef _SYS_PARAM_H_
#define _SYS_PARAM_H_

#include <conf/config.h>

#ifndef LOCORE
#include <sys/types.h>
#endif

/*
 * Machine-independent constants (some used in following include files).
 * Redefined constants are from POSIX 1003.1 limits file.
 */
#include <sys/syslimits.h>

#define	NCARGS		ARG_MAX		/* max bytes for an exec function */
#define	NGROUPS		NGROUPS_MAX	/* max number groups */
#define	NOFILE		OPEN_MAX	/* max open files per process */
#define	NOGROUP		65535		/* marker for empty group set member */
#define MAXHOSTNAMELEN	32		/* max hostname size */

#define	MAXTASKS	256		/* max number of tasks in system */
#define	MAXTHREADS	128		/* max number of threads per task */
#define	MAXOBJECTS	32		/* max number of objects per task */
#define	MAXSYNCS	512		/* max number of synch objects per task */
#define MAXMEM		(4*1024*1024)	/* max core per task - first # is Mb */

/* The following name length include a null-terminate character */
#define MAXTASKNAME	12		/* max task name */
#define MAXDEVNAME	12		/* max device name */
#define MAXOBJNAME	16		/* max object name */
#define MAXEVTNAME	12		/* max event name */

#define HZ		CONFIG_HZ	/* ticks per second */
#define MAXIRQS		32		/* max number of irq line */
#define	PASSWORD_LEN	4		/* fixed length, not counting NULL */

/*
 * Priorities.
 * Probably should not be altered too much.
 */
#define PRI_TIMER	15	/* priority for timer thread */
#define PRI_IST	 	16	/* top priority for interrupt threads */
#define PRI_DPC	 	33	/* priority for Deferred Procedure Call */
#define PRI_IDLE	255	/* priority for idle thread */
#define PRI_REALTIME	127	/* default priority for real-time thread */
#define PRI_DEFAULT	200	/* default user priority */

#define MAXPRI		0
#define MINPRI		255
#define NPRI		(MINPRI + 1)	/* number of thread priority */

/* Server priorities */
#define PRI_PROC	124	/* process server */
#define PRI_EXEC	125	/* exec server */
#define PRI_FS		126	/* file system server */
#define PRI_POW		100	/* power server */

#ifndef	NULL
#if !defined(__cplusplus)
#define	NULL	((void *)0)
#else
#define	NULL	0
#endif
#endif

/* More types and definitions used throughout the kernel. */
#ifdef KERNEL
#include <sys/cdefs.h>
#endif

#ifndef KERNEL
/* Signals. */
#include <sys/signal.h>
#endif

#include <machine/limits.h>
#include <machine/memory.h>

#define KSTACKSZ	768		/* kernel stack size */

#define USRSTACK	(0 + PAGE_SIZE)	/* base address of user stack */
#define DFLSTKSZ	4096		/* default size of user stack */

#ifdef CONFIG_MMU
#define user_area(a)	((vaddr_t)(a) <  (vaddr_t)USERLIMIT)
#else
#define user_area(a)	1
#endif

/* Address translation between physical address and kernel viritul address */
#define ptokv(pa)	(void *)((paddr_t)(pa) + KERNBASE)
#define kvtop(va)	((paddr_t)(va) - KERNBASE)

/*
 * Round p (pointer or byte index) up to a correctly-aligned value for all
 * data types (int, long, ...).   The result is u_long and must be cast to
 * any desired pointer type.
 */
#define	_ALIGNBYTES	(sizeof(int) - 1)
#define	ALIGN(p)	(((unsigned)(p) + _ALIGNBYTES) &~ _ALIGNBYTES)

/*
 * Memory page
 */
#define PAGE_MASK	(PAGE_SIZE-1)
#define trunc_page(x)	((x) & ~PAGE_MASK)
#define round_page(x)	(((x) + PAGE_MASK) & ~PAGE_MASK)

/*
 * MAXPATHLEN defines the longest permissable path length after expanding
 * symbolic links. It is used to allocate a temporary buffer from the buffer
 * pool in which to do the name expansion, hence should be a power of two,
 * and must be less than or equal to MAXBSIZE.  MAXSYMLINKS defines the
 * maximum number of symbolic links that may be expanded in a path name.
 * It should be set high enough to allow all legitimate uses, but halt
 * infinite loops reasonably quickly.
 */
#define	MAXPATHLEN	PATH_MAX
#define MAXSYMLINKS	8

/* Bit map related macros. */
#define	setbit(a,i)	((a)[(i)/NBBY] |= 1<<((i)%NBBY))
#define	clrbit(a,i)	((a)[(i)/NBBY] &= ~(1<<((i)%NBBY)))
#define	isset(a,i)	((a)[(i)/NBBY] & (1<<((i)%NBBY)))
#define	isclr(a,i)	(((a)[(i)/NBBY] & (1<<((i)%NBBY))) == 0)

/* Macros for counting and rounding. */
#ifndef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif
#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))
#define rounddown(x,y)  (((x)/(y))*(y))
#define powerof2(x)	((((x)-1)&(x))==0)

/* Macros for min/max. */
#define	MIN(a,b) (((a)<(b))?(a):(b))
#define	MAX(a,b) (((a)>(b))?(a):(b))

#define	BSIZE	512		/* size of secondary block (bytes) */

/*
 * Macro to convert milliseconds and tick.
 */
#define mstohz(ms)	(((ms) + 0UL) * HZ / 1000)

#define hztoms(tick)	((tick) >= 0x20000 ? \
			 (((tick) + 0u) / HZ) * 1000u : \
			 (((tick) + 0u) * 1000u) / HZ)

#endif /* !_SYS_PARAM_H_ */
