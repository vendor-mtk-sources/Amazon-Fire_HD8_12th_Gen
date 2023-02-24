/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __MT_IOMMU_DMA_H__
#define __MT_IOMMU_DMA_H__

#include <linux/dma-buf.h>
#include <linux/err.h>
#include <uapi/linux/dma-buf.h>

#define NOR_DMA_BUF		0
#define SEC_DMA_BUF		1

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC)

/* This function is only help to get dma address from the dma-buf share_fd since
 * this function flow looks the similar.
 * Note that this function only help get dma address, it don't help get
 * the dma_buf.
 * That means the user MUST keep the buffer will not be freed.
 * e.g. dma_buf_get before it use and dma_buf_put after the HW done.
 * Meanwhile, the dma_address will be unmapped while the buffer is release.
 *
 * [input]
 *	dev: iommu user device.
 *	share_fd: dma-buf share fd.
 * [output]
 *	dma_addr: dma address for normal buffer.
 *		  secure handler. for security buffer.
 *	is_sec: = 0 (NOR_DMA_BUF) for normal buffer.
 *		= 1 (SEC_DMA_BUF) for security buffer.
 * [return]
 *	0 is successful. < 0 is fail.
 */
int mtk_dmabuf_helper_get_dma_addr(struct device *dev, int share_fd,
				   dma_addr_t *dma_addr, int *is_sec);

/* This function is only help to get buffer name of dma-buf.
 * Note that it will fall into the spin lock(name_lock of dma-buf)!!
 *
 * [input]
 *	dmabuf: dma-buf pointer.
 * [output]
 *	name: An character array with at least DMA_BUF_NAME_LEN(=32) byte.
 * [return]
 *	>0 is successful. ==0 is fail.
 */
size_t mtk_dmabuf_helper_get_bufname(struct dma_buf *dmabuf, char *name);

#else	/* if !CONFIG_MTK_IOMMU_MISC */

static inline
int mtk_dmabuf_helper_get_dma_addr(struct device *dev, int share_fd,
				   dma_addr_t *dma_addr, int *is_sec)
{
	return -EFAULT;
}

static inline
size_t mtk_dmabuf_helper_get_bufname(struct dma_buf *dmabuf, char *name)
{
	return 0;
}

#endif	/* CONFIG_MTK_IOMMU_MISC */

#endif	/* __MT_IOMMU_DMA_H__ */
