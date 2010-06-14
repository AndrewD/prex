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
 * main.c - bootstrap server
 */

/*
 * A bootstrap server works to setup the POSIX environment for
 * 'init' process. It sends a setup message to other servers in
 * order to let them know that this task becomes 'init' process.
 * The bootstrap server is gone after it launches (exec) the
 * 'init' process.
 */

#include <sys/prex.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/syslog.h>

#include <ipc/fs.h>
#include <ipc/exec.h>
#include <ipc/proc.h>
#include <ipc/ipc.h>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>

#ifdef DEBUG
#define DPRINTF(a) sys_log a
#else
#define DPRINTF(a)
#endif

static const char *initargs[] = { "1", NULL };
static const char *initenvs[] = { "TERM=vt100", "USER=root", NULL };

static char iobuf[BUFSIZ];

/*
 * Base directories at root.
 */
static char *base_dir[] = {
	"/bin",		/* applications */
	"/boot",	/* system servers */
	"/dev",		/* device files */
	"/etc",		/* shareable read-only data */
	"/mnt",		/* mount point for file systems */
	"/private",	/* user's private data */
	"/tmp",		/* temporary files */
	NULL
};

static void
wait_server(const char *name, object_t *pobj)
{
	int i, error = 0;

	/* Give chance to run other servers. */
	thread_yield();

	/*
	 * Wait for server loading. timeout is 1 sec.
	 */
	for (i = 0; i < 100; i++) {
		error = object_lookup((char *)name, pobj);
		if (error == 0)
			break;

		/* Wait 10msec */
		timer_sleep(10, 0);
		thread_yield();
	}
	if (error)
		sys_panic("boot: server not found");
}

static void
send_bootmsg(object_t obj)
{
	struct msg m;
	int error;

	m.hdr.code = STD_BOOT;
	error = msg_send(obj, &m, sizeof(m));
	if (error)
		sys_panic("boot: server error");
}

static void
mount_fs(void)
{
	char line[128];
	FILE *fp;
	char *spec, *file, *type, *p;
	char nodev[] = "";
	int i;

	DPRINTF(("boot: mounting file systems\n"));

	/*
	 * Mount root.
	 */
	if (mount("", "/", "ramfs", 0, NULL) < 0)
		sys_panic("boot: mount failed");

	/*
	 * Create some default directories.
	 */
	i = 0;
	while (base_dir[i] != NULL) {
		if (mkdir(base_dir[i], 0) == -1)
			sys_panic("boot: mkdir failed");
		i++;
	}

	/*
	 * Mount file system for /boot.
	 */
	if (mount("/dev/ram0", "/boot", "arfs", 0, NULL) < 0)
		sys_panic("boot: mount failed");

	/*
	 * Mount file systems described in fstab.
	 */
	if ((fp = fopen("/boot/fstab", "r")) == NULL)
		sys_panic("boot: no fstab");

	for (;;) {
		if ((p = fgets(line, sizeof(line), fp)) == NULL)
			break;
		spec = strtok(p, " \t\n");
		if (spec == NULL || *spec == '#')
			continue;
		file = strtok(NULL, " \t\n");
		type = strtok(NULL, " \t\n");
		if (!strcmp(file, "/") || !strcmp(file, "/boot"))
			continue;
		if (!strcmp(spec, "none"))
			spec = nodev;

		/* We create the mount point automatically */
		mkdir(file, 0);
		mount(spec, file, type, 0, 0);
	}
	fclose(fp);
}

static int
exec_init(object_t execobj)
{
	struct exec_msg msg;
	int error, i, argc, envc;
	size_t bufsz;
	char *dest;
	char const *src;

	DPRINTF(("boot: execute init\n"));

	/* Get arg/env buffer size */
	bufsz = 0;
	argc = 0;
	while (initargs[argc]) {
		bufsz += (strlen(initargs[argc]) + 1);
		argc++;
	}
	envc = 0;
	while (initenvs[envc]) {
		bufsz += (strlen(initenvs[envc]) + 1);
		envc++;
	}
	if (bufsz >= ARG_MAX)
		sys_panic("boot: args too long");

	/*
	 * Build exec message.
	 */
	dest = msg.buf;
	for (i = 0; i < argc; i++) {
		src = initargs[i];
		while ((*dest++ = *src++) != 0);
	}
	for (i = 0; i < envc; i++) {
		src = initenvs[i];
		while ((*dest++ = *src++) != 0);
	}
	msg.hdr.code = EXEC_EXECVE;
	msg.argc = argc;
	msg.envc = envc;
	msg.bufsz = bufsz;
	strlcpy(msg.cwd, "/", sizeof(msg.cwd));
	strlcpy(msg.path, "/boot/init", sizeof(msg.path));

	do {
		error = msg_send(execobj, &msg, sizeof(msg));
		/*
		 * If exec server can execute new process
		 * properly, it will terminate the caller task
		 * automatically. So, the control never comes
		 * here in that case.
		 */
	} while (error == EINTR);
	return -1;
}

static void
copy_file(char *src, char *dest)
{
	int fold, fnew, n;
	struct stat stbuf;
	mode_t mode;

	if ((fold = open(src, O_RDONLY)) == -1)
		return;

	fstat(fold, &stbuf);
	mode = stbuf.st_mode;

	if ((fnew = creat(dest, mode)) == -1) {
		close(fold);
		return;
	}
	while ((n = read(fold, iobuf, BUFSIZ)) > 0) {
		if (write(fnew, iobuf, (size_t)n) != n) {
			close(fold);
			close(fnew);
			return;
		}
	}
	close(fold);
	close(fnew);
}

int
main(int argc, char *argv[])
{
	object_t execobj, procobj, fsobj;
	struct bind_msg bm;
	struct msg m;

	sys_log("Starting bootstrap server\n");

	thread_setpri(thread_self(), PRI_DEFAULT);

	/*
	 * Wait until all required system servers
	 * become available.
	 */
	wait_server("!proc", &procobj);
	wait_server("!fs", &fsobj);
	wait_server("!exec", &execobj);

	/*
	 * Send boot message to all servers.
	 * This is required to synchronize the server
	 * initialization without deadlock.
	 */
	send_bootmsg(execobj);
	send_bootmsg(procobj);
	send_bootmsg(fsobj);

	/*
	 * Request to bind a new capabilities for us.
	 */
	bm.hdr.code = EXEC_BINDCAP;
	strlcpy(bm.path, "/boot/boot", sizeof(bm.path));
	msg_send(execobj, &bm, sizeof(bm));

	/*
	 * Register this process as 'init'.
	 * We will become an init process later.
	 */
	m.hdr.code = PS_SETINIT;
	msg_send(procobj, &m, sizeof(m));

	/*
	 * Initialize a library for file I/O.
	 */
	fslib_init();

	/*
	 * Mount file systems.
	 */
	mount_fs();

	/*
	 * Copy some files.
	 * Note that almost applications including 'init'
	 * does not have an access right to /boot directory...
	 */
	copy_file("/boot/rc", "/etc/rc");
	copy_file("/boot/fstab", "/etc/fstab");

	/*
	 * Exec first application.
	 */
	exec_init(execobj);

	sys_panic("boot: failed to exec init");

	/* NOTREACHED */
	return 0;
}
