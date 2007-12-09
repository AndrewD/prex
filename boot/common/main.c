/*-
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
 * main.c - Prex boot loader main module
 */

#include <bootinfo.h>
#include <boot.h>
#include <ar.h>

u_long load_base;	/* Current load address */
u_long load_start;	/* Start address for loading */
int nr_img;		/* number of images */

/*
 * Pointer to boot information.
 * The boot information is placed at the pre-defined memory area.
 * 'BOOTINFO_ADDR' must be defined properly for each platform.
 */
static struct boot_info *boot_info = (struct boot_info *)BOOTINFO_ADDR;

/*
 * Define memory block as reserved area.
 */
void reserve_memory(u_long start, size_t size)
{
	int i;

	printk("reserve_memory: start=%x size=%x\n", start, size);

	for (i = 0; i < 8; i++) {
		if (boot_info->reserved[i].size == 0) {
			boot_info->reserved[i].start = start;
			boot_info->reserved[i].size = size;
			break;
		}
	}
	if (i == 8)
		panic("No memory slot to reserve");
}

/*
 * Load all images.
 * Return 0 on success, -1 on failure.
 */
static int load_image(struct ar_hdr *hdr, struct img_info *info)
{
	char *c;

	if (strncmp((char *)&hdr->eol, EOLSIG, 2))
		return -1;

	strncpy((char *)&info->name, (char *)&hdr->name, 16);
	c = (char *)&info->name;
	while (*c != '/' && *c != ' ')
		c++;
	*c = '\0';
 	printk("load_image hdr=%x info=%x name=%s\n",
	       (int)hdr, (int)info, (char *)&info->name);
	if (elf_load((char *)hdr + sizeof(struct ar_hdr), info))
		panic("Load error");
	return 0;
}

#ifdef CONFIG_RAMDISK
/*
 * Setup for RAMDISK
 */
static void setup_ramdisk(struct ar_hdr *hdr)
{
	struct mem_info *ram_disk;
	size_t size;

	if (strncmp((char *)&hdr->eol, EOLSIG, 2))
		return;
	size = (size_t)atol((char *)&hdr->size);
	if (size == 0)
		return;

	ram_disk = (struct mem_info *)&boot_info->ram_disk;
	ram_disk->start = (u_long)hdr + sizeof(struct ar_hdr);
	ram_disk->size = size;

	printk("RAM disk base=%x size=%x\n", ram_disk->start, ram_disk->size);
}
#endif /* CONFIG_RAMDISK */

/*
 * Setup OS images - kernel, driver and boot tasks.
 *
 * It reads each module file image and copy it to the appropriate
 * memory area. The image is built as generic archive (.a) file.
 *
 * The image information is strored into the boot information area.
 */
static void setup_image(void)
{
	char *hdr;
	struct img_info *img;
	char *magic;
	int i;
	long len;

	/*
	 *  Validate archive image
	 */
	magic = (char *)ARCHIVE_START;
	if (strncmp(magic, ARCMAG, 8))
		panic("Invalid OS image");

	/*
	 * Load kernel image
	 */
	hdr = (char *)((u_long)magic + 8);
	if (load_image((struct ar_hdr *)hdr, &boot_info->kernel))
		panic("Can not load kernel");

	/*
	 * Load driver module
	 */
	len = atol((char *)&((struct ar_hdr *)hdr)->size);
	if (len == 0)
		panic("Invalid OS image");
	hdr = (char *)((u_long)hdr + sizeof(struct ar_hdr) + len);
	if (load_image((struct ar_hdr *)hdr, &boot_info->driver))
		panic("Can not load driver");

	/*
	 * Load boot tasks
	 */
	i = 0;
	img = (struct img_info *)&boot_info->tasks[0];
	while (1) {
		/* Proceed to next archive header */
		len = atol((char *)&((struct ar_hdr *)hdr)->size);
		if (len == 0)
			break;
		hdr = (char *)((u_long)hdr + sizeof(struct ar_hdr) + len);

		/* Pad to even boundary */
		hdr += ((u_long)hdr % 2);

		/* Check archive header */
		if (strncmp((char *)&((struct ar_hdr *)hdr)->eol, EOLSIG, 2))
			break;

#ifdef CONFIG_RAMDISK
		/* Load RAM disk image */
		if (!strncmp((char *)&((struct ar_hdr *)hdr)->name,
			    "ramdisk.a", 9)) {
			setup_ramdisk((struct ar_hdr *)hdr);
			break;
		}
#endif /* CONFIG_RAMDISK */
		/* Load task */
		if (load_image((struct ar_hdr *)hdr, img))
			break;
		i++;
		img++;
	}
	boot_info->nr_tasks = i;

	if (boot_info->nr_tasks == 0)
		panic("No boot task found!");

	/*
	 * Save information for boot modules.
	 * This includes kernel, driver, and boot tasks.
	 */
	boot_info->boot_modules.start = load_start;
	boot_info->boot_modules.size = (size_t)(load_base - load_start);
}

#ifdef DEBUG_BOOT
static void dump_image(struct img_info *img)
{
	printk
	    ("%s: entry=%x phys=%x size=%x text=%x data=%x textsz=%x datasz=%x bss=%x\n",
	     img->name, (int)img->entry, (int)img->phys, (int)img->size,
	     (int)img->text, (int)img->data, (int)img->text_size,
	     (int)img->data_size, (int)img->bss_size);
}

static void dump_bootinfo(void)
{
	struct img_info *img;
	int i;

	printk("main memory start=%x size=%x\n",
	       (int)boot_info->main_mem.start,
	       (int)boot_info->main_mem.size);

	for (i = 0; i < 8; i++) {
		if (boot_info->reserved[i].size != 0) {
			printk("reserved mem start=%x size=%x\n",
			       (int)boot_info->reserved[i].start,
			       (int)boot_info->reserved[i].size);
		}
	}
	printk("ramdisk     start=%x size=%x\n",
	       (int)boot_info->ram_disk.start,
	       (int)boot_info->ram_disk.size);

	dump_image(&boot_info->kernel);
	dump_image(&boot_info->driver);

	img = (struct img_info *)&boot_info->tasks[0];
	for (i = 0; i < boot_info->nr_tasks; i++, img++)
		dump_image(img);
}
#endif

/*
 * C entry point
 */
void loader_main(void)
{
	u_int kernel_entry;

	printk("Prex Boot Loader V1.00\n");

	load_base = 0;
	load_start = 0;
	nr_img = 0;

	memset(boot_info, 0, sizeof(struct boot_info));
	get_meminfo(boot_info);

	setup_image();

#ifdef DEBUG_BOOT
	dump_bootinfo();
#endif
	kernel_entry = (unsigned int)phys_to_virt(boot_info->kernel.entry);
	printk("kernel_entry=%x\n", kernel_entry);

	start_kernel(kernel_entry, (unsigned int)boot_info);
}
