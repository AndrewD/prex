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

#ifndef _EXCEPT_H
#define _EXCEPT_H

#include <task.h>

/*
 * You can not change the following exception count because we are
 * using 32bit bitmap to handle the exception (and signal) mask.
 */
#define NR_EXCEPTIONS	32	/* Number of exceptions */

/*
 * Exception from kernel
 */
#define EXC_ILL		4	/* Illegal instruction (SIGILL) */
#define EXC_TRAP	5	/* Break point (SIGTRAP) */
#define EXC_FPE		8	/* Math error (SIGFPE) */
#define EXC_SEGV	11	/* Invalid memory access (SIGSEGV) */
#define EXC_ALRM	14	/* Alarm clock (SIGALRM) */

extern int exception_setup(void (*handler)(int, u_long));
extern int exception_return(void *regs);
extern int exception_raise(task_t task, int exc);
extern int exception_wait(int *exc);
extern void exception_deliver(void);
extern void exception_post(int exc);

extern int __exception_raise(task_t task, int exc);

#endif /* !_EXCEPT_H */
