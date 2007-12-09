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

#include <prex/prex.h>

#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/syslog.h>

#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>

/* #define DEBUG_DEVFS 1 */

#if DEBUG_DEVFS
#define dprintf(x,y...) syslog(LOG_DEBUG, "devfs: "x, ##y)
#else
#define dprintf(x,y...)
#endif

struct vnops devfs_vnops;

static int devfs_open(vnode_t vp, mode_t mode)
{
	char *path;
	device_t dev;
	int err;

	dprintf("devfs_open: path=%s\n", vp->v_path);

	path = vp->v_path;
	if (!strcmp(path, "/"))	/* root ? */
		return 0;

	if (*path == '/')
		path++;
	err = device_open(path, mode & DO_RWMASK, &dev);
	if (err) {
		dprintf("devfs_open: can not open device = %s error=%d\n",
		    path, err);
		return err;
	}
	vp->v_data = (void *)dev;	/* Store private data */

	/* XXX: set both flags for charactor/block device */
	vp->v_mode |= (S_IFCHR | S_IFBLK);
	return 0;
}

static int devfs_close(vnode_t vp, file_t fp)
{
	dprintf("devfs_close: fp=%x\n", fp);

	if (!strcmp(vp->v_path, "/"))	/* root ? */
		return 0;

	return device_close((device_t)vp->v_data);
}

static int devfs_read(vnode_t vp, file_t fp, void *buf, size_t size, size_t *result)
{
	int err;
	size_t len;

	len = size;
	err = device_read((device_t)vp->v_data, buf, &len, fp->f_offset);
	if (!err)
		*result = len;
	return err;
}

static int devfs_write(vnode_t vp, file_t fp, void *buf, size_t size, size_t *result)
{
	int err;
	size_t len;

	len = size;
	err = device_write((device_t)vp->v_data, buf, &len, fp->f_offset);
	if (!err)
		*result = len;
	return err;
}

static int devfs_ioctl(vnode_t vp, file_t fp, u_int cmd, u_long arg)
{
	int err;

	err = device_ioctl((device_t)vp->v_data, cmd, arg);
	return err;
}

static int devfs_lookup(vnode_t dvp, char *name, vnode_t vp)
{
	dprintf("devfs_lookup:%s\n", name);

	if (*name == '\0')
		return ENOENT;
	vp->v_type = VBLK;
	vp->v_mode = S_IRWXU | S_IRWXG | S_IRWXO;	/* All permission */
	return 0;
}

/*
 * @vp: vnode of the directory.
 */
static int devfs_readdir(vnode_t vp, file_t fp, struct dirent *dir)
{
	struct info_device info;
	int err, i;

	dprintf("devfs_readdir offset=%d\n", fp->f_offset);

	i = 0;
	err = 0;
	info.cookie = 0;
	do {
		err = sys_info(INFO_DEVICE, &info);
		if (err)
			return ENOENT;
	} while (i++ != fp->f_offset);

	dir->d_type = DT_CHR;
	strcpy((char *)&dir->d_name, info.name);
	dir->d_fileno = fp->f_offset;
	dir->d_namlen = strlen(dir->d_name);

	dprintf("devfs_readdir: %s\n", dir->d_name);
	fp->f_offset++;
	return 0;
}

struct vfsops devfs_vfsops = {
	.mount    = VFS_NULL,
	.unmount  = VFS_NULL,
	.sync     = VFS_NULL,
	.vget     = VFS_NULL,
	.statfs   = VFS_NULL,
	.vnops	  = &devfs_vnops,
};

struct vnops devfs_vnops = {
	.open     = devfs_open,
	.close    = devfs_close,
	.read     = devfs_read,
	.write    = devfs_write,
	.seek     = VOP_NULL,
	.ioctl    = devfs_ioctl,
	.fsync    = VOP_NULL,
	.readdir  = devfs_readdir,
	.lookup   = devfs_lookup,
	.create   = VOP_EINVAL,
	.remove   = VOP_EINVAL,
	.rename   = VOP_EINVAL,
	.mkdir    = VOP_EINVAL,
	.rmdir    = VOP_EINVAL,
	.getattr  = VOP_NULL,
	.setattr  = VOP_NULL,
	.inactive = VOP_NULL,
};

int devfs_init(void)
{
	return 0;
}
