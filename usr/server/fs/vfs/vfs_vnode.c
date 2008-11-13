/*
 * Copyright (c) 2005-2008, Kohsuke Ohtani
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
 * vnode.c - vnode service
 */

#include <prex/prex.h>
#include <sys/list.h>
#include <sys/vnode.h>
#include <sys/mount.h>

#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "vfs.h"

/*
 * Memo:
 *
 * Function   Ref count Lock
 * ---------- --------- ----------
 * vn_lock     *        Lock
 * vn_unlock   *        Unlock
 * vget        1        Lock
 * vput       -1        Unlock
 * vref       +1        *
 * vrele      -1        *
 */

#define VNODE_BUCKETS 32		/* size of vnode hash table */

/*
 * vnode table.
 * All active (opened) vnodes are stored on this hash table.
 * They can be accessed by its path name.
 */
static struct list vnode_table[VNODE_BUCKETS];

/*
 * Global lock to access all vnodes and vnode table.
 * If a vnode is already locked, there is no need to
 * lock this global lock to access internal data.
 */
#if CONFIG_FS_THREADS > 1
static mutex_t vnode_lock = MUTEX_INITIALIZER;
#define VNODE_LOCK()	mutex_lock(&vnode_lock)
#define VNODE_UNLOCK()	mutex_unlock(&vnode_lock)
#else
#define VNODE_LOCK()
#define VNODE_UNLOCK()
#endif


/*
 * Get the hash value from the mount point and path name.
 */
static u_int
vn_hash(mount_t mp, char *path)
{
	u_int val = 0;

	if (path) {
		while (*path)
			val = ((val << 5) + val) + *path++;
	}
	return (val ^ (u_int)mp) & (VNODE_BUCKETS - 1);
}

/*
 * Returns locked vnode for specified mount point and path.
 * vn_lock() will increment the reference count of vnode.
 */
vnode_t
vn_lookup(mount_t mp, char *path)
{
	list_t head, n;
	vnode_t vp;

	VNODE_LOCK();
	head = &vnode_table[vn_hash(mp, path)];
	for (n = list_first(head); n != head; n = list_next(n)) {
		vp = list_entry(n, struct vnode, v_link);
		if (vp->v_mount == mp &&
		    !strncmp(vp->v_path, path, PATH_MAX)) {
			vp->v_refcnt++;
			VNODE_UNLOCK();
			mutex_lock(&vp->v_lock);
			vp->v_nrlocks++;
			return vp;
		}
	}
	VNODE_UNLOCK();
	return NULL;		/* not found */
}

/*
 * Lock vnode
 */
void
vn_lock(vnode_t vp)
{
	ASSERT(vp);
	ASSERT(vp->v_refcnt > 0);

	mutex_lock(&vp->v_lock);
	vp->v_nrlocks++;
	DPRINTF(VFSDB_VNODE, ("vn_lock:   %s\n", vp->v_path));
}

/*
 * Unlock vnode
 */
void
vn_unlock(vnode_t vp)
{
	ASSERT(vp);
	ASSERT(vp->v_refcnt > 0);
	ASSERT(vp->v_nrlocks > 0);

	DPRINTF(VFSDB_VNODE, ("vn_unlock: %s\n", vp->v_path));
	vp->v_nrlocks--;
	mutex_unlock(&vp->v_lock);
}

/*
 * Allocate new vnode for specified path.
 * Increment its reference count and lock it.
 */
vnode_t
vget(mount_t mp, char *path)
{
	vnode_t vp;
	int err;

	DPRINTF(VFSDB_VNODE, ("vget: %s\n", path));

	if (!(vp = malloc(sizeof(struct vnode))))
		return NULL;
	memset(vp, 0, sizeof(struct vnode));

	if (!(vp->v_path = malloc(strlen(path) + 1))) {
		free(vp);
		return NULL;
	}
	vp->v_mount = mp;
	vp->v_refcnt = 1;
	vp->v_op = mp->m_op->vfs_vnops;
	strcpy(vp->v_path, path);
	mutex_init(&vp->v_lock);
	vp->v_nrlocks = 0;

	/*
	 * Request to allocate fs specific data for vnode.
	 */
	if ((err = VFS_VGET(mp, vp)) != 0) {
		mutex_destroy(&vp->v_lock);
		free(vp->v_path);
		free(vp);
		return NULL;
	}
	vfs_busy(vp->v_mount);
	mutex_lock(&vp->v_lock);
	vp->v_nrlocks++;

	VNODE_LOCK();
	list_insert(&vnode_table[vn_hash(mp, path)], &vp->v_link);
	VNODE_UNLOCK();
	return vp;
}

/*
 * Unlock vnode and decrement its reference count.
 */
void
vput(vnode_t vp)
{
	ASSERT(vp);
	ASSERT(vp->v_nrlocks > 0);
	ASSERT(vp->v_refcnt > 0);
	DPRINTF(VFSDB_VNODE, ("vput: ref=%d %s\n", vp->v_refcnt,
			      vp->v_path));

	vp->v_refcnt--;
	if (vp->v_refcnt > 0) {
		vn_unlock(vp);
		return;
	}
	VNODE_LOCK();
	list_remove(&vp->v_link);
	VNODE_UNLOCK();

	/*
	 * Deallocate fs specific vnode data
	 */
	VOP_INACTIVE(vp);
	vfs_unbusy(vp->v_mount);
	vp->v_nrlocks--;
	ASSERT(vp->v_nrlocks == 0);
	mutex_unlock(&vp->v_lock);
	mutex_destroy(&vp->v_lock);
	free(vp->v_path);
	free(vp);
}

/*
 * Increment the reference count on an active vnode.
 */
void
vref(vnode_t vp)
{
	ASSERT(vp);
	ASSERT(vp->v_refcnt > 0);	/* Need vget */

	VNODE_LOCK();
	DPRINTF(VFSDB_VNODE, ("vref: ref=%d %s\n", vp->v_refcnt,
			      vp->v_path));
	vp->v_refcnt++;
	VNODE_UNLOCK();
}

/*
 * Decrement the reference count of unlocked vnode.
 * Any code in the system which is using vnode should call vrele()
 * when it is finished with the vnode.
 * If count drops to zero, call inactive routine and return to freelist.
 */
void
vrele(vnode_t vp)
{
	ASSERT(vp);
	ASSERT(vp->v_nrlocks == 0);
	ASSERT(vp->v_refcnt > 0);

	VNODE_LOCK();
	DPRINTF(VFSDB_VNODE, ("vrele: ref=%d %s\n", vp->v_refcnt,
			      vp->v_path));
	vp->v_refcnt--;
	if (vp->v_refcnt > 0) {
		VNODE_UNLOCK();
		return;
	}
	list_remove(&vp->v_link);
	VNODE_UNLOCK();

	/*
	 * Deallocate fs specific vnode data
	 */
	VOP_INACTIVE(vp);
	vfs_unbusy(vp->v_mount);
	mutex_destroy(&vp->v_lock);
	free(vp->v_path);
	free(vp);
}

/*
 * vgone() is called when unlocked vnode is no longer valid.
 */
void
vgone(vnode_t vp)
{
	ASSERT(vp->v_nrlocks == 0);

	VNODE_LOCK();
	DPRINTF(VFSDB_VNODE, ("vgone: %s\n", vp->v_path));
	list_remove(&vp->v_link);
	vfs_unbusy(vp->v_mount);
	mutex_destroy(&vp->v_lock);
	free(vp->v_path);
	free(vp);
	VNODE_UNLOCK();
}

/*
 * Return reference count.
 */
int
vcount(vnode_t vp)
{
	int count;

	vn_lock(vp);
	count = vp->v_refcnt;
	vn_unlock(vp);
	return count;
}

/*
 * Remove all vnode in the vnode table for unmount.
 */
void
vflush(mount_t mp)
{
	int i;
	list_t head, n;
	vnode_t vp;

	VNODE_LOCK();
	for (i = 0; i < VNODE_BUCKETS; i++) {
		head = &vnode_table[i];
		for (n = list_first(head); n != head; n = list_next(n)) {
			vp = list_entry(n, struct vnode, v_link);
			if (vp->v_mount == mp) {
				/* XXX: */
			}
		}
	}
	VNODE_UNLOCK();
}

int
vn_stat(vnode_t vp, struct stat *st)
{
	mode_t mode;

	memset(st, 0, sizeof(struct stat));

	st->st_ino = (ino_t)vp;
	st->st_size = vp->v_size;
	mode = vp->v_mode;
	switch (vp->v_type) {
	case VREG:
		mode |= S_IFREG;
		break;
	case VDIR:
		mode |= S_IFDIR;
		break;
	case VBLK:
		mode |= S_IFBLK;
		break;
	case VCHR:
		mode |= S_IFCHR;
		break;
	case VLNK:
		mode |= S_IFLNK;
		break;
	case VSOCK:
		mode |= S_IFSOCK;
		break;
	case VFIFO:
		mode |= S_IFIFO;
		break;
	default:
		return EBADF;
	};
	st->st_mode = mode;
	st->st_blksize = BSIZE;
	st->st_blocks = vp->v_size / S_BLKSIZE;
	st->st_uid = 0;
	st->st_gid = 0;
	if (vp->v_type == VCHR || vp->v_type == VBLK)
		st->st_rdev = (dev_t)vp->v_data;

	return 0;
}

#ifdef DEBUG
/*
 * Dump all all vnode.
 */
void
vnode_dump(void)
{
	int i;
	list_t head, n;
	vnode_t vp;
	mount_t mp;
	char type[][6] = { "VNON ", "VREG ", "VDIR ", "VBLK ", "VCHR ",
			   "VLNK ", "VSOCK", "VFIFO" };

	VNODE_LOCK();
	dprintf("Dump vnode\n");
	dprintf(" vnode    mount    type  refcnt blkno    path\n");
	dprintf(" -------- -------- ----- ------ -------- ------------------------------\n");

	for (i = 0; i < VNODE_BUCKETS; i++) {
		head = &vnode_table[i];
		for (n = list_first(head); n != head; n = list_next(n)) {
			vp = list_entry(n, struct vnode, v_link);
			mp = vp->v_mount;

			dprintf(" %08x %08x %s %6d %8d %s%s\n", (u_int)vp,
				(u_int)mp, type[vp->v_type], vp->v_refcnt,
				(u_int)vp->v_blkno,
				(strlen(mp->m_path) == 1) ? "\0" : mp->m_path,
				vp->v_path);
		}
	}
	dprintf("\n");
	VNODE_UNLOCK();
}
#endif

int
vop_nullop(void)
{

	return 0;
}

int
vop_einval(void)
{

	return EINVAL;
}

void
vnode_init(void)
{
	int i;

	for (i = 0; i < VNODE_BUCKETS; i++)
		list_init(&vnode_table[i]);
}
