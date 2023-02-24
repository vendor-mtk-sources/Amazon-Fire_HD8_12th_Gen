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

static const struct mtk_gate_regs cam_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &cam_cg_regs, _shift, &mtk_clk_gate_ops_setclr)

static const struct mtk_gate cam_clks[] = {
	GATE_CAM(CLK_CAM_LARB13, "cam_larb13", "cam_sel", 0),
	GATE_CAM(CLK_CAM_DFP_VAD, "cam_dfp_vad", "cam_sel", 1),
	GATE_CAM(CLK_CAM_LARB14, "cam_larb14", "cam_sel", 2),
	GATE_CAM(CLK_CAM, "cam", "cam_sel", 6),
	GATE_CAM(CLK_CAMTG, "camtg", "cam_sel", 7),
	GATE_CAM(CLK_CAM_SENINF, "cam_seninf", "cam_sel", 8),
	GATE_CAM(CLK_CAMSV1, "camsv1", "cam_sel", 10),
	GATE_CAM(CLK_CAMSV2, "camsv2", "cam_sel", 11),
	GATE_CAM(CLK_CAMSV3, "camsv3", "cam_sel", 12),
	GATE_CAM(CLK_CAM_CCU0, "cam_ccu0", "cam_sel", 13),
	GATE_CAM(CLK_CAM_CCU1, "cam_ccu1", "cam_sel", 14),
	GATE_CAM(CLK_CAM_MRAW0, "cam_mraw0", "cam_sel", 15),
	GATE_CAM(CLK_CAM_FAKE_ENG, "cam_fake_eng", "cam_sel", 17),
	GATE_CAM(CLK_CAM_CCU_GALS, "cam_ccu_gals", "cam_sel", 18),
	GATE_CAM(CLK_CAM2MM_GALS, "cam2mm_gals", "cam_sel", 19),
};

static const struct mtk_gate cam_rawa_clks[] = {
	GATE_CAM(CLK_CAM_RAWA_LARBX_RAWA, "cam_rawa_larbx_rawa", "cam_sel", 0),
	GATE_CAM(CLK_CAM_RAWA, "cam_rawa", "cam_sel", 1),
	GATE_CAM(CLK_CAM_RAWA_CAMTG_RAWA, "cam_rawa_camtg_rawa", "cam_sel", 2),
};

static const struct mtk_gate cam_rawb_clks[] = {
	GATE_CAM(CLK_CAM_RAWB_LARBX_RAWB, "cam_rawb_larbx_rawb", "cam_sel", 0),
	GATE_CAM(CLK_CAM_RAWB, "cam_rawb", "cam_sel", 1),
	GATE_CAM(CLK_CAM_RAWB_CAMTG_RAWB, "cam_rawb_camtg_rawb", "cam_sel", 2),
};

static const struct mtk_clk_desc cam_desc = {
	.clks = cam_clks,
	.num_clks = ARRAY_SIZE(cam_clks),
};

static const struct mtk_clk_desc cam_rawa_desc = {
	.clks = cam_rawa_clks,
	.num_clks = ARRAY_SIZE(cam_rawa_clks),
};

static const struct mtk_clk_desc cam_rawb_desc = {
	.clks = cam_rawb_clks,
	.num_clks = ARRAY_SIZE(cam_rawb_clks),
};

static const struct of_device_id of_match_clk_mt8169_cam[] = {
	{
		.compatible = "mediatek,mt8169-camsys",
		.data = &cam_desc,
	}, {
		.compatible = "mediatek,mt8169-camsys_rawa",
		.data = &cam_rawa_desc,
	}, {
		.compatible = "mediatek,mt8169-camsys_rawb",
		.data = &cam_rawb_desc,
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_mt8169_cam_drv = {
	.probe = mtk_clk_simple_probe,
	.driver = {
		.name = "clk-mt8169-cam",
		.of_match_table = of_match_clk_mt8169_cam,
	},
};

static int __init clk_mt8169_cam_init(void)
{
	return platform_driver_register(&clk_mt8169_cam_drv);
}

static void __exit clk_mt8169_cam_exit(void)
{
	platform_driver_unregister(&clk_mt8169_cam_drv);
}

module_init(clk_mt8169_cam_init);
module_exit(clk_mt8169_cam_exit);
MODULE_LICENSE("GPL");
