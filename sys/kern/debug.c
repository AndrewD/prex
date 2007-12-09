/*-
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

/*
 * debug.c - kernel debug services
 */

#include <kernel.h>
#include <irq.h>

#ifdef DEBUG

#ifdef printk
#undef printk
#endif
#ifdef panic
#undef panic
#endif

typedef void (* volatile print_func_t)(char *);

#ifdef CONFIG_DMESG
#define LOGBUF_SIZE	1024	/* Size of ring buffer for log */
#define LOGBUF_MASK	(LOGBUF_SIZE-1)

static void log_save(char *buf);
static void log_dump(void);

static char log_buf[LOGBUF_SIZE];	/* Buffer for message log */
static u_long log_start;		/* Start index of log_buf */
static u_long log_end;			/* End index of log_buf */
static u_long log_len;			/* Length of logged char */
#endif /* CONFIG_DMESG */

#ifdef CONFIG_KTRACE
#define NR_TRACE	128		/* Number of traced functions */

static struct trace_entry trace_buf[NR_TRACE];	/* Ring buffer */
static int trace_index;			/* Current trace index */
static int trace_mode = 1;		/* True if trace is enabled */
#endif /* CONFIG_KTRACE */

static char msg_buf[LOGMSG_SIZE];	/* Buffer for a message */
static print_func_t alt_print;		/* Alternate print handler */

/*
 * Print out the specified string with a variable argument.
 *
 * An actual output is displayed via the platform specific device by
 * diag_print() routine. In addition, the device driver can replace
 * the print routine by using debug_attach().
 * All printk() inside the kernel are defined as a macro. The printk()
 * macro is compiled only when the debug option is enabled (NDEBUG=0).
 */
void printk(const char *fmt, ...)
{
	va_list args;

	irq_lock();
	va_start(args, fmt);
	vsprintf(msg_buf, fmt, args);
#ifdef CONFIG_DMESG
	log_save(msg_buf);
#endif
	if (alt_print)
		alt_print(msg_buf);
	else
		diag_print(msg_buf);
	va_end(args);
	irq_unlock();
}

/*
 * Kernel assertion.
 *
 * assert() is called only when the expression is false in ASSERT() macro.
 * ASSERT() macro is compiled only when the debug option is enabled.
 */
void assert(const char *file, int line, const char *exp)
{
	irq_lock();
	printk("\nAssertion failed: %s line:%d '%s'\n", file, line, exp);
	BREAKPOINT();
	for (;;)
		cpu_idle();
	/* NOTREACHED */
}

/*
 * Kernel panic.
 *
 * panic() is called for a fatal kernel error. It shows specified
 * information, and stop CPU.
 * If the kernel is not debug version, it just resets the system.
 */
void panic(const char *fmt, ...)
{
	va_list args;

	irq_lock();
	printk("\nKernel panic: ");
	va_start(args, fmt);
	vsprintf(msg_buf, fmt, args);
	printk(msg_buf);
	va_end(args);
	printk("\n");
	irq_unlock();
	BREAKPOINT();
	for (;;)
		cpu_idle();
	/* NOTREACHED */
}

#ifdef CONFIG_KTRACE
/*
 * Kernel Trace:
 *
 *  The function trace will record the function entry/exit of all kernel
 *  routines. It is helpful to debug some types of the asynchronous
 *  problem like a deadlock problem. To enable the kernel trace, set
 *  the environment variable "KTRACE=1" before compiling the kernel.
 */

/*
 * Start function trace.
 */
void __attribute__ ((no_instrument_function)) trace_on(void)
{
	trace_mode = 1;
}

/*
 * End of function trace.
 */
void __attribute__ ((no_instrument_function)) trace_off(void)
{
	trace_mode = 0;
}

/*
 * Dump the latest function trace.
 */
static void __attribute__ ((no_instrument_function)) trace_dump(void)
{
	int i, j, depth, mode;

	irq_lock();
	printk("trace_dump\n");

	/* Save current trace mode and disable tracing. */
	mode = trace_mode;
	trace_mode = 0;

	/*
	 * Dump the trace log with time order.
	 */
	depth = 0;
	i = trace_index;
	do {
		if (++i >= NR_TRACE)
			i = 0;
		if (trace_buf[i].type == FUNC_NONE)
			continue;
		/*
		 * Format display column.
		 */
		if (trace_buf[i].type == FUNC_ENTER)
			depth++;
		for (j = 0; j < depth + 1; j++)
			printk("  ");
		if (trace_buf[i].type == FUNC_EXIT)
			depth--;
		/*
		 * Show function address.
		 */
		printk("%s %x\n",
		       (trace_buf[i].type == FUNC_ENTER) ? "Enter" : "Exit ",
		       trace_buf[i].func);

	} while (i != trace_index);

	trace_mode = mode;
	irq_unlock();
}

/*
 * Store information for the function entery & exit.
 */
static inline void __attribute__ ((no_instrument_function))
     trace_log(int type, void *func)
{
	trace_index = ((trace_index + 1) & (NR_TRACE - 1));
	trace_buf[trace_index].type = type;
	trace_buf[trace_index].func = func;
}

/*
 * This is called at the entry of all functions.
 * gcc automatically inserts the call for this profiling routine
 * when "-finstrument-functions" option is applied.
 */
void __attribute__ ((no_instrument_function))
     __cyg_profile_func_enter(void *this_fn, void *call_site)
{
	if (trace_mode)
		trace_log(FUNC_ENTER, this_fn);
	return;
}

/*
 * This is called at the exit of all functions.
 */
void __attribute__ ((no_instrument_function))
     __cyg_profile_func_exit (void *this_fn, void *call_site)
{
	if (trace_mode)
		trace_log(FUNC_EXIT, this_fn);
	return;
}
#endif /* CONFIG_KTRACE */

#ifdef CONFIG_DMESG
/*
 * Save diag message to ring buffer
 */
static void log_save(char *buf)
{
	char *p;

	for (p = buf; *p != '\0'; p++) {
		log_buf[log_end & LOGBUF_MASK] = *p;
		log_end++;
		if (log_end - log_start > LOGBUF_SIZE)
			log_start = log_end - LOGBUF_SIZE;
		if (log_len < LOGBUF_SIZE)
			log_len++;
	}

}

/*
 * Dump log buffer
 */
static void log_dump(void)
{
	int i, len;
	u_long index;
	char c;

	index = log_start;
	len = log_len;
	if (log_len == LOGBUF_SIZE) {
		/* Skip first line */
		while (log_buf[index & LOGBUF_MASK] != '\n') {
			index++;
			len--;
		}
	}
	for (i = 0; i < len; i++) {
		c = log_buf[index & LOGBUF_MASK];
		printk("%c", c);
		index++;
	}
}
#endif /* CONFIG_DMESG */

/*
 * Dump kernel information.
 *
 * A keyboard driver may call this routine if a user presses
 * a predefined "dump" key.
 * Since interrupt is locked in this routine, there is no need
 * to lock the interrupt/scheduler in each dump function.
 */
int kernel_dump(int item)
{
#ifdef CONFIG_KDUMP
	int err = 0;

	printk("\n");
	irq_lock();
	switch (item) {
	case DUMP_THREAD:
		thread_dump();
		break;
	case DUMP_TASK:
		task_dump();
		break;
	case DUMP_OBJECT:
		object_dump();
		break;
	case DUMP_TIMER:
		timer_dump();
		break;
	case DUMP_IRQ:
		irq_dump();
		break;
	case DUMP_DEVICE:
		device_dump();
		break;
	case DUMP_VM:
		page_dump();
		kmem_dump();
		vm_dump();
		break;
	case DUMP_MSGLOG:
#ifdef CONFIG_DMESG
		log_dump();
#endif
		break;
	case DUMP_TRACE:
#ifdef CONFIG_KTRACE
		trace_dump();
#endif
		break;

	default:
		err = 1;
		break;
	}
	irq_unlock();
	return err;
#else
	return ENOSYS;
#endif
}

/*
 * Attach an alternate print handler.
 * A device driver can hook the function to display message.
 */
void debug_attach(void (*func)(char *))
{
	ASSERT(func);
	alt_print = func;
}

#else /* !DEBUG */

/*
 * Stubs for the release build.
 */
int kernel_dump(int item)
{
	return ENOSYS;
}

void debug_attach(void (*func)(char *))
{
}
#endif /* !DEBUG */

void debug_init(void)
{
}
