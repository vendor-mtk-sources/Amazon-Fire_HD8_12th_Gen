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

static const struct mtk_gate_regs audsys0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs audsys1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x4,
	.sta_ofs = 0x4,
};

static const struct mtk_gate_regs audsys2_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0x8,
	.sta_ofs = 0x8,
};

#define GATE_AUDSYS0(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &audsys0_cg_regs, _shift, &mtk_clk_gate_ops_no_setclr)

#define GATE_AUDSYS1(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &audsys1_cg_regs, _shift, &mtk_clk_gate_ops_no_setclr)

#define GATE_AUDSYS2(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &audsys2_cg_regs, _shift, &mtk_clk_gate_ops_no_setclr)

static const struct mtk_gate audsys_clks[] = {
	/* AUDSYS0 */
	GATE_AUDSYS0(CLK_AUDSYS_AFE, "aud_afe", "audio_sel", 2),
	GATE_AUDSYS0(CLK_AUDSYS_22M, "aud_22m", "aud_engen1_sel", 8),
	GATE_AUDSYS0(CLK_AUDSYS_24M, "aud_24m", "aud_engen2_sel", 9),
	GATE_AUDSYS0(CLK_AUDSYS_APLL2_TUNER, "aud_apll2_tuner", "aud_engen2_sel", 18),
	GATE_AUDSYS0(CLK_AUDSYS_APLL_TUNER, "aud_apll_tuner", "aud_engen1_sel", 19),
	GATE_AUDSYS0(CLK_AUDSYS_TDM, "aud_tdm_ck", "aud_1_sel", 20),
	GATE_AUDSYS0(CLK_AUDSYS_ADC, "aud_adc", "audio_sel", 24),
	GATE_AUDSYS0(CLK_AUDSYS_DAC, "aud_dac", "audio_sel", 25),
	GATE_AUDSYS0(CLK_AUDSYS_DAC_PREDIS, "aud_dac_predis", "audio_sel", 26),
	GATE_AUDSYS0(CLK_AUDSYS_TML, "aud_tml", "audio_sel", 27),
	GATE_AUDSYS0(CLK_AUDSYS_NLE, "aud_nle", "audio_sel", 28),
	/* AUDSYS1 */
	GATE_AUDSYS1(CLK_AUDSYS_I2S1_BCLK, "aud_i2s1_bclk", "audio_sel", 4),
	GATE_AUDSYS1(CLK_AUDSYS_I2S2_BCLK, "aud_i2s2_bclk", "audio_sel", 5),
	GATE_AUDSYS1(CLK_AUDSYS_I2S3_BCLK, "aud_i2s3_bclk", "audio_sel", 6),
	GATE_AUDSYS1(CLK_AUDSYS_I2S4_BCLK, "aud_i2s4_bclk", "audio_sel", 7),
	GATE_AUDSYS1(CLK_AUDSYS_CONNSYS_I2S_ASRC, "aud_connsys_i2s_asrc", "audio_sel", 12),
	GATE_AUDSYS1(CLK_AUDSYS_GENERAL1_ASRC, "aud_general1_asrc", "audio_sel", 13),
	GATE_AUDSYS1(CLK_AUDSYS_GENERAL2_ASRC, "aud_general2_asrc", "audio_sel", 14),
	GATE_AUDSYS1(CLK_AUDSYS_DAC_HIRES, "aud_dac_hires", "audio_h_sel", 15),
	GATE_AUDSYS1(CLK_AUDSYS_ADC_HIRES, "aud_adc_hires", "audio_h_sel", 16),
	GATE_AUDSYS1(CLK_AUDSYS_ADC_HIRES_TML, "aud_adc_hires_tml", "audio_h_sel", 17),
	GATE_AUDSYS1(CLK_AUDSYS_ADDA6_ADC, "aud_adda6_adc", "audio_sel", 20),
	GATE_AUDSYS1(CLK_AUDSYS_ADDA6_ADC_HIRES, "aud_adda6_adc_hires", "audio_h_sel", 21),
	GATE_AUDSYS1(CLK_AUDSYS_3RD_DAC, "aud_3rd_dac", "audio_sel", 28),
	GATE_AUDSYS1(CLK_AUDSYS_3RD_DAC_PREDIS, "aud_3rd_dac_predis", "audio_sel", 29),
	GATE_AUDSYS1(CLK_AUDSYS_3RD_DAC_TML, "aud_3rd_dac_tml", "audio_sel", 30),
	GATE_AUDSYS1(CLK_AUDSYS_3RD_DAC_HIRES, "aud_3rd_dac_hires", "audio_h_sel", 31),
	/* AUDSYS2 */
	GATE_AUDSYS2(CLK_AUDSYS_ETDM_IN1_BCLK, "aud_etdm_in1_bclk", "audio_sel", 23),
	GATE_AUDSYS2(CLK_AUDSYS_ETDM_OUT1_BCLK, "aud_etdm_out1_bclk", "audio_sel", 24),
};

static const struct mtk_clk_desc audsys_desc = {
	.clks = audsys_clks,
	.num_clks = ARRAY_SIZE(audsys_clks),
};

static const struct of_device_id of_match_clk_mt8169_audsys[] = {
	{
		.compatible = "mediatek,mt8169-audsys",
		.data = &audsys_desc,
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_mt8169_audsys_drv = {
	.probe = mtk_clk_simple_probe,
	.driver = {
		.name = "clk-mt8169-audsys",
		.of_match_table = of_match_clk_mt8169_audsys,
	},
};

static int __init clk_mt8169_audsys_init(void)
{
	return platform_driver_register(&clk_mt8169_audsys_drv);
}

static void __exit clk_mt8169_audsys_exit(void)
{
	platform_driver_unregister(&clk_mt8169_audsys_drv);
}

module_init(clk_mt8169_audsys_init);
module_exit(clk_mt8169_audsys_exit);
MODULE_LICENSE("GPL");
