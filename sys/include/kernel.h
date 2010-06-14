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

#ifndef _KERNEL_H
#define _KERNEL_H

#include <conf/config.h>
#include <types.h>
#include <machine/stdarg.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <task.h>
#include <thread.h>
#include <version.h>
#include <debug.h>
#include <libkern.h>

#define __s(x) __STRING(x)

#define HOSTNAME	"Preky"
#define PROFILE		__s(CONFIG_PROFILE)
#define MACHINE		__s(CONFIG_MACHINE)
#define VERSION		__s(MAJORVERSION) "." __s(MINORVERSION) "." __s(PATCHLEVEL)

#define BANNER		"Prex version " VERSION PROFILE " for " MACHINE \
			" ("__DATE__ ")\n" \
			"Copyright (c) 2005-2009 Kohsuke Ohtani\n"

/*
 * Global variables in the kernel.
 */
extern struct thread	*curthread;	/* pointer to the current thread */
extern struct task	kernel_task;	/* kernel task */

#endif /* !_KERNEL_H */
