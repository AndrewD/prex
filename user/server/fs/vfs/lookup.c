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

/*
 * lookup.c - vnode lookup function.
 */

#include <sys/vnode.h>
#include <sys/mount.h>

#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <stdlib.h>

#include "vfs.h"

/*
 * Convert a pathname into a pointer to a locked vnode.
 *
 * @path: full path name.
 * @vp:   vnode to be returned.
 */
int namei(char *path, vnode_t *vpp)
{
	char *p;
	char node[PATH_MAX];
	char name[PATH_MAX];
	mount_t mp;
	vnode_t dvp, vp;
	int err, i;

	vn_printf("namei: path=%s\n", path);

	/*
	 * Convert a full path name to its mount point and
	 * the local node in the file system.
	 */
	if (vfs_findroot(path, &mp, &p))
		return ENOTDIR;
	strcpy(node, "/");
	strlcat(node, p, PATH_MAX);
	vp = vn_lookup(mp, node);
	if (vp) {
		/* vnode is already active. */
		vref(vp);
		*vpp = vp;
		return 0;
	}
	/*
	 * Find target vnode, started from root directory. This is
	 * done to attach the a fs specific data to the target vnode.
	 */
	if ((dvp = mp->m_root) == NULL)
		panic("fs: no root");

	vref(dvp);
	vn_lock(dvp);
	node[0] = '\0';

	while (*p != '\0') {
		/*
		 * Get lower directory/file name.
		 */
		while (*p == '/')
			p++;
		for (i = 0; i < PATH_MAX; i++) {
			if (*p == '\0' || *p == '/')
				break;
			name[i] = *p++;
		}
		name[i] = '\0';

		/*
		 * Get a vnode for the target.
		 */
		strlcat(node, "/", PATH_MAX);
		strlcat(node, name, PATH_MAX);
		vp = vn_lookup(mp, node);
		if (vp) {
			vref(vp);
		} else {
			vp = vget(mp, node);
			if (vp == NULL) {
				vput(dvp);
				return ENOMEM;
			}
			/* Find a vnode in this directory. */
			err = VOP_LOOKUP(dvp, name, vp);
			if (err || (*p == '/' && vp->v_type != VDIR)) {
				/* Not found */
				vput(vp);
				vput(dvp);
				return err;
			}
		}
		vput(dvp);
		dvp = vp;
		while (*p != '\0' && *p != '/')
			p++;
	}
	*vpp = vp;
	return 0;
}

/*
 * Search a pathname.
 * This is a very central but not so complicated routine. ;-P
 * This routine returns a locked directory vnode and file name.
 *
 * @path: full path.
 * @vpp:  pointer to locked vnode for directory.
 * @name: pointer to file name in path.
 */
int lookup(char *path, vnode_t *vpp, char **name)
{
	char buf[PATH_MAX];
	char root[] = "/";
	char *file, *dir;
	vnode_t vp;
	int err;

	/*
	 * Get the path for directory.
	 */
	strcpy(buf, path);
	file = strrchr(buf, '/');
	if (!buf)
		return ENOTDIR;
	if (file == buf)
		dir = root;
	else {
		*file = '\0';
		dir = buf;
	}
	/*
	 * Get the vnode for directory
	 */
	if ((err = namei(dir, &vp)) != 0)
		return err;
	if (vp->v_type != VDIR) {
		vput(vp);
		return ENOTDIR;
	}
	*vpp = vp;

	/*
	 * Get the file name
	 */
	*name = strrchr(path, '/') + 1;
	return 0;
}
