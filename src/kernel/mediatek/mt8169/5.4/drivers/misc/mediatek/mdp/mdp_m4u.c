// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/dma-buf.h>
#include "mdp_m4u.h"
#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
#include "mdp_cmdq_helper_ext.h"
#else
#include "cmdq_record.h"
#ifndef MDP_META_IN_LEGACY_V2
#include "cmdq_helper_ext.h"
#else
#include "cmdq_core.h"
#endif
#endif
//#include <ion_drv.h>
#if IS_ENABLED(CONFIG_MTK_IOMMU_V2)
#include "mach/mt_iommu.h"
#include "mach/pseudo_m4u.h"
#include <soc/mediatek/smi.h>
#include "mtk_iommu_ext.h"
#endif

#if IS_ENABLED(CONFIG_MTK_IOMMU_V2)
static struct ion_client *g_mdp_ion_client;
#endif

#ifdef MTK_LEGACY_ION
int mdp_ion_get_mva(struct ion_handle *handle,
	unsigned long *mva, unsigned long fixed_mva, int port)
{
#if IS_ENABLED(CONFIG_MTK_IOMMU_V2)
	struct ion_mm_data mm_data;
	size_t mva_size;

	memset((void *)&mm_data, 0, sizeof(struct ion_mm_data));
	mm_data.mm_cmd = ION_MM_GET_IOVA;
	mm_data.get_phys_param.module_id = port;
	mm_data.get_phys_param.kernel_handle = handle;
	if (fixed_mva > 0) {
		mm_data.get_phys_param.phy_addr =
			(port << 24) | ION_FLAG_GET_FIXED_PHYS;
		mm_data.get_phys_param.len = ION_FLAG_GET_FIXED_PHYS;
		mm_data.get_phys_param.reserve_iova_start = fixed_mva;
		mm_data.get_phys_param.reserve_iova_end = fixed_mva;
	}

	if (ion_kernel_ioctl(g_mdp_ion_client, ION_CMD_MULTIMEDIA,
		(unsigned long)&mm_data) < 0) {
		CMDQ_ERR("%s: get mva failed.%p -%p\n",
			__func__, g_mdp_ion_client, handle);
		ion_free(g_mdp_ion_client, handle);
		return -1;
	}

	*mva = mm_data.get_phys_param.phy_addr;
	mva_size = mm_data.get_phys_param.len;
	if (*mva == 0)
		CMDQ_ERR("alloc mmu addr hnd=0x%p,mva=0x%08lx\n",
			handle, *mva);
#endif
	return 0;
}

struct ion_handle *mdp_ion_import_handle(int fd)
{
	struct ion_handle *handle = NULL;
#if IS_ENABLED(CONFIG_MTK_IOMMU_V2)
	/* If no need Ion support, do nothing! */
	if (fd <= 0) {
		CMDQ_ERR("NO NEED ion support, fd %d\n", fd);
		return handle;
	}

	if (!g_mdp_ion_client) {
		CMDQ_ERR("invalid ion client!\n");
		return handle;
	}

	handle = ion_import_dma_buf_fd(g_mdp_ion_client, fd);
	if (IS_ERR(handle)) {
		CMDQ_ERR("import ion handle failed!\n");
		return NULL;
	}

	CMDQ_MSG("import ion handle fd=%d,hnd=0x%p\n", fd, handle);
#endif
	return handle;
}

void mdp_ion_free_handle(struct ion_handle *handle)
{
#if IS_ENABLED(CONFIG_MTK_IOMMU_V2)
	if (!g_mdp_ion_client) {
		CMDQ_ERR("invalid ion client!\n");
		return;
	}
	if (!handle)
		return;

	ion_free(g_mdp_ion_client, handle);

	CMDQ_MSG("free ion handle 0x%p\n", handle);
#endif
}

void mdp_ion_cache_flush(struct ion_handle *handle)
{
#if IS_ENABLED(CONFIG_MTK_IOMMU_V2)
	struct ion_sys_data sys_data;
	void *buffer_va;

	if (!g_mdp_ion_client || !handle)
		return;

	sys_data.sys_cmd = ION_SYS_CACHE_SYNC;
	sys_data.cache_sync_param.kernel_handle = handle;
	sys_data.cache_sync_param.sync_type = ION_CACHE_INVALID_BY_RANGE;

	buffer_va = ion_map_kernel(g_mdp_ion_client, handle);
	sys_data.cache_sync_param.va = buffer_va;
	sys_data.cache_sync_param.size = handle->buffer->size;

	if (ion_kernel_ioctl(g_mdp_ion_client, ION_CMD_SYSTEM,
		(unsigned long)&sys_data))
		pr_info("ion cache flush failed!\n");
	ion_unmap_kernel(g_mdp_ion_client, handle);
#endif
}
#endif

void mdp_ion_free_handle(struct dma_buf *dma_buffer,
	struct dma_buf_attachment *dma_attach, struct sg_table *table)
{

	dma_buf_unmap_attachment(dma_attach, table, 0);
	dma_buf_detach(dma_buffer, dma_attach);
	dma_buf_put(dma_buffer);
	CMDQ_MSG("free ion handle %s\n", __func__);

}

void mdp_ion_create(const char *name)
{
#if IS_ENABLED(CONFIG_MTK_IOMMU_V2)
	if (g_ion_device)
		g_mdp_ion_client = ion_client_create(g_ion_device, name);
	else
		CMDQ_ERR("invalid g_ion_device\n");

	if (!g_mdp_ion_client)
		CMDQ_ERR("create ion client failed!\n");

#endif
}

void mdp_ion_destroy(void)
{
#if IS_ENABLED(CONFIG_MTK_IOMMU_V2)
	if (g_mdp_ion_client && g_ion_device)
		ion_client_destroy(g_mdp_ion_client);
#endif
}
