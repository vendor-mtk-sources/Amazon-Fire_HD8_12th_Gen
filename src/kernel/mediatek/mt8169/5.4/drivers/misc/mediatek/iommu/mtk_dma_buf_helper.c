// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#define pr_fmt(fmt)    "[mtk-dmabuf-helper] " fmt

#include <linux/iommu.h>
#include <linux/module.h>

#include "mtk_dma_buf_helper.h"

static int _mtk_dmabuf_helper_get_dma_addr(struct device *dev, int share_fd,
					   dma_addr_t *dma_addr, int *is_sec,
					   int add_fcnt)
{
	struct dma_buf *buf = dma_buf_get(share_fd);		/* f_cnt +1 */
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	int ret = 0, sec;
	const char *reason;

	if (IS_ERR(buf)) {
		reason = "buf_get";
		ret = (int)PTR_ERR(buf);
		goto out;
	}
	attach = dma_buf_attach(buf, dev);
	if (IS_ERR(attach)) {
		reason = "buf_attach";
		ret = (int)PTR_ERR(attach);
		goto put_buf;
	}
	table = dma_buf_map_attachment(attach, 0);
	if (IS_ERR(table)) {
		reason = "buf_map";
		ret = (int)PTR_ERR(table);
		goto detach_buf;
	}

	*dma_addr = sg_dma_address(table->sgl);
	sec = sg_page(table->sgl) ? NOR_DMA_BUF : SEC_DMA_BUF;
	if (is_sec)
		*is_sec = sec;

	if (add_fcnt) {
		pr_debug("%s: %s, share_id %d, f_cnt +1\n", __func__,
			 dev_name(dev), share_fd);
		get_dma_buf(buf);				/* f_cnt +1 */
	}

	dma_buf_unmap_attachment(attach, table, 0);
detach_buf:
	dma_buf_detach(buf, attach);
put_buf:
	dma_buf_put(buf);					/* f_cnt -1 */

out:
	if (ret)
		pr_err("%s: %s fail(%s:%d) share_id %d\n", __func__,
		       dev_name(dev), reason, ret, share_fd);
	else
		pr_debug("%s: %s, share_id %d, dma_addr 0x%lx(sec %d, add_cnt %d)\n",
			 __func__, dev_name(dev),
			 share_fd, (unsigned long)(*dma_addr),
			 sec, add_fcnt);
	return ret;
}

int mtk_dmabuf_helper_get_dma_addr(struct device *dev, int share_fd,
				   dma_addr_t *dma_addr, int *is_sec)
{
	if (!dma_addr)
		return -EINVAL;

	if (!iommu_get_domain_for_dev(dev)) {
		dev_err(dev, "is not the iommu device\n");
		WARN_ON_ONCE(1);
		return -EINVAL;
	}

	return _mtk_dmabuf_helper_get_dma_addr(dev, share_fd, dma_addr, is_sec,
					       0);
}
EXPORT_SYMBOL_GPL(mtk_dmabuf_helper_get_dma_addr);

size_t mtk_dmabuf_helper_get_bufname(struct dma_buf *dmabuf, char *name)
{
	size_t cpy_size = 0;

	if (!dmabuf || !name)
		return cpy_size;

	spin_lock(&dmabuf->name_lock);
	if (dmabuf->name)
		cpy_size = strlcpy(name, dmabuf->name, DMA_BUF_NAME_LEN);
	spin_unlock(&dmabuf->name_lock);

	return cpy_size;
}
EXPORT_SYMBOL_GPL(mtk_dmabuf_helper_get_bufname);

MODULE_LICENSE("GPL v2");
