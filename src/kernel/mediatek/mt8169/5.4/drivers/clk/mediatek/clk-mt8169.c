// SPDX-License-Identifier: GPL-2.0-only
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Chun-Jie Chen <chun-jie.chen@mediatek.com>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-mtk.h"
#include "clk-mux.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt8169-clk.h>

static DEFINE_SPINLOCK(mt8169_clk_lock);

static const struct mtk_fixed_clk top_fixed_clks[] = {
	FIXED_CLK(CLK_TOP_ULPOSC1, "ulposc1_ck", NULL, 250000000),
	FIXED_CLK(CLK_TOP_466M_FMEM, "hd_466m_fmem_ck", NULL, 466000000),
	FIXED_CLK(CLK_TOP_MPLL, "mpll_ck", NULL, 208000000),
};

static const struct mtk_fixed_factor top_divs[] = {
	FACTOR(CLK_TOP_ARMPLL_LL, "armpll_ll_ck", "armpll_ll", 1, 1),
	FACTOR(CLK_TOP_ARMPLL_BL, "armpll_bl_ck", "armpll_bl", 1, 1),
	FACTOR(CLK_TOP_CCIPLL, "ccipll_ck", "ccipll", 1, 1),
	FACTOR(CLK_TOP_MFGPLL, "mfgpll_ck", "mfgpll", 1, 1),
	FACTOR(CLK_TOP_MAINPLL, "mainpll_ck", "mainpll", 1, 1),
	FACTOR(CLK_TOP_MAINPLL_D2, "mainpll_d2", "mainpll_ck", 1, 2),
	FACTOR(CLK_TOP_MAINPLL_D2_D2, "mainpll_d2_d2", "mainpll_d2", 1, 2),
	FACTOR(CLK_TOP_MAINPLL_D2_D4, "mainpll_d2_d4", "mainpll_d2", 1, 4),
	FACTOR(CLK_TOP_MAINPLL_D2_D16, "mainpll_d2_d16", "mainpll_d2", 1, 16),
	FACTOR(CLK_TOP_MAINPLL_D3, "mainpll_d3", "mainpll_ck", 1, 3),
	FACTOR(CLK_TOP_MAINPLL_D3_D2, "mainpll_d3_d2", "mainpll_d3", 1, 2),
	FACTOR(CLK_TOP_MAINPLL_D3_D4, "mainpll_d3_d4", "mainpll_d3", 1, 4),
	FACTOR(CLK_TOP_MAINPLL_D5, "mainpll_d5", "mainpll_ck", 1, 5),
	FACTOR(CLK_TOP_MAINPLL_D5_D2, "mainpll_d5_d2", "mainpll_d5", 1, 2),
	FACTOR(CLK_TOP_MAINPLL_D5_D4, "mainpll_d5_d4", "mainpll_d5", 1, 4),
	FACTOR(CLK_TOP_MAINPLL_D7, "mainpll_d7", "mainpll_ck", 1, 7),
	FACTOR(CLK_TOP_MAINPLL_D7_D2, "mainpll_d7_d2", "mainpll_d7", 1, 2),
	FACTOR(CLK_TOP_MAINPLL_D7_D4, "mainpll_d7_d4", "mainpll_d7", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL, "univpll_ck", "univ2pll", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL_D2, "univpll_d2", "univpll_ck", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL_D2_D2, "univpll_d2_d2", "univpll_d2", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL_D2_D4, "univpll_d2_d4", "univpll_d2", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL_D3, "univpll_d3", "univpll_ck", 1, 3),
	FACTOR(CLK_TOP_UNIVPLL_D3_D2, "univpll_d3_d2", "univpll_d3", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL_D3_D4, "univpll_d3_d4", "univpll_d3", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL_D3_D8, "univpll_d3_d8", "univpll_d3", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL_D3_D32, "univpll_d3_d32", "univpll_d3", 1, 32),
	FACTOR(CLK_TOP_UNIVPLL_D5, "univpll_d5", "univpll_ck", 1, 5),
	FACTOR(CLK_TOP_UNIVPLL_D5_D2, "univpll_d5_d2", "univpll_d5", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL_D5_D4, "univpll_d5_d4", "univpll_d5", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL_D7, "univpll_d7", "univpll_ck", 1, 7),
	FACTOR(CLK_TOP_UNIVPLL_192M, "univpll_192m_ck", "univ2pll", 1, 13),
	FACTOR(CLK_TOP_UNIVPLL_192M_D4, "univpll_192m_d4", "univpll_192m_ck", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL_192M_D8, "univpll_192m_d8", "univpll_192m_ck", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL_192M_D16, "univpll_192m_d16", "univpll_192m_ck", 1, 16),
	FACTOR(CLK_TOP_UNIVPLL_192M_D32, "univpll_192m_d32", "univpll_192m_ck", 1, 32),
	FACTOR(CLK_TOP_APLL1, "apll1_ck", "apll1", 1, 1),
	FACTOR(CLK_TOP_APLL1_D2, "apll1_d2", "apll1_ck", 1, 2),
	FACTOR(CLK_TOP_APLL1_D4, "apll1_d4", "apll1_ck", 1, 4),
	FACTOR(CLK_TOP_APLL1_D8, "apll1_d8", "apll1_ck", 1, 8),
	FACTOR(CLK_TOP_APLL2, "apll2_ck", "apll2", 1, 1),
	FACTOR(CLK_TOP_APLL2_D2, "apll2_d2", "apll2_ck", 1, 2),
	FACTOR(CLK_TOP_APLL2_D4, "apll2_d4", "apll2_ck", 1, 4),
	FACTOR(CLK_TOP_APLL2_D8, "apll2_d8", "apll2_ck", 1, 8),
	FACTOR(CLK_TOP_MMPLL, "mmpll_ck", "mmpll", 1, 1),
	FACTOR(CLK_TOP_MMPLL_D2, "mmpll_d2", "mmpll_ck", 1, 2),
	FACTOR(CLK_TOP_TVDPLL, "tvdpll_ck", "tvdpll", 1, 1),
	FACTOR(CLK_TOP_TVDPLL_D2, "tvdpll_d2", "tvdpll_ck", 1, 2),
	FACTOR(CLK_TOP_TVDPLL_D4, "tvdpll_d4", "tvdpll_ck", 1, 4),
	FACTOR(CLK_TOP_TVDPLL_D8, "tvdpll_d8", "tvdpll_ck", 1, 8),
	FACTOR(CLK_TOP_TVDPLL_D16, "tvdpll_d16", "tvdpll_ck", 1, 16),
	FACTOR(CLK_TOP_TVDPLL_D32, "tvdpll_d32", "tvdpll_ck", 1, 32),
	FACTOR(CLK_TOP_MSDCPLL, "msdcpll_ck", "msdcpll", 1, 1),
	FACTOR(CLK_TOP_MSDCPLL_D2, "msdcpll_d2", "msdcpll_ck", 1, 2),
	FACTOR(CLK_TOP_ULPOSC1_D2, "ulposc1_d2", "ulposc1_ck", 1, 2),
	FACTOR(CLK_TOP_ULPOSC1_D4, "ulposc1_d4", "ulposc1_ck", 1, 4),
	FACTOR(CLK_TOP_ULPOSC1_D8, "ulposc1_d8", "ulposc1_ck", 1, 8),
	FACTOR(CLK_TOP_ULPOSC1_D10, "ulposc1_d10", "ulposc1_ck", 1, 10),
	FACTOR(CLK_TOP_ULPOSC1_D16, "ulposc1_d16", "ulposc1_ck", 1, 16),
	FACTOR(CLK_TOP_ULPOSC1_D32, "ulposc1_d32", "ulposc1_ck", 1, 32),
	FACTOR(CLK_TOP_ADSPPLL, "adsppll_ck", "adsppll", 1, 1),
	FACTOR(CLK_TOP_ADSPPLL_D2, "adsppll_d2", "adsppll_ck", 1, 2),
	FACTOR(CLK_TOP_ADSPPLL_D4, "adsppll_d4", "adsppll_ck", 1, 4),
	FACTOR(CLK_TOP_ADSPPLL_D8, "adsppll_d8", "adsppll_ck", 1, 8),
	FACTOR(CLK_TOP_NNAPLL, "nnapll_ck", "nnapll", 1, 1),
	FACTOR(CLK_TOP_NNAPLL_D2, "nnapll_d2", "nnapll_ck", 1, 2),
	FACTOR(CLK_TOP_NNAPLL_D4, "nnapll_d4", "nnapll_ck", 1, 4),
	FACTOR(CLK_TOP_NNAPLL_D8, "nnapll_d8", "nnapll_ck", 1, 8),
	FACTOR(CLK_TOP_NNA2PLL, "nna2pll_ck", "nna2pll", 1, 1),
	FACTOR(CLK_TOP_NNA2PLL_D2, "nna2pll_d2", "nna2pll_ck", 1, 2),
	FACTOR(CLK_TOP_NNA2PLL_D4, "nna2pll_d4", "nna2pll_ck", 1, 4),
	FACTOR(CLK_TOP_NNA2PLL_D8, "nna2pll_d8", "nna2pll_ck", 1, 8),
	FACTOR(CLK_TOP_F_BIST2FPC, "f_bist2fpc_ck", "univpll_d3_d2", 1, 1),
};

static const char * const mcu_armpll_ll_parents[] = {
	"clk26m",
	"armpll_ll_ck",
	"mainpll_ck",
	"univpll_d2"
};

static const char * const mcu_armpll_bl_parents[] = {
	"clk26m",
	"armpll_bl_ck",
	"mainpll_ck",
	"univpll_d2"
};

static const char * const mcu_armpll_bus_parents[] = {
	"clk26m",
	"ccipll_ck",
	"mainpll_ck",
	"univpll_d2"
};



static struct mtk_composite mcu_muxes[] = {
	/* CPU_PLLDIV_CFG0 */
	MUX(CLK_MCU_ARMPLL_LL_SEL, "mcu_armpll_ll_sel", mcu_armpll_ll_parents, 0xA2A0, 9, 2),
	/* CPU_PLLDIV_CFG1 */
	MUX(CLK_MCU_ARMPLL_BL_SEL, "mcu_armpll_bl_sel", mcu_armpll_bl_parents, 0xA2A4, 9, 2),
	/* BUS_PLLDIV_CFG */
	MUX(CLK_MCU_ARMPLL_BUS_SEL, "mcu_armpll_bus_sel", mcu_armpll_bus_parents, 0xA2E0, 9, 2),
};

static const char * const axi_parents[] = {
	"clk26m",
	"mainpll_d7",
	"mainpll_d2_d4",
	"univpll_d7"
};

static const char * const scp_parents[] = {
	"clk26m",
	"mainpll_d2_d4",
	"mainpll_d5",
	"mainpll_d2_d2",
	"mainpll_d3",
	"univpll_d3"
};

static const char * const mfg_parents[] = {
	"clk26m",
	"mfgpll_ck",
	"mainpll_d3",
	"mainpll_d5"
};

static const char * const camtg_parents[] = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d3_d8",
	"univpll_192m_d4",
	"univpll_d3_d32",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg1_parents[] = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d3_d8",
	"univpll_192m_d4",
	"univpll_d3_d32",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg2_parents[] = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d3_d8",
	"univpll_192m_d4",
	"univpll_d3_d32",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg3_parents[] = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d3_d8",
	"univpll_192m_d4",
	"univpll_d3_d32",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg4_parents[] = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d3_d8",
	"univpll_192m_d4",
	"univpll_d3_d32",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg5_parents[] = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d3_d8",
	"univpll_192m_d4",
	"univpll_d3_d32",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg6_parents[] = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d3_d8",
	"univpll_192m_d4",
	"univpll_d3_d32",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const uart_parents[] = {
	"clk26m",
	"univpll_d3_d8"
};

static const char * const spi_parents[] = {
	"clk26m",
	"mainpll_d5_d4",
	"mainpll_d3_d4",
	"mainpll_d5_d2",
	"mainpll_d2_d4",
	"mainpll_d7",
	"mainpll_d3_d2",
	"mainpll_d5"
};

static const char * const msdc5hclk_parents[] = {
	"clk26m",
	"mainpll_d2_d2",
	"mainpll_d7",
	"mainpll_d3_d2"
};

static const char * const msdc50_0_parents[] = {
	"clk26m",
	"msdcpll_ck",
	"univpll_d3",
	"msdcpll_d2",
	"mainpll_d7",
	"mainpll_d3_d2",
	"univpll_d2_d2"
};

static const char * const msdc30_1_parents[] = {
	"clk26m",
	"msdcpll_d2",
	"univpll_d3_d2",
	"mainpll_d3_d2",
	"mainpll_d7"
};

static const char * const audio_parents[] = {
	"clk26m",
	"mainpll_d5_d4",
	"mainpll_d7_d4",
	"mainpll_d2_d16"
};

static const char * const aud_intbus_parents[] = {
	"clk26m",
	"mainpll_d2_d4",
	"mainpll_d7_d2"
};

static const char * const aud_1_parents[] = {
	"clk26m",
	"apll1_ck"
};

static const char * const aud_2_parents[] = {
	"clk26m",
	"apll2_ck"
};

static const char * const aud_engen1_parents[] = {
	"clk26m",
	"apll1_d2",
	"apll1_d4",
	"apll1_d8"
};

static const char * const aud_engen2_parents[] = {
	"clk26m",
	"apll2_d2",
	"apll2_d4",
	"apll2_d8"
};

static const char * const disp_pwm_parents[] = {
	"clk26m",
	"univpll_d5_d2",
	"univpll_d3_d4",
	"ulposc1_d2",
	"ulposc1_d8"
};

static const char * const sspm_parents[] = {
	"clk26m",
	"mainpll_d2_d2",
	"mainpll_d3_d2",
	"mainpll_d5",
	"mainpll_d3"
};

static const char * const dxcc_parents[] = {
	"clk26m",
	"mainpll_d2_d2",
	"mainpll_d2_d4"
};

static const char * const usb_parents[] = {
	"clk26m",
	"univpll_d5_d4",
	"univpll_d5_d2"
};

static const char * const srck_parents[] = {
	"clk32k",
	"clk26m",
	"ulposc1_d10"
};

static const char * const spm_parents[] = {
	"clk32k",
	"ulposc1_d10",
	"clk26m",
	"mainpll_d7_d2"
};

static const char * const i2c_parents[] = {
	"clk26m",
	"univpll_d5_d4",
	"univpll_d3_d4",
	"univpll_d5_d2"
};

static const char * const pwm_parents[] = {
	"clk26m",
	"univpll_d3_d8",
	"univpll_d3_d4",
	"univpll_d2_d4"
};

static const char * const seninf_parents[] = {
	"clk26m",
	"univpll_d2_d4",
	"univpll_d2_d2",
	"univpll_d3_d2"
};

static const char * const seninf1_parents[] = {
	"clk26m",
	"univpll_d2_d4",
	"univpll_d2_d2",
	"univpll_d3_d2"
};

static const char * const seninf2_parents[] = {
	"clk26m",
	"univpll_d2_d4",
	"univpll_d2_d2",
	"univpll_d3_d2"
};

static const char * const seninf3_parents[] = {
	"clk26m",
	"univpll_d2_d4",
	"univpll_d2_d2",
	"univpll_d3_d2"
};

static const char * const aes_msdcfde_parents[] = {
	"clk26m",
	"univpll_d3",
	"mainpll_d3",
	"univpll_d2_d2",
	"mainpll_d2_d2",
	"mainpll_d2_d4"
};

static const char * const pwrap_ulposc_parents[] = {
	"clk26m",
	"univpll_d5_d4",
	"ulposc1_d4",
	"ulposc1_d8",
	"ulposc1_d10",
	"ulposc1_d16",
	"ulposc1_d32"
};

static const char * const camtm_parents[] = {
	"clk26m",
	"univpll_d2_d4",
	"univpll_d3_d2"
};

static const char * const venc_parents[] = {
	"clk26m",
	"mmpll_ck",
	"mainpll_d2_d2",
	"mainpll_d2",
	"univpll_d3",
	"univpll_d2_d2",
	"mainpll_d3",
	"mmpll_ck"
};

static const char * const cam_parents[] = {
	"clk26m",
	"mainpll_d2",
	"mainpll_d2_d2",
	"univpll_d3",
	"mainpll_d3",
	"mmpll_ck",
	"univpll_d5",
	"univpll_d2_d2",
	"mmpll_d2"
};

static const char * const img1_parents[] = {
	"clk26m",
	"mainpll_d2",
	"mainpll_d2_d2",
	"univpll_d3",
	"mainpll_d3",
	"mmpll_ck",
	"univpll_d5",
	"univpll_d2_d2",
	"mmpll_d2"
};

static const char * const ipe_parents[] = {
	"clk26m",
	"mainpll_d2",
	"mainpll_d2_d2",
	"univpll_d3",
	"mainpll_d3",
	"mmpll_ck",
	"univpll_d5",
	"univpll_d2_d2",
	"mmpll_d2"
};

static const char * const dpmaif_parents[] = {
	"clk26m",
	"univpll_d2_d2",
	"mainpll_d3",
	"mainpll_d2_d2",
	"univpll_d3_d2"
};

static const char * const vdec_parents[] = {
	"clk26m",
	"mainpll_d3",
	"mainpll_d2_d2",
	"univpll_d5",
	"mainpll_d2",
	"univpll_d3",
	"univpll_d2_d2"
};

static const char * const disp_parents[] = {
	"clk26m",
	"univpll_d3_d2",
	"mainpll_d5",
	"univpll_d5",
	"univpll_d2_d2",
	"mainpll_d3",
	"univpll_d3",
	"mainpll_d2",
	"mmpll_ck"
};

static const char * const mdp_parents[] = {
	"clk26m",
	"mainpll_d5",
	"univpll_d5",
	"mainpll_d2_d2",
	"univpll_d2_d2",
	"mainpll_d3",
	"univpll_d3",
	"mainpll_d2",
	"mmpll_ck"
};

static const char * const audio_h_parents[] = {
	"clk26m",
	"univpll_d7",
	"apll1_ck",
	"apll2_ck"
};

static const char * const ufs_parents[] = {
	"clk26m",
	"mainpll_d7",
	"univpll_d2_d4",
	"mainpll_d2_d4"
};

static const char * const aes_fde_parents[] = {
	"clk26m",
	"univpll_d3",
	"mainpll_d2_d2",
	"univpll_d5"
};

static const char * const audiodsp_parents[] = {
	"clk26m",
	"ulposc1_d10",
	"adsppll_ck",
	"adsppll_d2",
	"adsppll_d4",
	"adsppll_d8"
};

static const char * const dsi_occ_parents[] = {
	"clk26m",
	"univpll_d3_d2",
	"mpll_ck",
	"mainpll_d5"
};

static const char * const spmi_mst_parents[] = {
	"clk26m",
	"univpll_d5_d4",
	"ulposc1_d4",
	"ulposc1_d8",
	"ulposc1_d10",
	"ulposc1_d16",
	"ulposc1_d32"
};

static const char * const spinor_parents[] = {
	"clk26m",
	"clk13m",
	"mainpll_d7_d4",
	"univpll_d3_d8",
	"univpll_d5_d4",
	"mainpll_d7_d2"
};

static const char * const nna_parents[] = {
	"clk26m",
	"univpll_d3_d8",
	"mainpll_d2_d4",
	"univpll_d3_d2",
	"mainpll_d2_d2",
	"univpll_d2_d2",
	"mainpll_d3",
	"univpll_d3",
	"mmpll_ck",
	"mainpll_d2",
	"univpll_d2",
	"nnapll_d2",
	"nnapll_d4",
	"nnapll_d8",
	"nnapll_ck",
	"nna2pll_ck"
};

static const char * const nna1_parents[] = {
	"clk26m",
	"univpll_d3_d8",
	"mainpll_d2_d4",
	"univpll_d3_d2",
	"mainpll_d2_d2",
	"univpll_d2_d2",
	"mainpll_d3",
	"univpll_d3",
	"mmpll_ck",
	"mainpll_d2",
	"univpll_d2",
	"nnapll_d2",
	"nnapll_d4",
	"nnapll_d8",
	"nnapll_ck",
	"nna2pll_ck"
};

static const char * const nna2_parents[] = {
	"clk26m",
	"univpll_d3_d8",
	"mainpll_d2_d4",
	"univpll_d3_d2",
	"mainpll_d2_d2",
	"univpll_d2_d2",
	"mainpll_d3",
	"univpll_d3",
	"mmpll_ck",
	"mainpll_d2",
	"univpll_d2",
	"nna2pll_d2",
	"nna2pll_d4",
	"nna2pll_d8",
	"nnapll_ck",
	"nna2pll_ck"
};

static const char * const ssusb_xhci_parents[] = {
	"clk26m",
	"univpll_d5_d4",
	"univpll_d5_d2"
};

static const char * const ssusb_1p_parents[] = {
	"clk26m",
	"univpll_d5_d4",
	"univpll_d5_d2"
};

static const char * const ssusb_xhci_1p_parents[] = {
	"clk26m",
	"univpll_d5_d4",
	"univpll_d5_d2"
};

static const char * const wpe_parents[] = {
	"clk26m",
	"univpll_d3_d2",
	"mainpll_d5",
	"univpll_d5",
	"univpll_d2_d2",
	"mainpll_d3",
	"univpll_d3",
	"mainpll_d2",
	"mmpll_ck"
};

static const char * const dpi_parents[] = {
	"clk26m",
	"tvdpll_ck",
	"tvdpll_d2",
	"tvdpll_d4",
	"tvdpll_d8",
	"tvdpll_d16",
	"tvdpll_d32"
};

static const char * const u3_occ_250m_parents[] = {
	"clk26m",
	"univpll_d5"
};

static const char * const u3_occ_500m_parents[] = {
	"clk26m",
	"nna2pll_d2"
};

static const char * const adsp_bus_parents[] = {
	"clk26m",
	"ulposc1_d2",
	"mainpll_d5",
	"mainpll_d2_d2",
	"mainpll_d3",
	"mainpll_d2",
	"univpll_d3"
};

static const char * const apll_i2s0_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2s1_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2s2_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2s4_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_tdmout_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const struct mtk_mux top_mtk_muxes[] = {
	/* CLK_CFG_0 */
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_AXI_SEL, "axi_sel",
		axi_parents, 0x0040, 0x0044, 0x0048, 0, 2, 7, 0x0004, 0, CLK_IS_CRITICAL),
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_SCP_SEL, "scp_sel",
		scp_parents, 0x0040, 0x0044, 0x0048, 8, 3, 15, 0x0004, 1, CLK_IS_CRITICAL),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MFG_SEL, "mfg_sel",
		mfg_parents, 0x0040, 0x0044, 0x0048, 16, 2, 23, 0x0004, 2),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG_SEL, "camtg_sel",
		camtg_parents, 0x0040, 0x0044, 0x0048, 24, 3, 31, 0x0004, 3),
	/* CLK_CFG_1 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG1_SEL, "camtg1_sel",
		camtg1_parents, 0x0050, 0x0054, 0x0058, 0, 3, 7, 0x0004, 4),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG2_SEL, "camtg2_sel",
		camtg2_parents, 0x0050, 0x0054, 0x0058, 8, 3, 15, 0x0004, 5),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG3_SEL, "camtg3_sel",
		camtg3_parents, 0x0050, 0x0054, 0x0058, 16, 3, 23, 0x0004, 6),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG4_SEL, "camtg4_sel",
		camtg4_parents, 0x0050, 0x0054, 0x0058, 24, 3, 31, 0x0004, 7),
	/* CLK_CFG_2 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG5_SEL, "camtg5_sel",
		camtg5_parents, 0x0060, 0x0064, 0x0068, 0, 3, 7, 0x0004, 8),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG6_SEL, "camtg6_sel",
		camtg6_parents, 0x0060, 0x0064, 0x0068, 8, 3, 15, 0x0004, 9),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UART_SEL, "uart_sel",
		uart_parents, 0x0060, 0x0064, 0x0068, 16, 1, 23, 0x0004, 10),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI_SEL, "spi_sel",
		spi_parents, 0x0060, 0x0064, 0x0068, 24, 3, 31, 0x0004, 11),
	/* CLK_CFG_3 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_0_HCLK_SEL, "msdc5hclk_sel",
		msdc5hclk_parents, 0x0070, 0x0074, 0x0078, 0, 2, 7, 0x0004, 12),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_0_SEL, "msdc50_0_sel",
		msdc50_0_parents, 0x0070, 0x0074, 0x0078, 8, 3, 15, 0x0004, 13),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL, "msdc30_1_sel",
		msdc30_1_parents, 0x0070, 0x0074, 0x0078, 16, 3, 23, 0x0004, 14),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUDIO_SEL, "audio_sel",
		audio_parents, 0x0070, 0x0074, 0x0078, 24, 2, 31, 0x0004, 15),
	/* CLK_CFG_4 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL, "aud_intbus_sel",
		aud_intbus_parents, 0x0080, 0x0084, 0x0088, 0, 2, 7, 0x0004, 16),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_1_SEL, "aud_1_sel",
		aud_1_parents, 0x0080, 0x0084, 0x0088, 8, 1, 15, 0x0004, 17),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_2_SEL, "aud_2_sel",
		aud_2_parents, 0x0080, 0x0084, 0x0088, 16, 1, 23, 0x0004, 18),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_ENGEN1_SEL, "aud_engen1_sel",
		aud_engen1_parents, 0x0080, 0x0084, 0x0088, 24, 2, 31, 0x0004, 19),
	/* CLK_CFG_5 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_ENGEN2_SEL, "aud_engen2_sel",
		aud_engen2_parents, 0x0090, 0x0094, 0x0098, 0, 2, 7, 0x0004, 20),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DISP_PWM_SEL, "disp_pwm_sel",
		disp_pwm_parents, 0x0090, 0x0094, 0x0098, 8, 3, 15, 0x0004, 21),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SSPM_SEL, "sspm_sel",
		sspm_parents, 0x0090, 0x0094, 0x0098, 16, 3, 23, 0x0004, 22),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DXCC_SEL, "dxcc_sel",
		dxcc_parents, 0x0090, 0x0094, 0x0098, 24, 2, 31, 0x0004, 23),
	/* CLK_CFG_6 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_TOP_SEL, "usb_sel",
		usb_parents, 0x00a0, 0x00a4, 0x00a8, 0, 2, 7, 0x0004, 24),
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_SRCK_SEL, "srck_sel",
		srck_parents, 0x00a0, 0x00a4, 0x00a8, 8, 2, 15, 0x0004, 25, CLK_IS_CRITICAL),
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_SPM_SEL, "spm_sel",
		spm_parents, 0x00a0, 0x00a4, 0x00a8, 16, 2, 23, 0x0004, 26, CLK_IS_CRITICAL),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_I2C_SEL, "i2c_sel",
		i2c_parents, 0x00a0, 0x00a4, 0x00a8, 24, 2, 31, 0x0004, 27),
	/* CLK_CFG_7 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PWM_SEL, "pwm_sel",
		pwm_parents, 0x00b0, 0x00b4, 0x00b8, 0, 2, 7, 0x0004, 28),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF_SEL, "seninf_sel",
		seninf_parents, 0x00b0, 0x00b4, 0x00b8, 8, 2, 15, 0x0004, 29),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF1_SEL, "seninf1_sel",
		seninf1_parents, 0x00b0, 0x00b4, 0x00b8, 16, 2, 23, 0x0004, 30),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF2_SEL, "seninf2_sel",
		seninf2_parents, 0x00b0, 0x00b4, 0x00b8, 24, 2, 31, 0x0008, 0),
	/* CLK_CFG_8 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF3_SEL, "seninf3_sel",
		seninf3_parents, 0x00c0, 0x00c4, 0x00c8, 0, 2, 7, 0x0008, 1),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AES_MSDCFDE_SEL, "aes_msdcfde_sel",
		aes_msdcfde_parents, 0x00c0, 0x00c4, 0x00c8, 8, 3, 15, 0x0008, 2),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PWRAP_ULPOSC_SEL, "pwrap_ulposc_sel",
		pwrap_ulposc_parents, 0x00c0, 0x00c4, 0x00c8, 16, 3, 23, 0x0008, 3),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTM_SEL, "camtm_sel",
		camtm_parents, 0x00c0, 0x00c4, 0x00c8, 24, 2, 31, 0x0008, 4),
	/* CLK_CFG_9 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_VENC_SEL, "venc_sel",
		venc_parents, 0x00d0, 0x00d4, 0x00d8, 0, 3, 7, 0x0008, 5),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAM_SEL, "cam_sel",
		cam_parents, 0x00d0, 0x00d4, 0x00d8, 8, 4, 15, 0x0008, 6),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_IMG1_SEL, "img1_sel",
		img1_parents, 0x00d0, 0x00d4, 0x00d8, 16, 4, 23, 0x0008, 7),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_IPE_SEL, "ipe_sel",
		ipe_parents, 0x00d0, 0x00d4, 0x00d8, 24, 4, 31, 0x0008, 8),
	/* CLK_CFG_10 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DPMAIF_SEL, "dpmaif_sel",
		dpmaif_parents, 0x00e0, 0x00e4, 0x00e8, 0, 3, 7, 0x0008, 9),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_VDEC_SEL, "vdec_sel",
		vdec_parents, 0x00e0, 0x00e4, 0x00e8, 8, 3, 15, 0x0008, 10),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DISP_SEL, "disp_sel",
		disp_parents, 0x00e0, 0x00e4, 0x00e8, 16, 4, 23, 0x0008, 11),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MDP_SEL, "mdp_sel",
		mdp_parents, 0x00e0, 0x00e4, 0x00e8, 24, 4, 31, 0x0008, 12),
	/* CLK_CFG_11 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUDIO_H_SEL, "audio_h_sel",
		audio_h_parents, 0x00ec, 0x00f0, 0x00f4, 0, 2, 7, 0x0008, 13),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UFS_SEL, "ufs_sel",
		ufs_parents, 0x00ec, 0x00f0, 0x00f4, 8, 2, 15, 0x0008, 14),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AES_FDE_SEL, "aes_fde_sel",
		aes_fde_parents, 0x00ec, 0x00f0, 0x00f4, 16, 2, 23, 0x0008, 15),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUDIODSP_SEL, "audiodsp_sel",
		audiodsp_parents, 0x00ec, 0x00f0, 0x00f4, 24, 3, 31, 0x0008, 16),
	/* CLK_CFG_12 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DSI_OCC_SEL, "dsi_occ_sel",
		dsi_occ_parents, 0x0100, 0x0104, 0x0108, 8, 2, 15, 0x0008, 18),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPMI_MST_SEL, "spmi_mst_sel",
		spmi_mst_parents, 0x0100, 0x0104, 0x0108, 16, 3, 23, 0x0008, 19),
	/* CLK_CFG_13 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPINOR_SEL, "spinor_sel",
		spinor_parents, 0x0110, 0x0114, 0x0118, 0, 3, 6, 0x0008, 20),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_NNA_SEL, "nna_sel",
		nna_parents, 0x0110, 0x0114, 0x0118, 7, 4, 14, 0x0008, 21),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_NNA1_SEL, "nna1_sel",
		nna1_parents, 0x0110, 0x0114, 0x0118, 15, 4, 22, 0x0008, 22),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_NNA2_SEL, "nna2_sel",
		nna2_parents, 0x0110, 0x0114, 0x0118, 23, 4, 30, 0x0008, 23),
	/* CLK_CFG_14 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SSUSB_XHCI_SEL, "ssusb_xhci_sel",
		ssusb_xhci_parents, 0x0120, 0x0124, 0x0128, 0, 2, 5, 0x0008, 24),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SSUSB_TOP_1P_SEL, "ssusb_1p_sel",
		ssusb_1p_parents, 0x0120, 0x0124, 0x0128, 6, 2, 11, 0x0008, 25),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SSUSB_XHCI_1P_SEL, "ssusb_xhci_1p_sel",
		ssusb_xhci_1p_parents, 0x0120, 0x0124, 0x0128, 12, 2, 17, 0x0008, 26),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_WPE_SEL, "wpe_sel",
		wpe_parents, 0x0120, 0x0124, 0x0128, 18, 4, 25, 0x0008, 27),
	/* CLK_CFG_15 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DPI_SEL, "dpi_sel",
		dpi_parents, 0x0180, 0x0184, 0x0188, 0, 3, 6, 0x0008, 28),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_U3_OCC_250M_SEL, "u3_occ_250m_sel",
		u3_occ_250m_parents, 0x0180, 0x0184, 0x0188, 7, 1, 11, 0x0008, 29),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_U3_OCC_500M_SEL, "u3_occ_500m_sel",
		u3_occ_500m_parents, 0x0180, 0x0184, 0x0188, 12, 1, 16, 0x0008, 30),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ADSP_BUS_SEL, "adsp_bus_sel",
		adsp_bus_parents, 0x0180, 0x0184, 0x0188, 17, 3, 23, 0x0008, 31),
};

static struct mtk_composite top_muxes[] = {
	/* CLK_AUDDIV_0 */
	MUX(CLK_TOP_APLL_I2S0_MCK_SEL, "apll_i2s0_mck_sel", apll_i2s0_mck_parents, 0x0320, 16, 1),
	MUX(CLK_TOP_APLL_I2S1_MCK_SEL, "apll_i2s1_mck_sel", apll_i2s1_mck_parents, 0x0320, 17, 1),
	MUX(CLK_TOP_APLL_I2S2_MCK_SEL, "apll_i2s2_mck_sel", apll_i2s2_mck_parents, 0x0320, 18, 1),
	MUX(CLK_TOP_APLL_I2S4_MCK_SEL, "apll_i2s4_mck_sel", apll_i2s4_mck_parents, 0x0320, 19, 1),
	MUX(CLK_TOP_APLL_TDMOUT_MCK_SEL, "apll_tdmout_mck_sel", apll_tdmout_mck_parents,
		0x0320, 20, 1),
};

static const struct mtk_composite top_adj_divs[] = {
	DIV_GATE(CLK_TOP_APLL12_CK_DIV0, "apll12_div0", "apll_i2s0_mck_sel",
			0x0320, 0, 0x0328, 8, 0),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV1, "apll12_div1", "apll_i2s1_mck_sel",
			0x0320, 1, 0x0328, 8, 8),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV2, "apll12_div2", "apll_i2s2_mck_sel",
			0x0320, 2, 0x0328, 8, 16),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV4, "apll12_div4", "apll_i2s4_mck_sel",
			0x0320, 3, 0x0328, 8, 24),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_TDMOUT_M, "apll12_div_tdmout_m", "apll_tdmout_mck_sel",
			0x0320, 4, 0x0334, 8, 0),
};

static const struct mtk_gate_regs infra_ao0_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x84,
	.sta_ofs = 0x90,
};

static const struct mtk_gate_regs infra_ao1_cg_regs = {
	.set_ofs = 0x88,
	.clr_ofs = 0x8c,
	.sta_ofs = 0x94,
};

static const struct mtk_gate_regs infra_ao2_cg_regs = {
	.set_ofs = 0xa4,
	.clr_ofs = 0xa8,
	.sta_ofs = 0xac,
};

static const struct mtk_gate_regs infra_ao3_cg_regs = {
	.set_ofs = 0xc0,
	.clr_ofs = 0xc4,
	.sta_ofs = 0xc8,
};

#define GATE_INFRA_AO0_FLAGS(_id, _name, _parent, _shift, _flag)	\
	GATE_MTK_FLAGS(_id, _name, _parent, &infra_ao0_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr, _flag)

#define GATE_INFRA_AO0(_id, _name, _parent, _shift)			\
	GATE_INFRA_AO0_FLAGS(_id, _name, _parent, _shift, 0)

#define GATE_INFRA_AO1_FLAGS(_id, _name, _parent, _shift, _flag)	\
	GATE_MTK_FLAGS(_id, _name, _parent, &infra_ao1_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr, _flag)

#define GATE_INFRA_AO1(_id, _name, _parent, _shift)			\
	GATE_INFRA_AO1_FLAGS(_id, _name, _parent, _shift, 0)


#define GATE_INFRA_AO2(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &infra_ao2_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr)

 #define GATE_INFRA_AO3_FLAGS(_id, _name, _parent, _shift, _flag)        \
	GATE_MTK_FLAGS(_id, _name, _parent, &infra_ao3_cg_regs, _shift, \
		&mtk_clk_gate_ops_setclr, _flag)

#define GATE_INFRA_AO3(_id, _name, _parent, _shift)			\
	GATE_INFRA_AO3_FLAGS(_id, _name, _parent, _shift, 0)

static const struct mtk_gate infra_ao_clks[] = {
	/* INFRA_AO0 */
	GATE_INFRA_AO0(CLK_INFRA_AO_PMIC_TMR, "infra_ao_pmic_tmr", "pwrap_ulposc_sel", 0),
	GATE_INFRA_AO0(CLK_INFRA_AO_PMIC_AP, "infra_ao_pmic_ap", "pwrap_ulposc_sel", 1),
	GATE_INFRA_AO0(CLK_INFRA_AO_PMIC_MD, "infra_ao_pmic_md", "pwrap_ulposc_sel", 2),
	GATE_INFRA_AO0(CLK_INFRA_AO_PMIC_CONN, "infra_ao_pmic_conn", "pwrap_ulposc_sel", 3),
	GATE_INFRA_AO0_FLAGS(CLK_INFRA_AO_SEJ, "infra_ao_sej", "axi_sel", 5, CLK_IS_CRITICAL),
	GATE_INFRA_AO0(CLK_INFRA_AO_APXGPT, "infra_ao_apxgpt", "axi_sel", 6),
	GATE_INFRA_AO0(CLK_INFRA_AO_ICUSB, "infra_ao_icusb", "axi_sel", 8),
	GATE_INFRA_AO0(CLK_INFRA_AO_GCE, "infra_ao_gce", "axi_sel", 9),
	GATE_INFRA_AO0(CLK_INFRA_AO_THERM, "infra_ao_therm", "axi_sel", 10),
	GATE_INFRA_AO0(CLK_INFRA_AO_I2C_AP, "infra_ao_i2c_ap", "i2c_sel", 11),
	GATE_INFRA_AO0(CLK_INFRA_AO_I2C_CCU, "infra_ao_i2c_ccu", "i2c_sel", 12),
	GATE_INFRA_AO0(CLK_INFRA_AO_I2C_SSPM, "infra_ao_i2c_sspm", "i2c_sel", 13),
	GATE_INFRA_AO0(CLK_INFRA_AO_I2C_RSV, "infra_ao_i2c_rsv", "i2c_sel", 14),
	GATE_INFRA_AO0(CLK_INFRA_AO_PWM_HCLK, "infra_ao_pwm_hclk", "axi_sel", 15),
	GATE_INFRA_AO0(CLK_INFRA_AO_PWM1, "infra_ao_pwm1", "pwm_sel", 16),
	GATE_INFRA_AO0(CLK_INFRA_AO_PWM2, "infra_ao_pwm2", "pwm_sel", 17),
	GATE_INFRA_AO0(CLK_INFRA_AO_PWM3, "infra_ao_pwm3", "pwm_sel", 18),
	GATE_INFRA_AO0(CLK_INFRA_AO_PWM4, "infra_ao_pwm4", "pwm_sel", 19),
	GATE_INFRA_AO0(CLK_INFRA_AO_PWM5, "infra_ao_pwm5", "pwm_sel", 20),
	GATE_INFRA_AO0(CLK_INFRA_AO_PWM, "infra_ao_pwm", "pwm_sel", 21),
	GATE_INFRA_AO0(CLK_INFRA_AO_UART0, "infra_ao_uart0", "uart_sel", 22),
	GATE_INFRA_AO0(CLK_INFRA_AO_UART1, "infra_ao_uart1", "uart_sel", 23),
	GATE_INFRA_AO0(CLK_INFRA_AO_UART2, "infra_ao_uart2", "uart_sel", 24),
	GATE_INFRA_AO0(CLK_INFRA_AO_GCE_26M, "infra_ao_gce_26m", "clk26m", 27),
	GATE_INFRA_AO0(CLK_INFRA_AO_CQ_DMA_FPC, "infra_ao_dma", "axi_sel", 28),
	GATE_INFRA_AO0(CLK_INFRA_AO_BTIF, "infra_ao_btif", "axi_sel", 31),
	/* INFRA_AO1 */
	GATE_INFRA_AO1(CLK_INFRA_AO_SPI0, "infra_ao_spi0", "spi_sel", 1),
	GATE_INFRA_AO1(CLK_INFRA_AO_MSDC0, "infra_ao_msdc0", "msdc5hclk_sel", 2),
	GATE_INFRA_AO1(CLK_INFRA_AO_MSDCFDE, "infra_ao_msdcfde", "aes_msdcfde_sel", 3),
	GATE_INFRA_AO1(CLK_INFRA_AO_MSDC1, "infra_ao_msdc1", "axi_sel", 4),
	GATE_INFRA_AO1(CLK_INFRA_AO_GCPU, "infra_ao_gcpu", "axi_sel", 8),
	GATE_INFRA_AO1(CLK_INFRA_AO_TRNG, "infra_ao_trng", "axi_sel", 9),
	GATE_INFRA_AO1(CLK_INFRA_AO_AUXADC, "infra_ao_auxadc", "clk26m", 10),
	GATE_INFRA_AO1(CLK_INFRA_AO_CPUM, "infra_ao_cpum", "axi_sel", 11),
	GATE_INFRA_AO1(CLK_INFRA_AO_CCIF1_AP, "infra_ao_ccif1_ap", "axi_sel", 12),
	GATE_INFRA_AO1(CLK_INFRA_AO_CCIF1_MD, "infra_ao_ccif1_md", "axi_sel", 13),
	GATE_INFRA_AO1(CLK_INFRA_AO_AUXADC_MD, "infra_ao_auxadc_md", "clk26m", 14),
	GATE_INFRA_AO1(CLK_INFRA_AO_AP_DMA, "infra_ao_ap_dma", "axi_sel", 18),
	GATE_INFRA_AO1(CLK_INFRA_AO_XIU, "infra_ao_xiu", "axi_sel", 19),
	GATE_INFRA_AO1_FLAGS(CLK_INFRA_AO_DEVICE_APC, "infra_ao_dapc", "axi_sel", 20,
									CLK_IS_CRITICAL),
	GATE_INFRA_AO1(CLK_INFRA_AO_CCIF_AP, "infra_ao_ccif_ap", "axi_sel", 23),
	GATE_INFRA_AO1(CLK_INFRA_AO_DEBUGTOP, "infra_ao_debugtop", "axi_sel", 24),
	GATE_INFRA_AO1(CLK_INFRA_AO_AUDIO, "infra_ao_audio", "axi_sel", 25),
	GATE_INFRA_AO1(CLK_INFRA_AO_CCIF_MD, "infra_ao_ccif_md", "axi_sel", 26),
	GATE_INFRA_AO1(CLK_INFRA_AO_DXCC_SEC_CORE, "infra_ao_secore", "dxcc_sel", 27),
	GATE_INFRA_AO1(CLK_INFRA_AO_DXCC_AO, "infra_ao_dxcc_ao", "dxcc_sel", 28),
	GATE_INFRA_AO1(CLK_INFRA_AO_IMP_IIC, "infra_ao_imp_iic", "axi_sel", 29),
	GATE_INFRA_AO1(CLK_INFRA_AO_DRAMC_F26M, "infra_ao_dramc26", "clk26m", 31),
	/* INFRA_AO2 */
	GATE_INFRA_AO2(CLK_INFRA_AO_RG_PWM_FBCLK6_CK, "infra_ao_pwm_fbclk6_ck", "clk26m", 0),
	GATE_INFRA_AO2(CLK_INFRA_AO_SSUSB_TOP_HCLK, "infra_ao_ssusb_hclk", "axi_sel", 1),
	GATE_INFRA_AO2(CLK_INFRA_AO_DISP_PWM, "infra_ao_disp_pwm", "disp_pwm_sel", 2),
	GATE_INFRA_AO2(CLK_INFRA_AO_CLDMA_BCLK, "infra_ao_cldmabclk", "axi_sel", 3),
	GATE_INFRA_AO2(CLK_INFRA_AO_AUDIO_26M_BCLK, "infra_ao_audio26m", "clk26m", 4),
	GATE_INFRA_AO2(CLK_INFRA_AO_SSUSB_TOP_P1_HCLK, "infra_ao_ssusb_p1_hclk", "axi_sel", 5),
	GATE_INFRA_AO2(CLK_INFRA_AO_SPI1, "infra_ao_spi1", "spi_sel", 6),
	GATE_INFRA_AO2(CLK_INFRA_AO_I2C4, "infra_ao_i2c4", "i2c_sel", 7),
	GATE_INFRA_AO2(CLK_INFRA_AO_MODEM_TEMP_SHARE, "infra_ao_mdtemp", "clk26m", 8),
	GATE_INFRA_AO2(CLK_INFRA_AO_SPI2, "infra_ao_spi2", "spi_sel", 9),
	GATE_INFRA_AO2(CLK_INFRA_AO_SPI3, "infra_ao_spi3", "spi_sel", 10),
	GATE_INFRA_AO2(CLK_INFRA_AO_SSUSB_TOP_REF_CK, "infra_ao_ssusb_ref_ck", "clk26m", 11),
	GATE_INFRA_AO2(CLK_INFRA_AO_SSUSB_TOP_XHCI_CK,
			"infra_ao_ssusb_xhci_ck", "ssusb_xhci_sel", 12),
	GATE_INFRA_AO2(CLK_INFRA_AO_SSUSB_TOP_P1_REF_CK,
			"infra_ao_ssusb_p1_ref_ck", "clk26m", 13),
	GATE_INFRA_AO2(CLK_INFRA_AO_SSUSB_TOP_P1_XHCI_CK,
			"infra_ao_ssusb_p1_xhci_ck", "ssusb_xhci_1p_sel", 14),
	GATE_INFRA_AO2(CLK_INFRA_AO_SSPM, "infra_ao_sspm", "sspm_sel", 15),
	GATE_INFRA_AO2(CLK_INFRA_AO_SSUSB_TOP_P1_SYS_CK,
			"infra_ao_ssusb_p1_sys_ck", "ssusb_1p_sel", 16),
	GATE_INFRA_AO2(CLK_INFRA_AO_I2C5, "infra_ao_i2c5", "i2c_sel", 18),
	GATE_INFRA_AO2(CLK_INFRA_AO_I2C5_ARBITER, "infra_ao_i2c5a", "i2c_sel", 19),
	GATE_INFRA_AO2(CLK_INFRA_AO_I2C5_IMM, "infra_ao_i2c5_imm", "i2c_sel", 20),
	GATE_INFRA_AO2(CLK_INFRA_AO_I2C1_ARBITER, "infra_ao_i2c1a", "i2c_sel", 21),
	GATE_INFRA_AO2(CLK_INFRA_AO_I2C1_IMM, "infra_ao_i2c1_imm", "i2c_sel", 22),
	GATE_INFRA_AO2(CLK_INFRA_AO_I2C2_ARBITER, "infra_ao_i2c2a", "i2c_sel", 23),
	GATE_INFRA_AO2(CLK_INFRA_AO_I2C2_IMM, "infra_ao_i2c2_imm", "i2c_sel", 24),
	GATE_INFRA_AO2(CLK_INFRA_AO_SPI4, "infra_ao_spi4", "spi_sel", 25),
	GATE_INFRA_AO2(CLK_INFRA_AO_SPI5, "infra_ao_spi5", "spi_sel", 26),
	GATE_INFRA_AO2(CLK_INFRA_AO_CQ_DMA, "infra_ao_cq_dma", "axi_sel", 27),
	GATE_INFRA_AO2(CLK_INFRA_AO_BIST2FPC, "infra_ao_bist2fpc", "f_bist2fpc_ck", 28),
	/* INFRA_AO3 */
	GATE_INFRA_AO3(CLK_INFRA_AO_MSDC0_SELF, "infra_ao_msdc0sf", "msdc50_0_sel", 0),
	GATE_INFRA_AO3(CLK_INFRA_AO_MSDC1_SELF, "infra_ao_msdc1sf", "msdc50_0_sel", 1),
	GATE_INFRA_AO3(CLK_INFRA_AO_SSPM_26M_SELF, "infra_ao_sspm_26m", "clk26m", 3),
	GATE_INFRA_AO3(CLK_INFRA_AO_SSPM_32K_SELF, "infra_ao_sspm_32k", "clk32k", 4),
	GATE_INFRA_AO3(CLK_INFRA_AO_I2C6, "infra_ao_i2c6", "i2c_sel", 6),
	GATE_INFRA_AO3(CLK_INFRA_AO_AP_MSDC0, "infra_ao_ap_msdc0", "axi_sel", 7),
	GATE_INFRA_AO3(CLK_INFRA_AO_MD_MSDC0, "infra_ao_md_msdc0", "axi_sel", 8),
	GATE_INFRA_AO3(CLK_INFRA_AO_MSDC0_SRC, "infra_ao_msdc0_clk", "msdc50_0_sel", 9),
	GATE_INFRA_AO3(CLK_INFRA_AO_MSDC1_SRC, "infra_ao_msdc1_clk", "msdc30_1_sel", 10),
	GATE_INFRA_AO3_FLAGS(CLK_INFRA_AO_SEJ_F13M_CK, "infra_ao_sej_f13m_ck", "clk26m", 15,
				CLK_IS_CRITICAL),
	GATE_INFRA_AO3_FLAGS(CLK_INFRA_AO_AES_TOP0_BCLK_CK, "infra_ao_aes_top0_bclk_ck",
				"axi_sel", 16, CLK_IS_CRITICAL),
	GATE_INFRA_AO3(CLK_INFRA_AO_MCU_PM_BCLK_CK, "infra_ao_mcu_pm_bclk_ck", "axi_sel", 17),
	GATE_INFRA_AO3(CLK_INFRA_AO_CCIF2_AP, "infra_ao_ccif2_ap", "axi_sel", 18),
	GATE_INFRA_AO3(CLK_INFRA_AO_CCIF2_MD, "infra_ao_ccif2_md", "axi_sel", 19),
	GATE_INFRA_AO3(CLK_INFRA_AO_CCIF3_AP, "infra_ao_ccif3_ap", "axi_sel", 20),
	GATE_INFRA_AO3(CLK_INFRA_AO_CCIF3_MD, "infra_ao_ccif3_md", "axi_sel", 21),
	GATE_INFRA_AO3(CLK_INFRA_AO_FADSP_26M, "infra_ao_fadsp_26m", "clk26m", 22),
	GATE_INFRA_AO3(CLK_INFRA_AO_FADSP_32K, "infra_ao_fadsp_32k", "clk32k", 23),
	GATE_INFRA_AO3(CLK_INFRA_AO_CCIF4_AP, "infra_ao_ccif4_ap", "axi_sel", 24),
	GATE_INFRA_AO3(CLK_INFRA_AO_CCIF4_MD, "infra_ao_ccif4_md", "axi_sel", 25),
	GATE_INFRA_AO3(CLK_INFRA_AO_FADSP_CK, "infra_ao_fadsp_ck", "audiodsp_sel", 27),
	GATE_INFRA_AO3(CLK_INFRA_AO_FLASHIF_133M_CK, "infra_ao_flashif_133m_ck", "axi_sel", 28),
	GATE_INFRA_AO3(CLK_INFRA_AO_FLASHIF_66M_CK, "infra_ao_flashif_66m_ck", "axi_sel", 29),
};

#define MT8169_PLL_FMAX		(3800UL * MHZ)
#define MT8169_PLL_FMIN		(1500UL * MHZ)
#define MT8169_INTEGER_BITS	8

#define PLL(_id, _name, _reg, _pwr_reg, _en_mask, _flags,	\
			_rst_bar_mask, _pcwbits, _pd_reg, _pd_shift,	\
			_tuner_reg, _tuner_en_reg, _tuner_en_bit,	\
			_pcw_reg, _pcw_shift, _pcw_chg_reg,				\
			_en_reg, _pll_en_bit) {					\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.pwr_reg = _pwr_reg,					\
		.en_mask = _en_mask,					\
		.flags = _flags,					\
		.rst_bar_mask = _rst_bar_mask,				\
		.fmax = MT8169_PLL_FMAX,				\
		.fmin = MT8169_PLL_FMIN,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT8169_INTEGER_BITS,			\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,				\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcw_chg_reg = _pcw_chg_reg,				\
		.en_reg = _en_reg,					\
		.pll_en_bit = _pll_en_bit,				\
	}

static const struct mtk_pll_data plls[] = {
	PLL(CLK_APMIXED_ARMPLL_LL, "armpll_ll", 0x0204, 0x0210, 0,
		PLL_AO, 0, 22, 0x0208, 24, 0, 0, 0, 0x0208, 0, 0, 0, 0),
	PLL(CLK_APMIXED_ARMPLL_BL, "armpll_bl", 0x0214, 0x0220, 0,
		PLL_AO, 0, 22, 0x0218, 24, 0, 0, 0, 0x0218, 0, 0, 0, 0),
	PLL(CLK_APMIXED_CCIPLL, "ccipll", 0x0224, 0x0230, 0,
		PLL_AO, 0, 22, 0x0228, 24, 0, 0, 0, 0x0228, 0, 0, 0, 0),
	PLL(CLK_APMIXED_MAINPLL, "mainpll", 0x0244, 0x0250, 0xff000000,
		HAVE_RST_BAR, BIT(23), 22, 0x0248, 24, 0, 0, 0, 0x0248, 0, 0, 0, 0),
	PLL(CLK_APMIXED_UNIV2PLL, "univ2pll", 0x0324, 0x0330, 0xff000000,
		HAVE_RST_BAR, BIT(23), 22, 0x0328, 24, 0, 0, 0, 0x0328, 0, 0, 0, 0),
	PLL(CLK_APMIXED_MSDCPLL, "msdcpll", 0x038C, 0x0398, 0,
		0, 0, 22, 0x0390, 24, 0, 0, 0, 0x0390, 0, 0, 0, 0),
	PLL(CLK_APMIXED_MMPLL, "mmpll", 0x0254, 0x0260, 0,
		0, 0, 22, 0x0258, 24, 0, 0, 0, 0x0258, 0, 0, 0, 0),
	PLL(CLK_APMIXED_NNAPLL, "nnapll", 0x035C, 0x0368, 0,
		0, 0, 22, 0x0360, 24, 0, 0, 0, 0x0360, 0, 0, 0, 0),
	PLL(CLK_APMIXED_NNA2PLL, "nna2pll", 0x036C, 0x0378, 0,
		0, 0, 22, 0x0370, 24, 0, 0, 0, 0x0370, 0, 0, 0, 0),
	PLL(CLK_APMIXED_ADSPPLL, "adsppll", 0x0304, 0x0310, 0,
		0, 0, 22, 0x0308, 24, 0, 0, 0, 0x0308, 0, 0, 0, 0),
	PLL(CLK_APMIXED_MFGPLL, "mfgpll", 0x0314, 0x0320, 0,
		0, 0, 22, 0x0318, 24, 0, 0, 0, 0x0318, 0, 0, 0, 0),
	PLL(CLK_APMIXED_TVDPLL, "tvdpll", 0x0264, 0x0270, 0,
		0, 0, 22, 0x0268, 24, 0, 0, 0, 0x0268, 0, 0, 0, 0),
	PLL(CLK_APMIXED_APLL1, "apll1", 0x0334, 0x0344, 0,
		0, 0, 32, 0x0338, 24, 0x0040, 0x000C, 0, 0x033C, 0, 0, 0, 0),
	PLL(CLK_APMIXED_APLL2, "apll2", 0x0348, 0x0358, 0,
		0, 0, 32, 0x034C, 24, 0x0044, 0x000C, 5, 0x0350, 0, 0, 0, 0),
};

static int clk_mt8169_mcu_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;
	int r;
	void __iomem *base;

	pr_info("[%s] start\n", __func__);

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	clk_data = mtk_alloc_clk_data(CLK_MCU_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_composites(mcu_muxes, ARRAY_SIZE(mcu_muxes), base,
			&mt8169_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		goto free_mcu_data;

	pr_info("[%s] end\n", __func__);

	return r;

free_mcu_data:
	mtk_free_clk_data(clk_data);
	return r;
}

static int clk_mt8169_top_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;
	int r;
	void __iomem *base;

	pr_info("[%s] start\n", __func__);

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	clk_data = mtk_alloc_clk_data(CLK_TOP_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_fixed_clks(top_fixed_clks, ARRAY_SIZE(top_fixed_clks),
			clk_data);
	mtk_clk_register_factors(top_divs, ARRAY_SIZE(top_divs), clk_data);
	mtk_clk_register_muxes(top_mtk_muxes, ARRAY_SIZE(top_mtk_muxes), node,
			&mt8169_clk_lock, clk_data);

	mtk_clk_register_composites(top_muxes, ARRAY_SIZE(top_muxes), base,
			&mt8169_clk_lock, clk_data);
	mtk_clk_register_composites(top_adj_divs, ARRAY_SIZE(top_adj_divs), base,
			&mt8169_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		goto free_top_data;

	pr_info("[%s] end\n", __func__);

	return r;

free_top_data:
	mtk_free_clk_data(clk_data);
	return r;
}

static int clk_mt8169_infra_ao_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;
	int r;

	pr_info("[%s] start\n", __func__);

	clk_data = mtk_alloc_clk_data(CLK_INFRA_AO_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	r = mtk_clk_register_gates(node, infra_ao_clks, ARRAY_SIZE(infra_ao_clks), clk_data);
	if (r)
		goto free_infra_ao_data;

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		goto free_infra_ao_data;

	pr_info("[%s] end\n", __func__);

	return r;

free_infra_ao_data:
	mtk_free_clk_data(clk_data);
	return r;
}

static int clk_mt8169_apmixed_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;
	int r;

	pr_info("[%s] start\n", __func__);

	clk_data = mtk_alloc_clk_data(CLK_APMIXED_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_plls(node, plls, ARRAY_SIZE(plls), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		goto free_apmixed_data;

	pr_info("[%s] end\n", __func__);

	return r;

free_apmixed_data:
	mtk_free_clk_data(clk_data);
	return r;
}

static const struct of_device_id of_match_clk_mt8169[] = {
	{
		.compatible = "mediatek,mt8169-apmixedsys",
		.data = clk_mt8169_apmixed_probe,
	}, {
		.compatible = "mediatek,mt8169-topckgen",
		.data = clk_mt8169_top_probe,
	}, {
		.compatible = "mediatek,mt8169-infracfg_ao",
		.data = clk_mt8169_infra_ao_probe,
	}, {
		.compatible = "mediatek,mt8169-mcusys",
		.data = clk_mt8169_mcu_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt8169_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *pdev);
	int r;

	clk_probe = of_device_get_match_data(&pdev->dev);
	if (!clk_probe)
		return -EINVAL;

	r = clk_probe(pdev);
	if (r)
		dev_info(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt8169_drv = {
	.probe = clk_mt8169_probe,
	.driver = {
		.name = "clk-mt8169",
		.of_match_table = of_match_clk_mt8169,
	},
};

static int __init clk_mt8169_init(void)
{
	return platform_driver_register(&clk_mt8169_drv);
}

static void __exit clk_mt8169_exit(void)
{
	platform_driver_unregister(&clk_mt8169_drv);
}

module_init(clk_mt8169_init);
module_exit(clk_mt8169_exit);
MODULE_LICENSE("GPL");
