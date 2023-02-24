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

static const struct mtk_gate_regs mfg_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_MFG(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &mfg_cg_regs, _shift, &mtk_clk_gate_ops_setclr)

static const struct mtk_gate mfg_clks[] = {
	GATE_MFG(CLK_MFG_BG3D, "mfg_bg3d", "mfg_sel", 0),
};

static const struct mtk_clk_desc mfg_desc = {
	.clks = mfg_clks,
	.num_clks = ARRAY_SIZE(mfg_clks),
};

static const struct of_device_id of_match_clk_mt8169_mfg[] = {
	{
		.compatible = "mediatek,mt8169-mfgsys",
		.data = &mfg_desc,
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_mt8169_mfg_drv = {
	.probe = mtk_clk_simple_probe,
	.driver = {
		.name = "clk-mt8169-mfg",
		.of_match_table = of_match_clk_mt8169_mfg,
	},
};

static int __init clk_mt8169_mfg_init(void)
{
	return platform_driver_register(&clk_mt8169_mfg_drv);
}

static void __exit clk_mt8169_mfg_exit(void)
{
	platform_driver_unregister(&clk_mt8169_mfg_drv);
}

module_init(clk_mt8169_mfg_init);
module_exit(clk_mt8169_mfg_exit);
MODULE_LICENSE("GPL");
