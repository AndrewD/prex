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

#include <sys/list.h>
#include <sys/vnode.h>
#include <sys/mount.h>

#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "vfs.h"

/*
 * vnode.c - vnode service
 */

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

#define VNODE_BUCKETS 32		/* Size of vnode hash table */

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
#if NR_FS_THREADS > 1
static mutex_t vnode_lock = MUTEX_INITIALIZER;
#define VNODE_LOCK()	mutex_lock(&vnode_lock)
#define VNODE_UNLOCK()	mutex_unlock(&vnode_lock)
#else
#define VNODE_LOCK()
#define VNODE_UNLOCK()
#endif

/*
 * Get the hash value from path name and mount point.
 */
static u_int vn_hash(mount_t mp, char *path)
{
	u_int val = 0;

	if (path) {
		while (*path)
			val = ((val << 5) + val) + *path++;
	}
	return (val ^ (u_int) mp) & (VNODE_BUCKETS - 1);
}

/*
 * Returns locked vnode for specified mount point and path.
 */
vnode_t vn_lookup(mount_t mp, char *path)
{
	list_t head, n;
	vnode_t vp;

	VNODE_LOCK();
	head = &vnode_table[vn_hash(mp, path)];
	for (n = list_first(head); n != head; n = list_next(n)) {
		vp = list_entry(n, struct vnode, v_link);
		if (vp->v_mount == mp &&
		    !strncmp(vp->v_path, path, PATH_MAX)) {
			mutex_lock(&vp->v_lock);
			VNODE_UNLOCK();
			return vp;
		}
	}
	VNODE_UNLOCK();
	return NULL;		/* not found */
}

/*
 * Lock vnode
 */
void vn_lock(vnode_t vp)
{
	ASSERT(vp);
	vn_printf("vn_lock:   %s\n", vp->v_path);

	VNODE_LOCK();
	mutex_lock(&vp->v_lock);
	VNODE_UNLOCK();
}

/*
 * Unlock vnode
 */
void vn_unlock(vnode_t vp)
{
	ASSERT(vp);
	vn_printf("vn_unlock: %s\n", vp->v_path);

	VNODE_LOCK();
	mutex_unlock(&vp->v_lock);
	VNODE_UNLOCK();
}

/*
 * Allocate new vnode for specified path, and increment its reference count
 * and lock it.
 */
vnode_t vget(mount_t mp, char *path)
{
	vnode_t vp;
	int err;

	vn_printf("vget: %s\n", path);

	if (!(vp = malloc(sizeof(struct vnode))))
		return NULL;
	memset(vp, 0, sizeof(struct vnode));

	if (!(vp->v_path = malloc(strlen(path) + 1))) {
		free(vp);
		return NULL;
	}
	vp->v_mount = mp;
	vp->v_count = 1;
	vp->v_op = mp->m_op->vnops;
	strcpy(vp->v_path, path);
	mutex_init(&vp->v_lock);

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

	VNODE_LOCK();
	list_insert(&vnode_table[vn_hash(mp, path)], &vp->v_link);
	VNODE_UNLOCK();
	return vp;
}

/*
 * Unlock vnode and decrement its reference count.
 */
void vput(vnode_t vp)
{
	vn_printf("vput: count=%d %s\n", vp->v_count, vp->v_path);

	vp->v_count--;
	if (vp->v_count > 0) {
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
	mutex_destroy(&vp->v_lock);
	free(vp->v_path);
	free(vp);
}

/*
 * Increment the reference count on an active vnode.
 */
void vref(vnode_t vp)
{
	ASSERT(vp);
	ASSERT(vp->v_count > 0);	/* Need vget */
	vn_printf("vref: count=%d %s\n", vp->v_count, vp->v_path);

	VNODE_LOCK();
	vp->v_count++;
	VNODE_UNLOCK();
}

/*
 * Decrement the reference count of unlocked vnode.
 */
void vrele(vnode_t vp)
{
	ASSERT(vp);
	ASSERT(vp->v_count > 0);
	vn_printf("vrele: count=%d %s\n", vp->v_count, vp->v_path);

	VNODE_LOCK();
	vp->v_count--;
	if (vp->v_count > 0) {
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
 * vgone() is called when vnode is no longer valid.
 */
void vgone(vnode_t vp)
{
	vn_printf("vgone: %s\n", vp->v_path);

	VNODE_LOCK();
	list_remove(&vp->v_link);
	VNODE_UNLOCK();

	vfs_unbusy(vp->v_mount);
	mutex_destroy(&vp->v_lock);
	free(vp->v_path);
	free(vp);
}

/*
 * Return reference count.
 */
int vcount(vnode_t vp)
{
	int count;

	vn_lock(vp);
	count = vp->v_count;
	vn_unlock(vp);
	return count;
}


/*
 * Remove all vnode in the vnode table for unmount.
 */
void vflush(mount_t mp)
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

#ifdef DEBUG
/*
 * Dump all all vnode.
 */
void vnode_dump(void)
{
	int i;
	list_t head, n;
	vnode_t vp;
	mount_t mp;
	char type[][6] = { "VNON ", "VREG ", "VDIR ", "VBLK ", "VCHR ", 
			   "VLNK ", "VSOCK", "VFIFO" };

	VNODE_LOCK();
	printf("Dump vnode\n");
	printf(" vnode    mount    type  refcnt blkno    path\n");
	printf(" -------- -------- ----- ------ -------- ------------------------------\n");
	for (i = 0; i < VNODE_BUCKETS; i++) {
		head = &vnode_table[i];
		for (n = list_first(head); n != head; n = list_next(n)) {
			vp = list_entry(n, struct vnode, v_link);
			mp = vp->v_mount;
			printf(" %08x %08x %s %6d %8d %s%s\n", (u_int)vp,
			       (u_int)mp, type[vp->v_type], vp->v_count,
			       (u_int)vp->v_blkno,
			       (strlen(mp->m_path) == 1) ? "\0" : mp->m_path,
			       vp->v_path);
		}
	}
	printf("\n");
	VNODE_UNLOCK();
}
#endif

int vop_null(void)
{
	return 0;
}

int vop_einval(void)
{
	return EINVAL;
}

int vfs_null(void)
{
	return 0;
}

void vnode_init(void)
{
	int i;

	for (i = 0; i < VNODE_BUCKETS; i++)
		list_init(&vnode_table[i]);
}
