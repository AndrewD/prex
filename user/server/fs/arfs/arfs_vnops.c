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
 * arfs_vnops.c - vnode operations for archive file system.
 */

#include <prex/prex.h>

#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/dirent.h>
#include <sys/buf.h>

#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>

#include "arfs.h"


static char iobuf[BSIZE*2];
static mutex_t arfs_lock = MUTEX_INITIALIZER;

/*
 * Read two blocks.
 * iobuf is filled by read data.
 */
static int arfs_readblk(mount_t mp, int blkno)
{
	struct buf *bp;
	int err;

	/* Read two blocks for archive header */
	if ((err = bread(mp->m_dev, blkno, &bp)) != 0)
		return err;
	memcpy(iobuf, bp->b_data, BSIZE);
	brelse(bp);
	
	if ((err = bread(mp->m_dev, blkno + 1, &bp)) != 0)
		return err;
	memcpy(iobuf + BSIZE, bp->b_data, BSIZE);
	brelse(bp);
	return 0;
}

/*
 * Lookup vnode for the specified file/directory.
 * The vnode is filled properly.
 */
static int arfs_lookup(vnode_t dvp, char *name, vnode_t vp)
{
	struct ar_hdr *hdr;
	int blkno, err;
	off_t offset;
	size_t size;
	mount_t mp;
	char *p;

	dprintf("lookup: name=%s\n", name);
	if (*name == '\0')
		return ENOENT;

	mutex_lock(&arfs_lock);
	
	err = ENOENT;
	mp = vp->v_mount;
	blkno = 0;
	offset = SARMAG;	/* offset in archive image */
	for (;;) {
		/* Read two blocks for archive header */
		if ((err = arfs_readblk(mp, blkno)) != 0)
			goto out;

		/* Check file header */
		hdr = (struct ar_hdr *)(iobuf + (offset % BSIZE));
		if (strncmp(hdr->ar_fmag, ARFMAG, sizeof(ARFMAG) - 1))
			goto out;

		/* Get file size */
		size = (size_t)atol((char *)&hdr->ar_size);
		if (size == 0)
			goto out;

		/* Convert archive name */
		if ((p = memchr(&hdr->ar_name, '/', 16)) != NULL)
			*p = '\0';

		if (strncmp(name, (char *)&hdr->ar_name, 16) == 0)
			break;

		/* Proceed to next archive header */
		offset += (sizeof(struct ar_hdr) + size);
		offset += (offset % 2); /* Pad to even boundary */

		blkno = (int)(offset / BSIZE);
	}
	vp->v_type = VREG;
	vp->v_mode = (mode_t)(S_IRWXU | S_IRWXG | S_IRWXO | S_IFREG);
	vp->v_size = size;
	vp->v_blkno = blkno;
	vp->v_data = (void *)(offset + sizeof(struct ar_hdr));
	err = 0;
 out:
	mutex_unlock(&arfs_lock);
	dprintf("lookup: err=%d\n\n", err);
	return err;
}

static int arfs_read(vnode_t vp, file_t fp, void *buf, size_t size, size_t *result)
{
	off_t offset, file_pos, buf_pos;
	int blkno, err;
	size_t nr_read, nr_copy;
	mount_t mp;
	struct buf *bp;

	dprintf("****read: start size=%d\n", size);
	mutex_lock(&arfs_lock);

	*result = 0;
	mp = vp->v_mount;

	/* Check if current file position is already end of file. */
	file_pos = fp->f_offset;
	if (file_pos >= vp->v_size) {
		err = 0;
		goto out;
	}
	/* Get the actual read size. */
	if (vp->v_size - file_pos < size)
		size = vp->v_size - file_pos;

	/* Read and copy data */
	offset = (off_t)vp->v_data;
	nr_read = 0;
	for (;;) {
		dprintf("file_pos=%d buf=%x size=%d\n", file_pos, buf, size);

		blkno = (offset + file_pos) / BSIZE;
		buf_pos = (offset + file_pos) % BSIZE;
		if ((err = bread(mp->m_dev, blkno, &bp)) != 0)
			goto out;
		nr_copy = BSIZE;
		if (buf_pos > 0)
			nr_copy -= buf_pos;
		if (buf_pos + size < BSIZE)
			nr_copy = size;
		ASSERT(nr_copy > 0);
		memcpy(buf, bp->b_data + buf_pos, nr_copy);
		brelse(bp);

		file_pos += nr_copy;
		dprintf("file_pos=%d nr_copy=%d\n", file_pos, nr_copy);

		nr_read += nr_copy;
		size -= nr_copy;
		if (size <= 0)
			break;
		buf += nr_copy;
		buf_pos = 0;
	}
	fp->f_offset = file_pos;
	*result = nr_read;
	err = 0;
 out:
	mutex_unlock(&arfs_lock);
	dprintf("read: err=%d\n\n", err);
	return err;
}

/*
 * Check if the seek offset is valid.
 */
static int arfs_seek(vnode_t vp, file_t fp, off_t oldoff, off_t newoff)
{
	if (newoff > vp->v_size)
		return -1;
	return 0;
}

static int arfs_readdir(vnode_t vp, file_t fp, struct dirent *dir)
{
	struct ar_hdr *hdr;
	int blkno, i, err;
	off_t offset;
	size_t size;
	mount_t mp;
	char *p;

	dprintf("readdir: start\n");
	mutex_lock(&arfs_lock);
	
	i = 0;
	mp = vp->v_mount;
	blkno = 0;
	offset = SARMAG;	/* offset in archive image */
	for (;;) {
		/* Read two blocks for archive header */
		if ((err = arfs_readblk(mp, blkno)) != 0)
			goto out;

		hdr = (struct ar_hdr *)(iobuf + (offset % BSIZE));

		/* Get file size */
		size = (size_t)atol((char *)&hdr->ar_size);
		if (size == 0) {
			err = ENOENT;
			goto out;
		}
		if (i == fp->f_offset)
			break;

		/* Proceed to next archive header */
		offset += (sizeof(struct ar_hdr) + size);
		offset += (offset % 2); /* Pad to even boundary */

		blkno = offset / BSIZE;
		i++;
	}

	/* Convert archive name */
	if ((p = memchr(&hdr->ar_name, '/', 16)) != NULL)
		*p = '\0';

	strcpy((char *)&dir->d_name, (char *)&hdr->ar_name);
	dir->d_namlen = strlen(dir->d_name);
	dir->d_fileno = fp->f_offset;
	dir->d_type = DT_REG;

	fp->f_offset++;
	err = 0;
 out:
	mutex_unlock(&arfs_lock);
	return err;
}


const struct vnops arfs_vnops = {
	.open     = VOP_NULL,
	.close    = VOP_NULL,
	.read     = arfs_read,
	.write    = VOP_NULL,
	.seek     = arfs_seek,
	.ioctl    = VOP_EINVAL,
	.fsync    = VOP_NULL,
	.readdir  = arfs_readdir,
	.lookup   = arfs_lookup,
	.create   = VOP_EINVAL,
	.remove   = VOP_EINVAL,
	.rename   = VOP_EINVAL,
	.mkdir    = VOP_EINVAL,
	.rmdir    = VOP_EINVAL,
	.getattr  = VOP_NULL,
	.setattr  = VOP_NULL,
	.inactive = VOP_NULL,
};

int arfs_init(void)
{
	return 0;
}
