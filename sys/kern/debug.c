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

/* Type of an external print function */
typedef void (*print_func)(char *);

#ifdef CONFIG_KTRACE
static struct trace trace_buf[NR_TRACE];	/* Ring buffer */
static int trace_index;		/* Current trace index */
static int trace_mode = 1;	/* True if trace is enabled */
#endif

static char log_buf[LOGBUF_SIZE];	/* Buffer for message log */
static print_func plugin_print;		/* Alternate print handler */

/*
 * Print out the specified string with a variable argument.
 *
 * An actual output is displayed via the platform specific device by
 * diag_print() routine. In addition, the device driver can replace
 * the print routine by using debug_attach().
 *
 * All printk() inside the kernel are defined as a macro. The printk()
 * macro is compiled only when the debug option is enabled (NDEBUG=0).
 */
void printk(const char *fmt, ...)
{
	va_list args;

	irq_lock();
	va_start(args, fmt);

	vsprintf(log_buf, fmt, args);

	if (plugin_print)
		plugin_print(log_buf);
	else
		diag_print(log_buf);

	va_end(args);
	irq_unlock();
}

/*
 * Kernel assertion.
 *
 * assert() is called only when the expression is false in ASSERT() macro.
 * ASSERT() macro is compiled only when the debug option is enabled.
 * 
 * Note: Do not call this routine directly, but use ASSERT() macro instead.
 */
void assert(const char *file, int line, const char *func, const char *exp)
{
	irq_lock();
	printk("\nAssertion failed: %s line:%d in %s() '%s'\n",
	       file, line, func, exp);
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
	static const char line[] =
	    "\n=====================================================\n";

	irq_lock();

	printk(line);
	printk("Kernel panic!\n");

	va_start(args, fmt);
	vsprintf(log_buf, fmt, args);
	printk(log_buf);
	va_end(args);

	printk(line);

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
 * The function trace will log the function entry/exit of all kernel
 * routines. It is helpful to debug some types of the asynchronous
 * problem like a deadlock problem. To enable the kernel trace, set
 * the environment variable "KTRACE=1" before compiling the kernel.
 *
 * Please note that the kernel tracing will slow down the system
 * performance.
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
 *
 * TODO: Fixup the function address to human readable symbol.
 */
static void __attribute__ ((no_instrument_function)) trace_dump(void)
{
	int i, j, depth;
	int mode;

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
		if (trace_buf[i].type == 0)
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
 * Store information for the function enter & exit.
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
 *
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
 *
 * gcc automatically inserts the call for this profiling routine
 * when "-finstrument-functions" option is applied.
 */
void __attribute__ ((no_instrument_function))
     __cyg_profile_func_exit (void *this_fn, void *call_site)
{
	if (trace_mode)
		trace_log(FUNC_EXIT, this_fn);
	return;
}
#endif /* CONFIG_KTRACE */

/*
 * Dump kernel information.
 *
 * A keyboard driver may call this routine if a user presses
 * a predefined "dump" key.
 *
 * Since interrupt is locked in this routine, there is no need
 * to lock the interrupt/scheduler within the dump routine.
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
	case DUMP_TRACE:
#ifdef CONFIG_KTRACE
		trace_dump();
#endif /* CONFIG_KTRACE */
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
 * Attach an external print handler.
 */
void debug_attach(void (*func)(char *))
{
	ASSERT(func);
	plugin_print = func;
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
