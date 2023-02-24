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

static const struct mtk_gate_regs mm0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mm1_cg_regs = {
	.set_ofs = 0x1a4,
	.clr_ofs = 0x1a8,
	.sta_ofs = 0x1a0,
};

#define GATE_MM0(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &mm0_cg_regs, _shift, &mtk_clk_gate_ops_setclr)

#define GATE_MM1(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &mm1_cg_regs, _shift, &mtk_clk_gate_ops_setclr)

static const struct mtk_gate mm_clks[] = {
	/* MM0 */
	GATE_MM0(CLK_MM_DISP_MUTEX0, "mm_disp_mutex0", "disp_sel", 0),
	GATE_MM0(CLK_MM_APB_MM_BUS, "mm_apb_mm_bus", "disp_sel", 1),
	GATE_MM0(CLK_MM_DISP_OVL0, "mm_disp_ovl0", "disp_sel", 2),
	GATE_MM0(CLK_MM_DISP_RDMA0, "mm_disp_rdma0", "disp_sel", 3),
	GATE_MM0(CLK_MM_DISP_OVL0_2L, "mm_disp_ovl0_2l", "disp_sel", 4),
	GATE_MM0(CLK_MM_DISP_WDMA0, "mm_disp_wdma0", "disp_sel", 5),
	GATE_MM0(CLK_MM_DISP_RSZ0, "mm_disp_rsz0", "disp_sel", 7),
	GATE_MM0(CLK_MM_DISP_AAL0, "mm_disp_aal0", "disp_sel", 8),
	GATE_MM0(CLK_MM_DISP_CCORR0, "mm_disp_ccorr0", "disp_sel", 9),
	GATE_MM0(CLK_MM_DISP_COLOR0, "mm_disp_color0", "disp_sel", 10),
	GATE_MM0(CLK_MM_SMI_INFRA, "mm_smi_infra", "disp_sel", 11),
	GATE_MM0(CLK_MM_DISP_DSC_WRAP0, "mm_disp_dsc_wrap0", "disp_sel", 12),
	GATE_MM0(CLK_MM_DISP_GAMMA0, "mm_disp_gamma0", "disp_sel", 13),
	GATE_MM0(CLK_MM_DISP_POSTMASK0, "mm_disp_postmask0", "disp_sel", 14),
	GATE_MM0(CLK_MM_DISP_DITHER0, "mm_disp_dither0", "disp_sel", 16),
	GATE_MM0(CLK_MM_SMI_COMMON, "mm_smi_common", "disp_sel", 17),
	GATE_MM0(CLK_MM_DSI0, "mm_dsi0", "disp_sel", 19),
	GATE_MM0(CLK_MM_DISP_FAKE_ENG0, "mm_disp_fake_eng0", "disp_sel", 20),
	GATE_MM0(CLK_MM_DISP_FAKE_ENG1, "mm_disp_fake_eng1", "disp_sel", 21),
	GATE_MM0(CLK_MM_SMI_GALS, "mm_smi_gals", "disp_sel", 22),
	GATE_MM0(CLK_MM_SMI_IOMMU, "mm_smi_iommu", "disp_sel", 24),
	/* MM1 */
	GATE_MM1(CLK_MM_DSI0_DSI_CK_DOMAIN, "mm_dsi0_dsi_domain", "disp_sel", 0),
	GATE_MM1(CLK_MM_DISP_26M, "mm_disp_26m_ck", "disp_sel", 10),
};

static const struct mtk_clk_desc mm_desc = {
	.clks = mm_clks,
	.num_clks = ARRAY_SIZE(mm_clks),
};

static const struct of_device_id of_match_clk_mt8169_mm[] = {
	{
		.compatible = "mediatek,mt8169-mmsys",
		.data = &mm_desc,
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_mt8169_mm_drv = {
	.probe = mtk_clk_simple_probe,
	.driver = {
		.name = "clk-mt8169-mm",
		.of_match_table = of_match_clk_mt8169_mm,
	},
};

static int __init clk_mt8169_mm_init(void)
{
	return platform_driver_register(&clk_mt8169_mm_drv);
}

static void __exit clk_mt8169_mm_exit(void)
{
	platform_driver_unregister(&clk_mt8169_mm_drv);
}

module_init(clk_mt8169_mm_init);
module_exit(clk_mt8169_mm_exit);
MODULE_LICENSE("GPL");
