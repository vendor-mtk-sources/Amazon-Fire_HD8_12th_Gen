/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __MDP_M4U_H__
#define __MDP_M4U_H__

#if IS_ENABLED(CONFIG_MTK_IOMMU_V2)
#include "mach/mt_iommu.h"
#include <soc/mediatek/smi.h>
#endif
//#include <ion_priv.h>

void mdp_ion_create(const char *name);
void mdp_ion_destroy(void);
void mdp_ion_free_handle(struct dma_buf *dma_buffer,
	struct dma_buf_attachment *dma_attach, struct sg_table *table);
#ifdef MT8169_CMDQ_DEBUG
int mdp_ion_get_mva(struct ion_handle *handle,
	unsigned long *mva, unsigned long fixed_mva, int port);
struct ion_handle *mdp_ion_import_handle(int fd);
void mdp_ion_free_handle(struct ion_handle *handle);
void mdp_ion_cache_flush(struct ion_handle *handle);
#endif

#endif
