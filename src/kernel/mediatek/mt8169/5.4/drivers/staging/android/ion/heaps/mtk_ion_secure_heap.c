// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[ION][sec heap] " fmt

#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/dma-buf.h>
#include <linux/ion.h>
#include <linux/module.h>
#include <linux/sched/clock.h>

#if IS_ENABLED(CONFIG_MTK_ION_DEBUG)
#include "mtk_dma_buf_helper.h"
#endif

#if IS_ENABLED(CONFIG_OPTEE)
#define MTK_IN_HOUSE_SEC_ION_SUPPORT
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_mem.h"
#include "tz_cross/ta_m4u.h"
#include "kree/system.h"
#include "kree/mem.h"

#elif IS_ENABLED(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM)
#define SECMEM_KERNEL_API
#include "trusted_mem_api.h"
#include "memory_ssmr.h"

static char heap_name[32][MAX_HEAP_NAME];
#endif

#ifdef MTK_ION_SEC_HEAP_DBG
#define ION_DBG(msg, args...)	pr_info("%s: " msg, __func__, ##args)
#else
#define ION_DBG(msg, args...)	pr_debug("%s: " msg, __func__, ##args)
#endif

#define SEC_BUF_ALIGN		0x1000

struct ion_sec_buf_info {
	struct mutex		lock; /* protect buf_info */
	pid_t			pid;
	pid_t			tid;
	char			task_comm[TASK_COMM_LEN];
	char			thread_comm[TASK_COMM_LEN];
};

static size_t sec_heap_total_memory;

#define ION_FLAG_MM_HEAP_INIT_ZERO BIT(16)

struct ion_sec_heap {
	struct ion_heap heap;
#if IS_ENABLED(CONFIG_MTEE_CMA_SECURE_MEMORY)
	enum kree_mem_type type;
#endif
	int ssmr_id;
};

#if defined(MTK_IN_HOUSE_SEC_ION_SUPPORT)

enum ion_sec_heap_type {
	ION_SEC_HEAP_TYPE_SEC_CM_TZ	= 0,
#if IS_ENABLED(CONFIG_MTEE_CMA_SECURE_MEMORY)
	ION_SEC_HEAP_TYPE_SEC_CM_CMA,
#endif
	ION_SEC_HEAP_TYPE_NUM,
};

struct ion_sec_heap_plat_data {
	const char *name;
#if IS_ENABLED(CONFIG_MTEE_CMA_SECURE_MEMORY)
	enum kree_mem_type type;
#endif
};

static const struct ion_sec_heap_plat_data ion_svp_heap[ION_SEC_HEAP_TYPE_NUM] = {
	[ION_SEC_HEAP_TYPE_SEC_CM_TZ] = {
		.name = "ion_svp_heap",
#if IS_ENABLED(CONFIG_MTEE_CMA_SECURE_MEMORY)
		.type = KREE_MEM_SEC_CM_TZ,
#endif
	},
#if IS_ENABLED(CONFIG_MTEE_CMA_SECURE_MEMORY)
	[ION_SEC_HEAP_TYPE_SEC_CM_CMA] = {
		.name = "ion_svp_wfd_heap",
		.type = KREE_MEM_SEC_CM_CMA,
	},
#endif
};

static KREE_SESSION_HANDLE ion_session_handle(void)
{
	static KREE_SESSION_HANDLE ion_session;
	int ret;

	if (ion_session == KREE_SESSION_HANDLE_NULL) {
		ret = KREE_CreateSession(TZ_TA_MEM_UUID, &ion_session);
		if (ret != TZ_RESULT_SUCCESS) {
			pr_err("KREE_CreateSession fail, ret=%d\n", ret);
			return KREE_SESSION_HANDLE_NULL;
		}
	}

	return ion_session;
}

#endif	/* MTK_IN_HOUSE_SEC_ION_SUPPORT */

static struct sg_table *ion_sec_heap_map_dma(struct ion_heap *heap,
					     struct ion_buffer *buffer)
{
	struct sg_table *table;
	int ret;

	pr_debug("%s enter\n", __func__);

	table = kzalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		kfree(table);
		return ERR_PTR(ret);
	}

	sg_set_page(table->sgl, 0, 0, 0);
	pr_debug("%s exit\n", __func__);

	return table;
}

static void ion_sec_heap_unmap_dma(struct ion_heap *heap,
				   struct ion_buffer *buffer)
{
	pr_debug("%s\n", __func__);
	sg_free_table(buffer->sg_table);
}

static int ion_sec_heap_allocate(struct ion_heap *heap,
				 struct ion_buffer *buffer,
				 unsigned long size,
				 unsigned long flags)
{
	struct ion_sec_heap *sec_heap = container_of(heap,
						     struct ion_sec_heap,
						     heap);
	struct ion_sec_buf_info *buf_info;
	u32 sec_handle = 0;
	struct task_struct *task = current->group_leader;
#if defined(MTK_IN_HOUSE_SEC_ION_SUPPORT)
	int ret = 0;
#elif defined(SECMEM_KERNEL_API)
	u32 refcount = 0;
#endif
	unsigned long long tm_sta, tm_end;

	pr_debug("%s enter name:%s, ssmr_id:%d, size:%zu\n",
		 __func__, heap->name, sec_heap->ssmr_id,
		 buffer->size);

	buf_info = kzalloc(sizeof(*buf_info), GFP_KERNEL);
	if (!buf_info)
		return -ENOMEM;

	tm_sta = sched_clock();

#if defined(MTK_IN_HOUSE_SEC_ION_SUPPORT)

#if IS_ENABLED(CONFIG_MTEE_CMA_SECURE_MEMORY)
	/* Force to allocate the zeroed memory to protect stale security
	 * information.
	 */
	ret = KREE_ZallocSecurechunkmemWithTagEx(ion_session_handle(),
						 &sec_handle, sec_heap->type,
						 SEC_BUF_ALIGN, size,
						 heap->name);
#else
	ret = KREE_ZallocSecurechunkmemWithTag(ion_session_handle(),
					       &sec_handle,
					       SEC_BUF_ALIGN, size,
					       heap->name);
#endif

	if (ret != TZ_RESULT_SUCCESS) {
		pr_err("%s: failed(%d) heap %s, size 0x%lx (total size %zu)\n",
		       __func__, ret, heap->name, size,
		       sec_heap_total_memory);
		kfree(buf_info);
		return -ENOMEM;
	}

#elif defined(SECMEM_KERNEL_API)

	if (flags & ION_FLAG_MM_HEAP_INIT_ZERO)
		trusted_mem_api_alloc_zero(sec_heap->ssmr_id, 0, size,
					   &refcount, &sec_handle,
					   (uint8_t *)heap->name,
					   heap->id);
	else
		trusted_mem_api_alloc(sec_heap->ssmr_id, 0, size,
				      &refcount, &sec_handle,
				      (uint8_t *)heap->name, heap->id);

	if (sec_handle <= 0) {
		pr_err("trusted_mem_api_alloc failed, total size %zu\n",
		       sec_heap_total_memory);
		kfree(buf_info);
		return -ENOMEM;
	}
#endif

	tm_end = sched_clock() - tm_sta;

	buffer->flags &= ~ION_FLAG_CACHED;
	buffer->size = size;
	buffer->sg_table = ion_sec_heap_map_dma(heap, buffer);
	sg_dma_address(buffer->sg_table->sgl) = (dma_addr_t)sec_handle;
	sec_heap_total_memory += size;

	buffer->priv_virt = buf_info;
	mutex_init(&buf_info->lock);

	get_task_comm(buf_info->task_comm, task);
	buf_info->pid = task_pid_nr(task);
	buf_info->tid = task_pid_nr(current);
	get_task_comm(buf_info->thread_comm, current);

	ION_DBG("sec hdl %u (%s), sz 0x%lx (%s) -- %llu ns\n",
		sec_handle, heap->name, size, buf_info->task_comm,
		tm_end);

	if (tm_end > 10000000ULL) /* allocation time > 10ms */
		pr_info("%s: allocate (%s) sz=0x%lx takes %llu ns\n",
			__func__, heap->name, size, tm_end);

	return 0;
}

static void ion_sec_heap_free(struct ion_buffer *buffer)
{
	struct sg_table *table = buffer->sg_table;
	struct ion_heap *heap = buffer->heap;
	struct ion_sec_heap *sec_heap = container_of(heap,
						     struct ion_sec_heap,
						     heap);
	struct ion_sec_buf_info *buf_info = buffer->priv_virt;
	u32 sec_handle = 0;
	char cur_comm[TASK_COMM_LEN];
#if defined(MTK_IN_HOUSE_SEC_ION_SUPPORT)
	int ret = 0;
#endif

	pr_debug("%s enter name:%s, ssmr_id:%d, size:%zu\n",
		 __func__, heap->name, sec_heap->ssmr_id,
		 buffer->size);

	sec_handle = (u32)sg_dma_address(buffer->sg_table->sgl);

	get_task_comm(cur_comm, current->group_leader);
	ION_DBG("sec hdl %u (%s)\n", sec_handle, cur_comm);

#if defined(MTK_IN_HOUSE_SEC_ION_SUPPORT)
	ret = KREE_UnreferenceSecurechunkmem(ion_session_handle(), sec_handle);
	if (ret != TZ_RESULT_SUCCESS)
		pr_err("%s: failed(%d) heap %s, hdl %u\n", __func__,
		       ret, heap->name, sec_handle);

#elif defined(SECMEM_KERNEL_API)
	trusted_mem_api_unref(sec_heap->ssmr_id, sec_handle,
			      (uint8_t *)buffer->heap->name,
			      buffer->heap->id);
#endif

	sec_heap_total_memory -= buffer->size;
	ion_sec_heap_unmap_dma(heap, buffer);
	kfree(buf_info);
	kfree(table);

	pr_debug("%s exit, total %zu\n", __func__, sec_heap_total_memory);
}

#if IS_ENABLED(CONFIG_MTK_ION_DEBUG)
static int ion_sec_heap_debug_show(struct ion_heap *heap, struct seq_file *s,
				   void *unused)
{
	struct ion_buffer *buffer, *tmp_buffer;
	struct ion_sec_buf_info *buf_info;
	struct ion_dma_buf_attachment *a, *tmp_a;
	char buf_name[DMA_BUF_NAME_LEN];
	size_t ret;

	seq_printf(s, "%18.s %18.s %10.s %10.s %16.s(%5.s-%5.s) %32.s\n",
		   "buffer", "dma-buf", "sec-hdl", "size", "owner", "pid",
		   "tid", "buf-name");
	list_for_each_entry_safe(buffer, tmp_buffer, &heap->alloc_list, list) {
		mutex_lock(&buffer->lock);

		ret = mtk_dmabuf_helper_get_bufname(buffer->dmabuf, buf_name);
		buf_info = buffer->priv_virt;

		mutex_lock(&buf_info->lock);
		seq_printf(s, "0x%016pK 0x%016pK %10u %10u %16.s(%5.d-%5.d) %32.s\n",
			   buffer, buffer->dmabuf,
			   (u32)sg_dma_address(buffer->sg_table->sgl),
			   (u32)buffer->size, buf_info->thread_comm,
			   buf_info->pid, buf_info->tid,
			   (ret > 0) ? buf_name : "undefined");
		mutex_unlock(&buf_info->lock);

		seq_puts(s, "* attach_dev:\n");
		list_for_each_entry_safe(a, tmp_a, &buffer->attachments, list)
			seq_printf(s, "*    %s\n", dev_name(a->dev));

		mutex_unlock(&buffer->lock);
	}
	seq_puts(s, "----------------------------------------------------\n");

	return 0;
}
#endif

static struct
sg_table *mtk_ion_sec_buf_map_dma(struct dma_buf_attachment *attachment,
				  enum dma_data_direction direction)
{
	struct ion_buffer *buffer = attachment->dmabuf->priv;
	struct ion_dma_buf_attachment *a = attachment->priv;
	struct sg_table *table = a->table;

	mutex_lock(&buffer->lock);
	sg_dma_address(table->sgl) = sg_dma_address(buffer->sg_table->sgl);
	a->mapped = true;
	mutex_unlock(&buffer->lock);

	return table;
}

/* nothing to do for secure buffer */

static void mtk_ion_sec_buf_unmap_dma(struct dma_buf_attachment *attachment,
				      struct sg_table *table,
				      enum dma_data_direction direction)
{
}

static int mtk_ion_sec_buf_begin_cpu_access(struct dma_buf *dmabuf,
					    enum dma_data_direction direction)
{
	pr_debug("%s: fake ops\n", __func__);

	return -EOPNOTSUPP;
}

static int mtk_ion_sec_buf_end_cpu_access(struct dma_buf *dmabuf,
					  enum dma_data_direction direction)
{
	pr_debug("%s: fake ops\n", __func__);

	return -EOPNOTSUPP;
}

static void *mtk_ion_sec_buf_map(struct dma_buf *dmabuf, unsigned long offset)
{
	pr_debug("%s: fake ops\n", __func__);

	return NULL;
}

static int mtk_ion_sec_buf_mmap(struct dma_buf *dmabuf,
				struct vm_area_struct *vma)
{
	pr_debug("%s: fake ops\n", __func__);

	return -EOPNOTSUPP;
}

static void *mtk_ion_sec_buf_vmap(struct dma_buf *dmabuf)
{
	pr_debug("%s: fake ops\n", __func__);

	return NULL;
}

static void mtk_ion_sec_buf_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
}

static const struct dma_buf_ops mtk_ion_sec_buf_ops = {
	.map_dma_buf = mtk_ion_sec_buf_map_dma,
	.unmap_dma_buf = mtk_ion_sec_buf_unmap_dma,
	.begin_cpu_access = mtk_ion_sec_buf_begin_cpu_access,
	.end_cpu_access = mtk_ion_sec_buf_end_cpu_access,
	.mmap = mtk_ion_sec_buf_mmap,
	.map = mtk_ion_sec_buf_map,
	.vmap = mtk_ion_sec_buf_vmap,
	.vunmap = mtk_ion_sec_buf_vunmap,
};

static struct ion_heap_ops sec_heap_ops = {
	.allocate = ion_sec_heap_allocate,
	.free = ion_sec_heap_free,
#if IS_ENABLED(CONFIG_MTK_ION_DEBUG)
	.debug_show = ion_sec_heap_debug_show,
#endif
};

static struct ion_heap *__ion_sec_heap_create(int ssmr_id)
{
	struct ion_sec_heap *sec_heap;

	pr_info("%s enter\n", __func__);

	sec_heap = kzalloc(sizeof(*sec_heap), GFP_KERNEL);
	if (!sec_heap)
		return ERR_PTR(-ENOMEM);

	sec_heap->ssmr_id = ssmr_id;
	sec_heap->heap.ops = &sec_heap_ops;
	sec_heap->heap.type = ION_HEAP_TYPE_CUSTOM;
	sec_heap->heap.flags &= ~ION_HEAP_FLAG_DEFER_FREE;
#if IS_ENABLED(CONFIG_MTEE_CMA_SECURE_MEMORY)
	sec_heap->type = ion_svp_heap[ssmr_id].type;
#endif

	memcpy(&sec_heap->heap.buf_ops,
	       &mtk_ion_sec_buf_ops, sizeof(struct dma_buf_ops));

	return &sec_heap->heap;
}

static int ion_sec_heap_create(void)
{
	struct ion_heap *heap;
	int i, heap_total, ret;

#if defined(MTK_IN_HOUSE_SEC_ION_SUPPORT)
	heap_total = ARRAY_SIZE(ion_svp_heap);
	for (i = 0; i < heap_total; i++) {
		heap = __ion_sec_heap_create(i);
		if (IS_ERR(heap)) {
			pr_err("%s create %d failed\n", __func__, i);
			return PTR_ERR(heap);
		}
		heap->name = ion_svp_heap[i].name;
		ret = ion_device_add_heap(heap);
		pr_info("name:%s, heap_id:%u (ret %d)\n",
			heap->name, heap->id, ret);
	}

#elif defined(SECMEM_KERNEL_API)
	int ssmr_id;

	heap_total = ssmr_query_total_sec_heap_count();
	if (!heap_total)
		pr_warn("ssmr is not ready for secure heap\n");

	for (i = 0; i < heap_total; i++) {
		ssmr_id = ssmr_query_heap_info(i, heap_name[i]);
		heap = __ion_sec_heap_create(ssmr_id);
		if (IS_ERR(heap)) {
			pr_err("%s failed\n", __func__);
			return PTR_ERR(heap);
		}
		heap->name = heap_name[i];
		ret = ion_device_add_heap(heap);
		pr_info("ssmr_id:%d, name:%s, heap_id:%u (ret %d)\n",
			ssmr_id, heap->name, heap->id, ret);
	}
#else
	pr_info("ion secure heap not support\n");
#endif
	return 0;
}

device_initcall(ion_sec_heap_create);

MODULE_LICENSE("GPL v2");
