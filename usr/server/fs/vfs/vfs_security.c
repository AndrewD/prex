/*-
 * Copyright (c) 2007, Kohsuke Ohtani All rights reserved.
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
 * vfs_security.c - Routines to check security permission.
 */

/**
 * General Design:
 *
 * Prex supports the file access permission based on the path
 * name. It means that the applications can access to only
 * certain area of the file system.
 *
 * The file system has the following structure:
 *
 * /boot
 *
 *   This directory contains the server executable files and
 *   the system files required for OS boot. Reading this
 *   directory is allowed only to the server process which has
 *   CAP_SYSFILES capability.  Nobody can write to this
 *   directory at any time.
 *
 * /bin
 *
 *   This directory contains the trusted applications.  This
 *   is read-only directory for normal processes, and
 *   CAP_SYSFILES is required to write files to it. In typical
 *   case, a software installer has right to copy the
 *   executable files to this directory.
 *
 * /etc
 *
 *   This directory contains the various configuration files.
 *   This is read-only for normal processes, and CAP_SYSFILES
 *   is required to modify /etc. The system configurator has
 *   responsible to modify the contents in /etc.
 *
 * /private
 *
 *   This is the restricted system area and is inaccessible to
 *   the normal processes. It contains private user data like
 *   address book or calendar entries. CAP_USERFILES
 *   capability is required to access to the /private
 *   contents. The PIM application will have CAP_USERFILES.
 *
 * /all the rest
 *
 *   Access to all the other directories is unrestricted.
 *
 *
 * <Directories and required capabilities>
 *
 *  Directory  Read           Write          Execute
 *  ---------  -------------  -------------  --------------
 *  /boot      CAP_SYSFILES   Not Allowed    Any
 *
 *  /bin       Any            CAP_SYSFILES   Any
 *
 *  /etc       Any            CAP_SYSFILES   Not Allowed
 *
 *  /private   CAP_USERFILES  CAP_USERFILES  Not Allowed
 *
 *  /other     Any            Any            Not Allowed
 *
 */

#include <sys/prex.h>
#include <sys/list.h>
#include <sys/fcntl.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "vfs.h"

#define ACC_NG		-1		/* access is not allowed */
#define ACC_OK		0		/* access is allowed */

/*
 * Capability mapping for path and file access
 */
struct fscap_map {
	char	*path;			/* directory name */
	size_t	len;			/* length of directory name */
	int	cap_read;		/* required capability to read */
	int	cap_write;		/* required capability to write */
	int	cap_exec;		/* required capability to execute */
};

/*
 * Capability mapping table
 */
static const struct fscap_map fscap_table[] =
{
	/* path        len read           write          execute       */
	/* ----------- --- -------------- -------------- ------------- */
	{ "/boot/",     6, CAP_SYSFILES,  ACC_NG,        ACC_OK       },
	{ "/bin/",      5, ACC_OK,        CAP_SYSFILES,  ACC_OK       },
	{ "/etc/",      5, ACC_OK,        CAP_SYSFILES,  ACC_NG       },
	{ "/private/",  9, CAP_USERFILES, CAP_USERFILES, ACC_NG       },
	{ NULL,         0, 0,             0,             0            },
};


/*
 * Return true if the task has specified capability.
 */
static int
capable(task_t task, cap_t cap)
{

	if (cap == ACC_OK)
		return 1;

	if (cap == ACC_NG)
		return 0;

	if (task_chkcap(task, cap) != 0) {
		/* No capability */
		return 0;
	}
	return 1;
}

/*
 * Check if the task has capability to access the file.
 * Return 0 if it has capability.
 */
int
sec_file_permission(task_t task, char *path, int acc)
{
	const struct fscap_map *map;
	int found = 0;
	int error = 0;

	if (acc == 0)
		return 0;

	/*
	 * Look up capability mapping table.
	 */
	map = &fscap_table[0];
	while (map->path != NULL) {
		if (!strncmp(path, map->path, map->len)) {
			found = 1;
			break;
		}
		map++;
	}

	if (found) {
		/*
		 * File under known directory.
		 */
		if (acc & VREAD) {
			if (!capable(task, map->cap_read))
				error = EACCES;
		}
		if (acc & VWRITE) {
			if (!capable(task, map->cap_write))
				error = EACCES;
		}
		DPRINTF(VFSDB_CAP,
			("sec_file_permission: known directory "
			 "path=%s read=%x write=%x execute=%d\n",
			 path, map->cap_read, map->cap_write, map->cap_exec));
	}

	if (error != 0) {
		DPRINTF(VFSDB_CAP,
			("sec_file_permission: no capability for %02x "
			 "task=%08x path=%s\n",
			 acc, task, path));
	}
	return error;
}

/*
 * Check if the file is executable.
 */
int
sec_vnode_permission(char *path)
{
	const struct fscap_map *map;
	int found = 0;

	/*
	 * Look up capability mapping table.
	 */
	map = &fscap_table[0];
	while (map->path != NULL) {
		if (!strncmp(path, map->path, map->len)) {
			found = 1;
			break;
		}
		map++;
	}

	/*
	 * We allow the file execution only with the file
	 * under specific directories.
	 */
	if ((found == 1) && (map->cap_exec == ACC_OK)) {
		return 0;
	}
	return EACCES;
}
