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

static const struct mtk_gate_regs wpe_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

#define GATE_WPE(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &wpe_cg_regs, _shift, &mtk_clk_gate_ops_no_setclr_inv)

static const struct mtk_gate wpe_clks[] = {
	GATE_WPE(CLK_WPE_CK_EN, "wpe", "wpe_sel", 17),
	GATE_WPE(CLK_WPE_SMI_LARB8_CK_EN, "wpe_smi_larb8", "wpe_sel", 19),
	GATE_WPE(CLK_WPE_SYS_EVENT_TX_CK_EN, "wpe_sys_event_tx", "wpe_sel", 20),
	GATE_WPE(CLK_WPE_SMI_LARB8_PCLK_EN, "wpe_smi_larb8_p_en", "wpe_sel", 25),
};

static const struct mtk_clk_desc wpe_desc = {
	.clks = wpe_clks,
	.num_clks = ARRAY_SIZE(wpe_clks),
};

static const struct of_device_id of_match_clk_mt8169_wpe[] = {
	{
		.compatible = "mediatek,mt8169-wpesys",
		.data = &wpe_desc,
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_mt8169_wpe_drv = {
	.probe = mtk_clk_simple_probe,
	.driver = {
		.name = "clk-mt8169-wpe",
		.of_match_table = of_match_clk_mt8169_wpe,
	},
};

static int __init clk_mt8169_wpe_init(void)
{
	return platform_driver_register(&clk_mt8169_wpe_drv);
}

static void __exit clk_mt8169_wpe_exit(void)
{
	platform_driver_unregister(&clk_mt8169_wpe_drv);
}

module_init(clk_mt8169_wpe_init);
module_exit(clk_mt8169_wpe_exit);
MODULE_LICENSE("GPL");
