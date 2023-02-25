// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Ice Huang <ice.huang@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include "mtk_hms_comp.h"

static const char * const mtk_hms_comp_stem[MTK_HMS_COMP_TYPE_MAX] = {
	"mdp_disp_rdma",
	"mdp_hms",
	"mdp_disp_wdma",
	"mdp_mutex0",
};

struct mtk_hms_comp_match {
	enum mtk_hms_comp_type type;
	int alias_id;
};

static const struct mtk_hms_comp_match mtk_hms_matches[MTK_HMS_COMP_ID_MAX] = {
	{ MTK_HMS_RDMA,		0 },
	{ MTK_HMS_HMSENG,	0 },
	{ MTK_HMS_WDMA,		0 },
	{ MTK_HMS_MUTEX,		0 },
};

int mtk_hms_comp_get_id(struct device *dev, struct device_node *node,
			enum mtk_hms_comp_type comp_type)
{
	int id;
	int i;

	if (comp_type >= MTK_HMS_COMP_ID_MAX) {
		pr_info("comp_type is out of range");
		return -EINVAL;
	}
	id = of_alias_get_id(node, mtk_hms_comp_stem[comp_type]);
	for (i = 0; i < ARRAY_SIZE(mtk_hms_matches); i++) {
		if (comp_type == mtk_hms_matches[i].type &&
		    id == mtk_hms_matches[i].alias_id)
			return i;
	}
	return -EINVAL;
}

void mtk_hms_comp_clock_on(struct device *dev, struct mtk_hms_comp *comp)
{
	int i;
	int ret;

	if (!comp)
		return;

	if (comp->larb_dev)
		pm_runtime_get_sync(comp->larb_dev);

	for (i = 0; i < ARRAY_SIZE(comp->clk); i++) {
		if (IS_ERR(comp->clk[i]))
			continue;
		ret = clk_prepare_enable(comp->clk[i]);
		if (ret) {
			pr_info("clk_prepare_enable fail!\n");
			continue;
		}
	}
}

void mtk_hms_comp_clock_off(struct device *dev, struct mtk_hms_comp *comp)
{
	int i;

	if (!comp)
		return;

	for (i = 0; i < ARRAY_SIZE(comp->clk); i++) {
		if (IS_ERR(comp->clk[i]))
			continue;
		clk_disable_unprepare(comp->clk[i]);
	}

	if (comp->larb_dev)
		pm_runtime_put_sync(comp->larb_dev);
}

int mtk_hms_v4l2_comp_init(struct device *dev, struct device_node *node,
			   struct mtk_hms_comp *comp, enum mtk_hms_comp_id comp_id,
			   struct platform_device *pdev)
{
	struct resource *res;
	struct device_node *larb_node;
	struct platform_device *larb_pdev;
	int i;

	if (comp_id >= MTK_HMS_COMP_ID_MAX)
		return -EINVAL;

	comp->dev_node = of_node_get(node);
	comp->id = comp_id;
	comp->type = mtk_hms_matches[comp_id].type;
	//comp->regs = of_iomap(node, 0);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	comp->regs = devm_ioremap_resource(&pdev->dev, res);

	for (i = 0; i < ARRAY_SIZE(comp->clk); i++) {
		comp->clk[i] = of_clk_get(node, i);

		/* Only RDMA needs two clocks */
		if (comp->type != MTK_HMS_RDMA)
			break;
	}

	/* Only DMA capable components need the LARB property */
	comp->larb_dev = NULL;
	if (comp->type != MTK_HMS_RDMA &&
	    comp->type != MTK_HMS_WDMA)
		return 0;

	larb_node = of_parse_phandle(node, "mediatek,larbs", 0);
	if (!larb_node)
		return -EINVAL;

	larb_pdev = of_find_device_by_node(larb_node);
	if (!larb_pdev) {
		of_node_put(larb_node);
		return -EPROBE_DEFER;
	}
	of_node_put(larb_node);

	comp->larb_dev = &larb_pdev->dev;

	return 0;
}

void mtk_hms_comp_deinit(struct device *dev, struct mtk_hms_comp *comp)
{
	of_node_put(comp->dev_node);
}
