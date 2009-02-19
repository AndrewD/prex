/*-
 * Copyright (c) 2005-2008, Kohsuke Ohtani
 * Copyright (c) 2009, Andrew Dennison
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
#include <thread.h>
#include <vm.h>
#include <irq.h>
#include <verbose.h>

#ifdef DEBUG

/*
 * diag_print() is provided by architecture dependent layer.
 */
typedef void (*prtfn_t)(char *);
static prtfn_t	print_func = &diag_print;

static char	dbg_msg[DBGMSG_SIZE + 12]; /* printf adds system time */

/*
 * dmesg support
 */
static char	log_buf[LOGBUF_SIZE];
static u_long	log_tail;

#define LOGINDEX(x)	((x) & (LOGBUF_SIZE - 1))

/*
 * Print out the specified string.
 *
 * An actual output is displayed via the platform
 * specific device by diag_print() routine in kernel.
 * As an alternate option, the device driver can
 * replace the print routine by using debug_attach().
 */
void
printf(const char *fmt, ...)
{
	va_list args;
	int i = 0, len;
	static int eol;

	irq_lock();
	if (eol && *fmt != '\n')
		i = sprintf(dbg_msg, "%3.3u ", timer_count());

	va_start(args, fmt);
	len = i + vsprintf(dbg_msg + i, fmt, args);
	va_end(args);

	eol = (dbg_msg[len-1] == '\n');

	/*
	 * Record to log buffer
	 */
	for (i = 0; i < len; i++) {
		log_buf[LOGINDEX(log_tail)] = dbg_msg[i];
		log_tail++;
	}

	/* Print out (can block if required) */
	(*print_func)(dbg_msg);

	irq_unlock();
}

/*
 * Kernel assertion.
 *
 * assert() is called only when the expression is false in
 * ASSERT() macro. ASSERT() macro is compiled only when
 * the debug option is enabled.
 */
void
assert(const char *file, int line, const char *exp)
{

	irq_lock();
	printf("\nAssertion failed: %s line:%d '%s'\n",
	       file, line, exp);
	for (;;)
		machine_idle();
	/* NOTREACHED */
}

/*
 * Kernel panic.
 *
 * panic() is called for a fatal kernel error. It shows
 * specified message, and stops CPU.
 */
void
panic(const char *msg)
{

	irq_lock();
	printf("\nKernel panic: %s\n", msg);
	irq_unlock();
	for (;;)
		machine_idle();
	/* NOTREACHED */
}

/*
 * Copy log to user buffer.
 */
int
debug_getlog(char *buf)
{
	u_long buf_len, len;
	static u_long head;
	int rc = 0;
	char c;

	irq_lock();
	len = log_tail - head;
	if (len == 0)
		goto out;

	if (len > LOGBUF_SIZE) {
		/*
		 * Overrun found. Discard broken message.
		 */
		len = LOGBUF_SIZE;
		head = log_tail - len;
		do {
			c = log_buf[LOGINDEX(head)];
			head++;
			len--;
		} while (len > 0 && c != '\n');
	}
	rc = len;

	buf_len = LOGBUF_SIZE - LOGINDEX(head);

	if (buf_len < len) {
		/* buffer wraps */
		if (umem_copyout(log_buf + LOGINDEX(head), buf, buf_len)) {
			rc = DERR(-EFAULT);
			goto out;
		}
		head += buf_len;
		buf += buf_len;
		len -= buf_len;
	}

	if (len) {
		if (umem_copyout(log_buf + LOGINDEX(head), buf, len))
			rc = DERR(-EFAULT);
		else
			head += len;
	}
out:
	irq_unlock();
	return rc;
}

/*
 * Get pointer and length for the log buffer.
 */
int
debug_getbuf(char **buf)
{
	u_long buf_len, len;
	static u_long head;
	char c;

	irq_lock();
	len = log_tail - head;
	if (len == 0)
		goto out;

	if (len > LOGBUF_SIZE) {
		/*
		 * Overrun found. Discard broken message.
		 */
		len = LOGBUF_SIZE;
		head = log_tail - len;
		do {
			c = log_buf[LOGINDEX(head)];
			head++;
			len--;
		} while (len > 0 && c != '\n');
	}
	buf_len = LOGBUF_SIZE - LOGINDEX(head);

	if (buf_len < len)
		len = buf_len;

	*buf = log_buf + LOGINDEX(head);
	head += len;
out:
	irq_unlock();
	return len;
}

/*
 * Dump system information.
 *
 * A keyboard driver may call this routine if a user
 * presses a predefined "dump" key.  Since interrupt is
 * locked in this routine, there is no need to lock the
 * interrupt/scheduler in each dump function.
 */
int
debug_dump(int item)
{
	int err = 0;

	irq_lock();
	switch (item) {
	case DUMP_THREAD:
		thread_dump();
		break;
	case DUMP_TASK:
		task_dump();
		break;
	case DUMP_VM:
		vm_dump();
		break;
	case DUMP_KSYM:
		ksym_dump();
		break;
	default:
		err = DERR(-EINVAL);
		break;
	}
	irq_unlock();
	return err;
}

/*
 * Attach to a print handler.
 * A device driver can hook the function to display message.
 */
void
debug_attach(void (*fn)(char *))
{
	ASSERT(fn);

	print_func = fn;
}
#endif /* !DEBUG */
