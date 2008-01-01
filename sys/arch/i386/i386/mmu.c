/*-
 * Copyright (c) 2005, Kohsuke Ohtani
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

#include <kernel.h>
#include <page.h>
#include <cpu.h>

/*
 * Map physical memory range into virtual address
 *
 * Returns 0 on success, or -1 on failure.
 *
 * Map type can be one of the following type.
 *   PG_UNMAP - Remove mapping
 *   PG_READ  - Read only mapping
 *   PG_WRITE - Read/write allowed
 *
 * Setup the appropriate page tables for mapping. If there is no
 * page table for the specified address, new page table is allocated.
 *
 * This routine does not return any error even if the specified
 * address has been already mapped to other physical address.
 * In this case, it will just override the existing mapping.
 *
 * In order to unmap the page, pg_type is specified as 0.
 * But, the page tables are not released even if there is no valid
 * page entry in it. All page tables are released when mmu_delmap()
 * is called when task is terminated.
 *
 * TODO: TLB should be flushed for specific page by invalpg in case of i486.
 */
int
mmu_map(pgd_t pgd, void *phys, void *virt, size_t size, int type)
{
	long pg_type;
	page_table_t pte;
	void *pg;	/* page */
	u_long va, pa;

	pa = PAGE_ALIGN(phys);
	va = PAGE_ALIGN(virt);
	size = PAGE_TRUNC(size);

	/* Build page type */
	pg_type = 0;
	switch (type) {
	case PG_UNMAP:
		break;
	case PG_READ:
		pg_type = PTE_USER | PTE_PRESENT;
		break;
	case PG_WRITE:
		pg_type = PTE_USER | PTE_WRITE | PTE_PRESENT;
		break;
	}
	/* Map all pages */
	while (size > 0) {
		if (pte_present(pgd, va)) {
			/* Page table already exists for the address */
			pte = pgd_to_pte(pgd, va);
		} else {
			ASSERT(pg_type != 0);
			if ((pg = page_alloc(PAGE_SIZE)) == NULL) {
				printk("Error: MMU mapping failed\n");
				return -1;
			}
			pgd[PAGE_DIR(va)] =
			    (u_long)pg | PDE_PRESENT | PDE_WRITE | PDE_USER;
			pte = phys_to_virt(pg);
			memset(pte, 0, PAGE_SIZE);
		}
		/* Set new entry into page table */
		pte[PAGE_TABLE(va)] = pa | pg_type;

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
 * Returns a page directory on success, or NULL on failure.
 * This routine is called when new task is created. All page
 * map must have the same kernel page table in it. So, the kernel
 * page tables are copied to newly created map.
 */
pgd_t
mmu_newmap(void)
{
	void *pg;
	pgd_t pgd, kern_pgd;
	u_long i;

	/* Allocate page directory */
	if ((pg = page_alloc(PAGE_SIZE)) == NULL)
		return NULL;
	pgd = phys_to_virt(pg);
	memset(pgd, 0, PAGE_SIZE);

	/* Copy kernel page tables */
	kern_pgd = phys_to_virt(KERNEL_PGD);
	i = PAGE_DIR(PAGE_OFFSET);
	memcpy(&pgd[i], &kern_pgd[i], 1024 - i);
	return pgd;
}

/*
 * Delete all page map.
 */
void
mmu_delmap(pgd_t pgd)
{
	u_long i;
	page_table_t pte;

	flush_tlb();

	/* Release all user page table */
	for (i = 0; i < PAGE_DIR(PAGE_OFFSET); i++) {
		pte = (page_table_t)pgd[i];
		if (pte != 0)
			page_free((void *)((u_long)pte & PTE_ADDRESS),
				  PAGE_SIZE);
	}
	/* Release page directory */
	page_free(virt_to_phys(pgd), PAGE_SIZE);
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
	u_long phys = (u_long)virt_to_phys(pgd);

	if (phys != get_cr3())
		set_cr3(phys);
}

/*
 * Returns the physical address for the specified virtual address.
 * This routine checks if the virtual area actually exist.
 * It returns NULL if at least one page is not mapped.
 */
void *
mmu_extract(pgd_t pgd, void *virt, size_t size)
{
	page_table_t pte;
	u_long start, end, pg;

	start = PAGE_TRUNC(virt);
	end = PAGE_TRUNC((u_long)virt + size - 1);

	/* Check all pages exist */
	for (pg = start; pg <= end; pg += PAGE_SIZE) {
		if (!pte_present(pgd, pg))
			return NULL;
		pte = pgd_to_pte(pgd, pg);
		if (!page_present(pte, pg))
			return NULL;
	}

	/* Get physical address */
	pte = pgd_to_pte(pgd, start);
	pg = pte_to_page(pte, start);
	return (void *)(pg + ((u_long)virt - start));
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
mmu_init(void)
{
	pgd_t kern_pgd;
	int npages, nptes;
	u_long pte_entry, pgd_index, *pte;
	int i, j;
	void *pg;

	kern_pgd = phys_to_virt(KERNEL_PGD);
	npages = boot_info->main_mem.size / PAGE_SIZE;
	nptes = (npages + 1023) / 1024;
	pgd_index = PAGE_DIR(PAGE_OFFSET);
	pte_entry = 0 | PTE_PRESENT | PTE_WRITE;

	/*
	 * Build kernel page tables for whole physical pages.
	 */
	for (i = 0; i < nptes; i++) {
		/* Allocate new page table */
		if ((pg = page_alloc(PAGE_SIZE)) == NULL)
			panic("mmu_init: out of memory");
		pte = phys_to_virt(pg);
		memset(pte, 0, PAGE_SIZE);

		/* Fill all entries in this page table */
		for (j = 0; j < 1024; j++) {
			pte[j] = pte_entry;
			pte_entry += PAGE_SIZE;
			if (--npages <= 0)
				break;
		}
		/* Set the page table address into page directory. */
		kern_pgd[pgd_index] = (u_long)pg | PDE_PRESENT | PDE_WRITE;
		pgd_index++;
	}
	/* Unmap address 0 for NULL pointer detection in kernel mode */
	pte = phys_to_virt(kern_pgd[PAGE_DIR(PAGE_OFFSET)] & PDE_ADDRESS);
	pte[0] = 0;

	/* Flush translation look-aside buffer */
	flush_tlb();
}
