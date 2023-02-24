/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Ice Huang <ice.huang@mediatek.com>
 */

#ifndef __MTK_HMS_COMP_H__
#define __MTK_HMS_COMP_H__

/**
 * enum mtk_hms_comp_type - the HMS component type
 * @MTK_MDP_RDMA:	Read DMA
 * @MTK_HMS_HMSENG:	HMS engine
 * @MTK_HMS_WDMA:	Write DMA
 * @MTK_HMS_MUTEX:	Trigger SOF
 */
enum mtk_hms_comp_type {
	MTK_HMS_RDMA,
	MTK_HMS_HMSENG,
	MTK_HMS_WDMA,
	MTK_HMS_MUTEX,
	MTK_HMS_COMP_TYPE_MAX,
};

/**
 * enum mtk_hms_comp_id - the HMS component ID
 * @MTK_HMS_COMP_RDMA:	      Read DMA
 * @MTK_HMS_COMP_HMSENG:    HMS engine
 * @MTK_HMS_COMP_WDMA:	      Write DMA
 * @MTK_HMS_COMP_MUTEX:	      Trigger SOF
 */
enum mtk_hms_comp_id {
	MTK_HMS_COMP_RDMA,
	MTK_HMS_COMP_HMSENG,
	MTK_HMS_COMP_WDMA,
	MTK_HMS_COMP_MUTEX,
	MTK_HMS_COMP_ID_MAX,
};

/**
 * struct mtk_hms_comp - the HMS's function component data
 * @dev_node:	component device node
 * @clk:	clocks required for component
 * @regs:	Mapped address of component registers.
 * @larb_dev:	SMI device required for component
 * @type:	component type
 * @id:		component ID
 */
struct mtk_hms_comp {
	struct device_node	*dev_node;
	struct clk		*clk[2];
	void __iomem		*regs;
	struct device		*larb_dev;
	enum mtk_hms_comp_type	type;
	enum mtk_hms_comp_id		id;
};

int mtk_hms_v4l2_comp_init(struct device *dev, struct device_node *node,
			   struct mtk_hms_comp *comp,
			   enum mtk_hms_comp_id comp_id,
			   struct platform_device *pdev);
void mtk_hms_comp_deinit(struct device *dev, struct mtk_hms_comp *comp);
int mtk_hms_comp_get_id(struct device *dev, struct device_node *node,
			enum mtk_hms_comp_type comp_type);
void mtk_hms_comp_clock_on(struct device *dev, struct mtk_hms_comp *comp);
void mtk_hms_comp_clock_off(struct device *dev, struct mtk_hms_comp *comp);

#endif /* __MTK_HMS_COMP_H__ */
