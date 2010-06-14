/*
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

#include <sys/prex.h>
#include <ipc/exec.h>
#include <ipc/ipc.h>

#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

/*
 * Note:
 *
 * The state after exec() are as follows:
 * - Opened file descriptors remain open except FD_CLOEXEC flag is set.
 * - Opened directory streams are closed
 * - Signals set to the default action.
 * - Any asynchronous I/O operations are cancelled.
 */
int
execve(const char *path, char * const *argv, char * const *envp)
{
	object_t execobj;
	struct exec_msg msg;
	int error, i, argc, envc;
	size_t bufsz;
	char *dest, *src;

	if ((error = object_lookup("!exec", &execobj)) != 0)
		return ENOSYS;

	if (path == NULL)
		return EFAULT;
/*
    if (strlen(path) >= PATH_MAX)
        return ENAMETOOLONG;
*/

	/* Get arg/env buffer size */
	bufsz = 0;
	argc = 0;
	if (argv) {
		while (argv[argc]) {
			bufsz += (strlen(argv[argc]) + 1);
			argc++;
		}
	}
	envc = 0;
	if (envp) {
		while (envp[envc]) {
			bufsz += (strlen(envp[envc]) + 1);
			envc++;
		}
	}
	if (bufsz >= ARG_MAX)
		return E2BIG;

	dest = msg.buf;
	for (i = 0; i < argc; i++) {
		src = argv[i];
		while ((*dest++ = *src++) != 0);
	}
	for (i = 0; i < envc; i++) {
		src = envp[i];
		while ((*dest++ = *src++) != 0);
	}

	/* Request to exec server */
	msg.hdr.code = EXEC_EXECVE;
	msg.argc = argc;
	msg.envc = envc;
	msg.bufsz = bufsz;
	getcwd(msg.cwd, PATH_MAX);
	strlcpy(msg.path, path, PATH_MAX);
	do {
		error = msg_send(execobj, &msg, sizeof(msg));
	} while (error == EINTR);

	/*
	 * If exec() request is done successfully, control never comes here.
	 */
	errno = 0;
	if (error)
		errno = EIO;
	else if (msg.hdr.status)
		errno = msg.hdr.status;

	return -1;
}
