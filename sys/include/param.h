/*
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

#ifndef _PARAM_H
#define _PARAM_H

/*
 * Version
 */
#define SYSNAME		"Prex"
#define VERSION		0
#define PATCHLEVEL	5
#define SUBLEVEL	0

/*
 * Clock Rate
 */
#define HZ		CONFIG_HZ	/* Ticks per second */

/*
 * Page
 */
#define PAGE_SIZE	CONFIG_PAGE_SIZE	/* Bytes per page */

/*
 * Stack size
 */
#define USTACK_SIZE	4096	/* User stack size for boot tasks */
#define KSTACK_SIZE	2000	/* Kernel stack size */

/*
 * Scheduler
 */
#define TIME_SLICE	CONFIG_TIME_SLICE /* Context switch ratio (msec) */
#define NR_PRIO		256	/* Number of thread priority */
#define PRIO_TIMER	15	/* Priority for timer thread */
#define PRIO_INTERRUPT	16	/* Top priority for interrupt threads */
#define PRIO_DPC	33	/* Priority for Deferred Procedure Call */
#define PRIO_DEFAULT	200	/* Default priority for user threads */

/*
 * String
 */
#define MAX_OBJNAME	32	/* Max length of object name */
#define MAX_TASKNAME	12	/* Max length of task name */
#define MAX_DEVNAME	12	/* Max length of device name */

/*
 * Log
 */
#define LOGMSG_SIZE	128	/* Size of one message string */

/*
 * Security
 */
#define CAP_MASK	0xffffffff /* Capability mask for created task */

#endif /* !_PARAM_H */
