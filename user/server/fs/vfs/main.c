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
#include <server/fs.h>

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

#include "vfs.h"

#if NR_FS_THREADS > 1
extern int __isthreaded;	/* thread option in libc */
#endif

/*
 * Message mapping
 */
struct msg_map {
	int code;
	int (*func)();
};

/* Object for file service */
static object_t fs_obj;


static int fs_mount(tcb_t tcb, struct mount_msg *msg)
{
	cap_t cap;
	int err;

	/* Check mount capability. */
	if (task_getcap(msg->hdr.task, &cap))
		return EINVAL;
	if ((cap & CAP_FS_MOUNT) == 0)
		return EPERM;

	err = sys_mount(msg->dev, msg->dir, msg->fs, msg->flags,
			(void *)msg->data);
#ifdef DEBUG
	if (err)
		syslog(LOG_INFO, "fs: mount failed! fs=%s\n", msg->fs);
#endif
	return err;
}

static int fs_umount(tcb_t tcb, struct path_msg *msg)
{
	cap_t cap;

	/* Check mount capability. */
	if (task_getcap(msg->hdr.task, &cap))
		return EINVAL;
	if ((cap & CAP_FS_MOUNT) == 0)
		return EPERM;
	return sys_umount(msg->path);
}

static int fs_sync(tcb_t tcb, struct msg *msg)
{
	return sys_sync();
}

static int fs_open(tcb_t tcb, struct open_msg *msg)
{
	char path[PATH_MAX];
	file_t fp;
	int fd, err;

	/* Find empty slot for file descriptor */
	for (fd = 0; fd < OPEN_MAX; fd++)
		if (tcb->file[fd] == NULL)
			break;
	if (fd == OPEN_MAX)
		return EMFILE;
	if ((err = tcb_conv(tcb, msg->path, path)) != 0)
		return err;
	if ((err = sys_open(path, msg->flags, msg->mode, &fp)) != 0)
		return err;
	tcb->file[fd] = fp;
	tcb->nr_open++;
	msg->fd = fd;
	return 0;
}

static int fs_close(tcb_t tcb, struct msg *msg)
{
	file_t fp;
	int fd, err;

	fd = msg->data[0];
	if (fd >= OPEN_MAX)
		return EBADF;

	fp = tcb->file[fd];
	if (!fp)
		return EBADF;

	if ((err = sys_close(fp)) != 0)
		return err;

	tcb->file[fd] = NULL;
	tcb->nr_open--;
	return 0;
}

static int fs_mknod(tcb_t tcb, struct open_msg *msg)
{
	char path[PATH_MAX];
	int err;

	if ((err = tcb_conv(tcb, msg->path, path)) != 0)
		return err;
	return sys_mknod(path, msg->mode);
}

static int fs_lseek(tcb_t tcb, struct msg *msg)
{
	file_t fp;
	off_t offset, org;
	int err, type;

	if ((fp = tcb_getfp(tcb, msg->data[0])) == NULL)
		return EBADF;
	offset = (off_t)msg->data[1];
	type = msg->data[2];
	err = sys_lseek(fp, offset, type, &org);
	msg->data[0] = (int)org;
	return err;
}

static int fs_read(tcb_t tcb, struct io_msg *msg)
{
	file_t fp;
	void *buf;
	size_t size, bytes;
	int err;

	if ((fp = tcb_getfp(tcb, msg->fd)) == NULL)
		return EBADF;
	size = msg->size;
	if ((err = vm_map(msg->hdr.task, msg->buf, size, &buf)) != 0)
		return EFAULT;
	err = sys_read(fp, buf, size, &bytes);
	msg->size = bytes;
	vm_free(task_self(), buf);
	return err;
}

static int fs_write(tcb_t tcb, struct io_msg *msg)
{
	file_t fp;
	void *buf;
	size_t size, bytes;
	int err;

	if ((fp = tcb_getfp(tcb, msg->fd)) == NULL)
		return EBADF;
	size = msg->size;
	if ((err = vm_map(msg->hdr.task, msg->buf, size, &buf)) != 0)
		return EFAULT;
	err = sys_write(fp, buf, size, &bytes);
	msg->size = bytes;
	vm_free(task_self(), buf);
	return err;
}

static int fs_ioctl(tcb_t tcb, struct msg *msg)
{
	/* XXX */
	return 0;
}

static int fs_fsync(tcb_t tcb, struct msg *msg)
{
	file_t fp;

	if ((fp = tcb_getfp(tcb, msg->data[0])) == NULL)
		return EBADF;
	return sys_fsync(fp);
}

static int fs_fstat(tcb_t tcb, struct stat_msg *msg)
{
	file_t fp;
	struct stat *st;

	if ((fp = tcb_getfp(tcb, msg->fd)) == NULL)
		return EBADF;
	st = &msg->st;
	return sys_fstat(fp, st);
}

static int fs_opendir(tcb_t tcb, struct open_msg *msg)
{
	char path[PATH_MAX];
	file_t fp;
	int fd, err;

	/* Find empty slot for file structure */
	for (fd = 0; fd < OPEN_MAX; fd++)
		if (tcb->file[fd] == NULL)
			break;
	if (fd == OPEN_MAX)
		return EMFILE;

	/* Get the mounted file system and node */
	if ((err = tcb_conv(tcb, msg->path, path)) != 0)
		return err;
	if ((err = sys_opendir(path, &fp)) != 0)
		return err;
	tcb->file[fd] = fp;
	msg->fd = fd;
	return 0;
}

static int fs_closedir(tcb_t tcb, struct msg *msg)
{
	file_t fp;
	int fd, err;

	fd = msg->data[0];
	if (fd >= OPEN_MAX)
		return EBADF;
	fp = tcb->file[fd];
	if (fp == NULL)
		return EBADF;
	if ((err = sys_closedir(fp)) != 0)
		return err;
	tcb->file[fd] = NULL;
	return 0;
}

static int fs_readdir(tcb_t tcb, struct dir_msg *msg)
{
	file_t fp;

	if ((fp = tcb_getfp(tcb, msg->fd)) == NULL)
		return EBADF;
	return sys_readdir(fp, &msg->dirent);
}

static int fs_rewinddir(tcb_t tcb, struct msg *msg)
{
	file_t fp;

	if ((fp = tcb_getfp(tcb, msg->data[0])) == NULL)
		return EBADF;
	return sys_rewinddir(fp);
}

static int fs_seekdir(tcb_t tcb, struct msg *msg)
{
	file_t fp;
	long loc;

	if ((fp = tcb_getfp(tcb, msg->data[0])) == NULL)
		return EBADF;
	loc = msg->data[1];
	return sys_seekdir(fp, loc);
}

static int fs_telldir(tcb_t tcb, struct msg *msg)
{
	file_t fp;
	long loc;
	int err;

	if ((fp = tcb_getfp(tcb, msg->data[0])) == NULL)
		return EBADF;
	loc = msg->data[1];
	if ((err = sys_telldir(fp, &loc)) != 0)
		return err;
	msg->data[0] = loc;
	return 0;
}

static int fs_mkdir(tcb_t tcb, struct open_msg *msg)
{
	char path[PATH_MAX];
	int err;

	if ((err = tcb_conv(tcb, msg->path, path)) != 0)
		return err;
	return sys_mkdir(path, msg->mode);
}

static int fs_rmdir(tcb_t tcb, struct path_msg *msg)
{
	char path[PATH_MAX];
	int err;

	if (msg->path == NULL)
		return ENOENT;
	if ((err = tcb_conv(tcb, msg->path, path)) != 0)
		return err;
	return sys_rmdir(path);
}

static int fs_rename(tcb_t tcb, struct path_msg *msg)
{
	char src[PATH_MAX];
	char dest[PATH_MAX];
	int err;

	if (msg->path == NULL || msg->path2 == NULL)
		return ENOENT;
	if ((err = tcb_conv(tcb, msg->path, src)) != 0)
		return err;
	if ((err = tcb_conv(tcb, msg->path2, dest)) != 0)
		return err;
	return sys_rename(src, dest);
}

static int fs_chdir(tcb_t tcb, struct path_msg *msg)
{
	char path[PATH_MAX];
	file_t fp;
	int err;

	if (msg->path == NULL)
		return ENOENT;
	if ((err = tcb_conv(tcb, msg->path, path)) != 0)
		return err;
	/* Check if directory exits */
	if ((err = sys_opendir(path, &fp)) != 0)
		return err;
	if (tcb->cwd_fp)
		sys_closedir(tcb->cwd_fp);
	tcb->cwd_fp = fp;
	strcpy(tcb->cwd, path);
	return 0;
}

static int fs_link(tcb_t tcb, struct msg *msg)
{
	/* XXX */
	return EPERM;
}

static int fs_unlink(tcb_t tcb, struct path_msg *msg)
{
	char path[PATH_MAX];

	if (msg->path == NULL)
		return ENOENT;
	tcb_conv(tcb, msg->path, path);
	return sys_unlink(path);
}

static int fs_stat(tcb_t tcb, struct stat_msg *msg)
{
	char path[PATH_MAX];
	struct stat *st;
	file_t fp;
	int err;

	tcb_conv(tcb, msg->path, path);
	if ((err = sys_open(path, O_RDONLY, 0, &fp)) != 0)
		return err;
	st = &msg->st;
	err = sys_fstat(fp, st);
	sys_close(fp);
	return err;
}

static int fs_getcwd(tcb_t tcb, struct path_msg *msg)
{
	strcpy(msg->path, tcb->cwd);
	return 0;
}

static int fs_dup(tcb_t tcb, struct msg *msg)
{
	file_t fp;
	int old_fd, new_fd;

	old_fd = msg->data[0];
	if (old_fd >= OPEN_MAX)
		return EBADF;
	fp = tcb->file[old_fd];
	if (fp == NULL)
		return EBADF;
	/* Find smallest empty slot as new fd. */
	for (new_fd = 0; new_fd < OPEN_MAX; new_fd++)
		if (tcb->file[new_fd] == NULL)
			break;
	if (new_fd == OPEN_MAX)
		return EMFILE;
	tcb->file[new_fd] = fp;
	msg->data[0] = new_fd;
	return 0;
}

static int fs_dup2(tcb_t tcb, struct msg *msg)
{
	file_t fp, org;
	int old_fd, new_fd;
	int err;

	old_fd = msg->data[0];
	new_fd = msg->data[1];
	if (old_fd >= OPEN_MAX || new_fd >= OPEN_MAX)
		return EBADF;
	fp = tcb->file[old_fd];
	if (fp == NULL)
		return EBADF;
	org = tcb->file[new_fd];
	if (org != NULL) {
		/* Close previous file if it's opened. */
		err = sys_close(org);
	}
	tcb->file[new_fd] = fp;
	msg->data[0] = new_fd;
	return 0;
}

/*
 * Copy parent's cwd & file/directory descriptor to child's.
 */
static int fs_fork(tcb_t tcb, struct msg *msg)
{
	tcb_t new_tcb;
	file_t fp;
	int err, i;

	if ((err = tcb_alloc((task_t)msg->data[0], &new_tcb)) != 0)
		return err;
	/*
	 * Copy task related data
	 */
	new_tcb->cwd_fp = tcb->cwd_fp;
	strcpy(new_tcb->cwd, tcb->cwd);
	for (i = 0; i < OPEN_MAX; i++) {
		fp = tcb->file[i];
		new_tcb->file[i] = fp;
		/* Increment file reference if it's already opened. */
		if (fp != NULL) {
			vref(fp->f_vnode);
			fp->f_count++;
		}
	}
	if (new_tcb->cwd_fp)
		new_tcb->cwd_fp->f_count++;
	/* Increment cwd's reference count */
	if (new_tcb->cwd_fp)
		vref(new_tcb->cwd_fp->f_vnode);
	return 0;
}

/*
 * fs_exec() is called for POSIX exec().
 * It closes all directory stream.
 * File descriptor which is marked close-on-exec are also closed.
 */
static int fs_exec(tcb_t tcb, struct msg *msg)
{
	task_t old_task, new_task;
	tcb_t target;
	file_t fp;
	int fd;

	old_task = (task_t)msg->data[0];
	new_task = (task_t)msg->data[1];

	if (!(target = tcb_lookup(old_task)))
		return EINVAL;

	/* Update task id in the tcb. */
	tcb_update(target, new_task);

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
	tcb_unlock(target);
	return 0;
}

/*
 * fs_exit() cleans up data for task's termination.
 */
static int fs_exit(tcb_t tcb, struct msg *msg)
{
	file_t fp;
	int fd;

	/* Close all files opened by task. */
	for (fd = 0; fd < OPEN_MAX; fd++) {
		fp = tcb->file[fd];
		if (fp != NULL)
			sys_close(fp);
	}
	if (tcb->cwd_fp)
		sys_close(tcb->cwd_fp);
	tcb_free(tcb);
	return 0;
}

/*
 * fs_boot() is called by boot tasks.
 * This can be called even when no fs is mounted.
 */
static int fs_boot(tcb_t tcb, struct msg *msg)
{
	tcb_t t;
	int err;

	err = tcb_alloc(msg->hdr.task, &t);
	return err;
}

/*
 * Return version
 */
static int fs_version(tcb_t tcb, struct msg *msg)
{
	return 0;
}

/*
 * Shutdown
 */
static int fs_shutdown(tcb_t tcb, struct msg *msg)
{
	sys_sync();
	return 0;
}

static int fs_debug(tcb_t tcb, struct msg *msg)
{
#ifdef DEBUG
	printf("<File System Server>\n");
	tcb_dump();
	vnode_dump();
	mount_dump();
#endif
	return 0;
}

static void fs_init(void)
{
	const struct vfssw *fs;
	struct msg msg;

	/*
	 * Initialize VFS core.
	 */
	tcb_init();
	bio_init();
	vnode_init();

	/*
	 * Initialize each file system.
	 */
	for (fs = vfssw_table; fs->vs_name; fs++) {
		syslog(LOG_INFO, "Initializing %s\n", fs->vs_name);
		fs->vs_init();
	}

	/*
	 * Create task data for ourselves.
	 */
	msg.hdr.task = task_self();
	fs_boot(NULL, &msg);
}

/*
 * Run specified routine as a thread.
 */
static int thread_run(void *entry)
{
	task_t self;
	thread_t th;
	void *stack;
	int err;

	self = task_self();
	if ((err = thread_create(self, &th)) != 0)
		return err;
	if ((err = vm_allocate(self, &stack, USTACK_SIZE, 1)) != 0)
		return err;
	if ((err = thread_load(th, entry, stack + USTACK_SIZE)) != 0)
		return err;
	return thread_resume(th);
}

/*
 * Message mapping
 */
static const struct msg_map fsmsg_map[] = {
	{STD_VERSION,	fs_version},
	{STD_DEBUG,	fs_debug},
	{STD_SHUTDOWN,	fs_shutdown},
	{FS_MOUNT,	fs_mount},
	{FS_UMOUNT,	fs_umount},
	{FS_SYNC,	fs_sync},
	{FS_OPEN,	fs_open},
	{FS_CLOSE,	fs_close},
	{FS_MKNOD,	fs_mknod},
	{FS_LSEEK,	fs_lseek},
	{FS_READ,	fs_read},
	{FS_WRITE,	fs_write},
	{FS_IOCTL,	fs_ioctl},
	{FS_FSYNC,	fs_fsync},
	{FS_FSTAT,	fs_fstat},
	{FS_OPENDIR,	fs_opendir},
	{FS_CLOSEDIR,	fs_closedir},
	{FS_READDIR,	fs_readdir,},
	{FS_REWINDDIR,	fs_rewinddir},
	{FS_SEEKDIR,	fs_seekdir},
	{FS_TELLDIR,	fs_telldir},
	{FS_MKDIR,	fs_mkdir},
	{FS_RMDIR,	fs_rmdir},
	{FS_RENAME,	fs_rename},
	{FS_CHDIR,	fs_chdir},
	{FS_LINK,	fs_link},
	{FS_UNLINK,	fs_unlink},
	{FS_STAT,	fs_stat},
	{FS_GETCWD,	fs_getcwd},
	{FS_DUP,	fs_dup},
	{FS_DUP2,	fs_dup2},
	{FS_BOOT,	fs_boot},
	{FS_FORK,	fs_fork},
	{FS_EXEC,	fs_exec},
	{FS_EXIT,	fs_exit},
	{0,		0},
};

/*
 * File system thread.
 */
static void fs_thread(void)
{
	struct msg *msg;
	const struct msg_map *map;
	tcb_t tcb;
	int err;

	msg = (struct msg *)malloc(MSGBUF_SIZE);

	/*
	 * Message loop
	 */
	for (;;) {
		/*  Wait for an incoming request. */
		if ((err = msg_receive(fs_obj, msg, MSGBUF_SIZE)) != 0)
			continue;

		err = EINVAL;
		map = &fsmsg_map[0];
		while (map->code != 0) {
			if (map->code == msg->hdr.code) {
				if (map->code == FS_BOOT) {
					err = fs_boot(NULL, msg);
					break;
				}
				/* Lookup and lock tcb */
				tcb = tcb_lookup(msg->hdr.task);
				if (tcb == NULL)
					break;

				/* Dispatch request */
				err = map->func(tcb, msg);
				if (map->code != FS_EXIT)
					tcb_unlock(tcb);
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
		msg_reply(fs_obj, msg, MSGBUF_SIZE);
	}
}

/*
 * Main routine for file system service
 */
int main(int argc, char *argv[])
{
	int i;

	syslog(LOG_INFO, "Starting File System Server\n");

#if NR_FS_THREADS > 1
	/* Force enable a multi-thread flag of library */
	__isthreaded = 1;
#endif

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
		panic("fs: fail to create object");

	/*
	 * Create new server threads.
	 */
	i = NR_FS_THREADS;
	while (--i > 0) {
		if (thread_run(fs_thread))
			goto err;
	}
	fs_thread();
	exit(0);
 err:
	panic("fs: failed to create thread");
	return 0;
}
