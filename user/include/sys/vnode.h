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

#ifndef _SYS_VNODE_H
#define _SYS_VNODE_H

#include <prex/prex.h>

#include <sys/stat.h>
#include <sys/file.h>
#include <sys/list.h>
#include <sys/dirent.h>

#include <unistd.h>
#include <limits.h>
#include <stdio.h>

struct vfsops;
struct vnops;
struct vnode;
struct file;


/*
 * vnode types.
 */
enum {
	VNON,	    /* No type */
	VREG,	    /* Regular file  */
	VDIR,	    /* Directory */
	VDEV,	    /* Device */
	VLNK,	    /* Symbolic link */
	VSOCK,	    /* Socks */
	VFIFO,	    /* FIFO */
};

/*
 * vnode data
 */
struct vnode {
	struct list	v_link;		/* Link for hash list */
	struct mount	*v_mount;	/* Mounted vfs pointer */
	struct vnops	*v_op;		/* vnode operations */
	int		v_count;	/* Reference count */
	int		v_type;		/* vnode type */
	int		v_flags;	/* vnode flag */
	mode_t		v_mode;		/* File mode */
	size_t		v_size;		/* File size */
	int		v_blkno;	/* Block number */
	char		*v_path;	/* Pointer to path in fs */
	void		*v_data;	/* Private data for fs */
};
typedef struct vnode *vnode_t;

/*
 * vnode flags.
 */
#define VROOT		0x0001		/* Root of its file system */

/*
 * vnode attribute
 */
struct vattr {
	int		va_type;	/* vnode type */
	mode_t		va_mode;	/* File access mode */

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
	int (*open)	(vnode_t vp, mode_t mode);
	int (*close)	(vnode_t vp, file_t fp);
	int (*read)	(vnode_t vp, file_t fp, void *buf, size_t size, size_t *result);
	int (*write)	(vnode_t vp, file_t fp, void *buf, size_t size, size_t *result);
	int (*seek)	(vnode_t vp, file_t fp, off_t oldoff, off_t newoff);
	int (*ioctl)	(vnode_t vp, file_t fp, u_int cmd, u_long arg);
	int (*fsync)	(vnode_t vp, file_t fp);
	int (*readdir)	(vnode_t vp, file_t fp, struct dirent *dirent);
	int (*lookup)	(vnode_t dvp, char *name, vnode_t vp);
	int (*create)	(vnode_t dvp, char *name, mode_t mode);
	int (*remove)	(vnode_t dvp, vnode_t vp, char *name);
	int (*rename)	(vnode_t dvp1, vnode_t vp1, char *name1, vnode_t dvp2, vnode_t vp2, char *name2);
	int (*mkdir)	(vnode_t dvp, char *name, mode_t mode);
	int (*rmdir)	(vnode_t dvp, vnode_t vp, char *name);
	int (*getattr)	(vnode_t vp, struct vattr *vap);
	int (*setattr)	(vnode_t vp, struct vattr *vap);
	int (*inactive)	(vnode_t vp);
};

/*
 * vnode interface
 */
#define VOP_OPEN(VP, M)		   ((VP)->v_op->open)(VP, M)
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
#define VOP_GETATTR(VP, VAP)	   ((VP)->v_op->getattr)(VP, VAP)
#define VOP_SETATTR(VP, VAP)	   ((VP)->v_op->setattr)(VP, VAP)
#define VOP_INACTIVE(VP)	   ((VP)->v_op->inactive)(VP)

int vfs_default();
int vfs_error();

#endif /* !_SYS_VNODE_H */
