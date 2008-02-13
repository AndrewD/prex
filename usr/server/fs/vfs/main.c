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

#include <prex/prex.h>
#include <prex/capability.h>
#include <server/fs.h>
#include <server/proc.h>
#include <server/stdmsg.h>
#include <server/object.h>
#include <sys/list.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/file.h>
#include <sys/syslog.h>

#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#include "vfs.h"

/*
 * Message mapping
 */
struct msg_map {
	int code;
	int (*func)(struct task *, struct msg *);
};

#define MSGMAP(code, fn) {code, (int (*)(struct task *, struct msg *))fn}

/* object for file service */
static object_t fs_obj;


static int
fs_mount(struct task *t, struct mount_msg *msg)
{
	int err;

	/* Check mount capability. */
	if ((t->cap & CAP_ADMIN) == 0)
		return EPERM;

	err = sys_mount(msg->dev, msg->dir, msg->fs, msg->flags,
			(void *)msg->data);
#ifdef DEBUG
	if (err)
		syslog(LOG_INFO, "fs: mount failed! fs=%s\n", msg->fs);
#endif
	return err;
}

static int
fs_umount(struct task *t, struct path_msg *msg)
{

	/* Check mount capability. */
	if ((t->cap & CAP_ADMIN) == 0)
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
	int fd, err;
	mode_t mode;

	/* Find empty slot for file descriptor */
	for (fd = 0; fd < OPEN_MAX; fd++)
		if (t->file[fd] == NULL)
			break;
	if (fd == OPEN_MAX)
		return EMFILE;

	/* Check the capability of caller task */
	mode = msg->mode;
	if ((mode & 0111) && (t->cap & CAP_EXEC) == 0)
		return EACCES;
	if ((mode & 0222) && (t->cap & CAP_FS_WRITE) == 0)
		return EACCES;
	if ((mode & 0444) && (t->cap & CAP_FS_READ) == 0)
		return EACCES;

	if ((err = task_conv(t, msg->path, path)) != 0)
		return err;
	if ((err = sys_open(path, msg->flags, mode, &fp)) != 0)
		return err;

	t->file[fd] = fp;
	t->nr_open++;
	msg->fd = fd;
	return 0;
}

static int
fs_close(struct task *t, struct msg *msg)
{
	file_t fp;
	int fd, err;

	fd = msg->data[0];
	if (fd >= OPEN_MAX)
		return EBADF;

	fp = t->file[fd];
	if (fp == NULL)
		return EBADF;

	if ((err = sys_close(fp)) != 0)
		return err;

	t->file[fd] = NULL;
	t->nr_open--;
	return 0;
}

static int
fs_mknod(struct task *t, struct open_msg *msg)
{
	char path[PATH_MAX];
	int err;

	if ((err = task_conv(t, msg->path, path)) != 0)
		return err;
	return sys_mknod(path, msg->mode);
}

static int
fs_lseek(struct task *t, struct msg *msg)
{
	file_t fp;
	off_t offset, org;
	int err, type;

	if ((fp = task_getfp(t, msg->data[0])) == NULL)
		return EBADF;
	offset = (off_t)msg->data[1];
	type = msg->data[2];
	err = sys_lseek(fp, offset, type, &org);
	msg->data[0] = (int)org;
	return err;
}

static int
fs_read(struct task *t, struct io_msg *msg)
{
	file_t fp;
	void *buf;
	size_t size, bytes;
	int err;

	if ((fp = task_getfp(t, msg->fd)) == NULL)
		return EBADF;
	size = msg->size;
	if ((err = vm_map(msg->hdr.task, msg->buf, size, &buf)) != 0)
		return EFAULT;
	err = sys_read(fp, buf, size, &bytes);
	msg->size = bytes;
	vm_free(task_self(), buf);
	return err;
}

static int
fs_write(struct task *t, struct io_msg *msg)
{
	file_t fp;
	void *buf;
	size_t size, bytes;
	int err;

	if ((fp = task_getfp(t, msg->fd)) == NULL)
		return EBADF;
	size = msg->size;
	if ((err = vm_map(msg->hdr.task, msg->buf, size, &buf)) != 0)
		return EFAULT;
	err = sys_write(fp, buf, size, &bytes);
	msg->size = bytes;
	vm_free(task_self(), buf);
	return err;
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

	if ((fp = task_getfp(t, msg->fd)) == NULL)
		return EBADF;
	st = &msg->st;
	return sys_fstat(fp, st);
}

static int
fs_opendir(struct task *t, struct open_msg *msg)
{
	char path[PATH_MAX];
	file_t fp;
	int fd, err;

	/* Find empty slot for file structure */
	for (fd = 0; fd < OPEN_MAX; fd++)
		if (t->file[fd] == NULL)
			break;
	if (fd == OPEN_MAX)
		return EMFILE;

	/* Get the mounted file system and node */
	if ((err = task_conv(t, msg->path, path)) != 0)
		return err;
	if ((err = sys_opendir(path, &fp)) != 0)
		return err;
	t->file[fd] = fp;
	msg->fd = fd;
	return 0;
}

static int
fs_closedir(struct task *t, struct msg *msg)
{
	file_t fp;
	int fd, err;

	fd = msg->data[0];
	if (fd >= OPEN_MAX)
		return EBADF;
	fp = t->file[fd];
	if (fp == NULL)
		return EBADF;
	if ((err = sys_closedir(fp)) != 0)
		return err;
	t->file[fd] = NULL;
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
	int err;

	if ((fp = task_getfp(t, msg->data[0])) == NULL)
		return EBADF;
	loc = msg->data[1];
	if ((err = sys_telldir(fp, &loc)) != 0)
		return err;
	msg->data[0] = loc;
	return 0;
}

static int
fs_mkdir(struct task *t, struct open_msg *msg)
{
	char path[PATH_MAX];
	int err;

	if ((t->cap & CAP_FS_WRITE) == 0)
		return EACCES;
	if ((err = task_conv(t, msg->path, path)) != 0)
		return err;
	return sys_mkdir(path, msg->mode);
}

static int
fs_rmdir(struct task *t, struct path_msg *msg)
{
	char path[PATH_MAX];
	int err;

	if ((t->cap & CAP_FS_WRITE) == 0)
		return EACCES;
	if (msg->path == NULL)
		return ENOENT;
	if ((err = task_conv(t, msg->path, path)) != 0)
		return err;
	return sys_rmdir(path);
}

static int
fs_rename(struct task *t, struct path_msg *msg)
{
	char src[PATH_MAX];
	char dest[PATH_MAX];
	int err;

	if ((t->cap & CAP_FS_WRITE) == 0)
		return EACCES;
	if (msg->path == NULL || msg->path2 == NULL)
		return ENOENT;
	if ((err = task_conv(t, msg->path, src)) != 0)
		return err;
	if ((err = task_conv(t, msg->path2, dest)) != 0)
		return err;
	return sys_rename(src, dest);
}

static int
fs_chdir(struct task *t, struct path_msg *msg)
{
	char path[PATH_MAX];
	file_t fp;
	int err;

	if (msg->path == NULL)
		return ENOENT;
	if ((err = task_conv(t, msg->path, path)) != 0)
		return err;
	/* Check if directory exits */
	if ((err = sys_opendir(path, &fp)) != 0)
		return err;
	if (t->cwd_fp)
		sys_closedir(t->cwd_fp);
	t->cwd_fp = fp;
	strcpy(t->cwd, path);
	return 0;
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

	if ((t->cap & CAP_FS_WRITE) == 0)
		return EACCES;
	if (msg->path == NULL)
		return ENOENT;
	task_conv(t, msg->path, path);
	return sys_unlink(path);
}

static int
fs_stat(struct task *t, struct stat_msg *msg)
{
	char path[PATH_MAX];
	struct stat *st;
	file_t fp;
	int err;

	task_conv(t, msg->path, path);
	if ((err = sys_open(path, O_RDONLY, 0, &fp)) != 0)
		return err;
	st = &msg->st;
	err = sys_fstat(fp, st);
	sys_close(fp);
	return err;
}

static int
fs_getcwd(struct task *t, struct path_msg *msg)
{

	strcpy(msg->path, t->cwd);
	return 0;
}

/*
 * Duplicate a file descriptor.
 */
static int
fs_dup(struct task *t, struct msg *msg)
{
	file_t fp;
	int old_fd, new_fd;

	old_fd = msg->data[0];
	if (old_fd >= OPEN_MAX)
		return EBADF;
	fp = t->file[old_fd];
	if (fp == NULL)
		return EBADF;
	/* Find smallest empty slot as new fd. */
	for (new_fd = 0; new_fd < OPEN_MAX; new_fd++)
		if (t->file[new_fd] == NULL)
			break;
	if (new_fd == OPEN_MAX)
		return EMFILE;
	t->file[new_fd] = fp;

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
	int err;

	old_fd = msg->data[0];
	new_fd = msg->data[1];
	if (old_fd >= OPEN_MAX || new_fd >= OPEN_MAX)
		return EBADF;
	fp = t->file[old_fd];
	if (fp == NULL)
		return EBADF;
	org = t->file[new_fd];
	if (org != NULL) {
		/* Close previous file if it's opened. */
		err = sys_close(org);
	}
	t->file[new_fd] = fp;
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
		for (new_fd = arg; new_fd < OPEN_MAX; new_fd++)
			if (t->file[new_fd] == NULL)
				break;
		if (new_fd == OPEN_MAX)
			return EMFILE;
		t->file[new_fd] = fp;
		break;
	case F_GETFD:
		msg->arg = fp->f_flags & FD_CLOEXEC;
		break;
	case F_SETFD:
		fp->f_flags = (fp->f_flags & ~FD_CLOEXEC) |
			(msg->arg & FD_CLOEXEC);
		break;
	case F_GETFL:
		break;
	case F_SETFL:
		break;
	default:
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
	int mode, err;

	mode = msg->data[0];

	/* Check file permission */
	task_conv(t, msg->path, path);
	if ((err = sys_access(path, mode)) != 0)
		return err;

	/* Check task permission */
	err = EACCES;
	if ((mode & X_OK) && (t->cap & CAP_EXEC) == 0)
		goto out;
	if ((mode & W_OK) && (t->cap & CAP_FS_WRITE) == 0)
		goto out;
	if ((mode & R_OK) && (t->cap & CAP_FS_READ) == 0)
		goto out;
	err = 0;
 out:
	return err;
}

/*
 * Copy parent's cwd & file/directory descriptor to child's.
 */
static int
fs_fork(struct task *t, struct msg *msg)
{
	struct task *newtask;
	file_t fp;
	int err, i;

	dprintf("fs_fork\n", );

	if ((err = task_alloc((task_t)msg->data[0], &newtask)) != 0)
		return err;
	/*
	 * Copy task related data
	 */
	newtask->cwd_fp = t->cwd_fp;
	strcpy(newtask->cwd, t->cwd);
	for (i = 0; i < OPEN_MAX; i++) {
		fp = t->file[i];
		newtask->file[i] = fp;
		/* Increment file reference if it's already opened. */
		if (fp != NULL) {
			vref(fp->f_vnode);
			fp->f_count++;
		}
	}
	if (newtask->cwd_fp)
		newtask->cwd_fp->f_count++;
	/* Increment cwd's reference count */
	if (newtask->cwd_fp)
		vref(newtask->cwd_fp->f_vnode);
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
	task_update(target, new_id);

	/* Close all directory descriptor */
	for (fd = 0; fd < OPEN_MAX; fd++) {
		fp = target->file[fd];
		if (fp) {
			if (fp->f_vnode->v_type == VDIR) {
				sys_close(fp);
				target->file[fd] = NULL;
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

	/* Close all files opened by task. */
	for (fd = 0; fd < OPEN_MAX; fd++) {
		fp = t->file[fd];
		if (fp != NULL)
			sys_close(fp);
	}
	if (t->cwd_fp)
		sys_close(t->cwd_fp);
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
	cap_t cap;
	int err;

	dprintf("fs_register\n", );

	if (task_getcap(msg->hdr.task, &cap))
		return EINVAL;
	if ((cap & CAP_ADMIN) == 0)
		return EPERM;

	err = task_alloc(msg->hdr.task, &tmp);
	return err;
}

/*
 * Return version
 */
static int
fs_version(struct task *t, struct msg *msg)
{
	return 0;
}

/*
 * Prepare for shutdown
 */
static int
fs_shutdown(struct task *t, struct msg *msg)
{

	sys_sync();
	return 0;
}

static int
fs_debug(struct task *t, struct msg *msg)
{

#ifdef DEBUG
	printf("<File System Server>\n");
	task_dump();
	vnode_dump();
	mount_dump();
#endif
	return 0;
}

/*
 * Register to process server if it is loaded.
 */
static void
process_init(void)
{
	int i, err = 0;
	object_t obj = 0;
	struct msg m;

	/*
	 * Wait for server loading. timeout is 1 sec.
	 */
	for (i = 0; i < 100; i++) {
		err = object_lookup(OBJNAME_PROC, &obj);
		if (err == 0)
			break;

		/* Wait 10msec */
		timer_sleep(10, 0);
		thread_yield();
	}
	if (obj == 0)
		return;

	/*
	 * Notify to process server.
	 */
	m.hdr.code = PS_REGISTER;
	msg_send(obj, &m, sizeof(m), 0);
}

static void
fs_init(void)
{
	const struct vfssw *fs;
	struct msg msg;

	process_init();

	/*
	 * Initialize VFS core.
	 */
	task_init();
	bio_init();
	vnode_init();

	/*
	 * Initialize each file system.
	 */
	for (fs = vfssw_table; fs->vs_name; fs++) {
		syslog(LOG_INFO, "VFS: Initializing %s\n", fs->vs_name);
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
thread_run(void (*entry)(void))
{
	task_t self;
	thread_t th;
	void *stack;
	u_long *sp;
	int err;

	self = task_self();
	if ((err = thread_create(self, &th)) != 0)
		return err;
	if ((err = vm_allocate(self, &stack, USTACK_SIZE, 1)) != 0)
		return err;
	sp = (void *)((u_long)stack + USTACK_SIZE - sizeof(u_long));
	*sp = 0;		/* arg */
	if ((err = thread_load(th, entry, sp)) != 0)
		return err;
	if ((err = thread_setprio(th, PRIO_FS)) != 0)
		return err;

	return thread_resume(th);
}

/*
 * Message mapping
 */
static const struct msg_map fsmsg_map[] = {
	MSGMAP( STD_VERSION,	fs_version ),
	MSGMAP( STD_DEBUG,	fs_debug ),
	MSGMAP( STD_SHUTDOWN,	fs_shutdown ),
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
	int err;

	msg = (struct msg *)malloc(MAX_FSMSG);

	/*
	 * Message loop
	 */
	for (;;) {
		/*
		 * Wait for an incoming request.
		 */
		if ((err = msg_receive(fs_obj, msg, MAX_FSMSG, 0)) != 0)
			continue;

		err = EINVAL;
		map = &fsmsg_map[0];
		while (map->code != 0) {
			if (map->code == msg->hdr.code) {
				if (map->code == FS_REGISTER) {
					err = fs_register(NULL, msg);
					break;
				}
				/* Lookup and lock task */
				t = task_lookup(msg->hdr.task);
				if (t == NULL)
					break;

				/* Get the capability list of caller task. */
				if (task_getcap(msg->hdr.task, &t->cap))
					break;

				/* Dispatch request */
				err = map->func(t, msg);
				if (map->code != FS_EXIT)
					task_unlock(t);
				break;
			}
			map++;
		}
#ifdef DEBUG_VFS
		if (err)
			dprintf("code=%x error=%d\n", map->code, err);
#endif
		/*
		 * Reply to the client.
		 */
		msg->hdr.status = err;
		msg_reply(fs_obj, msg, MAX_FSMSG);
	}
}

/*
 * Main routine for file system service
 */
int
main(int argc, char *argv[])
{
	int i;

	syslog(LOG_INFO, "Starting File System Server\n");

	/*
	 * Boost current priority.
	 */
	thread_setprio(thread_self(), PRIO_FS);

	/*
	 * Initialize file systems.
	 */
	fs_init();

	/*
	 * Create an object to expose our service.
	 */
	if (object_create(OBJNAME_FS, &fs_obj))
		sys_panic("VFS: fail to create object");

	/*
	 * Create new server threads.
	 */
	syslog(LOG_INFO, "VFS: Number of fs threads: %d\n", CONFIG_FS_THREADS);

	i = CONFIG_FS_THREADS;
	while (--i > 0) {
		if (thread_run(fs_thread))
			goto err;
	}
	fs_thread();
	exit(0);
 err:
	sys_panic("VFS: failed to create thread");
	return 0;
}
