/*
 * Copyright (c) 2006-2007, Kohsuke Ohtani
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
 * rmafs_vnops.c - vnode operations for RAM file system.
 */

#include <prex/prex.h>

#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/dirent.h>
#include <sys/param.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include "ramfs.h"

static int ramfs_open	(vnode_t, int, mode_t);
static int ramfs_close	(vnode_t, file_t);
static int ramfs_read	(vnode_t, file_t, void *, size_t, size_t *);
static int ramfs_write	(vnode_t, file_t, void *, size_t, size_t *);
static int ramfs_seek	(vnode_t, file_t, off_t, off_t);
#define ramfs_ioctl	((vnop_ioctl_t)vop_einval)
#define ramfs_fsync	((vnop_fsync_t)vop_nullop)
static int ramfs_readdir(vnode_t, file_t, struct dirent *);
static int ramfs_lookup	(vnode_t, char *, vnode_t);
static int ramfs_create(vnode_t, char *, int, mode_t);
static int ramfs_remove	(vnode_t, vnode_t, char *);
static int ramfs_rename	(vnode_t, vnode_t, char *, vnode_t, vnode_t, char *);
static int ramfs_mkdir	(vnode_t, char *, mode_t);
static int ramfs_rmdir	(vnode_t, vnode_t, char *);
static int ramfs_mkfifo	(vnode_t, char *, mode_t);
#define ramfs_getattr	((vnop_getattr_t)vop_nullop)
#define ramfs_setattr	((vnop_setattr_t)vop_nullop)
#define ramfs_inactive	((vnop_inactive_t)vop_nullop)
static int ramfs_truncate(vnode_t);


#if CONFIG_FS_THREADS > 1
static mutex_t ramfs_lock = MUTEX_INITIALIZER;
#endif

/*
 * vnode operations
 */
struct vnops ramfs_vnops = {
	ramfs_open,		/* open */
	ramfs_close,		/* close */
	ramfs_read,		/* read */
	ramfs_write,		/* write */
	ramfs_seek,		/* seek */
	ramfs_ioctl,		/* ioctl */
	ramfs_fsync,		/* fsync */
	ramfs_readdir,		/* readdir */
	ramfs_lookup,		/* lookup */
	ramfs_create,		/* create */
	ramfs_remove,		/* remove */
	ramfs_rename,		/* remame */
	ramfs_mkdir,		/* mkdir */
	ramfs_rmdir,		/* rmdir */
	ramfs_mkfifo,		/* mkfifo */
	ramfs_getattr,		/* getattr */
	ramfs_setattr,		/* setattr */
	ramfs_inactive,		/* inactive */
	ramfs_truncate,		/* truncate */
};

struct ramfs_node *
ramfs_allocate_node(char *name, int type)
{
	struct ramfs_node *np;

	np = malloc(sizeof(struct ramfs_node));
	if (np == NULL)
		return NULL;
	memset(np, 0, sizeof(struct ramfs_node));

	np->rn_namelen = strlen(name);
	np->rn_name = malloc(np->rn_namelen + 1);
	if (np->rn_name == NULL) {
		free(np);
		return NULL;
	}
	strcpy(np->rn_name, name);
	np->rn_type = type;
	return np;
}

void
ramfs_free_node(struct ramfs_node *np)
{

	free(np->rn_name);
	free(np);
}

static struct ramfs_node *
ramfs_add_node(struct ramfs_node *dnp, char *name, int type)
{
	struct ramfs_node *np, *prev;

	np = ramfs_allocate_node(name, type);
	if (np == NULL)
		return NULL;

	mutex_lock(&ramfs_lock);

	/* Link to the directory list */
	if (dnp->rn_child == NULL) {
		dnp->rn_child = np;
	} else {
		prev = dnp->rn_child;
		while (prev->rn_next != NULL)
			prev = prev->rn_next;
		prev->rn_next = np;
	}
	mutex_unlock(&ramfs_lock);
	return np;
}

static int
ramfs_remove_node(struct ramfs_node *dnp, struct ramfs_node *np)
{
	struct ramfs_node *prev;

	if (dnp->rn_child == NULL)
		return EBUSY;

	mutex_lock(&ramfs_lock);

	/* Unlink from the directory list */
	if (dnp->rn_child == np) {
		dnp->rn_child = np->rn_next;
	} else {
		for (prev = dnp->rn_child; prev->rn_next != np;
		     prev = prev->rn_next) {
			if (prev->rn_next == NULL) {
				mutex_unlock(&ramfs_lock);
				return ENOENT;
			}
		}
		prev->rn_next = np->rn_next;
	}
	ramfs_free_node(np);

	mutex_unlock(&ramfs_lock);
	return 0;
}

static int
ramfs_rename_node(struct ramfs_node *np, char *name)
{
	size_t len;
	char *tmp;

	len = strlen(name);
	if (len <= np->rn_namelen) {
		/* Reuse current name buffer */
		strcpy(np->rn_name, name);
	} else {
		/* Expand name buffer */
		tmp = malloc(len + 1);
		if (tmp == NULL)
			return ENOMEM;
		strcpy(tmp, name);
		free(np->rn_name);
		np->rn_name = tmp;
	}
	np->rn_namelen = len;
	return 0;
}

static int
ramfs_lookup(vnode_t dvp, char *name, vnode_t vp)
{
	struct ramfs_node *np, *dnp;
	size_t len;
	int found;

	if (*name == '\0')
		return ENOENT;

	mutex_lock(&ramfs_lock);

	len = strlen(name);
	dnp = dvp->v_data;
	found = 0;
	for (np = dnp->rn_child; np != NULL; np = np->rn_next) {
		if (np->rn_namelen == len &&
		    memcmp(name, np->rn_name, len) == 0) {
			found = 1;
			break;
		}
	}
	if (found == 0) {
		mutex_unlock(&ramfs_lock);
		return ENOENT;
	}
	vp->v_data = np;
	vp->v_mode = ALLPERMS;
	vp->v_type = np->rn_type;
	vp->v_size = (vp->v_type == VFIFO) ? 0 : np->rn_size;

	mutex_unlock(&ramfs_lock);
	return 0;
}

static int
ramfs_mkdir(vnode_t dvp, char *name, mode_t mode)
{
	struct ramfs_node *np;

	DPRINTF(("mkdir %s\n", name));
	if (!S_ISDIR(mode))
		return EINVAL;

	np = ramfs_add_node(dvp->v_data, name, VDIR);
	if (np == NULL)
		return ENOMEM;
	np->rn_size = 0;
	return 0;
}

/* Remove a directory */
static int
ramfs_rmdir(vnode_t dvp, vnode_t vp, char *name)
{

	return ramfs_remove_node(dvp->v_data, vp->v_data);
}

/* notify read or write if blocked */
static void notify(vnode_t vp)
{
	if (vp->v_cond != COND_INITIALIZER)
		cond_signal(&vp->v_cond);
}

/*
 * special handling for opening fifos
 */
static int ramfs_open(vnode_t vp, int flags, mode_t mode)
{
	struct ramfs_node *node;

	if (vp->v_type != VFIFO)
		return 0;

	node = vp->v_data;
	if ((flags & (O_NONBLOCK | FREAD | FWRITE)) == (O_NONBLOCK | FWRITE)
	    && node->rn_read_fds == 0)
		return ENXIO;	/* posix */

	switch (flags & (FREAD | FWRITE)) {
	case FREAD:
		node->rn_read_fds++;
		break;

	case FWRITE:
		node->rn_write_fds++;
		break;

	default:
		return EINVAL;
	}
	return 0;
}

/*
 * special handling for closing fifos
 */
static int ramfs_close(vnode_t vp, file_t fp)
{
	if (vp->v_type == VFIFO) {
		struct ramfs_node *node = vp->v_data;
		if (fp->f_flags & FREAD) {
			if (--node->rn_read_fds == 0)
				notify(vp); /* wake blocked write */
		} else if (--node->rn_write_fds == 0)
			notify(vp); /* wake blocked read */
	}
	return 0;
}

/*
 * Create fifo.
 */
static int
ramfs_mkfifo(vnode_t dvp, char *name, mode_t mode)
{
	struct ramfs_node *node;

	DPRINTF(("mkfifo %s in %s\n", name, dvp->v_path));
	if (!S_ISFIFO(mode))
		return EINVAL;

	node = ramfs_add_node(dvp->v_data, name, VFIFO);
	if (node == NULL)
		return ENOMEM;

	node->rn_buf = malloc(PIPE_BUF);
	/* NOTE: node->bufsize node->size abused as fifo read / write counts */
	if (node->rn_buf == NULL) {
		ramfs_remove_node(dvp->v_data, node);
		return ENOMEM;
	}
	return 0;
}

/* Remove a file */
static int
ramfs_remove(vnode_t dvp, vnode_t vp, char *name)
{
	struct ramfs_node *np;

	DPRINTF(("remove %s in %s\n", name, dvp->v_path));
	np = vp->v_data;
	if (np->rn_buf != NULL) {
		if (vp->v_type == VFIFO)
			free(np->rn_buf);
		else
			vm_free(task_self(), np->rn_buf);
		np->rn_buf = NULL; /* incase remove_node fails */
		np->rn_bufsize = 0;
	}
	vp->v_size = 0;
	return ramfs_remove_node(dvp->v_data, np);
}

/* Truncate file */
static int
ramfs_truncate(vnode_t vp)
{
	struct ramfs_node *np;

	DPRINTF(("truncate %s\n", vp->v_path));
	np = vp->v_data;
	if (np->rn_buf != NULL) {
		vm_free(task_self(), np->rn_buf);
		np->rn_buf = NULL;
		np->rn_bufsize = 0;
	}
	vp->v_size = 0;
	return 0;
}

/*
 * Create empty file.
 */
static int
ramfs_create(vnode_t dvp, char *name, int flags, mode_t mode)
{
	struct ramfs_node *np;

	DPRINTF(("create %s in %s\n", name, dvp->v_path));
	if (!S_ISREG(mode))
		return EINVAL;

	np = ramfs_add_node(dvp->v_data, name, VREG);
	if (np == NULL)
		return ENOMEM;
	return 0;
}

/* From opengroup.org...
 * When attempting to read from an empty pipe or FIFO:

 * If no process has the pipe open for writing, read() will return 0
 to indicate end-of-file.

 * If some process has the pipe open for writing and O_NONBLOCK is
 set, read() will return -1 and set errno to [EAGAIN].

 * If some process has the pipe open for writing and O_NONBLOCK is
 clear, read() will block the calling thread until some data is
 written or the pipe is closed by all processes that had the pipe
 open for writing. */
static int
ramfs_read_fifo(vnode_t vp, file_t fp, void *buf, size_t size, size_t *result)
{
	int err = 0;
	size_t read = 0;	/* bytes read so far */
	struct ramfs_node *np = vp->v_data;

	while (size != 0) {
		size_t avail = np->rn_bufsize - np->rn_size;
		DPRINTF(("read: %d, %d remaining\n", read, size));
		if (avail == 0) {
			if (np->rn_write_fds == 0)
				break; /* no writers: EOF */
			if (fp->f_flags & O_NONBLOCK) {
				err = EAGAIN;
				break;
			}

			/* wait for write or close */
			err = cond_wait(&vp->v_cond, &vp->v_lock, 0);
			if (err)
				break;
			continue; /* validate data available */
		} else if (avail == PIPE_BUF)
			notify(vp); /* notify write: will have space when we
				       unlock the mutex */

		/* offset into circular buf */
		size_t off = np->rn_size & (PIPE_BUF-1);

		/* contiguius data available to end of curcular buffer */
		if (avail > PIPE_BUF - off)
			avail = PIPE_BUF - off;

		size_t len = (size < avail) ? size : avail;
		DPRINTF(("read: off %d len %d avail %d\n", off, len, avail));
		memcpy(buf, np->rn_buf + off, len);
		np->rn_size += len;
		read += len;
		size -= len;
                buf = (uint8_t *)buf + len;
	}

	*result = read;
	return (read) ? 0 : err;
}

static int
ramfs_write_fifo(vnode_t vp, file_t fp, void *buf, size_t size, size_t *result)
{
	int err = 0;
	size_t written = 0;	/* bytes written so far */
	struct ramfs_node *np = vp->v_data;

	while (size != 0) {
		if (np->rn_read_fds == 0) {
			err = EPIPE;
			break;
		}
		size_t free = PIPE_BUF - (np->rn_bufsize - np->rn_size);
		DPRINTF(("written: %d, %d remaining\n", written, size));
		if (free == 0) {
			if (fp->f_flags & O_NONBLOCK) {
				err = EAGAIN;
				break;
			}

			/* wait for read or close */
			err = cond_wait(&vp->v_cond, &vp->v_lock, 0);
			if (err)
				break;
			continue; /* calculate free again */
		} else if (free == PIPE_BUF)
			notify(vp); /* notify read: will have data
				       when we unlock the mutex */

		/* offset into circular buf */
		size_t off = np->rn_bufsize & (PIPE_BUF-1);
		if (free > PIPE_BUF - off)
			free = PIPE_BUF - off; /* space wrapped in buffer */

		size_t len = (size < free) ? size : free;
		DPRINTF(("write: off %d len %d free %d\n", off, len, free));
		memcpy(np->rn_buf + off, buf, len);
		np->rn_bufsize += len;
		written += len;
		size -= len;
                buf = (uint8_t *)buf + len;
	}

	*result = written;
	return (written) ? 0 : err;
}

static int
ramfs_read(vnode_t vp, file_t fp, void *buf, size_t size, size_t *result)
{
	struct ramfs_node *np;
	off_t off;

	*result = 0;
	if (vp->v_type == VFIFO)
		return ramfs_read_fifo(vp, fp, buf, size, result);
	if (vp->v_type == VDIR)
		return EISDIR;
	if (vp->v_type != VREG)
		return EINVAL;

	off = fp->f_offset;
	if (off >= (off_t)vp->v_size)
		return 0;

	if (vp->v_size - off < size)
		size = vp->v_size - off;

	np = vp->v_data;
	memcpy(buf, np->rn_buf + off, size);

	fp->f_offset += size;
	*result = size;
	return 0;
}

static int
ramfs_write(vnode_t vp, file_t fp, void *buf, size_t size, size_t *result)
{
	struct ramfs_node *np;
	off_t file_pos, end_pos;
	void *new_buf;
	size_t new_size;
	task_t task;

	*result = 0;
	if (vp->v_type == VFIFO)
		return ramfs_write_fifo(vp, fp, buf, size, result);
	if (vp->v_type == VDIR)
		return EISDIR;
	if (vp->v_type != VREG)
		return EINVAL;

	np = vp->v_data;
	/* Check if the file position exceeds the end of file. */
	end_pos = vp->v_size;
	file_pos = (fp->f_flags & O_APPEND) ? end_pos : fp->f_offset;
	if (file_pos + size > (size_t)end_pos) {
		/* Expand the file size before writing to it */
		end_pos = file_pos + size;
		if (end_pos > (off_t)np->rn_bufsize) {
			task = task_self();
			/*
			 * We allocate the data buffer in page boundary.
			 * So that we can reduce the memory allocation unless
			 * the file size exceeds next page boundary.
			 * This will prevent the memory fragmentation by
			 * many malloc/free calls.
			 */
			new_size = PAGE_ALIGN(end_pos);
			if (vm_allocate(task, &new_buf, new_size, 1))
				return EIO;
			if (np->rn_size != 0) {
				memcpy(new_buf, np->rn_buf, vp->v_size);
				vm_free(task, np->rn_buf);
			}
			if (vp->v_size < (size_t)file_pos) /* sparse file */
				memset((char *)new_buf + vp->v_size, 0,
				       file_pos - vp->v_size);
			np->rn_buf = new_buf;
			np->rn_bufsize = new_size;
		}
		np->rn_size = end_pos;
		vp->v_size = end_pos;
	}
	memcpy(np->rn_buf + file_pos, buf, size);
	fp->f_offset += size;
	*result = size;
	return 0;
}

static int
ramfs_seek(vnode_t vp, file_t fp, off_t prev_offs, off_t offs)
{
	if (vp->v_type == VFIFO)
		return ESPIPE;
	return 0;
}

static int
ramfs_rename(vnode_t dvp1, vnode_t vp1, char *name1,
	     vnode_t dvp2, vnode_t vp2, char *name2)
{
	struct ramfs_node *np, *old_np;
	int err;

	if (vp2) {
		/* Remove destination file, first */
		err = ramfs_remove_node(dvp2->v_data, vp2->v_data);
		if (err)
			return err;
	}
	/* Same directory ? */
	if (dvp1 == dvp2) {
		/* Change the name of existing file */
		err = ramfs_rename_node(vp1->v_data, name2);
		if (err)
			return err;
	} else {
		/* Create new file or directory */
		old_np = vp1->v_data;
		np = ramfs_add_node(dvp2->v_data, name2, VREG);
		if (np == NULL)
			return ENOMEM;

		if (vp1->v_type == VREG) {
			/* Copy file data */
			np->rn_buf = old_np->rn_buf;
			np->rn_size = old_np->rn_size;
			np->rn_bufsize = old_np->rn_bufsize;
		}
		/* Remove source file */
		ramfs_remove_node(dvp1->v_data, vp1->v_data);
	}
	return 0;
}

/*
 * @vp: vnode of the directory.
 */
static int
ramfs_readdir(vnode_t vp, file_t fp, struct dirent *dir)
{
	struct ramfs_node *np, *dnp;
	off_t i;

	mutex_lock(&ramfs_lock);

	if (fp->f_offset == 0) {
		dir->d_type = DT_DIR;
		strcpy((char *)&dir->d_name, ".");
	} else if (fp->f_offset == 1) {
		dir->d_type = DT_DIR;
		strcpy((char *)&dir->d_name, "..");
	} else {
		dnp = vp->v_data;
		np = dnp->rn_child;
		if (np == NULL) {
			mutex_unlock(&ramfs_lock);
			return ENOENT;
		}

		for (i = 0; i != (fp->f_offset - 2); i++) {
			np = np->rn_next;
			if (np == NULL) {
				mutex_unlock(&ramfs_lock);
				return ENOENT;
			}
		}
		if (np->rn_type == VDIR)
			dir->d_type = DT_DIR;
		else
			dir->d_type = DT_REG;
		strcpy((char *)&dir->d_name, np->rn_name);
	}
	dir->d_fileno = fp->f_offset;
	dir->d_namlen = strlen(dir->d_name);

	fp->f_offset++;

	mutex_unlock(&ramfs_lock);
	return 0;
}

int
ramfs_init(void)
{
	return 0;
}
