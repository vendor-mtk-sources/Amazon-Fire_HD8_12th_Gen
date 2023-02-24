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

static const struct mtk_gate_regs mdp0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mdp2_cg_regs = {
	.set_ofs = 0x124,
	.clr_ofs = 0x128,
	.sta_ofs = 0x120,
};

#define GATE_MDP0(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &mdp0_cg_regs, _shift, &mtk_clk_gate_ops_setclr)

#define GATE_MDP2(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &mdp2_cg_regs, _shift, &mtk_clk_gate_ops_setclr)

static const struct mtk_gate mdp_clks[] = {
	/* MDP0 */
	GATE_MDP0(CLK_MDP_RDMA0, "mdp_rdma0", "mdp_sel", 0),
	GATE_MDP0(CLK_MDP_TDSHP0, "mdp_tdshp0", "mdp_sel", 1),
	GATE_MDP0(CLK_MDP_IMG_DL_ASYNC0, "mdp_img_dl_async0", "mdp_sel", 2),
	GATE_MDP0(CLK_MDP_IMG_DL_ASYNC1, "mdp_img_dl_async1", "mdp_sel", 3),
	GATE_MDP0(CLK_MDP_DISP_RDMA, "mdp_disp_rdma", "mdp_sel", 4),
	GATE_MDP0(CLK_MDP_HMS, "mdp_hms", "mdp_sel", 5),
	GATE_MDP0(CLK_MDP_SMI0, "mdp_smi0", "mdp_sel", 6),
	GATE_MDP0(CLK_MDP_APB_BUS, "mdp_apb_bus", "mdp_sel", 7),
	GATE_MDP0(CLK_MDP_WROT0, "mdp_wrot0", "mdp_sel", 8),
	GATE_MDP0(CLK_MDP_RSZ0, "mdp_rsz0", "mdp_sel", 9),
	GATE_MDP0(CLK_MDP_HDR0, "mdp_hdr0", "mdp_sel", 10),
	GATE_MDP0(CLK_MDP_MUTEX0, "mdp_mutex0", "mdp_sel", 11),
	GATE_MDP0(CLK_MDP_WROT1, "mdp_wrot1", "mdp_sel", 12),
	GATE_MDP0(CLK_MDP_RSZ1, "mdp_rsz1", "mdp_sel", 13),
	GATE_MDP0(CLK_MDP_FAKE_ENG0, "mdp_fake_eng0", "mdp_sel", 14),
	GATE_MDP0(CLK_MDP_AAL0, "mdp_aal0", "mdp_sel", 15),
	GATE_MDP0(CLK_MDP_DISP_WDMA, "mdp_disp_wdma", "mdp_sel", 16),
	GATE_MDP0(CLK_MDP_COLOR, "mdp_color", "mdp_sel", 17),
	GATE_MDP0(CLK_MDP_IMG_DL_ASYNC2, "mdp_img_dl_async2", "mdp_sel", 18),
	/* MDP2 */
	GATE_MDP2(CLK_MDP_IMG_DL_RELAY0_ASYNC0, "mdp_img_dl_rel0_as0", "mdp_sel", 0),
	GATE_MDP2(CLK_MDP_IMG_DL_RELAY1_ASYNC1, "mdp_img_dl_rel1_as1", "mdp_sel", 8),
	GATE_MDP2(CLK_MDP_IMG_DL_RELAY2_ASYNC2, "mdp_img_dl_rel2_as2", "mdp_sel", 24),
};

static const struct mtk_clk_desc mdp_desc = {
	.clks = mdp_clks,
	.num_clks = ARRAY_SIZE(mdp_clks),
};

static const struct of_device_id of_match_clk_mt8169_mdp[] = {
	{
		.compatible = "mediatek,mt8169-mdpsys",
		.data = &mdp_desc,
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_mt8169_mdp_drv = {
	.probe = mtk_clk_simple_probe,
	.driver = {
		.name = "clk-mt8169-mdp",
		.of_match_table = of_match_clk_mt8169_mdp,
	},
};

static int __init clk_mt8169_mdp_init(void)
{
	return platform_driver_register(&clk_mt8169_mdp_drv);
}

static void __exit clk_mt8169_mdp_exit(void)
{
	platform_driver_unregister(&clk_mt8169_mdp_drv);
}

module_init(clk_mt8169_mdp_init);
module_exit(clk_mt8169_mdp_exit);
MODULE_LICENSE("GPL");
