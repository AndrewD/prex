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

#ifndef _FATFS_H
#define _FATFS_H

#include <prex/prex.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/syslog.h>
#include <sys/buf.h>

/* #define DEBUG_FATFS 1 */

#ifdef DEBUG_FATFS
#define DPRINTF(a)	dprintf a
#define ASSERT(e)	assert(e)
#else
#define DPRINTF(a)	do {} while (0)
#define ASSERT(e)
#endif


#if CONFIG_FS_THREADS > 1
#define malloc(s)		malloc_r(s)
#define free(p)			free_r(p)
#else
#define mutex_init(m)		do {} while (0)
#define mutex_destroy(m)	do {} while (0)
#define mutex_lock(m)		do {} while (0)
#define mutex_unlock(m)		do {} while (0)
#define mutex_trylock(m)	do {} while (0)
#endif


#define SEC_SIZE	512		/* sector size */
#define SEC_INVAL	0xffffffff	/* invalid sector */

/*
 * Pre-defined cluster number
 */
#define CL_ROOT		0		/* cluster 0 means the root directory */
#define CL_FREE		0		/* cluster 0 also means the free cluster */
#define CL_FIRST	2		/* first legal cluster */
#define CL_LAST		0xfffffff5	/* last legal cluster */
#define CL_EOF		0xffffffff	/* EOF cluster */

#define EOF_MASK	0xfffffff8	/* mask of eof */

#define FAT12_MASK	0x00000fff
#define FAT16_MASK	0x0000ffff
#define FAT32_MASK	0x0fffffff


/*
 * BIOS parameter block
 */
struct fat_bpb {
	uint16_t	jmp_instruction;	/* 0 ~ 1 	: 0x00 ~ 0x01	*/
	uint8_t		nop_instruction;	/* 2 		: 0x02		*/
	uint8_t		oem_id[8];		/* 3 ~ 10 	: 0x03 ~ 0x0A	*/
	uint16_t	bytes_per_sector;	/* 11 ~ 12 	: 0x0B ~ 0x0C	*/
	uint8_t		sectors_per_cluster;	/* 13 		: 0x0D		*/
	uint16_t	reserved_sectors;	/* 14 ~ 15 	: 0x0E ~ 0x0F	*/
	uint8_t		num_of_fats;		/* 16 		: 0x10		*/
	uint16_t	root_entries;		/* 17 ~ 18 	: 0x11 ~ 0x12	*/
	uint16_t	total_sectors;		/* 19 ~ 20 	: 0x13 ~ 0x14	*/
	uint8_t		media_descriptor;	/* 21 		: 0x15		*/
	uint16_t	sectors_per_fat;	/* 22 ~ 23 	: 0x16 ~ 0x17	*/
	uint16_t	sectors_per_track;	/* 24 ~ 25 	: 0x18 ~ 0x19	*/
	uint16_t	heads;			/* 26 ~ 27 	: 0x1A ~ 0x1B	*/
	uint32_t	hidden_sectors;		/* 28 ~ 31 	: 0x1C ~ 0x1F	*/
	uint32_t	big_total_sectors;	/* 32 ~ 35 	: 0x20 ~ 0x23	*/
	uint8_t		physical_drive;		/* 36 		: 0x24		*/
	uint8_t		reserved;		/* 37 		: 0x25		*/
	uint8_t		ext_boot_signature;	/* 38 		: 0x26		*/
	uint32_t	serial_no;		/* 39 ~ 42 	: 0x27 ~ 0x2A	*/
	uint8_t		volume_id[11];		/* 43 ~ 53 	: 0x2B ~ 0x35	*/
	uint8_t		file_sys_id[8];		/* 54 ~ 61 	: 0x36 ~ 0x3D	*/
} __packed;

struct fat32_bpb {
	uint16_t	jmp_instruction;	/* 0 ~ 1 	: 0x00 ~ 0x01	*/
	uint8_t		nop_instruction;	/* 2 		: 0x02		*/
	uint8_t		oem_id[8];		/* 3 ~ 10 	: 0x03 ~ 0x0A	*/
	uint16_t	bytes_per_sector;	/* 11 ~ 12 	: 0x0B ~ 0x0C	*/
	uint8_t		sectors_per_cluster;	/* 13 		: 0x0D		*/
	uint16_t	reserved_sectors;	/* 14 ~ 15 	: 0x0E ~ 0x0F	*/
	uint8_t		num_of_fats;		/* 16 		: 0x10		*/
	uint16_t	root_entries;		/* 17 ~ 18 	: 0x11 ~ 0x12	*/
	uint16_t	total_sectors;		/* 19 ~ 20 	: 0x13 ~ 0x14	*/
	uint8_t		media_descriptor;	/* 21 		: 0x15		*/
	uint16_t	sectors_per_fat;	/* 22 ~ 23 	: 0x16 ~ 0x17	*/
	uint16_t	sectors_per_track;	/* 24 ~ 25 	: 0x18 ~ 0x19	*/
	uint16_t	heads;			/* 26 ~ 27 	: 0x1A ~ 0x1B	*/
	uint32_t	hidden_sectors;		/* 28 ~ 31 	: 0x1C ~ 0x1F	*/
	uint32_t	big_total_sectors;	/* 32 ~ 35 	: 0x20 ~ 0x23	*/
	uint32_t	sectors_per_fat32;	/* 36 ~ 39 	: 0x24 ~ 0x27	*/
	uint16_t	multi_fat32;		/* 40 ~ 41 	: 0x28 ~ 0x29	*/
	uint16_t	version;		/* 42 ~ 43 	: 0x2A ~ 0x2B	*/
	uint32_t	root_clust;		/* 44 ~ 47 	: 0x2C ~ 0x2F	*/
	uint16_t	fsinfo;			/* 48 ~ 49 	: 0x30 ~ 0x31	*/
	uint16_t	backup;			/* 50 ~ 51 	: 0x32 ~ 0x33	*/
	uint8_t		reserved[12];		/* 52 ~ 63 	: 0x34 ~ 0x3F	*/
	uint8_t		physical_drive;		/* 64 		: 0x40		*/
	uint8_t		unused;			/* 65 		: 0x41		*/
	uint8_t		ext_boot_signature;	/* 66 		: 0x42		*/
	uint32_t	serial_no;		/* 67 ~ 70 	: 0x43 ~ 0x44	*/
	uint8_t		volume_id[11];		/* 71 ~ 81 	: 0x45 ~ 0x51	*/
	uint8_t		file_sys_id[8];		/* 82 ~ 89 	: 0x52 ~ 0x59	*/
} __packed;

/*
 * FAT directory entry
 */
struct fat_dirent {
	uint8_t		name[11];		/* 0 - 10 	: 0x00 ~ 0x0A	*/
	uint8_t		attr;			/* 11 		: 0x0B		*/
	uint8_t		reserve;		/* 12 		: 0x0C		*/
	uint8_t		ctime_sec;		/* 13 		: 0x0D		*/
	uint16_t	ctime_hms;		/* 14 ~ 15 	: 0x0E ~ 0x0F	*/
	uint16_t	cday;			/* 16 ~ 17 	: 0x10 ~ 0x11	*/
	uint16_t	aday;			/* 18 ~ 19 	: 0x12 ~ 0x13	*/
	uint16_t	cluster_hi;		/* 20 ~ 21 	: 0x14 ~ 0x15	*/
	uint16_t	time;			/* 22 ~ 23 	: 0x16 ~ 0x17	*/
	uint16_t	date;			/* 24 ~ 25 	: 0x18 ~ 0x19	*/
	uint16_t	cluster;		/* 26 ~ 27 	: 0x1A ~ 0x1B	*/
	uint32_t	size;			/* 28 ~ 31 	: 0x1C ~ 0x1F	*/
} __packed;

#define SLOT_EMPTY	0x00
#define SLOT_DELETED	0xe5

#define DIR_PER_SEC     (SEC_SIZE / sizeof(struct fat_dirent))
#define FAT_VALID_MEDIA(x) ((0xF8 <= (x) && (x) <= 0xFF) || (x) == 0xF0)

/*
 * FAT attribute for attr
 */
#define FA_RDONLY	0x01
#define FA_HIDDEN	0x02
#define FA_SYSTEM	0x04
#define FA_VOLID	0x08
#define FA_SUBDIR	0x10
#define FA_ARCH	0x20
#define FA_DEVICE	0x40

#define IS_DIR(de)	(((de)->attr) & FA_SUBDIR)
#define IS_VOL(de)	(((de)->attr) & FA_VOLID)
#define IS_FILE(de)	(!IS_DIR(de) && !IS_VOL(de))

#define IS_DELETED(de)  ((de)->name[0] == 0xe5)
#define IS_EMPTY(de)    ((de)->name[0] == 0)

/*
 * Mount data
 */
struct fatfsmount {
	int	fat_type;	/* 12 or 16 */
	u_long	root_start;	/* start sector for root directory */
	u_long	fat_start;	/* start sector for fat entries */
	u_long	data_start;	/* start sector for data */
	u_long	fat_eof;	/* id of end cluster */
	u_long	sec_per_cl;	/* sectors per cluster */
	u_long	cluster_size;	/* cluster size */
	u_long	last_cluster;	/* last cluser */
	u_long	fat_mask;	/* mask for cluster# */
	u_long	free_scan;	/* start cluster# to free search */
	vnode_t	root_vnode;	/* vnode for root */
	char	*io_buf;	/* local data buffer */
	char	*fat_buf;	/* buffer for fat entry */
	char	*dir_buf;	/* buffer for directory entry */
	dev_t	dev;		/* mounted device */
#if CONFIG_FS_THREADS > 1
	mutex_t lock;		/* file system lock */
#endif
};

#define FAT12(fat)	((fat)->fat_type == 12)
#define FAT16(fat)	((fat)->fat_type == 16)
#define FAT32(fat)	((fat)->fat_type == 32)

#define MBR_TABLE	446

#define IS_EOFCL(fat, cl) \
	(((cl) & EOF_MASK) == ((fat)->fat_mask & EOF_MASK))

/*
 * File/directory node
 */
struct fatfs_node {
	struct fat_dirent dirent; /* copy of directory entry */
	u_long	sector;		/* sector# for directory entry */
	u_long	offset;		/* offset of directory entry in sector */
};

extern struct vnops fatfs_vnops;

/* Macro to convert cluster# to logical sector# */
#define cl_to_sec(fat, cl) \
	(fat->data_start + (cl - 2) * fat->sec_per_cl)

__BEGIN_DECLS
int	 fat_next_cluster(struct fatfsmount *fmp, u_long cl, u_long *next);
int	 fat_set_cluster(struct fatfsmount *fmp, u_long cl, u_long next);
int	 fat_alloc_cluster(struct fatfsmount *fmp, u_long scan_start, u_long *free);
int	 fat_free_clusters(struct fatfsmount *fmp, u_long start);
int	 fat_seek_cluster(struct fatfsmount *fmp, u_long start, u_long offset,
			    u_long *cl);
int	 fat_expand_file(struct fatfsmount *fmp, u_long cl, int size);
int	 fat_expand_dir(struct fatfsmount *fmp, u_long cl, u_long *new_cl);

void	 fat_convert_name(char *org, char *name);
void	 fat_restore_name(char *org, char *name);
int	 fat_valid_name(char *name);
int	 fat_compare_name(char *n1, char *n2);
void	 fat_mode_to_attr(mode_t mode, u_char *attr);
void	 fat_attr_to_mode(u_char attr, mode_t *mode);

int	 fatfs_lookup_node(vnode_t dvp, char *name, struct fatfs_node *node);
int	 fatfs_get_node(vnode_t dvp, int index, struct fatfs_node *node);
int	 fatfs_put_node(struct fatfsmount *fmp, struct fatfs_node *node);
int	 fatfs_add_node(vnode_t dvp, struct fatfs_node *node);
__END_DECLS

#endif /* !_FATFS_H */
