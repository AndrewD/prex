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
 * Exec server - Execute various types of image files.
 */

#include <sys/prex.h>
#include <sys/capability.h>
#include <ipc/fs.h>
#include <ipc/proc.h>
#include <ipc/ipc.h>
#include <sys/list.h>

#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include "exec.h"

/* forward declarations */
static int exec_null(struct msg *);
static int exec_debug(struct msg *);
static int exec_boot(struct msg *);
static int exec_shutdown(struct msg *);

/*
 * Message mapping
 */
struct msg_map {
	int	code;
	int	(*func)(struct msg *);
};

#define MSGMAP(code, fn) {code, (int (*)(struct msg *))fn}

static const struct msg_map execmsg_map[] = {
	MSGMAP(EXEC_EXECVE,	exec_execve),
	MSGMAP(EXEC_BINDCAP,	exec_bindcap),
	MSGMAP(STD_BOOT,	exec_boot),
	MSGMAP(STD_SHUTDOWN,	exec_shutdown),
	MSGMAP(STD_DEBUG,	exec_debug),
	MSGMAP(0,		exec_null),
};

static void
register_process(void)
{
	struct msg m;
	object_t obj;
	int error;

	error = object_lookup("!proc", &obj);
	if (error)
		sys_panic("exec: no proc");

	m.hdr.code = PS_REGISTER;
	msg_send(obj, &m, sizeof(m));
}

static int
exec_null(struct msg *msg)
{

	return 0;
}

static int
exec_boot(struct msg *msg)
{

	/* Check client's capability. */
	if (task_chkcap(msg->hdr.task, CAP_PROTSERV) != 0)
		return EPERM;

	/* Register to process server */
	register_process();

	/* Register to file server */
	fslib_init();

	return 0;
}

static int
exec_debug(struct msg *msg)
{

#ifdef DEBUG
	/* mstat(); */
#endif
	return 0;
}

static int
exec_shutdown(struct msg *msg)
{

	DPRINTF(("exec_shutdown\n"));
	return 0;
}

/*
 * Initialize all exec loaders.
 */
static void
exec_init(void)
{
	struct exec_loader *ldr;
	int i;

	for (i = 0; i < nloader; i++) {
		ldr = &loader_table[i];
		DPRINTF(("Initialize \'%s\' loader\n", ldr->el_name));
		ldr->el_init();
	}
}

static void
exception_handler(int sig)
{

	exception_return();
}

/*
 * Main routine for exec service.
 */
int
main(int argc, char *argv[])
{
	const struct msg_map *map;
	struct msg *msg;
	object_t obj;
	int error;

	sys_log("Starting exec server\n");

	/* Boost thread priority. */
	thread_setpri(thread_self(), PRI_EXEC);

	/*
	 * Set capability for us
	 */
	bind_cap("/boot/exec", task_self());

	/*
	 * Setup exception handler.
	 */
	exception_setup(exception_handler);

	/*
	 * Initialize exec loaders.
	 */
	exec_init();

	/*
	 * Create an object to expose our service.
	 */
	error = object_create("!exec", &obj);
	if (error)
		sys_panic("fail to create object");

	msg = malloc(MAX_EXECMSG);
	ASSERT(msg);

	/*
	 * Message loop
	 */
	for (;;) {
		/*
		 * Wait for an incoming request.
		 */
		error = msg_receive(obj, msg, MAX_EXECMSG);
		if (error)
			continue;

		error = EINVAL;
		map = &execmsg_map[0];
		while (map->code != 0) {
			if (map->code == msg->hdr.code) {
				error = (*map->func)(msg);
				break;
			}
			map++;
		}
#ifdef DEBUG_EXEC
		if (error)
			DPRINTF(("exec: msg error=%d code=%x\n",
				 error, msg->hdr.code));
#endif
		/*
		 * Reply to the client.
		 *
		 * Note: If EXEC_EXECVE request is handled successfully,
		 * the receiver task has been terminated here. But, we
		 * have to call msg_reply() even in such case to reset
		 * our IPC state.
		 */
		msg->hdr.status = error;
		error = msg_reply(obj, msg, MAX_EXECMSG);
	}
}
