// SPDX-License-Identifier: GPL-2.0
/*
 * ION Memory Allocator - buffer interface
 *
 * Copyright (c) 2019, Google, Inc.
 */

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/dma-noncoherent.h>
#ifndef CONFIG_ARM64
#include <linux/highmem.h>
#include <asm/cacheflush.h>
#include <asm/outercache.h>
#endif

#define CREATE_TRACE_POINTS
#include "ion_trace.h"
#include "ion_private.h"

static atomic_long_t total_heap_bytes;

static void track_buffer_created(struct ion_buffer *buffer)
{
	long total = atomic_long_add_return(buffer->size, &total_heap_bytes);

	trace_ion_stat(buffer->sg_table, buffer->size, total);
}

static void track_buffer_destroyed(struct ion_buffer *buffer)
{
	long total = atomic_long_sub_return(buffer->size, &total_heap_bytes);

	trace_ion_stat(buffer->sg_table, -buffer->size, total);
}

/* this function should only be called while dev->lock is held */
static struct ion_buffer *ion_buffer_create(struct ion_heap *heap,
					    struct ion_device *dev,
					    unsigned long len,
					    unsigned long flags)
{
	struct ion_buffer *buffer;
	int ret;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	buffer->heap = heap;
	buffer->flags = flags;
	buffer->size = len;

	ret = heap->ops->allocate(heap, buffer, len, flags);

	if (ret) {
		if (!(heap->flags & ION_HEAP_FLAG_DEFER_FREE))
			goto err2;

		ion_heap_freelist_drain(heap, 0);
		ret = heap->ops->allocate(heap, buffer, len, flags);
		if (ret)
			goto err2;
	}

	if (!buffer->sg_table) {
		WARN_ONCE(1, "This heap needs to set the sgtable");
		ret = -EINVAL;
		goto err1;
	}

	spin_lock(&heap->stat_lock);
	heap->num_of_buffers++;
	heap->num_of_alloc_bytes += len;
	if (heap->num_of_alloc_bytes > heap->alloc_bytes_wm)
		heap->alloc_bytes_wm = heap->num_of_alloc_bytes;
	if (heap->num_of_buffers == 1) {
		/* This module reference lasts as long as at least one
		 * buffer is allocated from the heap. We are protected
		 * against ion_device_remove_heap() with dev->lock, so we can
		 * safely assume the module reference is going to* succeed.
		 */
		__module_get(heap->owner);
	}
	spin_unlock(&heap->stat_lock);

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);
	track_buffer_created(buffer);
	return buffer;

err1:
	heap->ops->free(buffer);
err2:
	kfree(buffer);
	return ERR_PTR(ret);
}

static int ion_clear_pages(struct page **pages, int num, pgprot_t pgprot)
{
	void *addr = vmap(pages, num, VM_MAP, pgprot);

	if (!addr)
		return -ENOMEM;
	memset(addr, 0, PAGE_SIZE * num);
	vunmap(addr);

	return 0;
}

static int ion_sglist_zero(struct scatterlist *sgl, unsigned int nents,
			   pgprot_t pgprot)
{
	int p = 0;
	int ret = 0;
	struct sg_page_iter piter;
	struct page *pages[32];

	for_each_sg_page(sgl, &piter, nents, 0) {
		pages[p++] = sg_page_iter_page(&piter);
		if (p == ARRAY_SIZE(pages)) {
			ret = ion_clear_pages(pages, p, pgprot);
			if (ret)
				return ret;
			p = 0;
		}
	}
	if (p)
		ret = ion_clear_pages(pages, p, pgprot);

	return ret;
}

struct ion_buffer *ion_buffer_alloc(struct ion_device *dev, size_t len,
				    unsigned int heap_id_mask,
				    unsigned int flags)
{
	struct ion_buffer *buffer = NULL;
	struct ion_heap *heap;
#if IS_ENABLED(CONFIG_MTK_ION_MM_HEAP)
	unsigned int heap_default_mask = ~0;
#endif

	if (!dev || !len) {
		pr_info("[ION] %s failed!! len:0x%zx\n", __func__, len);
		return ERR_PTR(-EINVAL);
	}

	/*
	 * traverse the list of heaps available in this system in priority
	 * order.  If the heap type is supported by the client, and matches the
	 * request of the caller allocate from it.  Repeat until allocate has
	 * succeeded or all heaps have been tried
	 */
	len = PAGE_ALIGN(len);
	if (!len) {
		pr_info("[ION] size is error! len:0x%zx\n", len);
		return ERR_PTR(-EINVAL);
	}

	down_read(&dev->lock);
#if IS_ENABLED(CONFIG_MTK_ION_MM_HEAP)
	if (heap_id_mask == heap_default_mask) {
		plist_for_each_entry(heap, &dev->heaps, node) {
			/* Some case (as C2 audio decoder) cannot set heap_id
			 * with AOSP framework. Therefore, specify
			 * mtk_ion_mm_heap as the default heap when it is
			 * supported by Mediatek.
			 */
			if (strcmp(heap->name, "mtk_ion_mm_heap"))
				continue;
			buffer = ion_buffer_create(heap, dev, len, flags);
			if (!IS_ERR(buffer))
				break;
		}
	} else
#endif
	{
		plist_for_each_entry(heap, &dev->heaps, node) {
			/* if the caller didn't specify this heap id */
			if (!((1 << heap->id) & heap_id_mask))
				continue;
			buffer = ion_buffer_create(heap, dev, len, flags);
			if (!IS_ERR(buffer))
				break;
		}
	}

	up_read(&dev->lock);

	if (!buffer) {
		pr_info("[ION] buffer is NULL! mask:0x%x\n", heap_id_mask);
		return ERR_PTR(-ENODEV);
	}

	if (IS_ERR(buffer)) {
		pr_info("[ION] buffer is error! mask:0x%x\n", heap_id_mask);
		return ERR_CAST(buffer);
	}

#if IS_ENABLED(CONFIG_MTK_ION_DEBUG)
	/* add buffer to alloc_list */
	mutex_lock(&heap->alloc_lock);
	list_add(&buffer->list, &heap->alloc_list);
	mutex_unlock(&heap->alloc_lock);
#endif

	return buffer;
}

int ion_buffer_zero(struct ion_buffer *buffer)
{
	struct sg_table *table;
	pgprot_t pgprot;

	if (!buffer)
		return -EINVAL;

	table = buffer->sg_table;
	if (buffer->flags & ION_FLAG_CACHED)
		pgprot = PAGE_KERNEL;
	else
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	return ion_sglist_zero(table->sgl, table->nents, pgprot);
}
EXPORT_SYMBOL_GPL(ion_buffer_zero);

void ion_buffer_prep_noncached(struct ion_buffer *buffer)
{
	struct sg_table *table;
	struct scatterlist *sg;
#ifdef CONFIG_ARM64
	int i;
#else
	struct sg_page_iter iter;
	struct page *pg;
	void *vaddr;
	phys_addr_t paddr;
#endif

	if (WARN_ONCE(!buffer || !buffer->sg_table,
		      "%s needs a buffer and a sg_table", __func__) ||
	    buffer->flags & ION_FLAG_CACHED)
		return;

	table = buffer->sg_table;

#ifdef CONFIG_ARM64
	for_each_sg(table->sgl, sg, table->orig_nents, i)
		arch_dma_prep_coherent(sg_page(sg), sg->length);
#else
	/* flush pages for ARM32:
	 * refer to arch/arm/mm/dma-mapping.c: __dma_clear_buffer()
	 */
	for_each_sg_page(table->sgl, &iter, table->orig_nents, 0) {
		pg = sg_page_iter_page(&iter);
		vaddr = kmap_atomic(pg);
		paddr = page_to_phys(pg);

		dmac_flush_range(vaddr, vaddr + PAGE_SIZE);
		outer_flush_range(paddr, paddr + PAGE_SIZE);
		kunmap_atomic(vaddr);
	}
#endif
}
EXPORT_SYMBOL_GPL(ion_buffer_prep_noncached);

void ion_buffer_release(struct ion_buffer *buffer)
{
	if (buffer->kmap_cnt > 0) {
		pr_warn_once("%s: buffer still mapped in the kernel\n",
			     __func__);
		ion_heap_unmap_kernel(buffer->heap, buffer);
	}
	buffer->heap->ops->free(buffer);
	spin_lock(&buffer->heap->stat_lock);
	buffer->heap->num_of_buffers--;
	buffer->heap->num_of_alloc_bytes -= buffer->size;
	if (buffer->heap->num_of_buffers == 0)
		module_put(buffer->heap->owner);
	spin_unlock(&buffer->heap->stat_lock);
	/* drop reference to the heap module */

	kfree(buffer);
}

int ion_buffer_destroy(struct ion_device *dev, struct ion_buffer *buffer)
{
	struct ion_heap *heap;

	if (!dev || !buffer) {
		pr_warn("%s: invalid argument\n", __func__);
		return -EINVAL;
	}

	heap = buffer->heap;
	track_buffer_destroyed(buffer);

#if IS_ENABLED(CONFIG_MTK_ION_DEBUG)
	/* remove buffer from alloc_list */
	mutex_lock(&heap->alloc_lock);
	list_del(&buffer->list);
	mutex_unlock(&heap->alloc_lock);
#endif

	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		ion_heap_freelist_add(heap, buffer);
	else
		ion_buffer_release(buffer);

	return 0;
}

void *ion_buffer_kmap_get(struct ion_buffer *buffer)
{
	void *vaddr;

	if (buffer->kmap_cnt) {
		buffer->kmap_cnt++;
		return buffer->vaddr;
	}
	vaddr = ion_heap_map_kernel(buffer->heap, buffer);
	if (WARN_ONCE(!vaddr,
		      "heap->ops->map_kernel should return ERR_PTR on error"))
		return ERR_PTR(-EINVAL);
	if (IS_ERR(vaddr))
		return vaddr;
	buffer->vaddr = vaddr;
	buffer->kmap_cnt++;
	return vaddr;
}

void ion_buffer_kmap_put(struct ion_buffer *buffer)
{
	buffer->kmap_cnt--;
	if (!buffer->kmap_cnt) {
		ion_heap_unmap_kernel(buffer->heap, buffer);
		buffer->vaddr = NULL;
	}
}

u64 ion_get_total_heap_bytes(void)
{
	return atomic_long_read(&total_heap_bytes);
}
