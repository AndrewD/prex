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

#ifndef _BOOT_H
#define _BOOT_H

#include <conf/config.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/cdefs.h>
#include <sys/null.h>
#include <sys/types.h>
#include <sys/elf.h>
#include <machine/stdarg.h>
#include <prex/bootinfo.h>

/* #define DEBUG_BOOT		1 */
/* #define DEBUG_BOOT_IMAGE	1 */

extern u_long load_base;
extern u_long load_start;

extern struct boot_info *boot_info;

#ifdef DEBUG_BOOT_IMAGE
#define elf_print(fmt, args...)	printk(fmt, ## args)
#else
#define elf_print(fmt...)	do {} while (0)
#endif

#ifdef DEBUG
extern void printk(const char *fmt, ...);
#else
#define printk(fmt...)	do {} while (0)
#endif

extern int elf_load(char *img, struct module *mod);
extern void reserve_memory(u_long start, size_t size);
extern void start_kernel(unsigned int entry, unsigned int boot_info);
extern int relocate_rel(Elf32_Rel *, Elf32_Addr, char *);
extern int relocate_rela(Elf32_Rela *, Elf32_Addr, char *);
extern void panic(const char *msg) __attribute__((noreturn));

extern char *strncpy(char *dest, const char *src, size_t count);
extern int strncmp(const char *src, const char *tgt, size_t count);
extern size_t strnlen(const char *str, size_t count);
extern void *memcpy(void *dest, const void *src, size_t count);
extern void *memset(void *dest, int ch, size_t count);
extern long atol(char *nptr);

struct kernel_symbol
{
	u_long value;
	const char *name;
};
#endif /* !_BOOT_H */
