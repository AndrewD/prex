/*-
 * Copyright (c) 2005-2009, Kohsuke Ohtani
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
 * load.c - Load OS modules
 */

#include <boot.h>
#include <load.h>
#include <sys/ar.h>
#include <sys/bootinfo.h>

/* forward declarations */
static int	load_module(struct ar_hdr *, struct module *);
static void	setup_bootdisk(struct ar_hdr *);

paddr_t	load_base;	/* current load address */
paddr_t	load_start;	/* start address for loading */
int	nr_img;		/* number of module images */


/*
 * Load OS images - kernel, driver and boot tasks.
 *
 * It reads each module file image and copy it to the appropriate
 * memory area. The image is built as generic an archive (.a) file.
 *
 * The image information is stored into the boot information area.
 */
void
load_os(void)
{
	char *hdr;
	struct bootinfo *bi = bootinfo;
	struct module *m;
	char *magic;
	int i;
	long len;

	/*
	 * Initialize our data.
	 */
	load_base = 0;
	load_start = 0;
	nr_img = 0;

	/*
	 *  Sanity check of archive image.
	 */
	magic = (char *)kvtop(CONFIG_BOOTIMG_BASE);
	if (strncmp(magic, ARMAG, 8))
		panic("Invalid OS image");

	/*
	 * Load kernel module.
	 */
	hdr = (char *)((paddr_t)magic + 8);
	if (load_module((struct ar_hdr *)hdr, &bi->kernel))
		panic("Can not load kernel");

	/*
	 * Load driver module.
	 */
	len = atol((char *)&((struct ar_hdr *)hdr)->ar_size);
	len += len % 2;	/* even alignment */
	if (len == 0)
		panic("Invalid driver image");
	hdr = (char *)((paddr_t)hdr + sizeof(struct ar_hdr) + len);
	if (load_module((struct ar_hdr *)hdr, &bi->driver))
		panic("Can not load driver");

	/*
	 * Load boot tasks.
	 */
	i = 0;
	m = (struct module *)&bi->tasks[0];
	while (1) {
		/* Proceed to next archive header */
		len = atol((char *)&((struct ar_hdr *)hdr)->ar_size);
		len += len % 2;	/* even alignment */
		if (len == 0)
			break;
		hdr = (char *)((paddr_t)hdr + sizeof(struct ar_hdr) + len);

		/* Check archive header */
		if (strncmp((char *)&((struct ar_hdr *)hdr)->ar_fmag,
			    ARFMAG, 2))
			break;

		/* Load boot disk image */
		if (!strncmp((char *)&((struct ar_hdr *)hdr)->ar_name,
			    "bootdisk.a", 10)) {
			setup_bootdisk((struct ar_hdr *)hdr);
			continue;
		}

		/* Load task */
		if (load_module((struct ar_hdr *)hdr, m))
			break;
		i++;
		m++;
	}

	bi->nr_tasks = i;

	if (bi->nr_tasks == 0)
		panic("No boot task found!");

	/*
	 * Reserve single memory block for all boot modules.
	 * This includes kernel, driver, and boot tasks.
	 */
	i = bi->nr_rams;
	bi->ram[i].base = load_start;
	bi->ram[i].size = (size_t)(load_base - load_start);
	bi->ram[i].type = MT_RESERVED;
	bi->nr_rams++;
}

/*
 * Load module.
 * Return 0 on success, -1 on failure.
 */
static int
load_module(struct ar_hdr *hdr, struct module *m)
{
	char *c;

	if (strncmp((char *)&hdr->ar_fmag, ARFMAG, 2)) {
		DPRINTF(("Invalid image %s\n", hdr->ar_name));
		return -1;
	}
	strlcpy(m->name, hdr->ar_name, sizeof(m->name));
	c = m->name;
	while (*c != '/' && *c != ' ')
		c++;
	*c = '\0';

 	DPRINTF(("loading: hdr=%lx module=%lx name=%s\n",
		 (paddr_t)hdr, (paddr_t)m, m->name));

	if (load_elf((char *)hdr + sizeof(struct ar_hdr), m))
		panic("Load error");

	return 0;
}

/*
 * Setup boot disk
 */
static void
setup_bootdisk(struct ar_hdr *hdr)
{
	struct bootinfo *bi = bootinfo;
	paddr_t base;
	size_t size;

	/*
	 * Store image information.
	 */
	if (strncmp((char *)&hdr->ar_fmag, ARFMAG, 2)) {
		DPRINTF(("Invalid bootdisk image\n"));
		return;
	}
	size = (size_t)atol((char *)&hdr->ar_size);
	size += size % 2;	/* even alignment */
	if (size == 0) {
		DPRINTF(("Size of bootdisk is zero\n"));
		return;
	}
	base = (paddr_t)hdr + sizeof(struct ar_hdr);
	bi->bootdisk.base = base;
	bi->bootdisk.size = size;

#if !defined(CONFIG_ROMBOOT)
	/*
	 * Reserve memory for boot disk if the image
	 * was copied to RAM.
	 */
	bi->ram[bi->nr_rams].base = base;
	bi->ram[bi->nr_rams].size = size;
	bi->ram[bi->nr_rams].type = MT_BOOTDISK;
	bi->nr_rams++;
#endif
	DPRINTF(("bootdisk base=%lx size=%lx\n",
		 bi->bootdisk.base, bi->bootdisk.size));
}
