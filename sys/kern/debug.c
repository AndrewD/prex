/*-
 * Copyright (c) 2005-2009, Kohsuke Ohtani
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
#include <sched.h>
#include <hal.h>
#include <sys/dbgctl.h>

typedef void (*diagfn_t)(char *);
typedef void (*abtfn_t)(void);

static abtfn_t	db_abort = &machine_abort;	/* abort handler */
static diagfn_t	db_puts = &diag_puts;		/* function to print string */
static char	db_msg[DBGMSGSZ];		/* debug message string */

static char	log_buf[LOGBUFSZ];		/* log buffer */
static u_long	log_head;			/* index for log head */
static u_long	log_tail;			/* iundex for log tail */
static u_long	log_len;			/* length of log */

#define LOGINDEX(x)	((x) & (LOGBUFSZ - 1))

/*
 * Scaled down version of C Library printf.
 * Only %s %u %d %c %x and zero pad flag are recognized.
 * Printf should not be used for chit-chat.
 */
void
printf(const char *fmt, ...)
{
	va_list args;
	int i, s;
	char c;

	s = splhigh();
	va_start(args, fmt);
	vsprintf(db_msg, fmt, args);

	(*db_puts)(db_msg);

	/*
	 * Record to log buffer.
	 */
	for (i = 0; i < DBGMSGSZ; i++) {
		c = db_msg[i];
		if (c == '\0')
			break;
		log_buf[LOGINDEX(log_tail)] = c;
		log_tail++;
		if (log_len < LOGBUFSZ)
			log_len++;
		else
			log_head = log_tail - LOGBUFSZ;
	}
	va_end(args);
	splx(s);
}

/*
 * Kernel assertion.
 */
void
assert(const char *file, int line, const char *exp)
{

	printf("Assertion failed: %s line:%d '%s'\n", file, line, exp);

	(*db_abort)();
	/* NOTREACHED */
}

/*
 * Kernel panic.
 */
void
panic(const char *msg)
{

	printf("Kernel panic: %s\n", msg);

	(*db_abort)();
	/* NOTREACHED */
}

/*
 * Copy log to the user's buffer.
 */
static int
getlog(char *buf)
{
	u_long cnt, len, i;
	int s, error = 0;
	char c;

	s = splhigh();
	i = log_head;
	len = log_len;
	if (len >= LOGBUFSZ) {
		/*
		 * Overrun found. Discard broken message.
		 */
		while (len > 0 && log_buf[LOGINDEX(i)] != '\n') {
			i++;
			len--;
		}
	}
	for (cnt = 0; cnt < LOGBUFSZ; cnt++) {
		if (cnt < len)
			c = log_buf[LOGINDEX(i)];
		else
			c = '\0';
		if (copyout(&c, buf, 1)) {
			error = EFAULT;
			break;
		}
		i++;
		buf++;
	}
	splx(s);
	return error;
}

/*
 * Debug control service.
 */
int
dbgctl(int cmd, void *data)
{
	int error = 0;
	size_t size;
	task_t task;
	struct diag_ops *dops;
	struct abort_ops *aops;

	switch (cmd) {
	case DBGC_LOGSIZE:
		size = LOGBUFSZ;
		error = copyout(&size, data, sizeof(size));
		break;

	case DBGC_GETLOG:
		error = getlog(data);
		break;

	case DBGC_TRACE:
		task = (task_t)data;
		if (task_valid(task)) {
			task->flags ^= TF_TRACE;
		}
		break;

	case DBGC_DUMPTRAP:
		context_dump(&curthread->ctx);
		break;

	case DBGC_SETDIAG:
		dops = data;
		db_puts = dops->puts;
		break;

	case DBGC_SETABORT:
		aops = data;
		db_abort = aops->abort;
		break;

	default:
		error = EINVAL;
		break;
	}
	return error;
}
