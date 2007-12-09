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

/*
 * BREAKPOINT()
 *
 * Break into debugger.
 * This works only with debug kernel, and only when debugger is attached.
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
extern void printk_attach(void (*handler)(char *));
#else
#define printk(fmt...)	do {} while (0)
#endif

/*
 * panic()
 *
 * Reset CPU for fatal error.
 * If debugger is attached, break into it.
 */
#ifdef DEBUG
extern void panic(const char *fmt, ...);
#else
extern void system_reset(void);
#define panic(fmt...) system_reset()
#endif

/*
 * ASSERT(exp)
 *
 * If exp is false(zero), stop with source info.
 * This is enabled only when DEBUG build.
 * An asserion check should be used only for invalid kernel condition.
 * It can not be used to check the input argument from user mode because
 * kernel must not do panic for it. The kernel should return an appropriate
 * error code in such case.
 */
#ifdef DEBUG
extern void assert(const char *file, int line, const char *func,
		   const char *exp);
#define ASSERT(exp)	do { if (!(exp)) \
    assert(__FILE__, __LINE__, __FUNCTION__, #exp); } while (0)
#else
#define ASSERT(exp)	do {} while (0)
#endif

/*
 * IRQ_ASSERT()
 *
 * Assert if processor mode is H/W interrupt level.
 * Some routine can not be called during interrupt level.
 * It can be checked by putting this macro in such routine.
 */
#ifdef DEBUG
extern volatile int irq_nesting;
#define IRQ_ASSERT() do { if (irq_nesting > 0) \
    assert(__FILE__, __LINE__, __FUNCTION__, \
            "bad irq level"); } while (0)
#else
#define IRQ_ASSERT()	do {} while (0)
#endif

/*
 * Entry for kernel trace
 */
struct trace {
	int	type;		/* Logging type */
	void	*func;		/* Pointer to function */
};

/*
 * Logging types 
 */
#define FUNC_NONE  0
#define FUNC_ENTER 1
#define FUNC_EXIT  2


/*
 * Command for sys_debug()
 */
#define DBGCMD_DUMP	0

/*
 * Items for kernel_dump()
 */
#define DUMP_THREAD	1
#define DUMP_TASK	2
#define DUMP_OBJECT	3
#define DUMP_TIMER	4
#define DUMP_IRQ	5
#define DUMP_DEVICE	6
#define DUMP_VM		7
#define DUMP_TRACE	8

#ifdef DEBUG
extern void thread_dump();
extern void task_dump();
extern void object_dump();
extern void timer_dump();
extern void irq_dump();
extern void page_dump();
extern void device_dump();
extern void kmem_dump();
extern void vm_dump();

extern void boot_dump();
extern void memory_dump(void *phys, size_t size);
#endif /* !DEBUG */

extern int kernel_dump(int item);
extern void debug_init(void);
extern void debug_attach(void (*func)(char *));

#endif /* !_DEBUG_H */
