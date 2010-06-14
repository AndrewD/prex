/*
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
 * main.c - File system server
 */

/*
 * All file systems work as a sub-modules under VFS (Virtual File System).
 * The routines in this file have the responsible to the following jobs.
 *
 *  - Interpret the IPC message and pass the request into VFS routines.
 *  - Validate the some of passed arguments in the message.
 *  - Mapping of the task ID and cwd/file pointers.
 *
 * Note: All path string is translated to the full path before passing
 * it to the sys_* routines.
 */

#include <sys/prex.h>
#include <sys/capability.h>
#include <sys/param.h>
#include <ipc/fs.h>
#include <ipc/proc.h>
#include <ipc/exec.h>
#include <ipc/ipc.h>
#include <sys/list.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/file.h>

#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#include "vfs.h"

#ifdef DEBUG_VFS
int	vfs_debug = VFSDB_FLAGS;
#endif

/*
 * Message mapping
 */
struct msg_map {
	int	code;
	int	(*func)(struct task *, struct msg *);
};

#define MSGMAP(code, fn) {code, (int (*)(struct task *, struct msg *))fn}

/* object for file service */
static object_t fsobj;

static int
fs_mount(struct task *t, struct mount_msg *msg)
{
	int error = 0;

	/* Check client's mount capability. */
	if (task_chkcap(t->t_taskid, CAP_DISKADMIN) != 0)
		error = EPERM;

	if (error == 0) {
		error = sys_mount(msg->dev, msg->dir, msg->fs, msg->flags,
				(void *)msg->data);
	}
#ifdef DEBUG
	if (error)
		dprintf("VFS: mount failed!\n");
#endif
	return error;
}

static int
fs_umount(struct task *t, struct path_msg *msg)
{

	/* Check mount capability. */
	if (task_chkcap(t->t_taskid, CAP_DISKADMIN) != 0)
		return EPERM;

	return sys_umount(msg->path);
}

static int
fs_sync(struct task *t, struct msg *msg)
{

	return sys_sync();
}

static int
fs_open(struct task *t, struct open_msg *msg)
{
	char path[PATH_MAX];
	file_t fp;
	int fd, error;
	int acc;

	/* Find empty slot for file descriptor. */
	if ((fd = task_newfd(t)) == -1)
		return EMFILE;

	acc = 0;
	switch (msg->flags & O_ACCMODE) {
	case O_RDONLY:
		acc = VREAD;
		break;
	case O_WRONLY:
		acc = VWRITE;
		break;
	case O_RDWR:
		acc = VREAD | VWRITE;
		break;
	}
	if ((error = task_conv(t, msg->path, acc, path)) != 0)
		return error;

	if ((error = sys_open(path, msg->flags, msg->mode, &fp)) != 0)
		return error;

	t->t_ofile[fd] = fp;
	t->t_nopens++;
	msg->fd = fd;
	return 0;
}

static int
fs_close(struct task *t, struct msg *msg)
{
	file_t fp;
	int fd, error;

	fd = msg->data[0];
	if (fd >= OPEN_MAX)
		return EBADF;

	fp = t->t_ofile[fd];
	if (fp == NULL)
		return EBADF;

	if ((error = sys_close(fp)) != 0)
		return error;

	t->t_ofile[fd] = NULL;
	t->t_nopens--;
	return 0;
}

static int
fs_mknod(struct task *t, struct open_msg *msg)
{
	char path[PATH_MAX];
	int error;

	if ((error = task_conv(t, msg->path, VWRITE, path)) != 0)
		return error;

	return sys_mknod(path, msg->mode);
}

static int
fs_lseek(struct task *t, struct msg *msg)
{
	file_t fp;
	off_t offset, org;
	int error, type;

	if ((fp = task_getfp(t, msg->data[0])) == NULL)
		return EBADF;
	offset = (off_t)msg->data[1];
	type = msg->data[2];

	error = sys_lseek(fp, offset, type, &org);
	msg->data[0] = (int)org;
	return error;
}

static int
fs_read(struct task *t, struct io_msg *msg)
{
	file_t fp;
	void *buf;
	size_t size, bytes;
	int error;

	if ((fp = task_getfp(t, msg->fd)) == NULL)
		return EBADF;
	size = msg->size;
	if ((error = vm_map(msg->hdr.task, msg->buf, size, &buf)) != 0)
		return EFAULT;

	error = sys_read(fp, buf, size, &bytes);
	msg->size = bytes;
	vm_free(task_self(), buf);
	return error;
}

static int
fs_write(struct task *t, struct io_msg *msg)
{
	file_t fp;
	void *buf;
	size_t size, bytes;
	int error;

	if ((fp = task_getfp(t, msg->fd)) == NULL)
		return EBADF;
	size = msg->size;
	if ((error = vm_map(msg->hdr.task, msg->buf, size, &buf)) != 0)
		return EFAULT;

	error = sys_write(fp, buf, size, &bytes);
	msg->size = bytes;
	vm_free(task_self(), buf);
	return error;
}

static int
fs_ioctl(struct task *t, struct ioctl_msg *msg)
{
	file_t fp;

	if ((fp = task_getfp(t, msg->fd)) == NULL)
		return EBADF;

	return sys_ioctl(fp, msg->request, msg->buf);
}

static int
fs_fsync(struct task *t, struct msg *msg)
{
	file_t fp;

	if ((fp = task_getfp(t, msg->data[0])) == NULL)
		return EBADF;

	return sys_fsync(fp);
}

static int
fs_fstat(struct task *t, struct stat_msg *msg)
{
	file_t fp;
	struct stat *st;
	int error;

	if ((fp = task_getfp(t, msg->fd)) == NULL)
		return EBADF;

	st = &msg->st;
	error = sys_fstat(fp, st);
	return error;
}

static int
fs_opendir(struct task *t, struct open_msg *msg)
{
	char path[PATH_MAX];
	file_t fp;
	int fd, error;

	/* Find empty slot for file descriptor. */
	if ((fd = task_newfd(t)) == -1)
		return EMFILE;

	/* Get the mounted file system and node */
	if ((error = task_conv(t, msg->path, VREAD, path)) != 0)
		return error;

	if ((error = sys_opendir(path, &fp)) != 0)
		return error;
	t->t_ofile[fd] = fp;
	msg->fd = fd;
	return 0;
}

static int
fs_closedir(struct task *t, struct msg *msg)
{
	file_t fp;
	int fd, error;

	fd = msg->data[0];
	if (fd >= OPEN_MAX)
		return EBADF;
	fp = t->t_ofile[fd];
	if (fp == NULL)
		return EBADF;

	if ((error = sys_closedir(fp)) != 0)
		return error;
	t->t_ofile[fd] = NULL;
	return 0;
}

static int
fs_readdir(struct task *t, struct dir_msg *msg)
{
	file_t fp;

	if ((fp = task_getfp(t, msg->fd)) == NULL)
		return EBADF;

	return sys_readdir(fp, &msg->dirent);
}

static int
fs_rewinddir(struct task *t, struct msg *msg)
{
	file_t fp;

	if ((fp = task_getfp(t, msg->data[0])) == NULL)
		return EBADF;

	return sys_rewinddir(fp);
}

static int
fs_seekdir(struct task *t, struct msg *msg)
{
	file_t fp;
	long loc;

	if ((fp = task_getfp(t, msg->data[0])) == NULL)
		return EBADF;
	loc = msg->data[1];

	return sys_seekdir(fp, loc);
}

static int
fs_telldir(struct task *t, struct msg *msg)
{
	file_t fp;
	long loc;
	int error;

	if ((fp = task_getfp(t, msg->data[0])) == NULL)
		return EBADF;
	loc = msg->data[1];

	if ((error = sys_telldir(fp, &loc)) != 0)
		return error;
	msg->data[0] = loc;
	return 0;
}

static int
fs_mkdir(struct task *t, struct open_msg *msg)
{
	char path[PATH_MAX];
	int error;

	if ((error = task_conv(t, msg->path, VWRITE, path)) != 0)
		return error;

	return sys_mkdir(path, msg->mode);
}

static int
fs_rmdir(struct task *t, struct path_msg *msg)
{
	char path[PATH_MAX];
	int error;

	if (msg->path == NULL)
		return ENOENT;
	if ((error = task_conv(t, msg->path, VWRITE, path)) != 0)
		return error;

	return sys_rmdir(path);
}

static int
fs_rename(struct task *t, struct path_msg *msg)
{
	char src[PATH_MAX];
	char dest[PATH_MAX];
	int error;

	if (msg->path == NULL || msg->path2 == NULL)
		return ENOENT;

	if ((error = task_conv(t, msg->path, VREAD, src)) != 0)
		return error;

	if ((error = task_conv(t, msg->path2, VWRITE, dest)) != 0)
		return error;

	return sys_rename(src, dest);
}

static int
fs_chdir(struct task *t, struct path_msg *msg)
{
	char path[PATH_MAX];
	file_t fp;
	int error;

	if (msg->path == NULL)
		return ENOENT;
	if ((error = task_conv(t, msg->path, VREAD, path)) != 0)
		return error;

	/* Check if directory exits */
	if ((error = sys_opendir(path, &fp)) != 0)
		return error;
	if (t->t_cwdfp)
		sys_closedir(t->t_cwdfp);
	t->t_cwdfp = fp;
	strlcpy(t->t_cwd, path, sizeof(t->t_cwd));
 	return 0;
}

static int
fs_fchdir(struct task *t, struct msg *msg)
{
	file_t fp;
	int fd;

	fd = msg->data[0];
	if ((fp = task_getfp(t, fd)) == NULL)
		return EBADF;

	if (t->t_cwdfp)
		sys_closedir(t->t_cwdfp);
	t->t_cwdfp = fp;
	return sys_fchdir(fp, t->t_cwd);
}

static int
fs_link(struct task *t, struct msg *msg)
{
	/* XXX */
	return EPERM;
}

static int
fs_unlink(struct task *t, struct path_msg *msg)
{
	char path[PATH_MAX];
	int error;

	if (msg->path == NULL)
		return ENOENT;
	if ((error = task_conv(t, msg->path, VWRITE, path)) != 0)
		return error;

	return sys_unlink(path);
}

static int
fs_stat(struct task *t, struct stat_msg *msg)
{
	char path[PATH_MAX];
	struct stat *st;
	int error;

	error = task_conv(t, msg->path, 0, path);
	if (error == 0) {
		st = &msg->st;
		error = sys_stat(path, st);
	}
	return error;
}

static int
fs_getcwd(struct task *t, struct path_msg *msg)
{

	strlcpy(msg->path, t->t_cwd, sizeof(msg->path));
	return 0;
}

/*
 * Duplicate a file descriptor
 */
static int
fs_dup(struct task *t, struct msg *msg)
{
	file_t fp;
	int old_fd, new_fd;

	old_fd = msg->data[0];
	if ((fp = task_getfp(t, old_fd)) == NULL)
		return EBADF;

	/* Find smallest empty slot as new fd. */
	if ((new_fd = task_newfd(t)) == -1)
		return EMFILE;

	t->t_ofile[new_fd] = fp;

	/* Increment file reference */
	vref(fp->f_vnode);
	fp->f_count++;

	msg->data[0] = new_fd;
	return 0;
}

/*
 * Duplicate a file descriptor to a particular value.
 */
static int
fs_dup2(struct task *t, struct msg *msg)
{
	file_t fp, org;
	int old_fd, new_fd;
	int error;

	old_fd = msg->data[0];
	new_fd = msg->data[1];
	if (old_fd >= OPEN_MAX || new_fd >= OPEN_MAX)
		return EBADF;
	fp = t->t_ofile[old_fd];
	if (fp == NULL)
		return EBADF;
	org = t->t_ofile[new_fd];
	if (org != NULL) {
		/* Close previous file if it's opened. */
		error = sys_close(org);
	}
	t->t_ofile[new_fd] = fp;

	/* Increment file reference */
	vref(fp->f_vnode);
	fp->f_count++;

	msg->data[0] = new_fd;
	return 0;
}

/*
 * The file control system call.
 */
static int
fs_fcntl(struct task *t, struct fcntl_msg *msg)
{
	file_t fp;
	int arg, new_fd;

	if ((fp = task_getfp(t, msg->fd)) == NULL)
		return EBADF;

	arg = msg->arg;
	switch (msg->cmd) {
	case F_DUPFD:
		if (arg >= OPEN_MAX)
			return EINVAL;
		/* Find smallest empty slot as new fd. */
		if ((new_fd = task_newfd(t)) == -1)
			return EMFILE;
		t->t_ofile[new_fd] = fp;

		/* Increment file reference */
		vref(fp->f_vnode);
		fp->f_count++;
		msg->arg = new_fd;
		break;
	case F_GETFD:
		msg->arg = fp->f_flags & FD_CLOEXEC;
		break;
	case F_SETFD:
		fp->f_flags = (fp->f_flags & ~FD_CLOEXEC) |
			(msg->arg & FD_CLOEXEC);
		msg->arg = 0;
		break;
	case F_GETFL:
	case F_SETFL:
		msg->arg = -1;
		break;
	default:
		msg->arg = -1;
		break;
	}
	return 0;
}

/*
 * Check permission for file access
 */
static int
fs_access(struct task *t, struct path_msg *msg)
{
	char path[PATH_MAX];
	int acc, mode, error = 0;

	mode = msg->data[0];
	acc = 0;
	if (mode & R_OK)
		acc |= VREAD;
	if (mode & W_OK)
		acc |= VWRITE;

	if ((error = task_conv(t, msg->path, acc, path)) != 0)
		return error;

	return sys_access(path, mode);
}

/*
 * Copy parent's cwd & file/directory descriptor to child's.
 */
static int
fs_fork(struct task *t, struct msg *msg)
{
	struct task *newtask;
	file_t fp;
	int error, i;

	DPRINTF(VFSDB_CORE, ("fs_fork\n"));

	if ((error = task_alloc((task_t)msg->data[0], &newtask)) != 0)
		return error;

	/*
	 * Copy task related data
	 */
	newtask->t_cwdfp = t->t_cwdfp;
	strlcpy(newtask->t_cwd, t->t_cwd, sizeof(newtask->t_cwd));
	for (i = 0; i < OPEN_MAX; i++) {
		fp = t->t_ofile[i];
		newtask->t_ofile[i] = fp;
		/*
		 * Increment file reference if it's
		 * already opened.
		 */
		if (fp != NULL) {
			vref(fp->f_vnode);
			fp->f_count++;
		}
	}
	if (newtask->t_cwdfp)
		newtask->t_cwdfp->f_count++;
	/* Increment cwd's reference count */
	if (newtask->t_cwdfp)
		vref(newtask->t_cwdfp->f_vnode);

	DPRINTF(VFSDB_CORE, ("fs_fork-complete\n"));
	return 0;
}

/*
 * fs_exec() is called for POSIX exec().
 * It closes all directory stream.
 * File descriptor which is marked close-on-exec are also closed.
 */
static int
fs_exec(struct task *t, struct msg *msg)
{
	task_t old_id, new_id;
	struct task *target;
	file_t fp;
	int fd;

	old_id = (task_t)msg->data[0];
	new_id = (task_t)msg->data[1];

	if (!(target = task_lookup(old_id)))
		return EINVAL;

	/* Update task id in the task. */
	task_setid(target, new_id);

	/* Close all directory descriptor */
	for (fd = 0; fd < OPEN_MAX; fd++) {
		fp = target->t_ofile[fd];
		if (fp) {
			if (fp->f_vnode->v_type == VDIR) {
				sys_close(fp);
				target->t_ofile[fd] = NULL;
			}

			/* XXX: need to check close-on-exec flag */
		}
	}
	task_unlock(target);
	return 0;
}

/*
 * fs_exit() cleans up data for task's termination.
 */
static int
fs_exit(struct task *t, struct msg *msg)
{
	file_t fp;
	int fd;

	DPRINTF(VFSDB_CORE, ("fs_exit\n"));

	/*
	 * Close all files opened by task.
	 */
	for (fd = 0; fd < OPEN_MAX; fd++) {
		fp = t->t_ofile[fd];
		if (fp != NULL)
			sys_close(fp);
	}
	if (t->t_cwdfp)
		sys_close(t->t_cwdfp);
	task_free(t);
	return 0;
}

/*
 * fs_register() is called by boot tasks.
 * This can be called even when no fs is mounted.
 */
static int
fs_register(struct task *t, struct msg *msg)
{
	struct task *tmp;
	int error;

	DPRINTF(VFSDB_CORE, ("fs_register\n"));

	error = task_alloc(msg->hdr.task, &tmp);
	return error;
}

static int
fs_pipe(struct task *t, struct msg *msg)
{
#ifdef CONFIG_FIFOFS
	char path[PATH_MAX];
	file_t rfp, wfp;
	int error, rfd, wfd;

	DPRINTF(VFSDB_CORE, ("fs_pipe\n"));

	if ((rfd = task_newfd(t)) == -1)
		return EMFILE;
	t->t_ofile[rfd] = (file_t)1; /* temp */

	if ((wfd = task_newfd(t)) == -1) {
		t->t_ofile[rfd] = NULL;
		return EMFILE;
	}
	sprintf(path, "/mnt/fifo/pipe-%x-%d", (u_int)t->t_taskid, rfd);

	if ((error = sys_mknod(path, S_IFIFO)) != 0)
		goto out;
	if ((error = sys_open(path, O_RDONLY | O_NONBLOCK, 0, &rfp)) != 0) {
		goto out;
	}
	if ((error = sys_open(path, O_WRONLY | O_NONBLOCK, 0, &wfp)) != 0) {
		goto out;
	}
	t->t_ofile[rfd] = rfp;
	t->t_ofile[wfd] = wfp;
	t->t_nopens += 2;
	msg->data[0] = rfd;
	msg->data[1] = wfd;
	return 0;
 out:
	t->t_ofile[rfd] = NULL;
	t->t_ofile[wfd] = NULL;
	return error;
#else
	return ENOSYS;
#endif
}

/*
 * Return if specified file is a tty
 */
static int
fs_isatty(struct task *t, struct msg *msg)
{
	file_t fp;
	int istty = 0;

	if ((fp = task_getfp(t, msg->data[0])) == NULL)
		return EBADF;

	if (fp->f_vnode->v_flags & VISTTY)
		istty = 1;
	msg->data[0] = istty;
	return 0;
}

static int
fs_truncate(struct task *t, struct path_msg *msg)
{
	char path[PATH_MAX];
	int error;

	if (msg->path == NULL)
		return ENOENT;
	if ((error = task_conv(t, msg->path, VWRITE, path)) != 0)
		return error;

	return sys_truncate(path, msg->data[0]);
}

static int
fs_ftruncate(struct task *t, struct msg *msg)
{
	file_t fp;

	if ((fp = task_getfp(t, msg->data[0])) == NULL)
		return EBADF;

	return sys_ftruncate(fp, msg->data[1]);
}

/*
 * Prepare for boot
 */
static int
fs_boot(struct task *t, struct msg *msg)
{
	object_t execobj, procobj;
	struct bind_msg bm;
	struct msg m;

	/* Check client's capability. */
	if (task_chkcap(msg->hdr.task, CAP_PROTSERV) != 0)
		return EPERM;

	/*
	 * Request exec server to bind an appropriate
	 * capability for us.
	 */
	if (object_lookup("!exec", &execobj) != 0)
		sys_panic("fs: no exec found");
	bm.hdr.code = EXEC_BINDCAP;
	strlcpy(bm.path, "/boot/fs", sizeof(bm.path));
	msg_send(execobj, &bm, sizeof(bm));

	/*
	 * Notify to process server.
	 */
	if (object_lookup("!proc", &procobj) != 0)
		sys_panic("fs: no proc found");
	m.hdr.code = PS_REGISTER;
	msg_send(procobj, &m, sizeof(m));

	return 0;
}

/*
 * Prepare for shutdown
 */
static int
fs_shutdown(struct task *t, struct msg *msg)
{

	DPRINTF(VFSDB_CORE, ("fs_shutdown\n"));
	return 0;
}

int
fs_noop(void)
{
	return 0;
}

#ifdef DEBUG_VFS
/*
 * Dump internal data.
 */
static int
fs_debug(struct task *t, struct msg *msg)
{

	dprintf("<File System Server>\n");
	task_dump();
	vnode_dump();
	mount_dump();
	return 0;
}
#endif

static void
vfs_init(void)
{
	const struct vfssw *fs;
	struct msg msg;

	/*
	 * Initialize VFS core.
	 */
	task_init();
	bio_init();
	vnode_init();

	/*
	 * Initialize each file system.
	 */
	for (fs = vfssw; fs->vs_name; fs++) {
		DPRINTF(VFSDB_CORE, ("VFS: initializing %s\n",
				     fs->vs_name));
		fs->vs_init();
	}

	/*
	 * Create task data for ourselves.
	 */
	msg.hdr.task = task_self();
	fs_register(NULL, &msg);
}

/*
 * Run specified routine as a thread.
 */
static int
run_thread(void (*entry)(void))
{
	task_t self;
	thread_t t;
	void *stack, *sp;
	int error;

	self = task_self();
	if ((error = thread_create(self, &t)) != 0)
		return error;
	if ((error = vm_allocate(self, &stack, DFLSTKSZ, 1)) != 0)
		return error;

	sp = (void *)((u_long)stack + DFLSTKSZ - sizeof(u_long) * 3);
	if ((error = thread_load(t, entry, sp)) != 0)
		return error;

	return thread_resume(t);
}

static void
exception_handler(int sig)
{

	exception_return();
}

/*
 * Message mapping
 */
static const struct msg_map fsmsg_map[] = {
	MSGMAP( FS_MOUNT,	fs_mount ),
	MSGMAP( FS_UMOUNT,	fs_umount ),
	MSGMAP( FS_SYNC,	fs_sync ),
	MSGMAP( FS_OPEN,	fs_open ),
	MSGMAP( FS_CLOSE,	fs_close ),
	MSGMAP( FS_MKNOD,	fs_mknod ),
	MSGMAP( FS_LSEEK,	fs_lseek ),
	MSGMAP( FS_READ,	fs_read ),
	MSGMAP( FS_WRITE,	fs_write ),
	MSGMAP( FS_IOCTL,	fs_ioctl ),
	MSGMAP( FS_FSYNC,	fs_fsync ),
	MSGMAP( FS_FSTAT,	fs_fstat ),
	MSGMAP( FS_OPENDIR,	fs_opendir ),
	MSGMAP( FS_CLOSEDIR,	fs_closedir ),
	MSGMAP( FS_READDIR,	fs_readdir ),
	MSGMAP( FS_REWINDDIR,	fs_rewinddir ),
	MSGMAP( FS_SEEKDIR,	fs_seekdir ),
	MSGMAP( FS_TELLDIR,	fs_telldir ),
	MSGMAP( FS_MKDIR,	fs_mkdir ),
	MSGMAP( FS_RMDIR,	fs_rmdir ),
	MSGMAP( FS_RENAME,	fs_rename ),
	MSGMAP( FS_CHDIR,	fs_chdir ),
	MSGMAP( FS_LINK,	fs_link ),
	MSGMAP( FS_UNLINK,	fs_unlink ),
	MSGMAP( FS_STAT,	fs_stat ),
	MSGMAP( FS_GETCWD,	fs_getcwd ),
	MSGMAP( FS_DUP,		fs_dup ),
	MSGMAP( FS_DUP2,	fs_dup2 ),
	MSGMAP( FS_FCNTL,	fs_fcntl ),
	MSGMAP( FS_ACCESS,	fs_access ),
	MSGMAP( FS_FORK,	fs_fork ),
	MSGMAP( FS_EXEC,	fs_exec ),
	MSGMAP( FS_EXIT,	fs_exit ),
	MSGMAP( FS_REGISTER,	fs_register ),
	MSGMAP( FS_PIPE,	fs_pipe ),
	MSGMAP( FS_ISATTY,	fs_isatty ),
	MSGMAP( FS_TRUNCATE,	fs_truncate ),
	MSGMAP( FS_FTRUNCATE,	fs_ftruncate ),
	MSGMAP( FS_FCHDIR,	fs_fchdir ),
	MSGMAP( STD_BOOT,	fs_boot ),
	MSGMAP( STD_SHUTDOWN,	fs_shutdown ),
#ifdef DEBUG_VFS
	MSGMAP( STD_DEBUG,	fs_debug ),
#endif
	MSGMAP( 0,		NULL ),
};

/*
 * File system thread.
 */
static void
fs_thread(void)
{
	struct msg *msg;
	const struct msg_map *map;
	struct task *t;
	int error;

	msg = malloc(MAX_FSMSG);

	/*
	 * Message loop
	 */
	for (;;) {
		/*
		 * Wait for an incoming request.
		 */
		if ((error = msg_receive(fsobj, msg, MAX_FSMSG)) != 0)
			continue;

		error = EINVAL;
		map = &fsmsg_map[0];
		while (map->code != 0) {
			if (map->code == msg->hdr.code) {
				/*
				 * Handle messages by non-registerd tasks
				 */
				if (map->code == STD_BOOT) {
					error = fs_boot(NULL, msg);
					break;
				}
				if (map->code == FS_REGISTER) {
					error = fs_register(NULL, msg);
					break;
				}

				/* Lookup and lock task */
				t = task_lookup(msg->hdr.task);
				if (t == NULL)
					break;

				/* Dispatch request */
				error = (*map->func)(t, msg);
				if (map->code != FS_EXIT)
					task_unlock(t);
				break;
			}
			map++;
		}
#ifdef DEBUG_VFS
		if (error)
			dprintf("VFS: task=%x code=%x error=%d\n",
				msg->hdr.task, map->code, error);
#endif
		/*
		 * Reply to the client.
		 */
		msg->hdr.status = error;
		msg_reply(fsobj, msg, MAX_FSMSG);
	}
}

/*
 * Main routine for file system service
 */
int
main(int argc, char *argv[])
{
	int i;

	sys_log("Starting file system server\n");

	DPRINTF(VFSDB_CORE, ("VFS: number of fs threads: %d\n",
			     CONFIG_FS_THREADS));

	/* Set thread priority. */
	thread_setpri(thread_self(), PRI_FS);

	/* Setup exception handler */
	exception_setup(exception_handler);

	/* Initialize the file systems. */
	vfs_init();

	/* Create an object to expose our service. */
	if (object_create("!fs", &fsobj))
		sys_panic("VFS: fail to create object");

	/*
	 * Create new server threads.
	 */
	i = CONFIG_FS_THREADS;
	while (--i > 0) {
		if (run_thread(fs_thread))
			goto err;
	}
	fs_thread();

	sys_panic("VFS: exit!");
	exit(0);
 err:
	sys_panic("VFS: failed to create thread");
	return 0;
}
