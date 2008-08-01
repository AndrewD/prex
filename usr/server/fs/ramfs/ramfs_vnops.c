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

static int ramfs_open(vnode_t, int, mode_t);
static int ramfs_close(vnode_t, file_t);
static int ramfs_read(vnode_t, file_t, void *, size_t, size_t *);
static int ramfs_write(vnode_t, file_t, void *, size_t, size_t *);
static int ramfs_seek(vnode_t, file_t, off_t, off_t);
#define ramfs_ioctl	((vnop_ioctl_t)vop_einval)
#define ramfs_fsync	((vnop_fsync_t)vop_nullop)
static int ramfs_readdir(vnode_t, file_t, struct dirent *);
static int ramfs_lookup(vnode_t, char *, vnode_t);
static int ramfs_create(vnode_t, char *, int, mode_t);
static int ramfs_remove(vnode_t, vnode_t, char *);
static int ramfs_rename(vnode_t, vnode_t, char *, vnode_t, vnode_t, char *);
static int ramfs_mkdir(vnode_t, char *, mode_t);
static int ramfs_rmdir(vnode_t, vnode_t, char *);
static int ramfs_mkfifo(vnode_t, char *, mode_t);
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
	struct ramfs_node *node;

	node = malloc(sizeof(struct ramfs_node));
	if (node == NULL)
		return NULL;
	memset(node, 0, sizeof(struct ramfs_node));

	node->namelen = strlen(name);
	node->name = malloc(node->namelen + 1);
	if (node->name == NULL) {
		free(node);
		return NULL;
	}
	strcpy(node->name, name);
	node->type = type;
	return node;
}

void
ramfs_free_node(struct ramfs_node *node)
{

	free(node->name);
	free(node);
}

static struct ramfs_node *
ramfs_add_node(struct ramfs_node *dir_node, char *name, int type)
{
	struct ramfs_node *node, *prev;

	node = ramfs_allocate_node(name, type);
	if (node == NULL)
		return NULL;

	mutex_lock(&ramfs_lock);

	/* Link to the directory list */
	if (dir_node->child == NULL) {
		dir_node->child = node;
	} else {
		prev = dir_node->child;
		while (prev->next != NULL)
			prev = prev->next;
		prev->next = node;
	}
	mutex_unlock(&ramfs_lock);
	return node;
}

static int
ramfs_remove_node(struct ramfs_node *dir_node, struct ramfs_node *node)
{
	struct ramfs_node *prev;

	if (dir_node->child == NULL)
		return EBUSY;

	mutex_lock(&ramfs_lock);

	/* Unlink from the directory list */
	if (dir_node->child == node) {
		dir_node->child = node->next;
	} else {
		for (prev = dir_node->child; prev->next != node;
		     prev = prev->next) {
			if (prev->next == NULL) {
				mutex_unlock(&ramfs_lock);
				return ENOENT;
			}
		}
		prev->next = node->next;
	}
	ramfs_free_node(node);

	mutex_unlock(&ramfs_lock);
	return 0;
}

static int
ramfs_rename_node(struct ramfs_node *node, char *name)
{
	size_t len;
	char *tmp;

	len = strlen(name);
	if (len <= node->namelen) {
		/* Reuse current name buffer */
		strcpy(node->name, name);
	} else {
		/* Expand name buffer */
		tmp = malloc(len + 1);
		if (tmp == NULL)
			return ENOMEM;
		strcpy(tmp, name);
		free(node->name);
		node->name = tmp;
	}
	node->namelen = len;
	return 0;
}

static int
ramfs_lookup(vnode_t dvp, char *name, vnode_t vp)
{
	struct ramfs_node *node, *dir_node;
	size_t len;

	if (*name == '\0')
		return ENOENT;

	mutex_lock(&ramfs_lock);

	len = strlen(name);
	dir_node = dvp->v_data;
	for (node = dir_node->child; node != NULL; node = node->next) {
		if (node->namelen == len &&
		    memcmp(name, node->name, len) == 0)
			break;
	}
	if (node == NULL) {
		mutex_unlock(&ramfs_lock);
		return ENOENT;
	}
	vp->v_data = node;
	vp->v_mode = ALLPERMS;
	vp->v_type = node->type;
	vp->v_size = (vp->v_type == VFIFO) ? 0 : node->size;

	mutex_unlock(&ramfs_lock);
	return 0;
}

static int
ramfs_mkdir(vnode_t dvp, char *name, mode_t mode)
{
	struct ramfs_node *node;

	dprintf("mkdir %s\n", name);
	if (!S_ISDIR(mode))
		return EINVAL;

	node = ramfs_add_node(dvp->v_data, name, VDIR);
	if (node == NULL)
		return ENOMEM;
	node->size = 0;
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
	    && node->read_fds == 0)
		return ENXIO;	/* posix */

	switch (flags & (FREAD | FWRITE)) {
	case FREAD:
		node->read_fds++;
		break;

	case FWRITE:
		node->write_fds++;
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
			if (--node->read_fds == 0)
				notify(vp); /* wake blocked write */
		} else if (--node->write_fds == 0)
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

	dprintf("mkfifo %s in %s\n", name, dvp->v_path);
	if (!S_ISFIFO(mode))
		return EINVAL;

	node = ramfs_add_node(dvp->v_data, name, VFIFO);
	if (node == NULL)
		return ENOMEM;

	node->buf = malloc(PIPE_BUF);
	/* NOTE: node->bufsize node->size abused as fifo read / write counts */
	if (node->buf == NULL) {
		ramfs_remove_node(dvp->v_data, node);
		return ENOMEM;
	}
	return 0;
}

/* Remove a file */
static int
ramfs_remove(vnode_t dvp, vnode_t vp, char *name)
{
	struct ramfs_node *node;

	dprintf("remove %s in %s\n", name, dvp->v_path);
	node = vp->v_data;
	if (node->buf != NULL) {
		if (vp->v_type == VFIFO)
			free(node->buf);
		else
			vm_free(task_self(), node->buf);
		node->buf = NULL; /* incase remove_node fails */
		node->bufsize = 0;
	}
	vp->v_size = 0;
	return ramfs_remove_node(dvp->v_data, node);
}

/* Truncate file */
static int
ramfs_truncate(vnode_t vp)
{
	struct ramfs_node *node;

	dprintf("truncate %s\n", vp->v_path);
	node = vp->v_data;
	if (node->buf != NULL) {
		vm_free(task_self(), node->buf);
		node->buf = NULL;
		node->bufsize = 0;
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
	struct ramfs_node *node;

	dprintf("create %s in %s\n", name, dvp->v_path);
	if (!S_ISREG(mode))
		return EINVAL;

	node = ramfs_add_node(dvp->v_data, name, VREG);
	if (node == NULL)
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
	struct ramfs_node *node = vp->v_data;

	while (size != 0) {
		size_t avail = node->bufsize - node->size;
		dprintf("read: %d, %d remaining\n", read, size);
		if (avail == 0) {
			if (node->write_fds == 0)
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
		size_t off = node->size & (PIPE_BUF-1);

		/* contiguius data available to end of curcular buffer */
		if (avail > PIPE_BUF - off)
			avail = PIPE_BUF - off;

		size_t len = (size < avail) ? size : avail;
		dprintf("read: off %d len %d avail %d\n", off, len, avail);
		memcpy(buf, node->buf + off, len);
		node->size += len;
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
	struct ramfs_node *node = vp->v_data;

	while (size != 0) {
		if (node->read_fds == 0) {
			err = EPIPE;
			break;
		}
		size_t free = PIPE_BUF - (node->bufsize - node->size);
		dprintf("written: %d, %d remaining\n", written, size);
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
		size_t off = node->bufsize & (PIPE_BUF-1);
		if (free > PIPE_BUF - off)
			free = PIPE_BUF - off; /* space wrapped in buffer */

		size_t len = (size < free) ? size : free;
		dprintf("write: off %d len %d free %d\n", off, len, free);
		memcpy(node->buf + off, buf, len);
		node->bufsize += len;
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
	struct ramfs_node *node;
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

	node = vp->v_data;
	memcpy(buf, node->buf + off, size);

	fp->f_offset += size;
	*result = size;
	return 0;
}

static int
ramfs_write(vnode_t vp, file_t fp, void *buf, size_t size, size_t *result)
{
	struct ramfs_node *node;
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

	node = vp->v_data;
	/* Check if the file position exceeds the end of file. */
	end_pos = vp->v_size;
	file_pos = (fp->f_flags & O_APPEND) ? end_pos : fp->f_offset;
	if (file_pos + size > (size_t)end_pos) {
		/* Expand the file size before writing to it */
		end_pos = file_pos + size;
		if (end_pos > (off_t)node->bufsize) {
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
			if (node->size != 0) {
				memcpy(new_buf, node->buf, vp->v_size);
				vm_free(task, node->buf);
			}
			if (vp->v_size < (size_t)file_pos) /* sparse file */
				memset((char *)new_buf + vp->v_size, 0,
				       file_pos - vp->v_size);
			node->buf = new_buf;
			node->bufsize = new_size;
		}
		node->size = end_pos;
		vp->v_size = end_pos;
	}
	memcpy(node->buf + file_pos, buf, size);
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
	struct ramfs_node *node, *old_node;
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
		old_node = vp1->v_data;
		node = ramfs_add_node(dvp2->v_data, name2, VREG);
		if (node == NULL)
			return ENOMEM;

		if (vp1->v_type == VREG) {
			/* Copy file data */
			node->buf = old_node->buf;
			node->size = old_node->size;
			node->bufsize = old_node->bufsize;
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
	struct ramfs_node *node, *dir_node;
	off_t i;

	mutex_lock(&ramfs_lock);

	if (fp->f_offset == 0) {
		dir->d_type = DT_DIR;
		strcpy((char *)&dir->d_name, ".");
	} else if (fp->f_offset == 1) {
		dir->d_type = DT_DIR;
		strcpy((char *)&dir->d_name, "..");
	} else {
		dir_node = vp->v_data;
		node = dir_node->child;
		if (node == NULL) {
			mutex_unlock(&ramfs_lock);
			return ENOENT;
		}

		for (i = 0; i != (fp->f_offset - 2); i++) {
			node = node->next;
			if (node == NULL) {
				mutex_unlock(&ramfs_lock);
				return ENOENT;
			}
		}
		if (node->type == VDIR)
			dir->d_type = DT_DIR;
		else
			dir->d_type = DT_REG;
		strcpy((char *)&dir->d_name, node->name);
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
