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
 * mmu.c - memory management unit support routines
 */

/*
 * This module provides virtual/physical address translation for
 * intel x86 MMU. This kernel will do only page level translation
 * and protection and it does not use x86 segment mechanism.
 */

#include <machine/syspage.h>
#include <kernel.h>
#include <page.h>
#include <mmu.h>
#include <cpu.h>
#include <cpufunc.h>

/*
 * Boot page directory.
 * This works as a template for all page directory.
 */
static pgd_t boot_pgd = (pgd_t)BOOT_PGD;

/*
 * Map physical memory range into virtual address
 *
 * Returns 0 on success, or ENOMEM on failure.
 *
 * Map type can be one of the following type.
 *   PG_UNMAP  - Remove mapping
 *   PG_READ   - Read only mapping
 *   PG_WRITE  - Read/write allowed
 *   PG_KERNEL - Kernel page
 *   PG_IO     - I/O memory
 *
 * Setup the appropriate page tables for mapping. If there is no
 * page table for the specified address, new page table is
 * allocated.
 *
 * This routine does not return any error even if the specified
 * address has been already mapped to other physical address.
 * In this case, it will just override the existing mapping.
 *
 * In order to unmap the page, pg_type is specified as 0.  But,
 * the page tables are not released even if there is no valid
 * page entry in it. All page tables are released when mmu_delmap()
 * is called when task is terminated.
 *
 * TODO: TLB should be flushed for specific page by invalpg in
 * case of i486.
 */
int
mmu_map(pgd_t pgd, paddr_t pa, vaddr_t va, size_t size, int type)
{
	uint32_t pte_flag = 0;
	uint32_t pde_flag = 0;
	pte_t pte;
	paddr_t pg;

	pa = round_page(pa);
	va = round_page(va);
	size = trunc_page(size);

	/*
	 * Set page flag
	 */
	switch (type) {
	case PG_UNMAP:
		pte_flag = 0;
		pde_flag = (uint32_t)(PDE_PRESENT | PDE_WRITE | PDE_USER);
		break;
	case PG_READ:
		pte_flag = (uint32_t)(PTE_PRESENT | PTE_USER);
		pde_flag = (uint32_t)(PDE_PRESENT | PDE_WRITE | PDE_USER);
		break;
	case PG_WRITE:
		pte_flag = (uint32_t)(PTE_PRESENT | PTE_WRITE | PTE_USER);
		pde_flag = (uint32_t)(PDE_PRESENT | PDE_WRITE | PDE_USER);
		break;
	case PG_SYSTEM:
		pde_flag = (uint32_t)(PDE_PRESENT | PDE_WRITE);
		pte_flag = (uint32_t)(PTE_PRESENT | PTE_WRITE);
		break;
	case PG_IOMEM:
		pde_flag = (uint32_t)(PDE_PRESENT | PDE_WRITE);
		pte_flag = (uint32_t)(PTE_PRESENT | PTE_WRITE | PTE_NCACHE);
		break;
	default:
		panic("mmu_map");
	}
	/*
	 * Map all pages
	 */
	while (size > 0) {
		if (pte_present(pgd, va)) {
			/* Page table already exists for the address */
			pte = vtopte(pgd, va);
		} else {
			ASSERT(pte_flag != 0);
			if ((pg = page_alloc(PAGE_SIZE)) == 0) {
				DPRINTF(("Error: MMU mapping failed\n"));
				return ENOMEM;
			}
			pgd[PAGE_DIR(va)] = (uint32_t)pg | pde_flag;
			pte = (pte_t)ptokv(pg);
			memset(pte, 0, PAGE_SIZE);
		}
		/* Set new entry into page table */
		pte[PAGE_TABLE(va)] = (uint32_t)pa | pte_flag;

		/* Process next page */
		pa += PAGE_SIZE;
		va += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	flush_tlb();
	return 0;
}

/*
 * Create new page map.
 *
 * Returns a page directory on success, or NULL on failure.  This
 * routine is called when new task is created. All page map must
 * have the same kernel page table in it. So, the kernel page
 * tables are copied to newly created map.
 */
pgd_t
mmu_newmap(void)
{
	paddr_t pg;
	pgd_t pgd;
	int i;

	/* Allocate page directory */
	if ((pg = page_alloc(PAGE_SIZE)) == 0)
		return NO_PGD;
	pgd = (pgd_t)ptokv(pg);
	memset(pgd, 0, PAGE_SIZE);

	/* Copy kernel page tables */
	i = PAGE_DIR(KERNBASE);
	memcpy(&pgd[i], &boot_pgd[i], (size_t)(1024 - i));
	return pgd;
}

/*
 * Terminate all page mapping.
 */
void
mmu_terminate(pgd_t pgd)
{
	int i;
	pte_t pte;

	flush_tlb();

	/* Release all user page table */
	for (i = 0; i < PAGE_DIR(KERNBASE); i++) {
		pte = (pte_t)pgd[i];
		if (pte != 0)
			page_free((paddr_t)((paddr_t)pte & PTE_ADDRESS),
				  PAGE_SIZE);
	}
	/* Release page directory */
	page_free(kvtop(pgd), PAGE_SIZE);
}

/*
 * Switch to new page directory
 *
 * This is called when context is switched.
 * Whole TLB are flushed automatically by loading
 * CR3 register.
 */
void
mmu_switch(pgd_t pgd)
{
	uint32_t phys = (uint32_t)kvtop(pgd);

	if (phys != get_cr3())
		set_cr3(phys);
}

/*
 * Returns the physical address for the specified virtual address.
 * This routine checks if the virtual area actually exist.
 * It returns 0 if at least one page is not mapped.
 */
paddr_t
mmu_extract(pgd_t pgd, vaddr_t va, size_t size)
{
	pte_t pte;
	vaddr_t start, end, pg;
	paddr_t pa;

	start = trunc_page(va);
	end = trunc_page(va + size - 1);

	/* Check all pages exist */
	for (pg = start; pg <= end; pg += PAGE_SIZE) {
		if (!pte_present(pgd, pg))
			return 0;
		pte = vtopte(pgd, pg);
		if (!page_present(pte, pg))
			return 0;
	}

	/* Get physical address */
	pte = vtopte(pgd, start);
	pa = (paddr_t)ptetopg(pte, start);
	return pa + (paddr_t)(va - start);
}

/*
 * Initialize mmu
 *
 * Paging is already enabled in locore.S. And, physical address
 * 0-4M has been already mapped into kernel space in locore.S.
 * Now, all physical memory is mapped into kernel virtual address
 * as straight 1:1 mapping. User mode access is not allowed for
 * these kernel pages.
 * page_init() must be called before calling this routine.
 *
 * Note: This routine requires 4K bytes to map 4M bytes memory. So,
 * if the system has a lot of RAM, the "used memory" by kernel will
 * become large, too. For example, page table requires 512K bytes
 * for 512M bytes system RAM.
 */
void
mmu_init(struct mmumap *mmumap_table)
{
	struct mmumap *map;
	int map_type = 0;

	for (map = mmumap_table; map->type != 0; map++) {
		switch (map->type) {
		case VMT_RAM:
		case VMT_ROM:
		case VMT_DMA:
			map_type = PG_SYSTEM;
			break;
		case VMT_IO:
			map_type = PG_IOMEM;
			break;
		}

		if (mmu_map(boot_pgd, map->phys, map->virt,
			    (size_t)map->size, map_type))
			panic("mmu_init");
	}
}
