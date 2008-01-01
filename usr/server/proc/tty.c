/*
 * Copyright (c) 2007, Kohsuke Ohtani
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

#include <prex/prex.h>
#include <server/proc.h>
#include <sys/list.h>
#include <sys/ioctl.h>

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>

#include "proc.h"

static device_t ttydev;

static void
tty_signal(int code)
{
	pid_t pid;

	if (device_ioctl(ttydev, TIOCGPGRP, (u_long)&pid) != 0)
		return;
	kill_pg(pid, code);
}

static void
exception_handler(int code, void *regs)
{
	/*
	 * Handle signals from tty input.
	 */
	switch (code) {
	case SIGINT:
	case SIGQUIT:
	case SIGTSTP:
	case SIGTTIN:
	case SIGTTOU:
	case SIGINFO:
	case SIGWINCH:
	case SIGIO:
		if (ttydev != DEVICE_NULL)
			tty_signal(code);
		break;
	}
	exception_return(regs);
}

void
tty_init(void)
{
	task_t self;

	/*
	 * Setup exception to recieve signals from tty.
	 */
	exception_setup(exception_handler);

	if (device_open("tty", 0, &ttydev) != 0)
		ttydev = DEVICE_NULL;
	else {
		self = task_self();
		device_ioctl(ttydev, TIOCSETSIGT, (u_long)&self);
	}
}
