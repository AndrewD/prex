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
#include <task.h>
#include <ipc.h>
#include <thread.h>
#include <device.h>
#include <page.h>
#include <kpage.h>
#include <kmem.h>
#include <vm.h>
#include <irq.h>

#ifdef DEBUG

#ifdef printk
#undef printk
#endif
#ifdef panic
#undef panic
#endif

#ifdef CONFIG_DMESG
#define LOG_SIZE	2048		/* size of ring buffer for log */
#define LOG_MASK	(LOG_SIZE-1)

static void log_save(char *buf);

static char log_buf[LOG_SIZE];		/* buffer for message log */
static u_long log_start;		/* start index of log_buf */
static u_long log_end;			/* end index of log_buf */
static u_long log_len;			/* length of logged char */
#endif

static char msg_buf[MSGBUFSZ];		/* temporary buffer for message */
static void (*alt_print)(char *);	/* alternate print handler */

/*
 * Print out the specified string with a variable argument.
 *
 * An actual output is displayed via the platform specific device by
 * diag_print() routine in kernel. As an alternate option, the device
 * driver can replace the print routine by using debug_attach().
 * All printk() inside the kernel are defined as a macro.
 * The printk() macro is compiled only when the debug option is
 * enabled (NDEBUG=0).
 */
void
printk(const char *fmt, ...)
{
	va_list args;

	irq_lock();
	va_start(args, fmt);
	vsprintf(msg_buf, fmt, args);
#ifdef CONFIG_DMESG
	log_save(msg_buf);
#endif
	if (alt_print != NULL)
		alt_print(msg_buf);
	else
		diag_print(msg_buf);
	va_end(args);
	irq_unlock();
}

/*
 * Kernel assertion.
 *
 * assert() is called only when the expression is false in ASSERT()
 * macro. ASSERT() macro is compiled only when the debug option is
 * enabled.
 */
void
assert(const char *file, int line, const char *exp)
{
	irq_lock();
	printk("\nAssertion failed: %s line:%d '%s'\n", file, line, exp);
	BREAKPOINT();
	for (;;)
		machine_idle();
	/* NOTREACHED */
}

/*
 * Kernel panic.
 *
 * panic() is called for a fatal kernel error. It shows specified
 * message, and stops CPU. If the kernel is not debug version,
 * panic() macro will reset the system instead of calling this
 * routine.
 */
void
panic(const char *fmt, ...)
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
		machine_idle();
	/* NOTREACHED */
}

#ifdef CONFIG_DMESG
/*
 * Save diag message to ring buffer
 */
static void
log_save(char *buf)
{
	char *p;

	for (p = buf; *p != '\0'; p++) {
		log_buf[log_end & LOG_MASK] = *p;
		log_end++;
		if (log_end - log_start > LOG_SIZE)
			log_start = log_end - LOG_SIZE;
		if (log_len < LOG_SIZE)
			log_len++;
	}
	/* Store end tag */
	log_buf[log_end & LOG_MASK] = -1;
}
#endif

/*
 * Return infomation about log
 */
int
log_get(char **buf, size_t *size)
{

#ifdef CONFIG_DMESG
	*buf = log_buf;
	*size = LOG_SIZE;
	return 0;
#else
	return -1;
#endif
}

#if defined(CONFIG_DMESG) && defined (CONFIG_KDUMP)
static void
log_dump(void)
{
	int i, len;
	u_long index;
	char c;

	index = log_start;
	len = log_len;
	if (log_len == LOG_SIZE) {
		/* Skip first line */
		while (log_buf[index & LOG_MASK] != '\n') {
			index++;
			len--;
		}
	}
	for (i = 0; i < len; i++) {
		c = log_buf[index & LOG_MASK];
		printk("%c", c);
		index++;
	}
}
#endif

/*
 * Dump system information.
 *
 * A keyboard driver may call this routine if a user presses
 * a predefined "dump" key.
 * Since interrupt is locked in this routine, there is no need
 * to lock the interrupt/scheduler in each dump function.
 */
int
debug_dump(int item)
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
		kpage_dump();
		page_dump();
		kmem_dump();
		vm_dump();
		break;
#ifdef CONFIG_DMESG
	case DUMP_MSGLOG:
		log_dump();
		break;
#endif
	case DUMP_BOOT:
		boot_dump();
		break;
	case DUMP_KSYM:
		ksym_dump();
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
void
debug_attach(void (*fn)(char *))
{
	ASSERT(fn);
	alt_print = fn;
}

#else /* !DEBUG */

/*
 * Stubs for the release build.
 */
int
debug_dump(int item)
{
	return ENOSYS;
}

void
debug_attach(void (*fn)(char *))
{
}
#endif /* !DEBUG */
