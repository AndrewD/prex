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

/*
 * Boot information
 *
 * The boot information is stored by an OS loader, and
 * it is refered by kernel later in boot time.
 *
 * IMPORTANT: If you change this file, you must change the  boot
 * information in kernel, too.
 */

#ifndef _BOOTINFO_H
#define _BOOTINFO_H

#include <types.h>

/*
 * Image information for kernel, driver, and boot tasks.
 * An OS loader will build this structure regardless of its file format.
 */
struct img_info
{
	char	name[16];	/* Name of image */
	u_long	phys;		/* Physical address */
	size_t	size;		/* Size of image */
	u_long	entry;		/* Entry address */
	u_long	text;		/* Text address */
	u_long	data;		/* Data address */
	size_t	text_size;	/* Text size */
	size_t	data_size;	/* Data size */
	size_t	bss_size;	/* Bss size */
};

/*
 * Memory area
 */
struct mem_info
{
	u_long	start;		/* Start address */
	size_t	size;		/* Size in bytes */
};

/*
 * Boot information
 */
struct boot_info
{
	struct mem_info main_mem;	/* Main memory */
	struct mem_info reserved[8];	/* Reserved memory */
	struct mem_info ram_disk;	/* RAM disk image in memory */
	struct mem_info boot_modules;	/* Memory include kernel, driver etc */
	int		nr_tasks;	/* Number of boot tasks */
	struct img_info kernel;		/* Kernel image */
	struct img_info driver;		/* Driver image */
	struct img_info tasks[1];	/* Boot tasks image */
};

#endif /* !_BOOTINFO_H */
