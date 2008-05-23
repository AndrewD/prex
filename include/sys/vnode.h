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

#ifndef _SYS_VNODE_H
#define _SYS_VNODE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/list.h>
#include <sys/dirent.h>
#include <sys/syslimits.h>

struct vfsops;
struct vnops;
struct vnode;
struct file;

/*
 * Vnode types.
 */
enum {
	VNON,	    /* no type */
	VREG,	    /* regular file  */
	VDIR,	    /* directory */
	VBLK,	    /* block device */
	VCHR,	    /* character device */
	VLNK,	    /* symbolic link */
	VSOCK,	    /* socks */
	VFIFO	    /* FIFO */
};

/*
 * Reading or writing any of these items requires holding the
 * appropriate lock.
 */
struct vnode {
	struct list	v_link;		/* link for hash list */
	struct mount	*v_mount;	/* mounted vfs pointer */
	struct vnops	*v_op;		/* vnode operations */
	int		v_refcount;	/* reference count */
	int		v_type;		/* vnode type */
	int		v_flags;	/* vnode flag */
	mode_t		v_mode;		/* file mode */
	size_t		v_size;		/* file size */
	mutex_t		v_lock;		/* lock for this vnode */
	cond_t		v_cond;		/* condition variable for this vnode */
	int		v_nrlocks;	/* lock count (for debug) */
	int		v_blkno;	/* block number */
	char		*v_path;	/* pointer to path in fs */
	void		*v_data;	/* private data for fs */
};
typedef struct vnode *vnode_t;

/* flags for vnode */
#define VROOT		0x0001		/* root of its file system */

/*
 * Vnode attribute
 */
struct vattr {
	int		va_type;	/* vnode type */
	mode_t		va_mode;	/* file access mode */
};

/*
 * Modes
 */
#define VREAD		0x0004
#define VWRITE		0x0002
#define VEXEC		0x0001

/*
 * vnode operations
 */
struct vnops {
	int (*open)	(vnode_t vp, int flags, mode_t mode);
	int (*close)	(vnode_t vp, file_t fp);
	int (*read)	(vnode_t vp, file_t fp, void *buf, size_t size, size_t *result);
	int (*write)	(vnode_t vp, file_t fp, void *buf, size_t size, size_t *result);
	int (*seek)	(vnode_t vp, file_t fp, off_t oldoff, off_t newoff);
	int (*ioctl)	(vnode_t vp, file_t fp, int cmd, u_long arg);
	int (*fsync)	(vnode_t vp, file_t fp);
	int (*readdir)	(vnode_t vp, file_t fp, struct dirent *dirent);
	int (*lookup)	(vnode_t dvp, char *name, vnode_t vp);
	int (*create)	(vnode_t dvp, char *name, mode_t mode);
	int (*remove)	(vnode_t dvp, vnode_t vp, char *name);
	int (*rename)	(vnode_t dvp1, vnode_t vp1, char *name1, vnode_t dvp2, vnode_t vp2, char *name2);
	int (*mkdir)	(vnode_t dvp, char *name, mode_t mode);
	int (*rmdir)	(vnode_t dvp, vnode_t vp, char *name);
	int (*mkfifo)	(vnode_t dvp, char *name, mode_t mode);
	int (*getattr)	(vnode_t vp, struct vattr *vap);
	int (*setattr)	(vnode_t vp, struct vattr *vap);
	int (*inactive)	(vnode_t vp);
	int (*truncate)	(vnode_t vp);
};

typedef	int (*vnop_open_t)	(vnode_t, int, mode_t);
typedef	int (*vnop_close_t)	(vnode_t, file_t);
typedef	int (*vnop_read_t)	(vnode_t, file_t, void *, size_t, size_t *);
typedef	int (*vnop_write_t)	(vnode_t, file_t, void *, size_t, size_t *);
typedef	int (*vnop_seek_t)	(vnode_t, file_t, off_t, off_t);
typedef	int (*vnop_ioctl_t)	(vnode_t, file_t, int, u_long);
typedef	int (*vnop_fsync_t)	(vnode_t, file_t);
typedef	int (*vnop_readdir_t)	(vnode_t, file_t, struct dirent *);
typedef	int (*vnop_lookup_t)	(vnode_t, char *, vnode_t);
typedef	int (*vnop_create_t)	(vnode_t, char *, mode_t);
typedef	int (*vnop_remove_t)	(vnode_t, vnode_t, char *);
typedef	int (*vnop_rename_t)	(vnode_t, vnode_t, char *, vnode_t, vnode_t, char *);
typedef	int (*vnop_mkdir_t)	(vnode_t, char *, mode_t);
typedef	int (*vnop_rmdir_t)	(vnode_t, vnode_t, char *);
typedef	int (*vnop_mkfifo_t)	(vnode_t, char *, mode_t);
typedef	int (*vnop_getattr_t)	(vnode_t, struct vattr *);
typedef	int (*vnop_setattr_t)	(vnode_t, struct vattr *);
typedef	int (*vnop_inactive_t)	(vnode_t);
typedef	int (*vnop_truncate_t)	(vnode_t);

/*
 * vnode interface
 */
#define VOP_OPEN(VP, F, M)	   ((VP)->v_op->open)(VP, F, M)
#define VOP_CLOSE(VP, FP)	   ((VP)->v_op->close)(VP, FP)
#define VOP_READ(VP, FP, B, S, C)  ((VP)->v_op->read)(VP, FP, B, S, C)
#define VOP_WRITE(VP, FP, B, S, C) ((VP)->v_op->write)(VP, FP, B, S, C)
#define VOP_SEEK(VP, FP, OLD, NEW) ((VP)->v_op->seek)(VP, FP, OLD, NEW)
#define VOP_IOCTL(VP, FP, C, A)	   ((VP)->v_op->ioctl)(VP, FP, C, A)
#define VOP_FSYNC(VP, FP)	   ((VP)->v_op->fsync)(VP, FP)
#define VOP_READDIR(VP, FP, DIR)   ((VP)->v_op->readdir)(VP, FP, DIR)
#define VOP_LOOKUP(DVP, N, VP)	   ((DVP)->v_op->lookup)(DVP, N, VP)
#define VOP_CREATE(DVP, N, M)	   ((DVP)->v_op->create)(DVP, N, M)
#define VOP_REMOVE(DVP, VP, N)	   ((DVP)->v_op->remove)(DVP, VP, N)
#define VOP_RENAME(DVP1, VP1, N1, DVP2, VP2, N2) \
			   ((DVP1)->v_op->rename)(DVP1, VP1, N1, DVP2, VP2, N2)
#define VOP_MKDIR(DVP, N, M)	   ((DVP)->v_op->mkdir)(DVP, N, M)
#define VOP_RMDIR(DVP, VP, N)	   ((DVP)->v_op->rmdir)(DVP, VP, N)
#define VOP_MKFIFO(DVP, N, M)	   ((DVP)->v_op->mkfifo)(DVP, N, M)
#define VOP_GETATTR(VP, VAP)	   ((VP)->v_op->getattr)(VP, VAP)
#define VOP_SETATTR(VP, VAP)	   ((VP)->v_op->setattr)(VP, VAP)
#define VOP_INACTIVE(VP)	   ((VP)->v_op->inactive)(VP)
#define VOP_TRUNCATE(VP)	   ((VP)->v_op->truncate)(VP)

__BEGIN_DECLS
int	 vop_nullop(void);
int	 vop_einval(void);
__END_DECLS

#endif /* !_SYS_VNODE_H */
