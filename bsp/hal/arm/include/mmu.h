/*-
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

#ifndef _ARM_MMU_H
#define _ARM_MMU_H

#include <sys/types.h>

typedef uint32_t	*pgd_t;		/* page directory */
typedef uint32_t	*pte_t;		/* page table entry */

#define L1TBL_SIZE	0x4000
#define L2TBL_SIZE	0x1000

/*
 * Page directory entry (L1)
 */
#define PDE_PRESENT	0x00000003
#define PDE_ADDRESS	0xfffff000

/*
 * Page table entry (L2)
 */
#define PTE_PRESENT	0x00000002
#define PTE_WBUF	0x00000004
#define PTE_CACHE	0x00000008
#define PTE_SYSTEM	0x00000010
#define PTE_USER_RO	0x00000020
#define PTE_USER_RW	0x00000030
#define PTE_ATTR_MASK	0x00000030
#define PTE_ADDRESS	0xfffffc00

/*
 *  Virtual and physical address translation
 */
#define PAGE_DIR(virt)      (int)((((vaddr_t)(virt)) >> 20) & 0xfff)
#define PAGE_TABLE(virt)    (int)((((vaddr_t)(virt)) >> 12) & 0xff)

#define pte_present(pgd, virt)  (pgd[PAGE_DIR(virt)] & PDE_PRESENT)

#define page_present(pte, virt) (pte[PAGE_TABLE(virt)] & PTE_PRESENT)

#define vtopte(pgd, virt) \
            (pte_t)ptokv((pgd)[PAGE_DIR(virt)] & PDE_ADDRESS)

#define ptetopg(pte, virt) \
            ((pte)[PAGE_TABLE(virt)] & PTE_ADDRESS)


#endif /* !_ARM_MMU_H */
