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

static const struct mtk_gate_regs vdec0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vdec1_cg_regs = {
	.set_ofs = 0x190,
	.clr_ofs = 0x190,
	.sta_ofs = 0x190,
};

static const struct mtk_gate_regs vdec2_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};

static const struct mtk_gate_regs vdec3_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xc,
	.sta_ofs = 0x8,
};

#define GATE_VDEC0(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &vdec0_cg_regs, _shift, &mtk_clk_gate_ops_setclr_inv)

#define GATE_VDEC1(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &vdec1_cg_regs, _shift, &mtk_clk_gate_ops_no_setclr_inv)

#define GATE_VDEC2(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &vdec2_cg_regs, _shift, &mtk_clk_gate_ops_setclr_inv)

#define GATE_VDEC3(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &vdec3_cg_regs, _shift, &mtk_clk_gate_ops_setclr_inv)

static const struct mtk_gate vdec_clks[] = {
	/* VDEC0 */
	GATE_VDEC0(CLK_VDEC_CKEN, "vdec_cken", "vdec_sel", 0),
	GATE_VDEC0(CLK_VDEC_ACTIVE, "vdec_active", "vdec_sel", 4),
	GATE_VDEC0(CLK_VDEC_CKEN_ENG, "vdec_cken_eng", "vdec_sel", 8),
	/* VDEC1 */
	GATE_VDEC1(CLK_VDEC_MINI_MDP_CKEN_CFG_RG, "vdec_mini_mdp_cken_cfg_rg", "vdec_sel", 0),
	/* VDEC2 */
	GATE_VDEC2(CLK_VDEC_LAT_CKEN, "vdec_lat_cken", "vdec_sel", 0),
	GATE_VDEC2(CLK_VDEC_LAT_ACTIVE, "vdec_lat_active", "vdec_sel", 4),
	GATE_VDEC2(CLK_VDEC_LAT_CKEN_ENG, "vdec_lat_cken_eng", "vdec_sel", 8),
	/* VDEC3 */
	GATE_VDEC3(CLK_VDEC_LARB1_CKEN, "vdec_larb1_cken", "vdec_sel", 0),
};

static const struct mtk_clk_desc vdec_desc = {
	.clks = vdec_clks,
	.num_clks = ARRAY_SIZE(vdec_clks),
};

static const struct of_device_id of_match_clk_mt8169_vdec[] = {
	{
		.compatible = "mediatek,mt8169-vdecsys",
		.data = &vdec_desc,
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_mt8169_vdec_drv = {
	.probe = mtk_clk_simple_probe,
	.driver = {
		.name = "clk-mt8169-vdec",
		.of_match_table = of_match_clk_mt8169_vdec,
	},
};

static int __init clk_mt8169_vdec_init(void)
{
	return platform_driver_register(&clk_mt8169_vdec_drv);
}

static void __exit clk_mt8169_vdec_exit(void)
{
	platform_driver_unregister(&clk_mt8169_vdec_drv);
}

module_init(clk_mt8169_vdec_init);
module_exit(clk_mt8169_vdec_exit);
MODULE_LICENSE("GPL");
