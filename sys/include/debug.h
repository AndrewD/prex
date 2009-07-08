/*-
 * Copyright (c) 2005-2006, Kohsuke Ohtani
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

#ifndef _DEBUG_H
#define _DEBUG_H

#define MSGBUFSZ	128 + MAXTHNAME	/* Size of one kernel message */

/*
 * BREAKPOINT()
 *
 * Break into debugger.
 * This works only with a debug version, and only when debugger
 * is attached.
 */
#ifdef DEBUG
#define BREAKPOINT()	breakpoint()
#else
#define BREAKPOINT()	do {} while (0)
#endif

/*
 * printk()
 *
 * Print out kernel message to the debug port.
 * The message is enabled only when DEBUG build.
 */
#ifdef DEBUG
extern void printk(const char *fmt, ...);
#else
#define printk(...)	do {} while (0)
#endif

/*
 * panic()
 *
 * Reset CPU for fatal error.
 * If debugger is attached, break into it.
 */
#ifdef DEBUG
extern void panic(const char *fmt, ...) __attribute__((noreturn));
#else
extern void machine_reset(void) __attribute__((noreturn));
#define panic(...) machine_reset()
#endif

/*
 * ASSERT(exp)
 *
 * If exp is false(zero), stop with source info.
 * This is enabled only when DEBUG build. An asserion check
 * should be used only for invalid kernel condition.
 * It can not be used to check the input argument from user
 * mode because kernel must not do panic for it. The kernel
 * should return an appropriate error code in such case.
 */
#ifdef DEBUG
extern void assert(const char *file, int line, const char *exp);
#define ASSERT(exp)	do { if (!(exp)) \
    assert(__FILE__, __LINE__, #exp); } while (0)
#else
#define ASSERT(exp)	do {} while (0)
#endif

/*
 * Command for sys_debug()
 */
#define DCMD_DUMP	0
#define DCMD_LOGSIZE	1
#define DCMD_GETLOG	2

/*
 * Items for debug_dump()
 */
#define DUMP_THREAD	1
#define DUMP_TASK	2
#define DUMP_OBJECT	3
#define DUMP_TIMER	4
#define DUMP_IRQ	5
#define DUMP_DEVICE	6
#define DUMP_VM		7
#define DUMP_MSGLOG	8
#define DUMP_TRACE	9
#define DUMP_BOOT	10
#define DUMP_KSYM	11

#ifdef DEBUG
extern void	 boot_dump(void);
extern void	 ksym_dump(void);
extern int	 log_get(char **, size_t *);
#endif
extern int	 debug_dump(int);
extern void	 debug_attach(void (*)(char *));

#endif /* !_DEBUG_H */
