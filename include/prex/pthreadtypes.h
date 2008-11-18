/*-
 * Copyright (c) 2008-2009, Andrew Dennison
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

#ifndef _PREX_PTHREADTYPES_H
#define _PREX_PTHREADTYPES_H
#ifndef __KERNEL__

typedef struct pthread_attr {
	int sched_priority;
	int sched_policy;
	int detached;
	unsigned long stacksize;
	unsigned long magic;
	char name[12];
	const void *key;	/* REVISIT: do keys better! */
} pthread_attr_t;

#if 0
typedef unsigned long	pthread_cond_t;
typedef unsigned long	pthread_condattr_t;
#endif
typedef unsigned long	pthread_key_t;
#if 0
typedef unsigned long	pthread_mutex_t;
typedef unsigned long	pthread_mutexattr_t;
typedef unsigned long	pthread_once_t;
typedef unsigned long	pthread_rwlock_t;
typedef unsigned long	pthread_rwlockattr_t;
#endif

struct pthread_info;
typedef struct pthread_info* pthread_t;

#endif /* __KERNEL__ */
#endif /* !_PREX_PTHREADTYPES_H */
