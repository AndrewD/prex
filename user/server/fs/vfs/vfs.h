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

#include <config.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/dirent.h>
#include <sys/syslog.h>

#include <assert.h>

/*
 * Tunable parameters
 */
#define NR_FS_THREADS	CONFIG_FS_THREADS	/* Number of fs thread */

#define NR_BUFFERS	CONFIG_BUF_CACHE	/* Number of buffer cache */
#define BUF_FLUSH_TIME	5000		/* Buffer flush ratio (msec) */

#define PRIO_FS		128		/* Priority of file system server */
#define MSGBUF_SIZE	1024
#define FSNAME_MAX	16		/* Max length of 'file system' name */


#ifdef DEBUG
/* #define DEBUG_VFS	1 */
/* #define DEBUG_VNODE	1 */
/* #define DEBUG_BIO	1 */

#define ASSERT(e)	assert(e)
#else
#define ASSERT(e)
#endif

#ifdef DEBUG_VFS
#define dprintf(x,y...)		syslog(LOG_DEBUG, "vfs: "x, ##y)
#else
#define dprintf(x,y...)
#endif
#ifdef DEBUG_VNODE
#define vn_printf(x,y...)	syslog(LOG_DEBUG, x, ##y)
#else
#define vn_printf(x,y...)
#endif
#ifdef DEBUG_BIO
#define bio_printf(x,y...)	syslog(LOG_DEBUG, x, ##y)
#else
#define bio_printf(x,y...)
#endif



/*
 * Task Control Block (TCB)
 */
struct tcb {
	struct list	link;		/* Hash link */
	task_t		task;		/* Task ID */
	char 		cwd[PATH_MAX];	/* Current working directory */
	file_t		cwd_fp;		/* Directory for cwd */
	file_t		file[OPEN_MAX];	/* Array of file pointers */
	int		nr_open;	/* Number of opening files */
	mutex_t		lock;		/* Lock for this task */
};
typedef struct tcb *tcb_t;

extern const struct vfssw vfssw_table[];

extern tcb_t tcb_lookup(task_t task);
extern int tcb_alloc(task_t task, tcb_t *tcb);
extern void tcb_free(tcb_t tcb);
extern void tcb_update(tcb_t tcb, task_t task);
extern void tcb_unlock(tcb_t tcb);
extern file_t tcb_getfp(tcb_t tcb, int fd);
extern int tcb_conv(tcb_t tcb, char *path, char *full);
extern void tcb_dump(void);
extern void tcb_init(void);

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
extern int sys_ioctl(file_t fl, u_int cmd, u_long arg);
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
extern int sys_mknod(char *path, mode_t mode);
extern int sys_rename(char *src, char *dest);
extern int sys_unlink(char *path);

#endif /* !_VFS_H */
