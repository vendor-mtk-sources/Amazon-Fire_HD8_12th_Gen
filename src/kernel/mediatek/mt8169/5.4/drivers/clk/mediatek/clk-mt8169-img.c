// SPDX-License-Identifier: GPL-2.0-only
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Chun-Jie Chen <chun-jie.chen@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt8169-clk.h>

static const struct mtk_gate_regs img_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMG(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &img_cg_regs, _shift, &mtk_clk_gate_ops_setclr)

static const struct mtk_gate img1_clks[] = {
	GATE_IMG(CLK_IMG1_LARB9_IMG1, "img1_larb9_img1", "img1_sel", 0),
	GATE_IMG(CLK_IMG1_LARB10_IMG1, "img1_larb10_img1", "img1_sel", 1),
	GATE_IMG(CLK_IMG1_DIP, "img1_dip", "img1_sel", 2),
	GATE_IMG(CLK_IMG1_GALS_IMG1, "img1_gals_img1", "img1_sel", 12),
};

static const struct mtk_gate img2_clks[] = {
	GATE_IMG(CLK_IMG2_LARB9_IMG2, "img2_larb9_img2", "img1_sel", 0),
	GATE_IMG(CLK_IMG2_LARB10_IMG2, "img2_larb10_img2", "img1_sel", 1),
	GATE_IMG(CLK_IMG2_MFB, "img2_mfb", "img1_sel", 6),
	GATE_IMG(CLK_IMG2_WPE, "img2_wpe", "img1_sel", 7),
	GATE_IMG(CLK_IMG2_MSS, "img2_mss", "img1_sel", 8),
	GATE_IMG(CLK_IMG2_GALS_IMG2, "img2_gals_img2", "img1_sel", 12),
};

static const struct mtk_clk_desc img1_desc = {
	.clks = img1_clks,
	.num_clks = ARRAY_SIZE(img1_clks),
};

static const struct mtk_clk_desc img2_desc = {
	.clks = img2_clks,
	.num_clks = ARRAY_SIZE(img2_clks),
};

static const struct of_device_id of_match_clk_mt8169_img[] = {
	{
		.compatible = "mediatek,mt8169-imgsys1",
		.data = &img1_desc,
	}, {
		.compatible = "mediatek,mt8169-imgsys2",
		.data = &img2_desc,
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_mt8169_img_drv = {
	.probe = mtk_clk_simple_probe,
	.driver = {
		.name = "clk-mt8169-img",
		.of_match_table = of_match_clk_mt8169_img,
	},
};

static int __init clk_mt8169_img_init(void)
{
	return platform_driver_register(&clk_mt8169_img_drv);
}

static void __exit clk_mt8169_img_exit(void)
{
	platform_driver_unregister(&clk_mt8169_img_drv);
}

device_initcall(clk_mt8169_img_init);
module_exit(clk_mt8169_img_exit);
MODULE_LICENSE("GPL");
