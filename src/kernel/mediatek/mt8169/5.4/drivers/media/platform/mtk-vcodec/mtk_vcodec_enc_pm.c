// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
//#include <soc/mediatek/smi.h>

#include "mtk_vcodec_enc_pm.h"
#include "mtk_vcodec_enc_pm_plat.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"
//#include "smi_public.h"

#if IS_ENABLED(CONFIG_MTK_PSEUDO_M4U)
#include <mach/mt_iommu.h>
#include "mach/pseudo_m4u.h"
#include "smi_port.h"
#endif

void mtk_venc_init_ctx_pm(struct mtk_vcodec_ctx *ctx)
{
}

int mtk_vcodec_init_enc_pm(struct mtk_vcodec_dev *mtkdev)
{
	int ret = 0;

	struct device_node *node;
	struct platform_device *pdev;
	struct device *dev;
	struct mtk_vcodec_pm *pm;
#ifndef FPGA_PWRCLK_API_DISABLE
	int clk_id = 0;
	const char *clk_name;
#endif
	struct mtk_venc_clks_data *clks_data;

	pdev = mtkdev->plat_dev;
	pm = &mtkdev->pm;
	memset(pm, 0, sizeof(struct mtk_vcodec_pm));
	pm->mtkdev = mtkdev;
	pm->dev = &pdev->dev;
	clks_data = &pm->venc_clks_data;
	dev = &pdev->dev;

	node = of_parse_phandle(dev->of_node, "mediatek,larbs", 0);
	if (!node) {
		mtk_v4l2_err("no mediatek,larb found");
		return -1;
	}
	pdev = of_find_device_by_node(node);
	if (!pdev) {
		mtk_v4l2_err("no mediatek,larb device found");
		return -1;
	}
	pm->larbvenc = &pdev->dev;

	pdev = mtkdev->plat_dev;
	pm->dev = &pdev->dev;

#ifndef FPGA_PWRCLK_API_DISABLE
	memset(clks_data, 0x00, sizeof(struct mtk_venc_clks_data));
	while (!of_property_read_string_index(
			pdev->dev.of_node, "clock-names", clk_id, &clk_name)) {
		mtk_v4l2_err("init clock, id: %d, name: %s", clk_id, clk_name);
		pm->venc_clks[clk_id] = devm_clk_get(&pdev->dev, clk_name);
		if (IS_ERR(pm->venc_clks[clk_id])) {
			mtk_v4l2_err(
				"[VCODEC][ERROR] Unable to devm_clk_get id: %d, name: %s\n",
				clk_id, clk_name);
			return PTR_ERR(pm->venc_clks[clk_id]);
		}
		clks_data->core_clks[clks_data->core_clks_len].clk_id = clk_id;
		clks_data->core_clks[clks_data->core_clks_len].clk_name = clk_name;
		clks_data->core_clks_len++;
		clk_id++;
	}
#endif
	pm_runtime_enable(&pdev->dev);

	return ret;
}

void mtk_vcodec_release_enc_pm(struct mtk_vcodec_dev *mtkdev)
{
#if ENC_EMI_BW
	/* do nothing */
#endif
	pm_runtime_disable(mtkdev->pm.dev);
}

void mtk_venc_deinit_ctx_pm(struct mtk_vcodec_ctx *ctx)
{
}

void mtk_vcodec_enc_clock_on(struct mtk_vcodec_ctx *ctx, int core_id)
{
#if IS_ENABLED(CONFIG_MTK_PSEUDO_M4U)
	int i, larb_port_num, larb_id;
	struct M4U_PORT_STRUCT port;
#endif
#ifndef FPGA_PWRCLK_API_DISABLE
	int j, clk_id;
	struct mtk_venc_clks_data *clks_data;
	struct mtk_vcodec_dev *dev = NULL;
#endif
	int ret = 0;
	struct mtk_vcodec_pm *pm = &ctx->dev->pm;

	ret = pm_runtime_get_sync(pm->dev);
	if (ret < 0)
		mtk_v4l2_err("pm_runtime_get_sync pwrdmain enc fail %d", ret);

#ifndef FPGA_PWRCLK_API_DISABLE
	dev = ctx->dev;
	time_check_start(MTK_FMT_ENC, core_id);

	clks_data = &pm->venc_clks_data;

	if (core_id == MTK_VENC_CORE_0 ||
		core_id == MTK_VENC_CORE_1) {
			// enable core clocks
		for (j = 0; j < clks_data->core_clks_len; j++) {
			clk_id = clks_data->core_clks[j].clk_id;
			ret = clk_prepare_enable(pm->venc_clks[clk_id]);
			if (ret)
				mtk_v4l2_err("clk_prepare_enable id: %d, name: %s fail %d",
					clk_id, clks_data->core_clks[j].clk_name, ret);
		}
	} else {
		mtk_v4l2_err("invalid core_id %d", core_id);
		time_check_end(MTK_FMT_ENC, core_id, 50);
		return;
	}
	time_check_end(MTK_FMT_ENC, core_id, 50);
#endif


#if IS_ENABLED(CONFIG_MTK_PSEUDO_M4U)
	time_check_start(MTK_FMT_ENC, core_id);
	if (core_id == MTK_VENC_CORE_0) {
		larb_port_num = SMI_LARB7_PORT_NUM;
		larb_id = 7;
	}

	//enable 34bits port configs
	for (i = 0; i < larb_port_num; i++) {
		port.ePortID = MTK_M4U_ID(larb_id, i);
		port.Direction = 0;
		port.Distance = 1;
		port.domain = 0;
		port.Security = 0;
		port.Virtuality = 1;
		m4u_config_port(&port);
	}
	time_check_end(MTK_FMT_ENC, core_id, 50);
#endif
}

void mtk_vcodec_enc_clock_off(struct mtk_vcodec_ctx *ctx, int core_id)
{
#ifndef FPGA_PWRCLK_API_DISABLE
	struct mtk_vcodec_pm *pm = &ctx->dev->pm;
	int i, clk_id, ret;
	struct mtk_venc_clks_data *clks_data;

	clks_data = &pm->venc_clks_data;

	if (core_id == MTK_VENC_CORE_0 ||
		core_id == MTK_VENC_CORE_1) {
		if (clks_data->core_clks_len > 0) {
			for (i = clks_data->core_clks_len - 1; i >= 0; i--) {
				clk_id = clks_data->core_clks[i].clk_id;
				clk_disable_unprepare(pm->venc_clks[clk_id]);
			}
		}
	} else
		mtk_v4l2_err("invalid core_id %d", core_id);
#endif

	ret = pm_runtime_put_sync(pm->dev);
	if (ret)
		mtk_v4l2_err("pm_runtime_put_sync fail");
}
