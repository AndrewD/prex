/*-
 * Copyright (c) 2008, Kohsuke Ohtani
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

#ifndef _X86_MMU_H
#define _X86_MMU_H

#include <sys/types.h>

typedef uint32_t	*pgd_t;		/* page directory */
typedef uint32_t	*pte_t;		/* page table entry */

/*
 * Page directory entry
 */
#define PDE_PRESENT	0x00000001
#define PDE_WRITE	0x00000002
#define PDE_USER	0x00000004
#define PDE_WTHRU	0x00000008
#define PDE_NCACHE	0x00000010
#define PDE_ACCESS	0x00000020
#define PDE_SIZE	0x00000080
#define PDE_AVAIL	0x00000e00
#define PDE_ADDRESS	0xfffff000

/*
 * Page table entry
 */
#define PTE_PRESENT	0x00000001
#define PTE_WRITE	0x00000002
#define PTE_USER	0x00000004
#define PTE_WTHRU	0x00000008
#define PTE_NCACHE	0x00000010
#define PTE_ACCESS	0x00000020
#define PTE_DIRTY	0x00000040
#define PTE_AVAIL	0x00000e00
#define PTE_ADDRESS	0xfffff000

/*
 *  Virtual and physical address translation
 */
#define PAGE_DIR(virt)      (int)((((vaddr_t)(virt)) >> 22) & 0x3ff)
#define PAGE_TABLE(virt)    (int)((((vaddr_t)(virt)) >> 12) & 0x3ff)

#define pte_present(pgd, virt)  (pgd[PAGE_DIR(virt)] & PDE_PRESENT)

#define page_present(pte, virt) (pte[PAGE_TABLE(virt)] & PTE_PRESENT)

#define vtopte(pgd, virt) \
            (pte_t)ptokv(((uint32_t *)pgd)[PAGE_DIR(virt)] & PDE_ADDRESS)

#define ptetopg(pte, virt) \
            ((pte)[PAGE_TABLE(virt)] & PTE_ADDRESS)

#endif /* !_X86_MMU_H */
