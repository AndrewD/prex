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

#ifndef _VFS_H
#define _VFS_H

#include <prex/prex.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/dirent.h>
#include <sys/syslog.h>

#include <assert.h>

/*
 * Tunable parameters
 */
#define PRIO_FS		128		/* priority of file system server */
#define FSMAXNAMES	16		/* max length of 'file system' name */

#ifdef DEBUG
/* #define DEBUG_VFS	1 */
/* #define DEBUG_VNODE	1 */
/* #define DEBUG_BIO	1 */
#endif

#ifdef DEBUG_VFS
#define dprintf(fmt, ...)	syslog(LOG_DEBUG, "vfs: "fmt, ## __VA_ARGS__)
#else
#define dprintf(fmt, ...)		do {} while (0)
#endif
#ifdef DEBUG_VNODE
#define vn_printf(fmt, ...)	syslog(LOG_DEBUG, fmt, ## __VA_ARGS__)
#else
#define vn_printf(fmt, ...)	do {} while (0)
#endif
#ifdef DEBUG_BIO
#define bio_printf(fmt, ...) syslog(LOG_DEBUG, fmt, ## __VA_ARGS__)
#else
#define bio_printf(fmt, ...)	 do {} while (0)
#endif

#ifdef DEBUG
#define ASSERT(e)	assert(e)
#else
#define ASSERT(e)
#endif

#if CONFIG_FS_THREADS > 1
#define malloc(s)		malloc_r(s)
#define free(p)			free_r(p)
#else
#define mutex_init(m)		do {} while (0)
#define mutex_destroy(m)	do {} while (0)
#define mutex_lock(m)		do {} while (0)
#define mutex_unlock(m)		do {} while (0)
#define mutex_trylock(m)	do {} while (0)
#endif

/*
 * per task data
 */
struct task {
	struct list	link;		/* hash link */
	task_t		task;		/* task id */
	char 		cwd[PATH_MAX];	/* current working directory */
	file_t		cwd_fp;		/* directory for cwd */
	file_t		file[OPEN_MAX];	/* array of file pointers */
	int		nr_open;	/* number of opening files */
	mutex_t		lock;		/* lock for this task */
	cap_t		cap;		/* task capabilities */
};

extern const struct vfssw vfssw_table[];

extern struct task *task_lookup(task_t task);
extern int task_alloc(task_t task, struct task **pt);
extern void task_free(struct task *t);
extern void task_update(struct task *t, task_t task);
extern void task_unlock(struct task *t);
extern file_t task_getfp(struct task *t, int fd);
extern int task_conv(struct task *t, char *path, char *full);
extern void task_dump(void);
extern void task_init(void);

extern int namei(char *path, vnode_t *vpp);
extern int lookup(char *path, vnode_t *vpp, char **name);

extern vnode_t vn_lookup(mount_t mp, char *path);
extern void vn_lock(vnode_t vp);
extern void vn_unlock(vnode_t vp);
extern vnode_t vget(mount_t mp, char *path);
extern void vput(vnode_t vp);
extern void vgone(vnode_t vp);
extern void vref(vnode_t vp);
extern void vrele(vnode_t vp);
extern int vcount(vnode_t vp);
extern void vflush(mount_t mp);
extern void vnode_init(void);
#ifdef DEBUG
extern void vnode_dump(void);
#endif

extern void bio_init(void);

extern int sys_mount(char *dev, char *dir, char *fsname, int flags, void *data);
extern int sys_umount(char *path);
extern int sys_sync(void);
extern int vfs_findroot(char *path, mount_t *mp, char **root);
extern void vfs_busy(mount_t mp);
extern void vfs_unbusy(mount_t mp);
#ifdef DEBUG
extern void mount_dump(void);
#endif

extern int sys_open(char *path, int flags, mode_t mode, file_t * file);
extern int sys_close(file_t fl);
extern int sys_read(file_t fl, void *buf, size_t size, size_t *result);
extern int sys_write(file_t fl, void *buf, size_t size, size_t *result);
extern int sys_lseek(file_t fl, off_t off, int type, off_t * cur_off);
extern int sys_ioctl(file_t fl, int request, char *buf);
extern int sys_fstat(file_t fl, struct stat *st);
extern int sys_fsync(file_t fl);

extern int sys_opendir(char *path, file_t * file);
extern int sys_closedir(file_t fl);
extern int sys_readdir(file_t fl, struct dirent *dirent);
extern int sys_rewinddir(file_t fl);
extern int sys_seekdir(file_t fl, long loc);
extern int sys_telldir(file_t fl, long *loc);
extern int sys_mkdir(char *path, mode_t mode);
extern int sys_rmdir(char *path);
extern int sys_mkfifo(char *path, mode_t mode);
extern int sys_mknod(char *path, mode_t mode);
extern int sys_rename(char *src, char *dest);
extern int sys_unlink(char *path);
extern int sys_access(char *path, int mode);

#endif /* !_VFS_H */
