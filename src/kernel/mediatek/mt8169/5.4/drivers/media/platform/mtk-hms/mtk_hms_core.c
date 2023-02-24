// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Ice Huang <ice.huang@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/workqueue.h>
#include <linux/iommu.h>

#include "mtk_hms_core.h"
#include "mtk_hms_m2m.h"

static const struct of_device_id mtk_hms_comp_dt_ids[] = {
	{
		.compatible = "mediatek,mt8169-mdp-disp-rdma",
		.data = (void *)MTK_HMS_RDMA
	}, {
		.compatible = "mediatek,mt8169-mdp-hms",
		.data = (void *)MTK_HMS_HMSENG
	}, {
		.compatible = "mediatek,mt8169-mdp-disp-wdma",
		.data = (void *)MTK_HMS_WDMA
	}, {
		.compatible = "mediatek,mdp_mutex0",
		.data = (void *)MTK_HMS_MUTEX
	},
	{ },
};

static const struct of_device_id mtk_hms_of_ids[] = {
	{ .compatible = "mediatek,mt8169-mdp-hms", .data = "platform:mt8169" },
	{ },
};
MODULE_DEVICE_TABLE(of, mtk_hms_of_ids);

static void mtk_hms_clock_on(struct mtk_hms_dev *hms)
{
	struct device *dev = &hms->pdev->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(hms->comp); i++)
		mtk_hms_comp_clock_on(dev, hms->comp[i]);
}

static void mtk_hms_clock_off(struct mtk_hms_dev *hms)
{
	struct device *dev = &hms->pdev->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(hms->comp); i++)
		mtk_hms_comp_clock_off(dev, hms->comp[i]);
}

static void mtk_hms_wdt_worker(struct work_struct *work)
{
	struct mtk_hms_dev *hms =
			container_of(work, struct mtk_hms_dev, wdt_work);
	struct mtk_hms_ctx *ctx;

	list_for_each_entry(ctx, &hms->ctx_list, list) {
		pr_info("[%d] Change as state error\n", ctx->id);
		mtk_hms_ctx_state_lock_set(ctx, MTK_HMS_CTX_ERROR);
	}
}

static irqreturn_t mtk_hms_irq(int irq, void *priv)
{
	struct mtk_hms_dev *hms = priv;
	struct mtk_hms_ctx *ctx;
	//struct vb2_v4l2_buffer *src_buf, *dst_buf;
	//struct mtk_jpeg_src_buf *jpeg_src_buf;
	enum vb2_buffer_state buf_state = VB2_BUF_STATE_ERROR;
	//u32 result_size;

	spin_lock(&hms->hw_lock);

	ctx = v4l2_m2m_get_curr_priv(hms->m2m_dev); // call in process done
	if (!ctx) {
		v4l2_err(&hms->v4l2_dev, "Context is NULL\n");
		spin_unlock(&hms->hw_lock);
		return IRQ_HANDLED;
	}

	mtk_hms_clear_int_status(ctx);
	//mtk_mdp_process_done
	mtk_hms_process_done(hms, buf_state);

	spin_unlock(&hms->hw_lock);
	return IRQ_HANDLED;
}

static int mtk_hms_probe(struct platform_device *pdev)
{
	struct mtk_hms_dev *hms;
	struct device *dev = &pdev->dev;
	struct device_node *node, *parent;
	struct iommu_domain *iommu;
	struct platform_device *cmdq_dev;
	int i, ret = 0;

	hms = devm_kzalloc(&pdev->dev, sizeof(*hms), GFP_KERNEL);
	if (!hms)
		return -ENOMEM;

	iommu = iommu_get_domain_for_dev(dev);
	if (!iommu) {
		v4l2_err(&hms->v4l2_dev, "iommu_get_domain_for_dev fail\n");
		return -EPROBE_DEFER;
	}

	node = of_parse_phandle(dev->of_node, "mediatek,mailbox-gce", 0);
	if (!node)
		return -EINVAL;

	cmdq_dev = of_find_device_by_node(node);
	if (!cmdq_dev || !cmdq_dev->dev.driver) {
		of_node_put(node);
		return -EPROBE_DEFER;
	}

	ret = of_property_read_u32(dev->of_node, "mediatek,hmsid",
		(u32 *)(void *)&hms->id);
	if (ret) {
		dev_info(dev, "not set mediatek,hmsid, use default id 0.\n");
		hms->id = 0;
	}

	hms->pdev = pdev;
	INIT_LIST_HEAD(&hms->ctx_list);

	mutex_init(&hms->lock);
	spin_lock_init(&hms->hw_lock);

	/* Old dts had the components as child nodes */
	if (of_get_next_child(dev->of_node, NULL)) {
		parent = dev->of_node;
		v4l2_err(&hms->v4l2_dev, "device tree is out of date\n");
	} else {
		parent = dev->of_node->parent;
	}

	/* Iterate over sibling MDP function blocks */
	for_each_child_of_node(parent, node) {
		const struct of_device_id *of_id;
		enum mtk_hms_comp_type comp_type;
		int comp_id;
		struct mtk_hms_comp *comp;

		of_id = of_match_node(mtk_hms_comp_dt_ids, node);
		if (!of_id) {
			v4l2_err(&hms->v4l2_dev, "of_match_node fail\n");
			continue;
		}
		if (!of_device_is_available(node)) {
			v4l2_err(&hms->v4l2_dev, "of_device_is_available fail\n");
			continue;
		}

		comp_type = (enum mtk_hms_comp_type)of_id->data;
		comp_id = comp_type;//mtk_hms_comp_get_id(dev, node, comp_type);
		if (comp_id < 0) {
			v4l2_err(&hms->v4l2_dev, "get_id fail, id = %d\n", comp_id);
			continue;
		}

		comp = devm_kzalloc(dev, sizeof(*comp), GFP_KERNEL);
		if (!comp) {
			ret = -ENOMEM;
			goto err_comp;
		}
		hms->comp[comp_id] = comp;

		ret = mtk_hms_v4l2_comp_init(dev, node, comp, comp_id, pdev);
		if (ret) {
			v4l2_err(&hms->v4l2_dev, "mtk_hms_v4l2_comp_init failed at %d\n", comp_id);
			goto err_comp;
		}
	}

	hms->job_wq = create_singlethread_workqueue(MTK_HMS_MODULE_NAME);
	if (!hms->job_wq) {
		ret = -ENOMEM;
		goto err_alloc_job_wq;
	}

	hms->wdt_wq = create_singlethread_workqueue("hms_wdt_wq");
	if (!hms->wdt_wq) {
		ret = -ENOMEM;
		goto err_alloc_wdt_wq;
	}
	INIT_WORK(&hms->wdt_work, mtk_hms_wdt_worker);

	ret = v4l2_device_register(dev, &hms->v4l2_dev);
	if (ret) {
		ret = -EINVAL;
		goto err_dev_register;
	}

	ret = mtk_hms_register_m2m_device(hms);
	if (ret) {
		v4l2_err(&hms->v4l2_dev, "Failed to init mem2mem device\n");
		goto err_m2m_register;
	}

	hms->irq = platform_get_irq(pdev, 0);
	if (hms->irq < 0) {
		v4l2_err(&hms->v4l2_dev, "Failed to get hms_irq %d.\n", hms->irq);
		//return hms->irq;
		goto err_m2m_register;
	}

	ret = devm_request_irq(&pdev->dev, hms->irq, mtk_hms_irq,
				       0, pdev->name, hms);
	if (ret) {
		v4l2_err(&hms->v4l2_dev, "Failed to request hms_irq %d (%d)\n",
			hms->irq, ret);
		goto err_m2m_register;
	}

	platform_set_drvdata(pdev, hms);

	vb2_dma_contig_set_max_seg_size(&pdev->dev, DMA_BIT_MASK(32));

	pm_runtime_enable(dev);

	return 0;

err_m2m_register:
	v4l2_device_unregister(&hms->v4l2_dev);

err_dev_register:
	destroy_workqueue(hms->wdt_wq);

err_alloc_wdt_wq:
	destroy_workqueue(hms->job_wq);

err_alloc_job_wq:

err_comp:
	for (i = 0; i < ARRAY_SIZE(hms->comp); i++)
		mtk_hms_comp_deinit(dev, hms->comp[i]);

	dev_dbg(dev, "err %d\n", ret);

	return ret;
}

static int mtk_hms_remove(struct platform_device *pdev)
{
	struct mtk_hms_dev *hms = platform_get_drvdata(pdev);
	int i;

	pm_runtime_disable(&pdev->dev);

	vb2_dma_contig_clear_max_seg_size(&pdev->dev);

	mtk_hms_unregister_m2m_device(hms);
	v4l2_device_unregister(&hms->v4l2_dev);

	flush_workqueue(hms->job_wq);
	destroy_workqueue(hms->job_wq);

	for (i = 0; i < ARRAY_SIZE(hms->comp); i++)
		mtk_hms_comp_deinit(&pdev->dev, hms->comp[i]);

	return 0;
}

static int __maybe_unused mtk_hms_pm_suspend(struct device *dev)
{
	struct mtk_hms_dev *hms = dev_get_drvdata(dev);

	mtk_hms_clock_off(hms);

	return 0;
}

static int __maybe_unused mtk_hms_pm_resume(struct device *dev)
{
	struct mtk_hms_dev *hms = dev_get_drvdata(dev);

	mtk_hms_clock_on(hms);

	return 0;
}

static int __maybe_unused mtk_hms_suspend(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 0;

	return mtk_hms_pm_suspend(dev);
}

static int __maybe_unused mtk_hms_resume(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 0;

	return mtk_hms_pm_resume(dev);
}

static const struct dev_pm_ops mtk_hms_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_hms_suspend, mtk_hms_resume)
	SET_RUNTIME_PM_OPS(mtk_hms_pm_suspend, mtk_hms_pm_resume, NULL)
};

static struct platform_driver mtk_hms_driver = {
	.probe		= mtk_hms_probe,
	.remove		= mtk_hms_remove,
	.driver = {
		.name	= MTK_HMS_MODULE_NAME,
		.pm	= &mtk_hms_pm_ops,
		.of_match_table = mtk_hms_of_ids,
	}
};

module_platform_driver(mtk_hms_driver);

MODULE_AUTHOR("Ice Huang <ice.huang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek motion statistics driver");
MODULE_LICENSE("GPL v2");
