// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/bug.h>
#include <linux/crc32.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <asm/memory.h>
#include <asm/sections.h>

#include <mt-plat/mrdump.h>
#include "mrdump_private.h"

struct mrdump_control_block *mrdump_cblock;

#if defined(CONFIG_KALLSYMS) && !defined(CONFIG_KALLSYMS_BASE_RELATIVE)
static void mrdump_cblock_kallsyms_init(struct mrdump_ksyms_param *kparam)
{
	unsigned long start_addr = (unsigned long)kallsyms_addresses;

	kparam->tag[0] = 'K';
	kparam->tag[1] = 'S';
	kparam->tag[2] = 'Y';
	kparam->tag[3] = 'M';

	switch (sizeof(unsigned long)) {
	case 4:
		kparam->flag = KSYM_32;
		break;
	case 8:
		kparam->flag = KSYM_64;
		break;
	default:
		BUILD_BUG();
	}
	kparam->start_addr = __pa_symbol(start_addr);
	kparam->size = (unsigned long)&kallsyms_token_index - start_addr + 512;
	kparam->crc = crc32(0, (unsigned char *)start_addr, kparam->size);
	kparam->addresses_off = (unsigned long)&kallsyms_addresses - start_addr;
	kparam->num_syms_off = (unsigned long)&kallsyms_num_syms - start_addr;
	kparam->names_off = (unsigned long)&kallsyms_names - start_addr;
	kparam->markers_off = (unsigned long)&kallsyms_markers - start_addr;
	kparam->token_table_off =
		(unsigned long)&kallsyms_token_table - start_addr;
	kparam->token_index_off =
		(unsigned long)&kallsyms_token_index - start_addr;
}
#else
static void mrdump_cblock_kallsyms_init(struct mrdump_ksyms_param *unused)
{
}

#endif

static void *remap_lowmem(phys_addr_t start, phys_addr_t size)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

	prot = pgprot_noncached(PAGE_KERNEL);

	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return NULL;

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;

		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);
	if (!vaddr) {
		pr_notice("%s: Failed to map %u pages\n", __func__, page_count);
		return NULL;
	}

	return vaddr + offset_in_page(start);
}

__init void mrdump_cblock_init(phys_addr_t cb_addr, phys_addr_t cb_size)
{
	struct mrdump_machdesc *machdesc_p;

	if (cb_addr == 0) {
		pr_notice("%s: mrdump control address cannot be 0\n",
			  __func__);
		return;
	}
	if (cb_size < sizeof(struct mrdump_control_block)) {
		pr_notice("%s: not enough space for mrdump control block\n",
			  __func__);
		return;
	}

	if (cb_addr < 0x40000000) {
		mrdump_cblock = ioremap(cb_addr, cb_size);
	} else {
		mrdump_cblock = remap_lowmem(cb_addr, cb_size);
	}

	if (!mrdump_cblock) {
		pr_notice("%s: mrdump_cb not mapped\n", __func__);
		return;
	}
	memset_io(mrdump_cblock, 0, sizeof(struct mrdump_control_block));
	memcpy_toio(mrdump_cblock->sig, MRDUMP_GO_DUMP,
			sizeof(mrdump_cblock->sig));

	machdesc_p = &mrdump_cblock->machdesc;
	machdesc_p->nr_cpus = nr_cpu_ids;
	machdesc_p->page_offset = (uint64_t)PAGE_OFFSET;
	machdesc_p->high_memory = (uintptr_t)high_memory;

#if defined(KIMAGE_VADDR)
	machdesc_p->kimage_vaddr = KIMAGE_VADDR;
#endif
#if defined(TEXT_OFFSET)
	machdesc_p->kimage_vaddr += TEXT_OFFSET;
#endif
	machdesc_p->dram_start = (uint64_t)aee_memblock_start_of_DRAM();
	machdesc_p->dram_end = (uint64_t)aee_memblock_end_of_DRAM();
	machdesc_p->kimage_stext = (uint64_t)aee_get_text();
	machdesc_p->kimage_etext = (uint64_t)aee_get_etext();
	machdesc_p->kimage_stext_real = (uint64_t)aee_get_stext();
#if defined(CONFIG_ARM64)
	machdesc_p->kimage_voffset = (unsigned long)kimage_voffset;
#endif
	machdesc_p->kimage_sdata = (uint64_t)aee_get_sdata();
	machdesc_p->kimage_edata = (uint64_t)aee_get_edata();

	machdesc_p->vmalloc_start = (uint64_t)VMALLOC_START;
	machdesc_p->vmalloc_end = (uint64_t)VMALLOC_END;

	machdesc_p->modules_start = (uint64_t)MODULES_VADDR;
	machdesc_p->modules_end = (uint64_t)MODULES_END;

	machdesc_p->phys_offset = (uint64_t)(phys_addr_t)PHYS_OFFSET;
	machdesc_p->master_page_table = mrdump_get_mpt();

#if defined(CONFIG_SPARSEMEM_VMEMMAP)
	machdesc_p->memmap = (uintptr_t)vmemmap;
#endif

	machdesc_p->pageflags = (1UL << PG_uptodate) + (1UL << PG_dirty) +
				(1UL << PG_lru) + (1UL << PG_writeback);

	machdesc_p->struct_page_size = (uint32_t)sizeof(struct page);

	mrdump_cblock_kallsyms_init(&machdesc_p->kallsyms);
	mrdump_cblock->machdesc_crc = crc32(0, machdesc_p,
			sizeof(struct mrdump_machdesc));

	pr_notice("%s: done.\n", __func__);
}
