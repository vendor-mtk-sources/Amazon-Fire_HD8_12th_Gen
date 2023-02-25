// SPDX-License-Identifier: GPL-2.0
/*
 *  Mediatek ALSA SoC AFE platform driver for 8169
 *
 *  Copyright (c) 2021 MediaTek Inc.
 *  Author: Jiaxin Yu <jiaxin.yu@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>
#include <linux/io.h>

#if IS_ENABLED(CONFIG_MTK_ACAO_SUPPORT)
#include "mtk_mcdi_governor_hint.h"
#endif

#include "../common/mtk-afe-debug.h"
#include "../common/mtk-afe-platform-driver.h"
#include "../common/mtk-afe-fe-dai.h"
#include "../common/mtk-sp-pcm-ops.h"
#include "../common/mtk-sram-manager.h"

#if IS_ENABLED(CONFIG_MTK_ION)
#include "../common/mtk-mmap-ion.h"
#endif

#include "mt8169-afe-common.h"
#include "mt8169-afe-clk.h"
#include "mt8169-afe-gpio.h"
#include "mt8169-interconnection.h"

#if IS_ENABLED(CONFIG_MTK_HIFIXDSP_SUPPORT)
static struct mtk_base_afe *g_priv;
#endif

static const struct snd_pcm_hardware mt8169_afe_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.formats = (SNDRV_PCM_FMTBIT_S16_LE |
		    SNDRV_PCM_FMTBIT_S24_LE |
		    SNDRV_PCM_FMTBIT_S32_LE),
	.period_bytes_min = 96,
	.period_bytes_max = 4 * 48 * 1024,
	.periods_min = 2,
	.periods_max = 256,
	.buffer_bytes_max = 4 * 48 * 1024,
	.fifo_size = 0,
};

struct gnt_memif_info gnt_memif_data[MT8169_MEMIF_NUM] = {
	[MT8169_MEMIF_DL1] = {
		.dai_id = MT8169_MEMIF_DL1,
		.dai_name = "DL1",
		.pbuf_size_reg = AFE_DL1_CON0,
		.pbuf_size_shift = 12,
		.pbuf_size_mask = 0x3,
		.pbuf_size_unit = 32,
		.irq_mon_reg = AFE_IRQ0_MCU_CNT_MON,
		.irq_mon_sft_mask = 0x3ffff,
		.cur_tolerance = 2,
	},
	[MT8169_MEMIF_DL12] = {
		.dai_id = MT8169_MEMIF_DL12,
		.dai_name = "DL12",
		.pbuf_size_reg = AFE_DL12_CON0,
		.pbuf_size_shift = 12,
		.pbuf_size_mask = 0x3,
		.pbuf_size_unit = 32,
		.irq_mon_reg = AFE_IRQ9_MCU_CNT_MON,
		.irq_mon_sft_mask = 0x3ffff,
		.cur_tolerance = 2,
	},
	[MT8169_MEMIF_DL2] = {
		.dai_id = MT8169_MEMIF_DL2,
		.dai_name = "DL2",
		.pbuf_size_reg = AFE_DL2_CON0,
		.pbuf_size_shift = 12,
		.pbuf_size_mask = 0x3,
		.pbuf_size_unit = 32,
		.irq_mon_reg = AFE_IRQ1_MCU_CNT_MON,
		.irq_mon_sft_mask = 0x3ffff,
		.cur_tolerance = 2,
	},
	[MT8169_MEMIF_DL3] = {
		.dai_id = MT8169_MEMIF_DL3,
		.dai_name = "DL3",
		.pbuf_size_reg = AFE_DL3_CON0,
		.pbuf_size_shift = 12,
		.pbuf_size_mask = 0x3,
		.pbuf_size_unit = 32,
		.irq_mon_reg = AFE_IRQ2_MCU_CNT_MON,
		.irq_mon_sft_mask = 0x3ffff,
		.cur_tolerance = 2,
	},
	[MT8169_MEMIF_DL4] = {
		.dai_id = MT8169_MEMIF_DL4,
		.dai_name = "DL4",
		.pbuf_size_reg = AFE_DL4_CON0,
		.pbuf_size_shift = 12,
		.pbuf_size_mask = 0x3,
		.pbuf_size_unit = 32,
		.irq_mon_reg = AFE_IRQ3_MCU_CNT_MON,
		.irq_mon_sft_mask = 0x3ffff,
		.cur_tolerance = 2,
	},
	[MT8169_MEMIF_DL5] = {
		.dai_id = MT8169_MEMIF_DL5,
		.dai_name = "DL5",
		.pbuf_size_reg = AFE_DL5_CON0,
		.pbuf_size_shift = 12,
		.pbuf_size_mask = 0x3,
		.pbuf_size_unit = 32,
		.irq_mon_reg = AFE_IRQ4_MCU_CNT_MON,
		.irq_mon_sft_mask = 0x3ffff,
		.cur_tolerance = 2,
	},
	[MT8169_MEMIF_DL6] = {
		.dai_id = MT8169_MEMIF_DL6,
		.dai_name = "DL6",
		.pbuf_size_reg = AFE_DL6_CON0,
		.pbuf_size_shift = 12,
		.pbuf_size_mask = 0x3,
		.pbuf_size_unit = 32,
		.irq_mon_reg = AFE_IRQ5_MCU_CNT_MON,
		.irq_mon_sft_mask = 0x3ffff,
		.cur_tolerance = 2,
	},
	[MT8169_MEMIF_DL7] = {
		.dai_id = MT8169_MEMIF_DL7,
		.dai_name = "DL7",
		.pbuf_size_reg = AFE_DL7_CON0,
		.pbuf_size_shift = 12,
		.pbuf_size_mask = 0x3,
		.pbuf_size_unit = 32,
		.irq_mon_reg = AFE_IRQ6_MCU_CNT_MON,
		.irq_mon_sft_mask = 0x3ffff,
		.cur_tolerance = 2,
	},
	[MT8169_MEMIF_DL8] = {
		.dai_id = MT8169_MEMIF_DL8,
		.dai_name = "DL8",
		.pbuf_size_reg = AFE_DL8_CON0,
		.pbuf_size_shift = 12,
		.pbuf_size_mask = 0x3,
		.pbuf_size_unit = 32,
		.irq_mon_reg = AFE_IRQ7_MCU_CNT_MON,
		.irq_mon_sft_mask = 0x3ffff,
		.cur_tolerance = 2,
	},
};

static const struct mtk_base_memif_data memif_data[MT8169_MEMIF_NUM] = {
	[MT8169_MEMIF_DL1] = {
		.name = "DL1",
		.id = MT8169_MEMIF_DL1,
		.reg_ofs_base = AFE_DL1_BASE,
		.reg_ofs_cur = AFE_DL1_CUR,
		.reg_ofs_end = AFE_DL1_END,
		.reg_ofs_base_msb = AFE_DL1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL1_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL1_END_MSB,
		.fs_reg = AFE_DL1_CON0,
		.fs_shift = DL1_MODE_SFT,
		.fs_maskbit = DL1_MODE_MASK,
		.mono_reg = AFE_DL1_CON0,
		.mono_shift = DL1_MONO_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = DL1_ON_SFT,
		.hd_reg = AFE_DL1_CON0,
		.hd_shift = DL1_HD_MODE_SFT,
		.hd_align_reg = AFE_DL1_CON0,
		.hd_align_mshift = DL1_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL1_CON0,
		.pbuf_mask = DL1_PBUF_SIZE_MASK,
		.pbuf_shift = DL1_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL1_CON0,
		.minlen_mask = DL1_MINLEN_MASK,
		.minlen_shift = DL1_MINLEN_SFT,
	},
	[MT8169_MEMIF_DL12] = {
		.name = "DL12",
		.id = MT8169_MEMIF_DL12,
		.reg_ofs_base = AFE_DL12_BASE,
		.reg_ofs_cur = AFE_DL12_CUR,
		.reg_ofs_end = AFE_DL12_END,
		.reg_ofs_base_msb = AFE_DL12_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL12_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL12_END_MSB,
		.fs_reg = AFE_DL12_CON0,
		.fs_shift = DL12_MODE_SFT,
		.fs_maskbit = DL12_MODE_MASK,
		.mono_reg = AFE_DL12_CON0,
		.mono_shift = DL12_MONO_SFT,
		.quad_ch_reg = AFE_DL12_CON0,
		.quad_ch_mask = DL12_4CH_EN_MASK,
		.quad_ch_shift = DL12_4CH_EN_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = DL12_ON_SFT,
		.hd_reg = AFE_DL12_CON0,
		.hd_shift = DL12_HD_MODE_SFT,
		.hd_align_reg = AFE_DL12_CON0,
		.hd_align_mshift = DL12_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL12_CON0,
		.pbuf_mask = DL12_PBUF_SIZE_MASK,
		.pbuf_shift = DL12_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL12_CON0,
		.minlen_mask = DL12_MINLEN_MASK,
		.minlen_shift = DL12_MINLEN_SFT,
	},
	[MT8169_MEMIF_DL2] = {
		.name = "DL2",
		.id = MT8169_MEMIF_DL2,
		.reg_ofs_base = AFE_DL2_BASE,
		.reg_ofs_cur = AFE_DL2_CUR,
		.reg_ofs_end = AFE_DL2_END,
		.reg_ofs_base_msb = AFE_DL2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL2_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL2_END_MSB,
		.fs_reg = AFE_DL2_CON0,
		.fs_shift = DL2_MODE_SFT,
		.fs_maskbit = DL2_MODE_MASK,
		.mono_reg = AFE_DL2_CON0,
		.mono_shift = DL2_MONO_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = DL2_ON_SFT,
		.hd_reg = AFE_DL2_CON0,
		.hd_shift = DL2_HD_MODE_SFT,
		.hd_align_reg = AFE_DL2_CON0,
		.hd_align_mshift = DL2_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL2_CON0,
		.pbuf_mask = DL2_PBUF_SIZE_MASK,
		.pbuf_shift = DL2_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL2_CON0,
		.minlen_mask = DL2_MINLEN_MASK,
		.minlen_shift = DL2_MINLEN_SFT,
	},
	[MT8169_MEMIF_DL3] = {
		.name = "DL3",
		.id = MT8169_MEMIF_DL3,
		.reg_ofs_base = AFE_DL3_BASE,
		.reg_ofs_cur = AFE_DL3_CUR,
		.reg_ofs_end = AFE_DL3_END,
		.reg_ofs_base_msb = AFE_DL3_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL3_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL3_END_MSB,
		.fs_reg = AFE_DL3_CON0,
		.fs_shift = DL3_MODE_SFT,
		.fs_maskbit = DL3_MODE_MASK,
		.mono_reg = AFE_DL3_CON0,
		.mono_shift = DL3_MONO_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = DL3_ON_SFT,
		.hd_reg = AFE_DL3_CON0,
		.hd_shift = DL3_HD_MODE_SFT,
		.hd_align_reg = AFE_DL3_CON0,
		.hd_align_mshift = DL3_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL3_CON0,
		.pbuf_mask = DL3_PBUF_SIZE_MASK,
		.pbuf_shift = DL3_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL3_CON0,
		.minlen_mask = DL3_MINLEN_MASK,
		.minlen_shift = DL3_MINLEN_SFT,
	},
	[MT8169_MEMIF_DL4] = {
		.name = "DL4",
		.id = MT8169_MEMIF_DL4,
		.reg_ofs_base = AFE_DL4_BASE,
		.reg_ofs_cur = AFE_DL4_CUR,
		.reg_ofs_end = AFE_DL4_END,
		.reg_ofs_base_msb = AFE_DL4_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL4_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL4_END_MSB,
		.fs_reg = AFE_DL4_CON0,
		.fs_shift = DL4_MODE_SFT,
		.fs_maskbit = DL4_MODE_MASK,
		.mono_reg = AFE_DL4_CON0,
		.mono_shift = DL4_MONO_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = DL4_ON_SFT,
		.hd_reg = AFE_DL4_CON0,
		.hd_shift = DL4_HD_MODE_SFT,
		.hd_align_reg = AFE_DL4_CON0,
		.hd_align_mshift = DL4_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL4_CON0,
		.pbuf_mask = DL4_PBUF_SIZE_MASK,
		.pbuf_shift = DL4_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL4_CON0,
		.minlen_mask = DL4_MINLEN_MASK,
		.minlen_shift = DL4_MINLEN_SFT,
	},
	[MT8169_MEMIF_DL5] = {
		.name = "DL5",
		.id = MT8169_MEMIF_DL5,
		.reg_ofs_base = AFE_DL5_BASE,
		.reg_ofs_cur = AFE_DL5_CUR,
		.reg_ofs_end = AFE_DL5_END,
		.reg_ofs_base_msb = AFE_DL5_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL5_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL5_END_MSB,
		.fs_reg = AFE_DL5_CON0,
		.fs_shift = DL5_MODE_SFT,
		.fs_maskbit = DL5_MODE_MASK,
		.mono_reg = AFE_DL5_CON0,
		.mono_shift = DL5_MONO_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = DL5_ON_SFT,
		.hd_reg = AFE_DL5_CON0,
		.hd_shift = DL5_HD_MODE_SFT,
		.hd_align_reg = AFE_DL5_CON0,
		.hd_align_mshift = DL5_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL5_CON0,
		.pbuf_mask = DL5_PBUF_SIZE_MASK,
		.pbuf_shift = DL5_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL5_CON0,
		.minlen_mask = DL5_MINLEN_MASK,
		.minlen_shift = DL5_MINLEN_SFT,
	},
	[MT8169_MEMIF_DL6] = {
		.name = "DL6",
		.id = MT8169_MEMIF_DL6,
		.reg_ofs_base = AFE_DL6_BASE,
		.reg_ofs_cur = AFE_DL6_CUR,
		.reg_ofs_end = AFE_DL6_END,
		.reg_ofs_base_msb = AFE_DL6_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL6_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL6_END_MSB,
		.fs_reg = AFE_DL6_CON0,
		.fs_shift = DL6_MODE_SFT,
		.fs_maskbit = DL6_MODE_MASK,
		.mono_reg = AFE_DL6_CON0,
		.mono_shift = DL6_MONO_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = DL6_ON_SFT,
		.hd_reg = AFE_DL6_CON0,
		.hd_shift = DL6_HD_MODE_SFT,
		.hd_align_reg = AFE_DL6_CON0,
		.hd_align_mshift = DL6_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL6_CON0,
		.pbuf_mask = DL6_PBUF_SIZE_MASK,
		.pbuf_shift = DL6_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL6_CON0,
		.minlen_mask = DL6_MINLEN_MASK,
		.minlen_shift = DL6_MINLEN_SFT,
	},
	[MT8169_MEMIF_DL7] = {
		.name = "DL7",
		.id = MT8169_MEMIF_DL7,
		.reg_ofs_base = AFE_DL7_BASE,
		.reg_ofs_cur = AFE_DL7_CUR,
		.reg_ofs_end = AFE_DL7_END,
		.reg_ofs_base_msb = AFE_DL7_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL7_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL7_END_MSB,
		.fs_reg = AFE_DL7_CON0,
		.fs_shift = DL7_MODE_SFT,
		.fs_maskbit = DL7_MODE_MASK,
		.mono_reg = AFE_DL7_CON0,
		.mono_shift = DL7_MONO_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = DL7_ON_SFT,
		.hd_reg = AFE_DL7_CON0,
		.hd_shift = DL7_HD_MODE_SFT,
		.hd_align_reg = AFE_DL7_CON0,
		.hd_align_mshift = DL7_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL7_CON0,
		.pbuf_mask = DL7_PBUF_SIZE_MASK,
		.pbuf_shift = DL7_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL7_CON0,
		.minlen_mask = DL7_MINLEN_MASK,
		.minlen_shift = DL7_MINLEN_SFT,
	},
	[MT8169_MEMIF_DL8] = {
		.name = "DL8",
		.id = MT8169_MEMIF_DL8,
		.reg_ofs_base = AFE_DL8_BASE,
		.reg_ofs_cur = AFE_DL8_CUR,
		.reg_ofs_end = AFE_DL8_END,
		.reg_ofs_base_msb = AFE_DL8_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL8_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL8_END_MSB,
		.fs_reg = AFE_DL8_CON0,
		.fs_shift = DL8_MODE_SFT,
		.fs_maskbit = DL8_MODE_MASK,
		.mono_reg = AFE_DL8_CON0,
		.mono_shift = DL8_MONO_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = DL8_ON_SFT,
		.hd_reg = AFE_DL8_CON0,
		.hd_shift = DL8_HD_MODE_SFT,
		.hd_align_reg = AFE_DL8_CON0,
		.hd_align_mshift = DL8_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL8_CON0,
		.pbuf_mask = DL8_PBUF_SIZE_MASK,
		.pbuf_shift = DL8_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL8_CON0,
		.minlen_mask = DL8_MINLEN_MASK,
		.minlen_shift = DL8_MINLEN_SFT,
	},
	[MT8169_MEMIF_VUL12] = {
		.name = "VUL12",
		.id = MT8169_MEMIF_VUL12,
		.reg_ofs_base = AFE_VUL12_BASE,
		.reg_ofs_cur = AFE_VUL12_CUR,
		.reg_ofs_end = AFE_VUL12_END,
		.reg_ofs_base_msb = AFE_VUL12_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL12_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL12_END_MSB,
		.fs_reg = AFE_VUL12_CON0,
		.fs_shift = VUL12_MODE_SFT,
		.fs_maskbit = VUL12_MODE_MASK,
		.mono_reg = AFE_VUL12_CON0,
		.mono_shift = VUL12_MONO_SFT,
		.quad_ch_reg = AFE_VUL12_CON0,
		.quad_ch_mask = VUL12_4CH_EN_MASK,
		.quad_ch_shift = VUL12_4CH_EN_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = VUL12_ON_SFT,
		.hd_reg = AFE_VUL12_CON0,
		.hd_shift = VUL12_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL12_CON0,
		.hd_align_mshift = VUL12_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8169_MEMIF_VUL2] = {
		.name = "VUL2",
		.id = MT8169_MEMIF_VUL2,
		.reg_ofs_base = AFE_VUL2_BASE,
		.reg_ofs_cur = AFE_VUL2_CUR,
		.reg_ofs_end = AFE_VUL2_END,
		.reg_ofs_base_msb = AFE_VUL2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL2_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL2_END_MSB,
		.fs_reg = AFE_VUL2_CON0,
		.fs_shift = VUL2_MODE_SFT,
		.fs_maskbit = VUL2_MODE_MASK,
		.mono_reg = AFE_VUL2_CON0,
		.mono_shift = VUL2_MONO_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = VUL2_ON_SFT,
		.hd_reg = AFE_VUL2_CON0,
		.hd_shift = VUL2_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL2_CON0,
		.hd_align_mshift = VUL2_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8169_MEMIF_AWB] = {
		.name = "AWB",
		.id = MT8169_MEMIF_AWB,
		.reg_ofs_base = AFE_AWB_BASE,
		.reg_ofs_cur = AFE_AWB_CUR,
		.reg_ofs_end = AFE_AWB_END,
		.reg_ofs_base_msb = AFE_AWB_BASE_MSB,
		.reg_ofs_cur_msb = AFE_AWB_CUR_MSB,
		.reg_ofs_end_msb = AFE_AWB_END_MSB,
		.fs_reg = AFE_AWB_CON0,
		.fs_shift = AWB_MODE_SFT,
		.fs_maskbit = AWB_MODE_MASK,
		.mono_reg = AFE_AWB_CON0,
		.mono_shift = AWB_MONO_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = AWB_ON_SFT,
		.hd_reg = AFE_AWB_CON0,
		.hd_shift = AWB_HD_MODE_SFT,
		.hd_align_reg = AFE_AWB_CON0,
		.hd_align_mshift = AWB_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8169_MEMIF_AWB2] = {
		.name = "AWB2",
		.id = MT8169_MEMIF_AWB2,
		.reg_ofs_base = AFE_AWB2_BASE,
		.reg_ofs_cur = AFE_AWB2_CUR,
		.reg_ofs_end = AFE_AWB2_END,
		.reg_ofs_base_msb = AFE_AWB2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_AWB2_CUR_MSB,
		.reg_ofs_end_msb = AFE_AWB2_END_MSB,
		.fs_reg = AFE_AWB2_CON0,
		.fs_shift = AWB2_MODE_SFT,
		.fs_maskbit = AWB2_MODE_MASK,
		.mono_reg = AFE_AWB2_CON0,
		.mono_shift = AWB2_MONO_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = AWB2_ON_SFT,
		.hd_reg = AFE_AWB2_CON0,
		.hd_shift = AWB2_HD_MODE_SFT,
		.hd_align_reg = AFE_AWB2_CON0,
		.hd_align_mshift = AWB2_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8169_MEMIF_VUL3] = {
		.name = "VUL3",
		.id = MT8169_MEMIF_VUL3,
		.reg_ofs_base = AFE_VUL3_BASE,
		.reg_ofs_cur = AFE_VUL3_CUR,
		.reg_ofs_end = AFE_VUL3_END,
		.reg_ofs_base_msb = AFE_VUL3_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL3_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL3_END_MSB,
		.fs_reg = AFE_VUL3_CON0,
		.fs_shift = VUL3_MODE_SFT,
		.fs_maskbit = VUL3_MODE_MASK,
		.mono_reg = AFE_VUL3_CON0,
		.mono_shift = VUL3_MONO_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = VUL3_ON_SFT,
		.hd_reg = AFE_VUL3_CON0,
		.hd_shift = VUL3_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL3_CON0,
		.hd_align_mshift = VUL3_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8169_MEMIF_VUL4] = {
		.name = "VUL4",
		.id = MT8169_MEMIF_VUL4,
		.reg_ofs_base = AFE_VUL4_BASE,
		.reg_ofs_cur = AFE_VUL4_CUR,
		.reg_ofs_end = AFE_VUL4_END,
		.reg_ofs_base_msb = AFE_VUL4_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL4_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL4_END_MSB,
		.fs_reg = AFE_VUL4_CON0,
		.fs_shift = VUL4_MODE_SFT,
		.fs_maskbit = VUL4_MODE_MASK,
		.mono_reg = AFE_VUL4_CON0,
		.mono_shift = VUL4_MONO_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = VUL4_ON_SFT,
		.hd_reg = AFE_VUL4_CON0,
		.hd_shift = VUL4_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL4_CON0,
		.hd_align_mshift = VUL4_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8169_MEMIF_VUL5] = {
		.name = "VUL5",
		.id = MT8169_MEMIF_VUL5,
		.reg_ofs_base = AFE_VUL5_BASE,
		.reg_ofs_cur = AFE_VUL5_CUR,
		.reg_ofs_end = AFE_VUL5_END,
		.reg_ofs_base_msb = AFE_VUL5_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL5_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL5_END_MSB,
		.fs_reg = AFE_VUL5_CON0,
		.fs_shift = VUL5_MODE_SFT,
		.fs_maskbit = VUL5_MODE_MASK,
		.mono_reg = AFE_VUL5_CON0,
		.mono_shift = VUL5_MONO_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = VUL5_ON_SFT,
		.hd_reg = AFE_VUL5_CON0,
		.hd_shift = VUL5_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL5_CON0,
		.hd_align_mshift = VUL5_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT8169_MEMIF_VUL6] = {
		.name = "VUL6",
		.id = MT8169_MEMIF_VUL6,
		.reg_ofs_base = AFE_VUL6_BASE,
		.reg_ofs_cur = AFE_VUL6_CUR,
		.reg_ofs_end = AFE_VUL6_END,
		.reg_ofs_base_msb = AFE_VUL6_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL6_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL6_END_MSB,
		.fs_reg = AFE_VUL6_CON0,
		.fs_shift = VUL6_MODE_SFT,
		.fs_maskbit = VUL6_MODE_MASK,
		.mono_reg = AFE_VUL6_CON0,
		.mono_shift = VUL6_MONO_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = VUL6_ON_SFT,
		.hd_reg = AFE_VUL6_CON0,
		.hd_shift = VUL6_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL6_CON0,
		.hd_align_mshift = VUL6_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
};

static const struct mtk_base_irq_data irq_data[MT8169_IRQ_NUM] = {
	[MT8169_IRQ_0] = {
		.id = MT8169_IRQ_0,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT0,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON1,
		.irq_fs_shift = IRQ0_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ0_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ0_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ0_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ0_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ0_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ0_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_1] = {
		.id = MT8169_IRQ_1,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON1,
		.irq_fs_shift = IRQ1_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ1_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ1_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ1_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ1_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ1_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ1_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_2] = {
		.id = MT8169_IRQ_2,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT2,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON1,
		.irq_fs_shift = IRQ2_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ2_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ2_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ2_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ2_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ2_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ2_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_3] = {
		.id = MT8169_IRQ_3,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT3,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON1,
		.irq_fs_shift = IRQ3_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ3_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ3_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ3_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ3_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ3_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ3_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_4] = {
		.id = MT8169_IRQ_4,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT4,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON1,
		.irq_fs_shift = IRQ4_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ4_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ4_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ4_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ4_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ4_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ4_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_5] = {
		.id = MT8169_IRQ_5,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT5,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON1,
		.irq_fs_shift = IRQ5_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ5_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ5_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ5_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ5_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ5_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ5_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_6] = {
		.id = MT8169_IRQ_6,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT6,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON1,
		.irq_fs_shift = IRQ6_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ6_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ6_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ6_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ6_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ6_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ6_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_7] = {
		.id = MT8169_IRQ_7,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT7,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON1,
		.irq_fs_shift = IRQ7_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ7_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ7_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ7_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ7_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ7_MCU_SCP_EN_SFT,
	},
	[MT8169_IRQ_8] = {
		.id = MT8169_IRQ_8,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT8,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON2,
		.irq_fs_shift = IRQ8_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ8_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ8_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ8_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ8_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ8_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ8_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_9] = {
		.id = MT8169_IRQ_9,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT9,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON2,
		.irq_fs_shift = IRQ9_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ9_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ9_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ9_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ9_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ9_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ9_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_10] = {
		.id = MT8169_IRQ_10,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT10,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON2,
		.irq_fs_shift = IRQ10_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ10_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ10_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ10_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ10_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ10_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ10_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_11] = {
		.id = MT8169_IRQ_11,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT11,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON2,
		.irq_fs_shift = IRQ11_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ11_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ11_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ11_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ11_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ11_MCU_SCP_EN_SFT,
	},
	[MT8169_IRQ_12] = {
		.id = MT8169_IRQ_12,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT12,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON2,
		.irq_fs_shift = IRQ12_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ12_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ12_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ12_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ12_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ12_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ12_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_13] = {
		.id = MT8169_IRQ_13,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT13,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON2,
		.irq_fs_shift = IRQ13_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ13_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ13_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ13_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ13_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ13_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ13_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_14] = {
		.id = MT8169_IRQ_14,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT14,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON2,
		.irq_fs_shift = IRQ14_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ14_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ14_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ14_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ14_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ14_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ14_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_15] = {
		.id = MT8169_IRQ_15,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT15,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON2,
		.irq_fs_shift = IRQ15_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ15_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ15_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ15_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ15_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ15_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ15_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_16] = {
		.id = MT8169_IRQ_16,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT16,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON3,
		.irq_fs_shift = IRQ16_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ16_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ16_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ16_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ16_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ16_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ16_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_17] = {
		.id = MT8169_IRQ_17,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT17,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON3,
		.irq_fs_shift = IRQ17_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ17_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ17_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ17_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ17_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ17_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ17_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_18] = {
		.id = MT8169_IRQ_18,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT18,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON3,
		.irq_fs_shift = IRQ18_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ18_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ18_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ18_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ18_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ18_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ18_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_19] = {
		.id = MT8169_IRQ_19,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT19,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON3,
		.irq_fs_shift = IRQ19_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ19_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ19_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ19_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ19_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ19_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ19_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_20] = {
		.id = MT8169_IRQ_20,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT20,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON3,
		.irq_fs_shift = IRQ20_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ20_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ20_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ20_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ20_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ20_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ20_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_21] = {
		.id = MT8169_IRQ_21,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT21,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON3,
		.irq_fs_shift = IRQ21_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ21_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ21_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ21_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ21_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ21_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ21_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_22] = {
		.id = MT8169_IRQ_22,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT22,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON3,
		.irq_fs_shift = IRQ22_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ22_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ22_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ22_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ22_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ22_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ22_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_23] = {
		.id = MT8169_IRQ_23,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT23,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON3,
		.irq_fs_shift = IRQ23_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ23_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ23_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ23_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ23_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ23_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ23_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_24] = {
		.id = MT8169_IRQ_24,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT24,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON4,
		.irq_fs_shift = IRQ24_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ24_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ24_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ24_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ24_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ24_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ24_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_25] = {
		.id = MT8169_IRQ_25,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT25,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON4,
		.irq_fs_shift = IRQ25_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ25_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ25_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ25_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ25_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ25_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ25_MCU_DSP_EN_SFT,
	},
	[MT8169_IRQ_26] = {
		.id = MT8169_IRQ_26,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT26,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ_MCU_CON4,
		.irq_fs_shift = IRQ26_MCU_MODE_SFT,
		.irq_fs_maskbit = IRQ26_MCU_MODE_MASK,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ26_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ26_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ26_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ26_MCU_SCP_EN_SFT,
		.irq_dsp_en_reg = AFE_IRQ_MCU_DSP_EN,
		.irq_dsp_en_shift = IRQ26_MCU_DSP_EN_SFT,
	},
};

static int mt8169_fe_startup(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int memif_num = rtd->cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	const struct snd_pcm_hardware *mtk_afe_hardware = afe->mtk_afe_hardware;
	int ret;

	memif->substream = substream;

	snd_pcm_hw_constraint_step(substream->runtime, 0,
				   SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 16);

	snd_soc_set_runtime_hwparams(substream, mtk_afe_hardware);

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		dev_info(afe->dev, "snd_pcm_hw_constraint_integer failed\n");

	/* dynamic allocate irq to memif */
	if (memif->irq_usage < 0) {
		int irq_id = mtk_dynamic_irq_acquire(afe);

		if (irq_id != afe->irqs_size) {
			/* link */
			memif->irq_usage = irq_id;
		} else {
			dev_info(afe->dev, "%s() error: no more asys irq\n",
				__func__);
			ret = -EBUSY;
		}
	}

	return ret;
}

static void mt8169_fe_shutdown(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	int memif_num = rtd->cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;

	memif->substream = NULL;
	afe_priv->irq_cnt[memif_num] = 0;
	afe_priv->xrun_assert[memif_num] = 0;

	if (!memif->const_irq) {
		mtk_dynamic_irq_release(afe, irq_id);
		memif->irq_usage = -1;
		memif->substream = NULL;
	}
}

static int mt8169_fe_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int memif_id = rtd->cpu_dai->id;
	unsigned int channels = params_channels(params);
	unsigned int rate = params_rate(params);
	int ret;

	ret = mtk_afe_fe_hw_params(substream, params, dai);
	if (ret)
		goto exit;

	/* channel merge configuration, enable control is in UL5_IN_MUX */
	if (memif_id == MT8169_MEMIF_VUL3) {
		int update_cnt = 8;
		unsigned int val = 0;
		unsigned int mask = 0;
		int fs_mode = mt8169_rate_transform(afe->dev, rate, memif_id);

		/* set rate, channel, update cnt, disable sgen */
		val = fs_mode << CM1_FS_SELECT_SFT |
			(channels - 1) << CHANNEL_MERGE0_CHNUM_SFT |
			update_cnt << CHANNEL_MERGE0_UPDATE_CNT_SFT |
			0 << CHANNEL_MERGE0_DEBUG_MODE_SFT |
			0 << CM1_DEBUG_MODE_SEL_SFT;
		mask = CM1_FS_SELECT_MASK_SFT |
			CHANNEL_MERGE0_CHNUM_MASK_SFT |
			CHANNEL_MERGE0_UPDATE_CNT_MASK_SFT |
			CHANNEL_MERGE0_DEBUG_MODE_MASK_SFT |
			CM1_DEBUG_MODE_SEL_MASK_SFT;
		regmap_update_bits(afe->regmap, AFE_CM1_CON, mask, val);
	}

exit:
	return ret;
}

static int mt8169_fe_hw_free(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	int ret;

	ret = mtk_afe_fe_hw_free(substream, dai);
	if (ret)
		goto exit;

	/* wait for some platform related operation */
exit:
	return ret;
}

static int mt8169_fe_trigger(struct snd_pcm_substream *substream, int cmd,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	int id = rtd->cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	int irq_id = memif->irq_usage;
	struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	unsigned int counter = runtime->period_size;
	int fs;
	int ret;

	dev_info(afe->dev, "%s(), %s cmd %d, irq_id %d\n",
		 __func__, memif->data->name, cmd, irq_id);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		ret = mtk_memif_set_enable(afe, id);

		if (id == MT8169_MEMIF_VUL3)
			regmap_update_bits(afe->regmap,
					   AFE_CM1_CON,
					   CHANNEL_MERGE0_EN_MASK_SFT,
					   1 << CHANNEL_MERGE0_EN_SFT);
		/*
		 * for small latency record
		 * ul memif need read some data before irq enable
		 */
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			udelay(50);
		}

		if (ret) {
			dev_info(afe->dev, "%s(), error, id %d, memif enable, ret %d\n",
				__func__, id, ret);
			return ret;
		}

		/* set irq counter */
		if (afe_priv->irq_cnt[id] > 0)
			counter = afe_priv->irq_cnt[id];

		regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit
				   << irq_data->irq_cnt_shift,
				   counter << irq_data->irq_cnt_shift);

		/* set irq fs */
		fs = afe->irq_fs(substream, runtime->rate);

		if (fs < 0)
			return -EINVAL;

		regmap_update_bits(afe->regmap, irq_data->irq_fs_reg,
				       irq_data->irq_fs_maskbit
				       << irq_data->irq_fs_shift,
				       fs << irq_data->irq_fs_shift);


		/* enable interrupt */
		if (runtime->stop_threshold != ~(0U))
			regmap_update_bits(afe->regmap,
					       irq_data->irq_en_reg,
					       1 << irq_data->irq_en_shift,
					       1 << irq_data->irq_en_shift);
		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (afe_priv->xrun_assert[id] > 0) {
			if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
				int avail = snd_pcm_capture_avail(runtime);

				if (avail >= runtime->buffer_size) {
					dev_info(afe->dev, "%s(), id %d, xrun assert\n",
						 __func__, id);
				}
			}
		}

		if (id == MT8169_MEMIF_VUL3)
			regmap_update_bits(afe->regmap,
					   AFE_CM1_CON,
					   CHANNEL_MERGE0_EN_MASK_SFT,
					   0 << CHANNEL_MERGE0_EN_SFT);

		ret = mtk_memif_set_disable(afe, id);
		if (ret) {
			dev_info(afe->dev, "%s(), error, id %d, memif enable, ret %d\n",
				__func__, id, ret);
		}

		/* disable interrupt */
		if (runtime->stop_threshold != ~(0U))
			regmap_update_bits(afe->regmap,
					   irq_data->irq_en_reg,
					   1 << irq_data->irq_en_shift,
					   0 << irq_data->irq_en_shift);

		/* clear pending IRQ */
		regmap_write(afe->regmap, irq_data->irq_clr_reg,
			     1 << irq_data->irq_clr_shift);

		return ret;
	default:
		return -EINVAL;
	}
}

static int mt8169_memif_fs(struct snd_pcm_substream *substream,
			   unsigned int rate)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	int id = rtd->cpu_dai->id;

	return mt8169_rate_transform(afe->dev, rate, id);
}

static int mt8169_get_dai_fs(struct mtk_base_afe *afe,
			     int dai_id, unsigned int rate)
{
	return mt8169_rate_transform(afe->dev, rate, dai_id);
}

static int mt8169_irq_fs(struct snd_pcm_substream *substream, unsigned int rate)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);

	return mt8169_general_rate_transform(afe->dev, rate);
}

int mt8169_get_memif_pbuf_size(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	if ((runtime->period_size * 1000) / runtime->rate > 10)
		return MT8169_MEMIF_PBUF_SIZE_256_BYTES;
	else
		return MT8169_MEMIF_PBUF_SIZE_32_BYTES;
}

static int mt8169_fe_prepare(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int id = rtd->cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	int irq_id = memif->irq_usage;
	struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	unsigned int counter = runtime->period_size;
	int fs;
	int ret;

	ret = mtk_afe_fe_prepare(substream, dai);
	if (ret)
		goto exit;

	/* set irq counter */
	regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
			   irq_data->irq_cnt_maskbit
			   << irq_data->irq_cnt_shift,
			   counter << irq_data->irq_cnt_shift);

	/* set irq fs */
	fs = afe->irq_fs(substream, runtime->rate);

	if (fs < 0)
		return -EINVAL;

	regmap_update_bits(afe->regmap, irq_data->irq_fs_reg,
			   irq_data->irq_fs_maskbit
			   << irq_data->irq_fs_shift,
			   fs << irq_data->irq_fs_shift);
exit:
	return ret;
}

static snd_pcm_sframes_t
mt8169_fe_delay(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime;
	struct mtk_base_afe_memif *memif = &afe->memif[rtd->cpu_dai->id];
	struct mtk_base_afe_irq *irqs = &afe->irqs[memif->irq_usage];
	const struct gnt_memif_info *info = memif->gnt_info;
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	int reg_ofs_base = memif->data->reg_ofs_base;
	int reg_ofs_cur = memif->data->reg_ofs_cur;
	unsigned int hw_ptr = 0, hw_base = 0;
	int max_prefetch_size;
	int pbuf_size = 0;
	int delay_size = 0;
	int rptr_offset = 0;
	int hwptr_offset = 0;
	int period = 0;
	int delta = 0;
	int irq_size = 0;
	int irq_count = 0;
	int rate;

	if (rtd->cpu_dai->id != MT8169_MEMIF_DL1)
		return 0;

	if (info == NULL)
		return 0;

	runtime = substream->runtime;
	if (runtime == NULL) {
		dev_info(afe->dev, "%s() playback substream not opened(%d)\n",
			 __func__, substream->hw_opened);
		return 0;
	}

	period = (int)(runtime->period_size);
	rate = (int)(runtime->rate);

	memif->timestamp = (long long)mtk_timer_get_cnt(6);
	regmap_read(afe->regmap, info->irq_mon_reg, &irq_count);
	regmap_read(afe->regmap, reg_ofs_cur, &hw_ptr);
	regmap_read(afe->regmap, reg_ofs_base, &hw_base);
	regmap_read(afe->regmap, info->pbuf_size_reg, &pbuf_size);
	regmap_read(afe->regmap, irq_data->irq_cnt_reg, &irq_size);

	/* irq size may not equal period_size */
	if (irq_size != period && (irq_size % period == 0))
		period = irq_size;

	/* max prefetch buffer size by frames */
	pbuf_size = ((pbuf_size >> info->pbuf_size_shift) & info->pbuf_size_mask);
	pbuf_size = (info->pbuf_size_unit) << pbuf_size;
	max_prefetch_size = bytes_to_frames(runtime, pbuf_size);

	/* hw point by frames */
	hw_ptr -= hw_base;
	hwptr_offset = bytes_to_frames(runtime, hw_ptr);

	/* hw point offset by frames */
	if (period != 0)
		rptr_offset = (int)(hwptr_offset % period);
	if (rptr_offset != 0)
		delta = period - rptr_offset;

	/* irq left by frames */
	irq_count &= info->irq_mon_sft_mask;
	if (delta <= irq_count)
		delay_size = irq_count - delta;
	else
		delay_size = irq_count + rptr_offset;

	/* unormal case check */
	if (delay_size > (max_prefetch_size + info->cur_tolerance)) {
		dev_info(afe->dev, "%s() invalid delay_size(%d), periods(%d)\n",
			 __func__, delay_size, period);
		dev_info(afe->dev, "%s() invalid rptr_offset(%d), irq_count(%d)\n",
			 __func__, rptr_offset, irq_count);
		dev_info(afe->dev, "%s() max_prefetch_size = %d timestamp = 0x%16llx\n",
			 __func__, max_prefetch_size, memif->timestamp);
		return 0;
	}

	return delay_size;
}

/* FE DAIs */
static const struct snd_soc_dai_ops mt8169_memif_dai_ops = {
	.startup	= mt8169_fe_startup,
	.shutdown	= mt8169_fe_shutdown,
	.hw_params	= mt8169_fe_hw_params,
	.hw_free	= mt8169_fe_hw_free,
	.prepare	= mt8169_fe_prepare,
	.trigger	= mt8169_fe_trigger,
	.delay		= mt8169_fe_delay,
};

#define MTK_PCM_RATES (SNDRV_PCM_RATE_8000_48000 |\
		       SNDRV_PCM_RATE_88200 |\
		       SNDRV_PCM_RATE_96000 |\
		       SNDRV_PCM_RATE_176400 |\
		       SNDRV_PCM_RATE_192000)

#define MTK_PCM_DAI_RATES (SNDRV_PCM_RATE_8000 |\
			   SNDRV_PCM_RATE_16000 |\
			   SNDRV_PCM_RATE_32000 |\
			   SNDRV_PCM_RATE_48000)

#define MTK_PCM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S24_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mt8169_memif_dai_driver[] = {
	/* FE DAIs: memory intefaces to CPU */
	{
		.name = "DL1",
		.id = MT8169_MEMIF_DL1,
		.playback = {
			.stream_name = "DL1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8169_memif_dai_ops,
	},
	{
		.name = "DL12",
		.id = MT8169_MEMIF_DL12,
		.playback = {
			.stream_name = "DL12",
			.channels_min = 1,
			.channels_max = 4,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8169_memif_dai_ops,
	},
	{
		.name = "DL2",
		.id = MT8169_MEMIF_DL2,
		.playback = {
			.stream_name = "DL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8169_memif_dai_ops,
	},
	{
		.name = "DL3",
		.id = MT8169_MEMIF_DL3,
		.playback = {
			.stream_name = "DL3",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8169_memif_dai_ops,
	},
	{
		.name = "DL4",
		.id = MT8169_MEMIF_DL4,
		.playback = {
			.stream_name = "DL4",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8169_memif_dai_ops,
	},
	{
		.name = "DL5",
		.id = MT8169_MEMIF_DL5,
		.playback = {
			.stream_name = "DL5",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8169_memif_dai_ops,
	},
	{
		.name = "DL6",
		.id = MT8169_MEMIF_DL6,
		.playback = {
			.stream_name = "DL6",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8169_memif_dai_ops,
	},
	{
		.name = "DL7",
		.id = MT8169_MEMIF_DL7,
		.playback = {
			.stream_name = "DL7",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8169_memif_dai_ops,
	},
	{
		.name = "DL8",
		.id = MT8169_MEMIF_DL8,
		.playback = {
			.stream_name = "DL8",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8169_memif_dai_ops,
	},
	{
		.name = "UL1",
		.id = MT8169_MEMIF_VUL12,
		.capture = {
			.stream_name = "UL1",
			.channels_min = 1,
			.channels_max = 4,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8169_memif_dai_ops,
	},
	{
		.name = "UL2",
		.id = MT8169_MEMIF_AWB,
		.capture = {
			.stream_name = "UL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8169_memif_dai_ops,
	},
	{
		.name = "UL3",
		.id = MT8169_MEMIF_VUL2,
		.capture = {
			.stream_name = "UL3",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8169_memif_dai_ops,
	},
	{
		.name = "UL4",
		.id = MT8169_MEMIF_AWB2,
		.capture = {
			.stream_name = "UL4",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8169_memif_dai_ops,
	},
	{
		.name = "UL5",
		.id = MT8169_MEMIF_VUL3,
		.capture = {
			.stream_name = "UL5",
			.channels_min = 1,
			.channels_max = 12,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8169_memif_dai_ops,
	},
	{
		.name = "UL6",
		.id = MT8169_MEMIF_VUL4,
		.capture = {
			.stream_name = "UL6",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8169_memif_dai_ops,
	},
	{
		.name = "UL7",
		.id = MT8169_MEMIF_VUL5,
		.capture = {
			.stream_name = "UL7",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8169_memif_dai_ops,
	},
	{
		.name = "UL8",
		.id = MT8169_MEMIF_VUL6,
		.capture = {
			.stream_name = "UL8",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8169_memif_dai_ops,
	}
};

/* kcontrol */
static int mt8169_irq_cnt1_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] =
		afe_priv->irq_cnt[MT8169_PRIMARY_MEMIF];
	return 0;
}

static int mt8169_irq_cnt1_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT8169_PRIMARY_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;
	int irq_cnt = afe_priv->irq_cnt[memif_num];

	dev_info(afe->dev, "%s(), irq_id %d, irq_cnt = %d, value = %ld\n",
		 __func__,
		 irq_id, irq_cnt,
		 ucontrol->value.integer.value[0]);

	if (irq_cnt == ucontrol->value.integer.value[0])
		return 0;

	irq_cnt = ucontrol->value.integer.value[0];
	afe_priv->irq_cnt[memif_num] = irq_cnt;

	if (pm_runtime_status_suspended(afe->dev) || irq_id < 0) {
		dev_info(afe->dev, "%s(), suspended || irq_id %d, not set\n",
			 __func__, irq_id);
	} else {
		struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
		const struct mtk_base_irq_data *irq_data = irqs->irq_data;

		regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit
				   << irq_data->irq_cnt_shift,
				   irq_cnt << irq_data->irq_cnt_shift);
	}

	return 0;
}

static int mt8169_irq_cnt2_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] =
		afe_priv->irq_cnt[MT8169_RECORD_MEMIF];
	return 0;
}

static int mt8169_irq_cnt2_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT8169_RECORD_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;
	int irq_cnt = afe_priv->irq_cnt[memif_num];

	dev_info(afe->dev, "%s(), irq_id %d, irq_cnt = %d, value = %ld\n",
		 __func__,
		 irq_id, irq_cnt,
		 ucontrol->value.integer.value[0]);

	if (irq_cnt == ucontrol->value.integer.value[0])
		return 0;

	irq_cnt = ucontrol->value.integer.value[0];
	afe_priv->irq_cnt[memif_num] = irq_cnt;

	if (pm_runtime_status_suspended(afe->dev) || irq_id < 0) {
		dev_info(afe->dev, "%s(), suspended || irq_id %d, not set\n",
			 __func__, irq_id);
	} else {
		struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
		const struct mtk_base_irq_data *irq_data = irqs->irq_data;

		regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit
				   << irq_data->irq_cnt_shift,
				   irq_cnt << irq_data->irq_cnt_shift);
	}

	return 0;
}

static int mt8169_deep_irq_cnt_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->irq_cnt[MT8169_DEEP_MEMIF];
	return 0;
}

static int mt8169_deep_irq_cnt_set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT8169_DEEP_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;
	int irq_cnt = afe_priv->irq_cnt[memif_num];

	dev_info(afe->dev, "%s(), irq_id %d, irq_cnt = %d, value = %ld\n",
		 __func__,
		 irq_id, irq_cnt,
		 ucontrol->value.integer.value[0]);

	if (irq_cnt == ucontrol->value.integer.value[0])
		return 0;

	irq_cnt = ucontrol->value.integer.value[0];
	afe_priv->irq_cnt[memif_num] = irq_cnt;

	if (pm_runtime_status_suspended(afe->dev) || irq_id < 0) {
		dev_info(afe->dev, "%s(), suspended || irq_id %d, not set\n",
			 __func__, irq_id);
	} else {
		struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
		const struct mtk_base_irq_data *irq_data = irqs->irq_data;

		regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit
				   << irq_data->irq_cnt_shift,
				   irq_cnt << irq_data->irq_cnt_shift);
	}

	return 0;
}

static int mt8169_voip_rx_irq_cnt_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->irq_cnt[MT8169_VOIP_MEMIF];
	return 0;
}

static int mt8169_voip_rx_irq_cnt_set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT8169_VOIP_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;
	int irq_cnt = afe_priv->irq_cnt[memif_num];

	dev_info(afe->dev, "%s(), irq_id %d, irq_cnt = %d, value = %ld\n",
		 __func__,
		 irq_id, irq_cnt,
		 ucontrol->value.integer.value[0]);

	if (irq_cnt == ucontrol->value.integer.value[0])
		return 0;

	irq_cnt = ucontrol->value.integer.value[0];
	afe_priv->irq_cnt[memif_num] = irq_cnt;

	if (pm_runtime_status_suspended(afe->dev) || irq_id < 0) {
		dev_info(afe->dev, "%s(), suspended || irq_id %d, not set\n",
			 __func__, irq_id);
	} else {
		struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
		const struct mtk_base_irq_data *irq_data = irqs->irq_data;

		regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit
				   << irq_data->irq_cnt_shift,
				   irq_cnt << irq_data->irq_cnt_shift);
	}

	return 0;
}

static int mt8169_deep_scene_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->deep_playback_state;
	return 0;
}

static int mt8169_deep_scene_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT8169_DEEP_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->deep_playback_state = ucontrol->value.integer.value[0];

	if (afe_priv->deep_playback_state == 1) {
		memif->ack_enable = true;
#if IS_ENABLED(CONFIG_MTK_ACAO_SUPPORT)
		system_idle_hint_request(SYSTEM_IDLE_HINT_USER_AUDIO, 1);
#endif
	} else {
		memif->ack_enable = false;
#if IS_ENABLED(CONFIG_MTK_ACAO_SUPPORT)
		system_idle_hint_request(SYSTEM_IDLE_HINT_USER_AUDIO, 0);
#endif
	}

	return 0;
}

static int mt8169_fast_scene_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->fast_playback_state;
	return 0;
}

static int mt8169_fast_scene_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT8169_FAST_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->fast_playback_state = ucontrol->value.integer.value[0];

	if ((afe_priv->fast_playback_state == 1) &&
	    (afe_priv->use_sram_force[memif_num] == 0))
		memif->use_dram_only = 1;
	else
		memif->use_dram_only = 0;

	return 0;
}

static int mt8169_primary_scene_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->primary_playback_state;
	return 0;
}

static int mt8169_primary_scene_set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT8169_PRIMARY_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->primary_playback_state = ucontrol->value.integer.value[0];

	if ((afe_priv->primary_playback_state == 1) &&
	    (afe_priv->use_sram_force[memif_num] == 0))
		memif->use_dram_only = 1;
	else
		memif->use_dram_only = 0;

	return 0;
}

static int mt8169_voip_scene_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->voip_rx_state;
	return 0;
}

static int mt8169_voip_scene_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT8169_VOIP_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->voip_rx_state = ucontrol->value.integer.value[0];

	if ((afe_priv->voip_rx_state == 1) &&
	    (afe_priv->use_sram_force[memif_num] == 0))
		memif->use_dram_only = 1;
	else
		memif->use_dram_only = 0;

	return 0;
}

static int mt8169_record_xrun_assert_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	int xrun_assert = afe_priv->xrun_assert[MT8169_RECORD_MEMIF];

	ucontrol->value.integer.value[0] = xrun_assert;
	return 0;
}

static int mt8169_record_xrun_assert_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	int xrun_assert = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), xrun_assert %d\n", __func__, xrun_assert);
	afe_priv->xrun_assert[MT8169_RECORD_MEMIF] = xrun_assert;
	return 0;
}

static int mt8169_echo_ref_xrun_assert_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	int xrun_assert = afe_priv->xrun_assert[MT8169_ECHO_REF_MEMIF];

	ucontrol->value.integer.value[0] = xrun_assert;
	return 0;
}

static int mt8169_echo_ref_xrun_assert_set(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	int xrun_assert = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), xrun_assert %d\n", __func__, xrun_assert);
	afe_priv->xrun_assert[MT8169_ECHO_REF_MEMIF] = xrun_assert;
	return 0;
}

static int mt8169_sram_size_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_audio_sram *sram = afe->sram;

	ucontrol->value.integer.value[0] =
		mtk_audio_sram_get_size(sram, sram->prefer_mode);

	return 0;
}

#if IS_ENABLED(CONFIG_MTK_ION)
static int mt8169_mmap_dl_scene_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->mmap_playback_state;
	return 0;
}

static int mt8169_mmap_dl_scene_set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT8169_MMAP_DL_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->mmap_playback_state = ucontrol->value.integer.value[0];

	if (afe_priv->mmap_playback_state == 1) {
		unsigned long phy_addr;
		void *vir_addr;

		mtk_get_mmap_dl_buffer(&phy_addr, &vir_addr);

		if (phy_addr != 0x0 && vir_addr)
			memif->use_mmap_share_mem = 1;
	} else {
		memif->use_mmap_share_mem = 0;
	}

	dev_info(afe->dev, "%s(), state %d, mem %d\n", __func__,
		 afe_priv->mmap_playback_state, memif->use_mmap_share_mem);
	return 0;
}

static int mt8169_mmap_ul_scene_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->mmap_record_state;
	return 0;
}

static int mt8169_mmap_ul_scene_set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT8169_MMAP_UL_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->mmap_record_state = ucontrol->value.integer.value[0];

	if (afe_priv->mmap_record_state == 1) {
		unsigned long phy_addr;
		void *vir_addr;

		mtk_get_mmap_ul_buffer(&phy_addr, &vir_addr);

		if (phy_addr != 0x0 && vir_addr)
			memif->use_mmap_share_mem = 2;
	} else {
		memif->use_mmap_share_mem = 0;
	}

	dev_info(afe->dev, "%s(), state %d, mem %d\n", __func__,
		 afe_priv->mmap_record_state, memif->use_mmap_share_mem);
	return 0;
}

static int mt8169_mmap_ion_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int mt8169_mmap_ion_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	/* for bring up */
	return 0;
}

static int mt8169_dl_mmap_fd_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT8169_MMAP_DL_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	ucontrol->value.integer.value[0] = (memif->use_mmap_share_mem == 1) ?
					    mtk_get_mmap_dl_fd() : 0;
	dev_info(afe->dev, "%s, fd %ld\n", __func__,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int mt8169_dl_mmap_fd_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int mt8169_ul_mmap_fd_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT8169_MMAP_UL_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	ucontrol->value.integer.value[0] = (memif->use_mmap_share_mem == 2) ?
					    mtk_get_mmap_ul_fd() : 0;
	dev_info(afe->dev, "%s, fd %ld\n", __func__,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int mt8169_ul_mmap_fd_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}
#endif

static int mt8169_afe_runtime_suspend_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->runtime_suspend;

	return 0;
}

static int mt8169_afe_runtime_suspend_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	struct device *dev = afe->dev;
	int ret;

	dev_info(dev, "%s() %d\n", __func__, ucontrol->value.integer.value[0]);

	afe_priv->runtime_suspend = ucontrol->value.integer.value[0];

	if (afe_priv->runtime_suspend)
		ret = pm_runtime_put_sync(dev);
	else
		ret = pm_runtime_get_sync(dev);

	if (ret)
		dev_info(dev, "put_ret:%d, rpm_error:%d\n",
			 ret, dev->power.runtime_error);

	return ret;
}

static const struct snd_kcontrol_new mt8169_pcm_kcontrols[] = {
	SOC_SINGLE_EXT("Audio IRQ1 CNT", SND_SOC_NOPM, 0, 0x3ffff, 0,
		       mt8169_irq_cnt1_get, mt8169_irq_cnt1_set),
	SOC_SINGLE_EXT("Audio IRQ2 CNT", SND_SOC_NOPM, 0, 0x3ffff, 0,
		       mt8169_irq_cnt2_get, mt8169_irq_cnt2_set),
	SOC_SINGLE_EXT("deep_buffer_irq_cnt", SND_SOC_NOPM, 0, 0x3ffff, 0,
		       mt8169_deep_irq_cnt_get, mt8169_deep_irq_cnt_set),
	SOC_SINGLE_EXT("voip_rx_irq_cnt", SND_SOC_NOPM, 0, 0x3ffff, 0,
		       mt8169_voip_rx_irq_cnt_get, mt8169_voip_rx_irq_cnt_set),
	SOC_SINGLE_EXT("deep_buffer_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt8169_deep_scene_get, mt8169_deep_scene_set),
	SOC_SINGLE_EXT("record_xrun_assert", SND_SOC_NOPM, 0, 0x1, 0,
		       mt8169_record_xrun_assert_get,
		       mt8169_record_xrun_assert_set),
	SOC_SINGLE_EXT("echo_ref_xrun_assert", SND_SOC_NOPM, 0, 0x1, 0,
		       mt8169_echo_ref_xrun_assert_get,
		       mt8169_echo_ref_xrun_assert_set),
	SOC_SINGLE_EXT("fast_play_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt8169_fast_scene_get, mt8169_fast_scene_set),
	SOC_SINGLE_EXT("primary_play_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt8169_primary_scene_get, mt8169_primary_scene_set),
	SOC_SINGLE_EXT("voip_rx_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt8169_voip_scene_get, mt8169_voip_scene_set),
	SOC_SINGLE_EXT("sram_size", SND_SOC_NOPM, 0, 0xffffffff, 0,
		       mt8169_sram_size_get, NULL),
#if IS_ENABLED(CONFIG_MTK_ION)
	SOC_SINGLE_EXT("mmap_play_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt8169_mmap_dl_scene_get, mt8169_mmap_dl_scene_set),
	SOC_SINGLE_EXT("mmap_record_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt8169_mmap_ul_scene_get, mt8169_mmap_ul_scene_set),
	SOC_SINGLE_EXT("aaudio_ion",
		       SND_SOC_NOPM, 0, 0xffffffff, 0,
		       mt8169_mmap_ion_get,
		       mt8169_mmap_ion_set),
	SOC_SINGLE_EXT("aaudio_dl_mmap_fd",
		       SND_SOC_NOPM, 0, 0xffffffff, 0,
		       mt8169_dl_mmap_fd_get,
		       mt8169_dl_mmap_fd_set),
	SOC_SINGLE_EXT("aaudio_ul_mmap_fd",
		       SND_SOC_NOPM, 0, 0xffffffff, 0,
		       mt8169_ul_mmap_fd_get,
		       mt8169_ul_mmap_fd_set),
#endif
	SOC_SINGLE_EXT("afe_runtime_suspend", SND_SOC_NOPM, 0, 1, 0,
		       mt8169_afe_runtime_suspend_get,
		       mt8169_afe_runtime_suspend_set),

};

/* dma widget & routes*/
static const struct snd_kcontrol_new memif_ul1_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN21,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN21,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN21,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("TDM_IN_CH1", AFE_CONN21_1,
				    I_TDM_IN_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH1", AFE_CONN21,
				    I_I2S2_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul1_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN22,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN22,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN22,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN22,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("TDM_IN_CH2", AFE_CONN22_1,
				    I_TDM_IN_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH2", AFE_CONN22,
				    I_I2S2_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul1_ch3_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN9,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN9,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN9,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("TDM_IN_CH3", AFE_CONN9_1,
				    I_TDM_IN_CH3, 1, 0),
};

static const struct snd_kcontrol_new memif_ul1_ch4_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN10,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN10,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN10,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN10,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("TDM_IN_CH4", AFE_CONN10_1,
				    I_TDM_IN_CH4, 1, 0),
};

static const struct snd_kcontrol_new memif_ul2_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH1", AFE_CONN5,
				    I_I2S0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN5,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH1", AFE_CONN5,
				    I_DL12_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN5,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN5,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN5_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN5_1,
				    I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN5_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN5,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH1", AFE_CONN5,
				    I_I2S2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH1", AFE_CONN5_1,
				    I_CONNSYS_I2S_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("SRC_1_OUT_CH1", AFE_CONN5_1,
				    I_SRC_1_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul2_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH2", AFE_CONN6,
				    I_I2S0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN6,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH2", AFE_CONN6,
				    I_DL12_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN6,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN6,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN6_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN6_1,
				    I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN6_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN6,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH2", AFE_CONN6,
				    I_I2S2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH2", AFE_CONN6_1,
				    I_CONNSYS_I2S_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("SRC_1_OUT_CH2", AFE_CONN6_1,
				    I_SRC_1_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul3_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH1", AFE_CONN32_1,
				    I_CONNSYS_I2S_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN32,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN32,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN32,
				    I_PCM_1_CAP_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul3_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH2", AFE_CONN33_1,
				    I_CONNSYS_I2S_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN33,
				    I_PCM_1_CAP_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul4_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN38,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH1", AFE_CONN38,
				    I_I2S0_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul4_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN39,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH2", AFE_CONN39,
				    I_I2S0_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul5_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN44,
				    I_ADDA_UL_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul5_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN45,
				    I_ADDA_UL_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul6_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN46,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN46,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH1", AFE_CONN46,
				    I_DL12_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN46_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN46,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN46,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN46_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN46,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("GAIN1_OUT_CH1", AFE_CONN46,
				    I_GAIN1_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul6_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN47,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN47,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH2", AFE_CONN47,
				    I_DL12_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN47_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN47,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN47,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN47_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN47,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("GAIN1_OUT_CH2", AFE_CONN47,
				    I_GAIN1_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul7_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN48,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN2_OUT_CH1", AFE_CONN48,
				    I_GAIN2_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH1", AFE_CONN48_1,
				    I_SRC_2_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul7_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN49,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN2_OUT_CH2", AFE_CONN49,
				    I_GAIN2_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH2", AFE_CONN49_1,
				    I_SRC_2_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul8_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN50,
				    I_ADDA_UL_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul8_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN51,
				    I_ADDA_UL_CH2, 1, 0),
};

static const struct snd_kcontrol_new hw_cm1_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("TDM_IN_CH1", AFE_CONN58_1,
				    I_TDM_IN_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH1", AFE_CONN58,
				    I_I2S0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH1", AFE_CONN58,
				    I_I2S2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN58,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN58,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH1", AFE_CONN58,
				    I_DL12_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH3", AFE_CONN58,
				    I_DL12_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN58,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN58,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN58_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN58_1,
				    I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC1_OUT_CH1", AFE_CONN58_1,
				    I_SRC_1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC2_OUT_CH1", AFE_CONN58_1,
				    I_SRC_2_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new hw_cm1_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("TDM_IN_CH2", AFE_CONN59_1,
				    I_TDM_IN_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH2", AFE_CONN59,
				    I_I2S0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH2", AFE_CONN59,
				    I_I2S2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN59,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN59,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH2", AFE_CONN59,
				    I_DL12_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH4", AFE_CONN59,
				    I_DL12_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN59,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN59,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN59_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN59_1,
				    I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC1_OUT_CH2", AFE_CONN59_1,
				    I_SRC_1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC2_OUT_CH2", AFE_CONN59_1,
				    I_SRC_2_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new hw_cm1_ch3_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("TDM_IN_CH3", AFE_CONN60_1,
				    I_TDM_IN_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH1", AFE_CONN60,
				    I_I2S0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH1", AFE_CONN60,
				    I_I2S2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN60,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN60,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH1", AFE_CONN60,
				    I_DL12_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH3", AFE_CONN60,
				    I_DL12_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN60,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN60,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN60_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN60_1,
				    I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC1_OUT_CH1", AFE_CONN60_1,
				    I_SRC_1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC2_OUT_CH1", AFE_CONN60_1,
				    I_SRC_2_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new hw_cm1_ch4_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("TDM_IN_CH4", AFE_CONN61_1,
				    I_TDM_IN_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH2", AFE_CONN61,
				    I_I2S0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH2", AFE_CONN61,
				    I_I2S2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN61,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN61,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH2", AFE_CONN61,
				    I_DL12_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH4", AFE_CONN61,
				    I_DL12_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN61,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN61,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN61_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN61_1,
				    I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC1_OUT_CH2", AFE_CONN61_1,
				    I_SRC_1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC2_OUT_CH2", AFE_CONN61_1,
				    I_SRC_2_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new hw_cm1_ch5_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("TDM_IN_CH5", AFE_CONN62_1,
				    I_TDM_IN_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH1", AFE_CONN62,
				    I_I2S0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH1", AFE_CONN62,
				    I_I2S2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN62,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN62,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH1", AFE_CONN62,
				    I_DL12_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH3", AFE_CONN62,
				    I_DL12_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN62,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN62,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN62_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN62_1,
				    I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC1_OUT_CH1", AFE_CONN62_1,
				    I_SRC_1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC2_OUT_CH1", AFE_CONN62_1,
				    I_SRC_2_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new hw_cm1_ch6_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("TDM_IN_CH6", AFE_CONN63_1,
				    I_TDM_IN_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH2", AFE_CONN63,
				    I_I2S0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH2", AFE_CONN63,
				    I_I2S2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN63,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN63,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH2", AFE_CONN63,
				    I_DL12_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH4", AFE_CONN63,
				    I_DL12_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN63,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN63,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN63_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN63_1,
				    I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC1_OUT_CH2", AFE_CONN63_1,
				    I_SRC_1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC2_OUT_CH2", AFE_CONN63_1,
				    I_SRC_2_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new hw_cm1_ch7_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("TDM_IN_CH7", AFE_CONN64_1,
				    I_TDM_IN_CH7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH1", AFE_CONN64,
				    I_I2S0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH1", AFE_CONN64,
				    I_I2S2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN64,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN64,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH1", AFE_CONN64,
				    I_DL12_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH3", AFE_CONN64,
				    I_DL12_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN64,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN64,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN64_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN64_1,
				    I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC1_OUT_CH1", AFE_CONN64_1,
				    I_SRC_1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC2_OUT_CH1", AFE_CONN64_1,
				    I_SRC_2_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new hw_cm1_ch8_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("TDM_IN_CH8", AFE_CONN65_1,
				    I_TDM_IN_CH8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH2", AFE_CONN65,
				    I_I2S0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH2", AFE_CONN65,
				    I_I2S2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN65,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN65,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH2", AFE_CONN65,
				    I_DL12_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH4", AFE_CONN65,
				    I_DL12_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN65,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN65,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN65_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN65_1,
				    I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC1_OUT_CH2", AFE_CONN65_1,
				    I_SRC_1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC2_OUT_CH2", AFE_CONN65_1,
				    I_SRC_2_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new hw_cm1_ch9_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH1", AFE_CONN66,
				    I_I2S0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH1", AFE_CONN66,
				    I_I2S2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN66,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN66,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH1", AFE_CONN66,
				    I_DL12_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH3", AFE_CONN66,
				    I_DL12_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN66,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN66,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN66_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN66_1,
				    I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC1_OUT_CH1", AFE_CONN66_1,
				    I_SRC_1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC2_OUT_CH1", AFE_CONN66_1,
				    I_SRC_2_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new hw_cm1_ch10_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH2", AFE_CONN67,
				    I_I2S0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH2", AFE_CONN67,
				    I_I2S2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN67,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN67,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH2", AFE_CONN67,
				    I_DL12_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH4", AFE_CONN67,
				    I_DL12_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN67,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN67,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN67_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN67_1,
				    I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC1_OUT_CH2", AFE_CONN67_1,
				    I_SRC_1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC2_OUT_CH2", AFE_CONN67_1,
				    I_SRC_2_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new hw_cm1_ch11_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH1", AFE_CONN68,
				    I_I2S0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH1", AFE_CONN68,
				    I_I2S2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN68,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN68,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH1", AFE_CONN68,
				    I_DL12_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH3", AFE_CONN68,
				    I_DL12_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN68,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN68,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN68_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN68_1,
				    I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC1_OUT_CH1", AFE_CONN68_1,
				    I_SRC_1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC2_OUT_CH1", AFE_CONN68_1,
				    I_SRC_2_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new hw_cm1_ch12_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH2", AFE_CONN69,
				    I_I2S0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH2", AFE_CONN69,
				    I_I2S2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN69,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN69,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH2", AFE_CONN69,
				    I_DL12_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH4", AFE_CONN69,
				    I_DL12_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN69,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN69,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN69_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN69_1,
				    I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC1_OUT_CH2", AFE_CONN69_1,
				    I_SRC_1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC2_OUT_CH2", AFE_CONN69_1,
				    I_SRC_2_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_dsp_dl_playback_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL1", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL2", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL12", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL6", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL3", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL4", SND_SOC_NOPM, 0, 1, 0),
};

/* ADDA UL MUX */
enum {
	UL5_IN_MUX_CM1 = 0,
	UL5_IN_MUX_NORMAL,
	UL5_IN_MUX_MASK = 0x1,
};

static const char * const ul5_in_mux_map[] = {
	"UL5_IN_FROM_CM1", "UL5_IN_FROM_Normal"
};

static int ul5_in_map_value[] = {
	UL5_IN_MUX_CM1,
	UL5_IN_MUX_NORMAL,
};

static SOC_VALUE_ENUM_SINGLE_DECL(ul5_in_mux_map_enum,
				  AFE_CM1_CON,
				  VUL3_BYPASS_CM_SFT,
				  VUL3_BYPASS_CM_MASK,
				  ul5_in_mux_map,
				  ul5_in_map_value);

static const struct snd_kcontrol_new ul5_in_mux_control =
	SOC_DAPM_ENUM("UL5_IN_MUX Select", ul5_in_mux_map_enum);

static const struct snd_soc_dapm_widget mt8169_memif_widgets[] = {
	/* inter-connections */
	SND_SOC_DAPM_MIXER("UL1_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul1_ch1_mix, ARRAY_SIZE(memif_ul1_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL1_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul1_ch2_mix, ARRAY_SIZE(memif_ul1_ch2_mix)),
	SND_SOC_DAPM_MIXER("UL1_CH3", SND_SOC_NOPM, 0, 0,
			   memif_ul1_ch3_mix, ARRAY_SIZE(memif_ul1_ch3_mix)),
	SND_SOC_DAPM_MIXER("UL1_CH4", SND_SOC_NOPM, 0, 0,
			   memif_ul1_ch4_mix, ARRAY_SIZE(memif_ul1_ch4_mix)),

	SND_SOC_DAPM_MIXER("UL2_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul2_ch1_mix, ARRAY_SIZE(memif_ul2_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL2_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul2_ch2_mix, ARRAY_SIZE(memif_ul2_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL3_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul3_ch1_mix, ARRAY_SIZE(memif_ul3_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL3_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul3_ch2_mix, ARRAY_SIZE(memif_ul3_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL4_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul4_ch1_mix, ARRAY_SIZE(memif_ul4_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL4_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul4_ch2_mix, ARRAY_SIZE(memif_ul4_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL5_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul5_ch1_mix, ARRAY_SIZE(memif_ul5_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL5_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul5_ch2_mix, ARRAY_SIZE(memif_ul5_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL6_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul6_ch1_mix, ARRAY_SIZE(memif_ul6_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL6_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul6_ch2_mix, ARRAY_SIZE(memif_ul6_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL7_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul7_ch1_mix, ARRAY_SIZE(memif_ul7_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL7_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul7_ch2_mix, ARRAY_SIZE(memif_ul7_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL8_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul8_ch1_mix, ARRAY_SIZE(memif_ul8_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL8_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul8_ch2_mix, ARRAY_SIZE(memif_ul8_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL5_2CH", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("HW_CM1", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("HW_CM1_CH1", SND_SOC_NOPM, 0, 0,
			   hw_cm1_ch1_mix, ARRAY_SIZE(hw_cm1_ch1_mix)),
	SND_SOC_DAPM_MIXER("HW_CM1_CH2", SND_SOC_NOPM, 0, 0,
			   hw_cm1_ch2_mix, ARRAY_SIZE(hw_cm1_ch2_mix)),
	SND_SOC_DAPM_MIXER("HW_CM1_CH3", SND_SOC_NOPM, 0, 0,
			   hw_cm1_ch3_mix, ARRAY_SIZE(hw_cm1_ch3_mix)),
	SND_SOC_DAPM_MIXER("HW_CM1_CH4", SND_SOC_NOPM, 0, 0,
			   hw_cm1_ch4_mix, ARRAY_SIZE(hw_cm1_ch4_mix)),
	SND_SOC_DAPM_MIXER("HW_CM1_CH5", SND_SOC_NOPM, 0, 0,
			   hw_cm1_ch5_mix, ARRAY_SIZE(hw_cm1_ch5_mix)),
	SND_SOC_DAPM_MIXER("HW_CM1_CH6", SND_SOC_NOPM, 0, 0,
			   hw_cm1_ch6_mix, ARRAY_SIZE(hw_cm1_ch6_mix)),
	SND_SOC_DAPM_MIXER("HW_CM1_CH7", SND_SOC_NOPM, 0, 0,
			   hw_cm1_ch7_mix, ARRAY_SIZE(hw_cm1_ch7_mix)),
	SND_SOC_DAPM_MIXER("HW_CM1_CH8", SND_SOC_NOPM, 0, 0,
			   hw_cm1_ch8_mix, ARRAY_SIZE(hw_cm1_ch8_mix)),
	SND_SOC_DAPM_MIXER("HW_CM1_CH9", SND_SOC_NOPM, 0, 0,
			   hw_cm1_ch9_mix, ARRAY_SIZE(hw_cm1_ch9_mix)),
	SND_SOC_DAPM_MIXER("HW_CM1_CH10", SND_SOC_NOPM, 0, 0,
			   hw_cm1_ch10_mix, ARRAY_SIZE(hw_cm1_ch10_mix)),
	SND_SOC_DAPM_MIXER("HW_CM1_CH11", SND_SOC_NOPM, 0, 0,
			   hw_cm1_ch11_mix, ARRAY_SIZE(hw_cm1_ch11_mix)),
	SND_SOC_DAPM_MIXER("HW_CM1_CH12", SND_SOC_NOPM, 0, 0,
			   hw_cm1_ch12_mix, ARRAY_SIZE(hw_cm1_ch12_mix)),

	SND_SOC_DAPM_MIXER("DSP_DL", SND_SOC_NOPM, 0, 0,
			   mtk_dsp_dl_playback_mix,
			   ARRAY_SIZE(mtk_dsp_dl_playback_mix)),
#if IS_ENABLED(CONFIG_MTK_HIFIXDSP_SUPPORT)
	SND_SOC_DAPM_MIXER("DSP_DL3_VIRT", SND_SOC_NOPM, 0, 0, NULL, 0),
#endif
	SND_SOC_DAPM_MUX("UL5_IN_MUX", SND_SOC_NOPM, 0, 0,
			 &ul5_in_mux_control),

	SND_SOC_DAPM_INPUT("UL1_VIRTUAL_INPUT"),
	SND_SOC_DAPM_INPUT("UL2_VIRTUAL_INPUT"),
	SND_SOC_DAPM_INPUT("UL3_VIRTUAL_INPUT"),
	SND_SOC_DAPM_INPUT("UL4_VIRTUAL_INPUT"),
	SND_SOC_DAPM_INPUT("UL5_VIRTUAL_INPUT"),
	SND_SOC_DAPM_INPUT("UL6_VIRTUAL_INPUT"),

	SND_SOC_DAPM_OUTPUT("DL_TO_DSP"),
};

static const struct snd_soc_dapm_route mt8169_memif_routes[] = {
	{"UL1", NULL, "UL1_CH1"},
	{"UL1", NULL, "UL1_CH2"},
	{"UL1", NULL, "UL1_CH3"},
	{"UL1", NULL, "UL1_CH4"},
	{"UL1_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL1_CH1", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL1_CH2", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL1_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL1_CH3", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL1_CH3", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL1_CH4", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL1_CH4", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL1_CH1", "TDM_IN_CH1", "TDM IN"},
	{"UL1_CH2", "TDM_IN_CH2", "TDM IN"},
	{"UL1_CH3", "TDM_IN_CH3", "TDM IN"},
	{"UL1_CH4", "TDM_IN_CH4", "TDM IN"},
	{"UL1_CH1", "I2S2_CH1", "I2S2"},
	{"UL1_CH2", "I2S2_CH2", "I2S2"},

	{"UL2", NULL, "UL2_CH1"},
	{"UL2", NULL, "UL2_CH2"},

	/* cannot connect FE to FE directly */
	{"UL2_CH1", "DL1_CH1", "Hostless_UL2 UL"},
	{"UL2_CH2", "DL1_CH2", "Hostless_UL2 UL"},
	{"UL2_CH1", "DL12_CH1", "Hostless_UL2 UL"},
	{"UL2_CH2", "DL12_CH2", "Hostless_UL2 UL"},
	{"UL2_CH1", "DL6_CH1", "Hostless_UL2 UL"},
	{"UL2_CH2", "DL6_CH2", "Hostless_UL2 UL"},
	{"UL2_CH1", "DL2_CH1", "Hostless_UL2 UL"},
	{"UL2_CH2", "DL2_CH2", "Hostless_UL2 UL"},
	{"UL2_CH1", "DL3_CH1", "Hostless_UL2 UL"},
	{"UL2_CH2", "DL3_CH2", "Hostless_UL2 UL"},
	{"UL2_CH1", "DL4_CH1", "Hostless_UL2 UL"},
	{"UL2_CH2", "DL4_CH2", "Hostless_UL2 UL"},
	{"UL2_CH1", "DL5_CH1", "Hostless_UL2 UL"},
	{"UL2_CH2", "DL5_CH2", "Hostless_UL2 UL"},

	{"Hostless_UL2 UL", NULL, "UL2_VIRTUAL_INPUT"},

	{"UL2_CH1", "I2S0_CH1", "I2S0"},
	{"UL2_CH2", "I2S0_CH2", "I2S0"},
	{"UL2_CH1", "I2S2_CH1", "I2S2"},
	{"UL2_CH2", "I2S2_CH2", "I2S2"},

	{"UL2_CH1", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL2_CH2", "PCM_1_CAP_CH1", "PCM 1 Capture"},

	{"UL2_CH1", "CONNSYS_I2S_CH1", "Connsys I2S"},
	{"UL2_CH2", "CONNSYS_I2S_CH2", "Connsys I2S"},

	{"UL2_CH1", "SRC_1_OUT_CH1", "HW_SRC_1_Out"},
	{"UL2_CH2", "SRC_1_OUT_CH2", "HW_SRC_1_Out"},

	{"UL3", NULL, "UL3_CH1"},
	{"UL3", NULL, "UL3_CH2"},
	{"UL3_CH1", "CONNSYS_I2S_CH1", "Connsys I2S"},
	{"UL3_CH2", "CONNSYS_I2S_CH2", "Connsys I2S"},

	{"UL3_CH1", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL3_CH2", "PCM_1_CAP_CH2", "PCM 1 Capture"},

	{"UL4", NULL, "UL4_CH1"},
	{"UL4", NULL, "UL4_CH2"},
	{"UL4_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL4_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL4_CH1", "I2S0_CH1", "I2S0"},
	{"UL4_CH2", "I2S0_CH2", "I2S0"},

	{"UL5", NULL, "UL5_IN_MUX"},
	{"UL5_IN_MUX", "UL5_IN_FROM_Normal", "UL5_2CH"},
	{"UL5_IN_MUX", "UL5_IN_FROM_CM1", "HW_CM1"},
	{"UL5_2CH", NULL, "UL5_CH1"},
	{"UL5_2CH", NULL, "UL5_CH2"},
	{"UL5_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL5_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"HW_CM1", NULL, "HW_CM1_CH1"},
	{"HW_CM1", NULL, "HW_CM1_CH2"},
	{"HW_CM1", NULL, "HW_CM1_CH3"},
	{"HW_CM1", NULL, "HW_CM1_CH4"},
	{"HW_CM1", NULL, "HW_CM1_CH5"},
	{"HW_CM1", NULL, "HW_CM1_CH6"},
	{"HW_CM1", NULL, "HW_CM1_CH7"},
	{"HW_CM1", NULL, "HW_CM1_CH8"},
	{"HW_CM1", NULL, "HW_CM1_CH9"},
	{"HW_CM1", NULL, "HW_CM1_CH10"},
	{"HW_CM1", NULL, "HW_CM1_CH11"},
	{"HW_CM1", NULL, "HW_CM1_CH12"},

	{"HW_CM1_CH1", "TDM_IN_CH1", "TDM IN"},
	{"HW_CM1_CH2", "TDM_IN_CH2", "TDM IN"},
	{"HW_CM1_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"HW_CM1_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"HW_CM1_CH1", "I2S0_CH1", "I2S0"},
	{"HW_CM1_CH2", "I2S0_CH2", "I2S0"},
	{"HW_CM1_CH1", "I2S2_CH1", "I2S2"},
	{"HW_CM1_CH2", "I2S2_CH2", "I2S2"},

	{"HW_CM1_CH3", "TDM_IN_CH3", "TDM IN"},
	{"HW_CM1_CH4", "TDM_IN_CH4", "TDM IN"},
	{"HW_CM1_CH3", "DL1_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH4", "DL1_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH3", "DL12_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH4", "DL12_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH3", "DL12_CH3", "Hostless_UL5 UL"},
	{"HW_CM1_CH4", "DL12_CH4", "Hostless_UL5 UL"},
	{"HW_CM1_CH3", "DL2_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH4", "DL2_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH3", "DL3_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH4", "DL3_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH3", "DL4_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH4", "DL4_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH3", "DL5_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH4", "DL5_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH3", "HW_SRC1_OUT_CH1", "HW_SRC_1_Out"},
	{"HW_CM1_CH4", "HW_SRC1_OUT_CH2", "HW_SRC_1_Out"},
	{"HW_CM1_CH3", "HW_SRC2_OUT_CH1", "HW_SRC_2_Out"},
	{"HW_CM1_CH4", "HW_SRC2_OUT_CH2", "HW_SRC_2_Out"},

	{"HW_CM1_CH5", "TDM_IN_CH5", "TDM IN"},
	{"HW_CM1_CH6", "TDM_IN_CH6", "TDM IN"},
	{"HW_CM1_CH5", "DL1_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH6", "DL1_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH5", "DL12_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH6", "DL12_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH5", "DL12_CH3", "Hostless_UL5 UL"},
	{"HW_CM1_CH6", "DL12_CH4", "Hostless_UL5 UL"},
	{"HW_CM1_CH5", "DL2_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH6", "DL2_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH5", "DL3_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH6", "DL3_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH5", "DL4_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH6", "DL4_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH5", "DL5_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH6", "DL5_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH5", "HW_SRC1_OUT_CH1", "HW_SRC_1_Out"},
	{"HW_CM1_CH6", "HW_SRC1_OUT_CH2", "HW_SRC_1_Out"},
	{"HW_CM1_CH5", "HW_SRC2_OUT_CH1", "HW_SRC_2_Out"},
	{"HW_CM1_CH6", "HW_SRC2_OUT_CH2", "HW_SRC_2_Out"},

	{"HW_CM1_CH7", "TDM_IN_CH7", "TDM IN"},
	{"HW_CM1_CH8", "TDM_IN_CH8", "TDM IN"},
	{"HW_CM1_CH7", "DL1_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH8", "DL1_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH7", "DL12_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH8", "DL12_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH7", "DL12_CH3", "Hostless_UL5 UL"},
	{"HW_CM1_CH8", "DL12_CH4", "Hostless_UL5 UL"},
	{"HW_CM1_CH7", "DL2_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH8", "DL2_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH7", "DL3_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH8", "DL3_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH7", "DL4_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH8", "DL4_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH7", "DL5_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH8", "DL5_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH7", "HW_SRC1_OUT_CH1", "HW_SRC_1_Out"},
	{"HW_CM1_CH8", "HW_SRC1_OUT_CH2", "HW_SRC_1_Out"},
	{"HW_CM1_CH7", "HW_SRC2_OUT_CH1", "HW_SRC_2_Out"},
	{"HW_CM1_CH8", "HW_SRC2_OUT_CH2", "HW_SRC_2_Out"},

	{"HW_CM1_CH9", "DL1_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH10", "DL1_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH9", "DL12_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH10", "DL12_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH9", "DL12_CH3", "Hostless_UL5 UL"},
	{"HW_CM1_CH10", "DL12_CH4", "Hostless_UL5 UL"},
	{"HW_CM1_CH9", "HW_SRC1_OUT_CH1", "HW_SRC_1_Out"},
	{"HW_CM1_CH10", "HW_SRC1_OUT_CH2", "HW_SRC_1_Out"},
	{"HW_CM1_CH9", "HW_SRC2_OUT_CH1", "HW_SRC_2_Out"},
	{"HW_CM1_CH10", "HW_SRC2_OUT_CH2", "HW_SRC_2_Out"},
	{"HW_CM1_CH9", "DL5_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH10", "DL5_CH2", "Hostless_UL5 UL"},

	{"HW_CM1_CH11", "DL12_CH1", "Hostless_UL5 UL"},
	{"HW_CM1_CH12", "DL12_CH2", "Hostless_UL5 UL"},
	{"HW_CM1_CH11", "DL12_CH3", "Hostless_UL5 UL"},
	{"HW_CM1_CH12", "DL12_CH4", "Hostless_UL5 UL"},
	{"HW_CM1_CH11", "HW_SRC1_OUT_CH1", "HW_SRC_1_Out"},
	{"HW_CM1_CH12", "HW_SRC1_OUT_CH2", "HW_SRC_1_Out"},
	{"HW_CM1_CH11", "HW_SRC2_OUT_CH1", "HW_SRC_2_Out"},
	{"HW_CM1_CH12", "HW_SRC2_OUT_CH2", "HW_SRC_2_Out"},

	{"Hostless_UL5 UL", NULL, "UL5_VIRTUAL_INPUT"},

	{"UL6", NULL, "UL6_CH1"},
	{"UL6", NULL, "UL6_CH2"},

	{"UL6_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL6_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL6_CH1", "DL1_CH1", "Hostless_UL6 UL"},
	{"UL6_CH2", "DL1_CH2", "Hostless_UL6 UL"},
	{"UL6_CH1", "DL2_CH1", "Hostless_UL6 UL"},
	{"UL6_CH2", "DL2_CH2", "Hostless_UL6 UL"},
	{"UL6_CH1", "DL12_CH1", "Hostless_UL6 UL"},
	{"UL6_CH2", "DL12_CH2", "Hostless_UL6 UL"},
	{"UL6_CH1", "DL6_CH1", "Hostless_UL6 UL"},
	{"UL6_CH2", "DL6_CH2", "Hostless_UL6 UL"},
	{"UL6_CH1", "DL3_CH1", "Hostless_UL6 UL"},
	{"UL6_CH2", "DL3_CH2", "Hostless_UL6 UL"},
	{"UL6_CH1", "DL4_CH1", "Hostless_UL6 UL"},
	{"UL6_CH2", "DL4_CH2", "Hostless_UL6 UL"},
	{"Hostless_UL6 UL", NULL, "UL6_VIRTUAL_INPUT"},
	{"UL6_CH1", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL6_CH2", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL6_CH1", "GAIN1_OUT_CH1", "HW Gain 1 Out"},
	{"UL6_CH2", "GAIN1_OUT_CH2", "HW Gain 1 Out"},

	{"UL7", NULL, "UL7_CH1"},
	{"UL7", NULL, "UL7_CH2"},
	{"UL7_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL7_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL7_CH1", "HW_GAIN2_OUT_CH1", "HW Gain 2 Out"},
	{"UL7_CH2", "HW_GAIN2_OUT_CH2", "HW Gain 2 Out"},
	{"UL7_CH1", "HW_SRC_2_OUT_CH1", "HW_SRC_2_Out"},
	{"UL7_CH2", "HW_SRC_2_OUT_CH2", "HW_SRC_2_Out"},

	{"UL8", NULL, "UL8_CH1"},
	{"UL8", NULL, "UL8_CH2"},
	{"UL8_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL8_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},


	{"HW_GAIN2_IN_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"HW_GAIN2_IN_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
};

static const int memif_irq_usage[MT8169_MEMIF_NUM] = {
	/* TODO: verify each memif & irq */
	[MT8169_MEMIF_DL1] = MT8169_IRQ_0,
	[MT8169_MEMIF_DL2] = MT8169_IRQ_1,
	[MT8169_MEMIF_DL3] = MT8169_IRQ_2,
	[MT8169_MEMIF_DL4] = MT8169_IRQ_3,
	[MT8169_MEMIF_DL5] = MT8169_IRQ_4,
	[MT8169_MEMIF_DL6] = MT8169_IRQ_5,
	[MT8169_MEMIF_DL7] = MT8169_IRQ_6,
	[MT8169_MEMIF_DL8] = MT8169_IRQ_7,
	[MT8169_MEMIF_DL12] = MT8169_IRQ_9,
	[MT8169_MEMIF_VUL12] = MT8169_IRQ_10,
	[MT8169_MEMIF_VUL2] = MT8169_IRQ_11,
	[MT8169_MEMIF_AWB] = MT8169_IRQ_12,
	[MT8169_MEMIF_AWB2] = MT8169_IRQ_13,
	[MT8169_MEMIF_VUL3] = MT8169_IRQ_14,
	[MT8169_MEMIF_VUL4] = MT8169_IRQ_15,
	[MT8169_MEMIF_VUL5] = MT8169_IRQ_16,
	[MT8169_MEMIF_VUL6] = MT8169_IRQ_17,
};

static bool mt8169_is_volatile_reg(struct device *dev, unsigned int reg)
{
	/* these auto-gen reg has read-only bit, so put it as volatile */
	/* volatile reg cannot be cached, so cannot be set when power off */
	switch (reg) {
	case AUDIO_TOP_CON0:	/* reg bit controlled by CCF */
	case AUDIO_TOP_CON1:	/* reg bit controlled by CCF */
	case AUDIO_TOP_CON2:
	case AUDIO_TOP_CON3:
	case AFE_DL1_CUR_MSB:
	case AFE_DL1_CUR:
	case AFE_DL2_CUR_MSB:
	case AFE_DL2_CUR:
	case AFE_DL3_CUR_MSB:
	case AFE_DL3_CUR:
	case AFE_DL4_CUR_MSB:
	case AFE_DL4_CUR:
	case AFE_DL12_CUR_MSB:
	case AFE_DL12_CUR:
	case AFE_ADDA_SRC_DEBUG_MON0:
	case AFE_ADDA_SRC_DEBUG_MON1:
	case AFE_ADDA_UL_SRC_MON0:
	case AFE_ADDA_UL_SRC_MON1:
	case AFE_SECURE_CON0:
	case AFE_SRAM_BOUND:
	case AFE_SECURE_CON1:
	case AFE_VUL_CUR_MSB:
	case AFE_VUL_CUR:
	case AFE_SIDETONE_MON:
	case AFE_SIDETONE_CON0:
	case AFE_SIDETONE_COEFF:
	case AFE_VUL2_CUR_MSB:
	case AFE_VUL2_CUR:
	case AFE_VUL3_CUR_MSB:
	case AFE_VUL3_CUR:
	case AFE_I2S_MON:
	case AFE_DAC_MON:
	case AFE_IRQ0_MCU_CNT_MON:
	case AFE_IRQ6_MCU_CNT_MON:
	case AFE_VUL4_CUR_MSB:
	case AFE_VUL4_CUR:
	case AFE_VUL12_CUR_MSB:
	case AFE_VUL12_CUR:
	case AFE_IRQ3_MCU_CNT_MON:
	case AFE_IRQ4_MCU_CNT_MON:
	case AFE_IRQ_MCU_STATUS:
	case AFE_IRQ_MCU_CLR:
	case AFE_IRQ_MCU_MON2:
	case AFE_IRQ1_MCU_CNT_MON:
	case AFE_IRQ2_MCU_CNT_MON:
	case AFE_IRQ5_MCU_CNT_MON:
	case AFE_IRQ7_MCU_CNT_MON:
	case AFE_IRQ_MCU_MISS_CLR:
	case AFE_GAIN1_CUR:
	case AFE_GAIN2_CUR:
	case AFE_SRAM_DELSEL_CON1:
	case PCM_INTF_CON2:
	case FPGA_CFG0:
	case FPGA_CFG1:
	case FPGA_CFG2:
	case FPGA_CFG3:
	case AUDIO_TOP_DBG_MON0:
	case AUDIO_TOP_DBG_MON1:
	case AFE_IRQ8_MCU_CNT_MON:
	case AFE_IRQ11_MCU_CNT_MON:
	case AFE_IRQ12_MCU_CNT_MON:
	case AFE_IRQ9_MCU_CNT_MON:
	case AFE_IRQ10_MCU_CNT_MON:
	case AFE_IRQ13_MCU_CNT_MON:
	case AFE_IRQ14_MCU_CNT_MON:
	case AFE_IRQ15_MCU_CNT_MON:
	case AFE_IRQ16_MCU_CNT_MON:
	case AFE_IRQ17_MCU_CNT_MON:
	case AFE_IRQ18_MCU_CNT_MON:
	case AFE_IRQ19_MCU_CNT_MON:
	case AFE_IRQ20_MCU_CNT_MON:
	case AFE_IRQ21_MCU_CNT_MON:
	case AFE_IRQ22_MCU_CNT_MON:
	case AFE_IRQ23_MCU_CNT_MON:
	case AFE_IRQ24_MCU_CNT_MON:
	case AFE_IRQ25_MCU_CNT_MON:
	case AFE_IRQ26_MCU_CNT_MON:
	case AFE_IRQ31_MCU_CNT_MON:
	case AFE_CBIP_MON0:
	case AFE_CBIP_SLV_MUX_MON0:
	case AFE_CBIP_SLV_DECODER_MON0:
	case AFE_ADDA6_MTKAIF_MON0:
	case AFE_ADDA6_MTKAIF_MON1:
	case AFE_AWB_CUR_MSB:
	case AFE_AWB_CUR:
	case AFE_AWB2_CUR_MSB:
	case AFE_AWB2_CUR:
	case AFE_ADDA6_SRC_DEBUG_MON0:
	case AFE_ADD6A_UL_SRC_MON0:
	case AFE_ADDA6_UL_SRC_MON1:
	case AFE_AWB_RCH_MON:
	case AFE_AWB_LCH_MON:
	case AFE_VUL_RCH_MON:
	case AFE_VUL_LCH_MON:
	case AFE_VUL12_RCH_MON:
	case AFE_VUL12_LCH_MON:
	case AFE_VUL2_RCH_MON:
	case AFE_VUL2_LCH_MON:
	case AFE_AWB2_RCH_MON:
	case AFE_AWB2_LCH_MON:
	case AFE_VUL3_RCH_MON:
	case AFE_VUL3_LCH_MON:
	case AFE_VUL4_RCH_MON:
	case AFE_VUL4_LCH_MON:
	case AFE_VUL5_RCH_MON:
	case AFE_VUL5_LCH_MON:
	case AFE_VUL6_RCH_MON:
	case AFE_VUL6_LCH_MON:
	case AFE_DL1_RCH_MON:
	case AFE_DL1_LCH_MON:
	case AFE_DL2_RCH_MON:
	case AFE_DL2_LCH_MON:
	case AFE_DL12_RCH1_MON:
	case AFE_DL12_LCH1_MON:
	case AFE_DL12_RCH2_MON:
	case AFE_DL12_LCH2_MON:
	case AFE_DL3_RCH_MON:
	case AFE_DL3_LCH_MON:
	case AFE_DL4_RCH_MON:
	case AFE_DL4_LCH_MON:
	case AFE_DL5_RCH_MON:
	case AFE_DL5_LCH_MON:
	case AFE_DL6_RCH_MON:
	case AFE_DL6_LCH_MON:
	case AFE_DL7_RCH_MON:
	case AFE_DL7_LCH_MON:
	case AFE_DL8_RCH_MON:
	case AFE_DL8_LCH_MON:
	case AFE_VUL5_CUR_MSB:
	case AFE_VUL5_CUR:
	case AFE_VUL6_CUR_MSB:
	case AFE_VUL6_CUR:
	case AFE_ADDA_DL_SDM_FIFO_MON:
	case AFE_ADDA_DL_SRC_LCH_MON:
	case AFE_ADDA_DL_SRC_RCH_MON:
	case AFE_ADDA_DL_SDM_OUT_MON:
	case AFE_CONNSYS_I2S_MON:
	case AFE_ASRC_2CH_CON0:
	case AFE_ASRC_2CH_CON2:
	case AFE_ASRC_2CH_CON3:
	case AFE_ASRC_2CH_CON4:
	case AFE_ASRC_2CH_CON5:
	case AFE_ASRC_2CH_CON7:
	case AFE_ASRC_2CH_CON8:
	case AFE_ASRC_2CH_CON12:
	case AFE_ASRC_2CH_CON13:
	case AFE_ADDA_MTKAIF_MON0:
	case AFE_ADDA_MTKAIF_MON1:
	case AFE_AUD_PAD_TOP:
	case AFE_DL_NLE_R_MON0:
	case AFE_DL_NLE_R_MON1:
	case AFE_DL_NLE_R_MON2:
	case AFE_DL_NLE_L_MON0:
	case AFE_DL_NLE_L_MON1:
	case AFE_DL_NLE_L_MON2:
	case AFE_GENERAL1_ASRC_2CH_CON0:
	case AFE_GENERAL1_ASRC_2CH_CON2:
	case AFE_GENERAL1_ASRC_2CH_CON3:
	case AFE_GENERAL1_ASRC_2CH_CON4:
	case AFE_GENERAL1_ASRC_2CH_CON5:
	case AFE_GENERAL1_ASRC_2CH_CON7:
	case AFE_GENERAL1_ASRC_2CH_CON8:
	case AFE_GENERAL1_ASRC_2CH_CON12:
	case AFE_GENERAL1_ASRC_2CH_CON13:
	case AFE_GENERAL2_ASRC_2CH_CON0:
	case AFE_GENERAL2_ASRC_2CH_CON2:
	case AFE_GENERAL2_ASRC_2CH_CON3:
	case AFE_GENERAL2_ASRC_2CH_CON4:
	case AFE_GENERAL2_ASRC_2CH_CON5:
	case AFE_GENERAL2_ASRC_2CH_CON7:
	case AFE_GENERAL2_ASRC_2CH_CON8:
	case AFE_GENERAL2_ASRC_2CH_CON12:
	case AFE_GENERAL2_ASRC_2CH_CON13:
	case AFE_DL5_CUR_MSB:
	case AFE_DL5_CUR:
	case AFE_DL6_CUR_MSB:
	case AFE_DL6_CUR:
	case AFE_DL7_CUR_MSB:
	case AFE_DL7_CUR:
	case AFE_DL8_CUR_MSB:
	case AFE_DL8_CUR:
	case AFE_PROT_SIDEBAND_MON:
	case AFE_DOMAIN_SIDEBAND0_MON:
	case AFE_DOMAIN_SIDEBAND1_MON:
	case AFE_DOMAIN_SIDEBAND2_MON:
	case AFE_DOMAIN_SIDEBAND3_MON:
	case AFE_APLL1_TUNER_CFG:	/* [20:31] is monitor */
	case AFE_APLL2_TUNER_CFG:	/* [20:31] is monitor */
	/* these reg would change in scp/adsp */
	case AFE_DAC_CON0:
	case AFE_IRQ_MCU_CON0:
	case AFE_IRQ_MCU_EN:
	case AFE_IRQ_MCU_DSP_EN:
	case AFE_IRQ_MCU_SCP_EN:
		return true;
	default:
		return false;
	};
}

static const struct regmap_config mt8169_afe_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,

	.volatile_reg = mt8169_is_volatile_reg,

	.max_register = AFE_MAX_REGISTER,
	.num_reg_defaults_raw = AFE_MAX_REGISTER,

	.cache_type = REGCACHE_FLAT,
};

#if IS_ENABLED(CONFIG_MTK_HIFIXDSP_SUPPORT)
struct mtk_base_afe *mt8169_afe_pcm_get_info(void)
{
	return g_priv;
}
EXPORT_SYMBOL_GPL(mt8169_afe_pcm_get_info);

static int mt8169_adsp_set_afe_memif(struct mtk_base_afe *afe,
				     int memif_id,
				     unsigned int rate,
				     unsigned int channels,
				     snd_pcm_format_t format)
{
	int ret;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_id];

	if (memif_id == MT8169_MEMIF_VUL3) {
		int bypass_cm = 0;
		int update_cnt = 8;
		unsigned int val = 0;
		unsigned int mask = 0;
		int fs_mode = mt8169_rate_transform(afe->dev, rate, memif_id);

		/* set rate, channel, update cnt, disable sgen */
		val = fs_mode << CM1_FS_SELECT_SFT |
			(channels - 1) << CHANNEL_MERGE0_CHNUM_SFT |
			update_cnt << CHANNEL_MERGE0_UPDATE_CNT_SFT |
			0 << CHANNEL_MERGE0_DEBUG_MODE_SFT |
			0 << CM1_DEBUG_MODE_SEL_SFT |
			bypass_cm << VUL3_BYPASS_CM_SFT;
		mask = CM1_FS_SELECT_MASK_SFT |
			CHANNEL_MERGE0_CHNUM_MASK_SFT |
			CHANNEL_MERGE0_UPDATE_CNT_MASK_SFT|
			CHANNEL_MERGE0_DEBUG_MODE_MASK_SFT|
			CM1_DEBUG_MODE_SEL_MASK_SFT |
			VUL3_BYPASS_CM_MASK_SFT;
		regmap_update_bits(afe->regmap, AFE_CM1_CON, mask, val);
	}

	/* set addr */
	ret = mtk_memif_set_addr(afe, memif_id,
				 NULL,
				 memif->phys_buf_addr,
				 memif->buffer_size);
	if (ret) {
		dev_info(afe->dev, "%s(), error, id %d, set addr, ret %d\n",
			__func__, memif_id, ret);
		return ret;
	}

	/* set channel */
	ret = mtk_memif_set_channel(afe, memif_id, channels);
	if (ret) {
		dev_info(afe->dev, "%s(), error, id %d, set channel %d, ret %d\n",
			__func__, memif_id, channels, ret);
		return ret;
	}

	/* set rate */
	ret = mtk_memif_set_rate(afe, memif_id, rate);
	if (ret) {
		dev_info(afe->dev, "%s(), error, id %d, set rate %d, ret %d\n",
			__func__, memif_id, rate, ret);
		return ret;
	}

	/* set format */
	ret = mtk_memif_set_format(afe, memif_id, format);
	if (ret) {
		dev_info(afe->dev, "%s(), error, id %d, set format %d, ret %d\n",
			__func__, memif_id, format, ret);
		return ret;
	}

	return 0;

}

static int mt8169_adsp_afe_memif_enable(struct mtk_base_afe *afe,
					int memif_id,
					unsigned int rate,
					unsigned int period_size,
					int enable)
{
	int fs_mode;
	unsigned int counter = period_size;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_id];
	struct mtk_base_afe_irq *irqs = &afe->irqs[memif->irq_usage];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;

	dev_info(afe->dev, "%s memif %d %s\n", __func__, memif_id,
		enable?"enable":"disable");

	if (enable) {
		if (memif->data->agent_disable_shift >= 0)
			regmap_update_bits(afe->regmap,
					   memif->data->agent_disable_reg,
					   1 << memif->data->agent_disable_shift,
					   0 << memif->data->agent_disable_shift);
		/* mt8169 channel merge should enable after UL agent,
		 * so we move the enable to DSP side
		 */
		/*
		if (memif_id == MT8169_MEMIF_VUL3)
			regmap_update_bits(afe->regmap,
					   AFE_CM1_CON,
					   CHANNEL_MERGE0_EN_MASK_SFT,
					   enable << CHANNEL_MERGE0_EN_SFT);
		*/

		/* set irq counter */
		if (irq_data->irq_cnt_reg >= 0)
			regmap_update_bits(afe->regmap,
					   irq_data->irq_cnt_reg,
					   irq_data->irq_cnt_maskbit
					   << irq_data->irq_cnt_shift,
					   counter << irq_data->irq_cnt_shift);
		/* set irq fs */
		fs_mode = mt8169_rate_transform(afe->dev, rate, memif_id);

		if (fs_mode < 0)
			return -EINVAL;

		if (irq_data->irq_fs_reg >= 0)
			regmap_update_bits(afe->regmap,
					   irq_data->irq_fs_reg,
					   irq_data->irq_fs_maskbit
					   << irq_data->irq_fs_shift,
					   fs_mode << irq_data->irq_fs_shift);
	} else {
		/* mt8169 channel merge should disable after UL agent,
		 * so we move the disable to DSP side
		 */
		/*
		if (memif_id == MT8169_MEMIF_VUL3)
			regmap_update_bits(afe->regmap,
					   AFE_CM1_CON,
					   CHANNEL_MERGE0_EN_MASK_SFT,
					   enable << CHANNEL_MERGE0_EN_SFT);
		*/

		if (memif->data->agent_disable_shift >= 0)
			regmap_update_bits(afe->regmap,
					   memif->data->agent_disable_reg,
					   1 << memif->data->agent_disable_shift,
					   1 << memif->data->agent_disable_shift);
	}

	return 0;
}

static int mt8169_afe_irq_direction_enable(struct mtk_base_afe *afe,
	int irq_id,
	int direction)
{
	int irq_dsp_en, irq_ap_en;
	struct mtk_base_afe_irq *irq;

	if (irq_id >= MT8169_IRQ_NUM)
		return -1;

	irq = &afe->irqs[irq_id];

	if (direction == MT8169_AFE_IRQ_DIR_MCU) {
		irq_dsp_en = 0;
		irq_ap_en = 1;
	} else if (direction == MT8169_AFE_IRQ_DIR_DSP) {
		irq_dsp_en = 1;
		irq_ap_en = 0;
	} else {
		irq_dsp_en = 1;
		irq_ap_en = 1;
	}
	regmap_update_bits(afe->regmap, irq->irq_data->irq_ap_en_reg,
			   1 << irq->irq_data->irq_ap_en_shift,
			   irq_ap_en << irq->irq_data->irq_ap_en_shift);
	regmap_update_bits(afe->regmap, irq->irq_data->irq_dsp_en_reg,
			   1 << irq->irq_data->irq_dsp_en_shift,
			   irq_dsp_en << irq->irq_data->irq_dsp_en_shift);

	return 0;
}

static int mt8169_afe_memif_init(struct mtk_base_afe *afe, int memif_id)
{
	struct mtk_base_afe_memif *memif = &afe->memif[memif_id];

	mt8169_afe_irq_direction_enable(afe,
					memif->irq_usage,
					MT8169_AFE_IRQ_DIR_DSP);

	return 0;
}

static int mt8169_afe_memif_uninit(struct mtk_base_afe *afe, int memif_id)
{
	struct mtk_base_afe_memif *memif = &afe->memif[memif_id];
	int irq_id = memif->irq_usage;
	struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;

	mt8169_afe_irq_direction_enable(afe,
					memif->irq_usage,
					MT8169_AFE_IRQ_DIR_MCU);

	mt8169_adsp_afe_memif_enable(afe, memif_id, 0, 0, 0);
	mtk_memif_set_disable(afe, memif_id);
	/* disable interrupt */
	regmap_update_bits(afe->regmap,
			   irq_data->irq_en_reg,
			   1 << irq_data->irq_en_shift,
			   0 << irq_data->irq_en_shift);

	/* clear pending IRQ */
	regmap_write(afe->regmap, irq_data->irq_clr_reg,
		     1 << irq_data->irq_clr_shift);
	return 0;
}

static int mt8169_afe_sram_get(struct mtk_base_afe *afe,
			       dma_addr_t *paddr,
			       unsigned char **vaddr,
			       unsigned int size,
			       void *user)
{
	int ret = 0;
	unsigned char *virt_addr = NULL;

	ret = mtk_audio_sram_allocate(afe->sram, paddr, &virt_addr,
				      size, user, 0, true);
	*vaddr = virt_addr;

	return ret;
}

static int mt8169_afe_sram_put(struct mtk_base_afe *afe,
			       dma_addr_t *paddr,
			       void *user)
{
	return mtk_audio_sram_free(afe->sram, user);
}

static void mt8169_adsp_afe_init(struct mtk_base_afe *afe)
{
	struct device *dev = afe->dev;

	dev_info(dev, "%s\n", __func__);
	/* enable audio power always on */
	device_init_wakeup(dev, true);
}

static void mt8169_adsp_afe_uninit(struct mtk_base_afe *afe)
{
	struct device *dev = afe->dev;

	dev_info(dev, "%s\n", __func__);

	/* disable audio power always on */
	device_init_wakeup(dev, false);
}
#endif

static irqreturn_t mt8169_afe_irq_handler(int irq_id, void *dev)
{
	struct mtk_base_afe *afe = dev;
	struct mtk_base_afe_irq *irq;
	unsigned int status = 0;
	unsigned int status_mcu;
	unsigned int mcu_en = 0;
	int ret;
	int i;

	/* get irq that is sent to MCU */
	regmap_read(afe->regmap, AFE_IRQ_MCU_EN, &mcu_en);

	ret = regmap_read(afe->regmap, AFE_IRQ_MCU_STATUS, &status);
	/* only care IRQ which is sent to MCU */
	status_mcu = status & mcu_en & AFE_IRQ_STATUS_BITS;

	if (ret || status_mcu == 0) {
		dev_info(afe->dev, "%s(), irq status err, ret %d, status 0x%x, mcu_en 0x%x\n",
			__func__, ret, status, mcu_en);

		goto err_irq;
	}

	for (i = 0; i < MT8169_MEMIF_NUM; i++) {
		struct mtk_base_afe_memif *memif = &afe->memif[i];

		if (!memif->substream)
			continue;

		if (memif->irq_usage < 0)
			continue;

		irq = &afe->irqs[memif->irq_usage];

		if (status_mcu & (1 << irq->irq_data->irq_en_shift))
			snd_pcm_period_elapsed(memif->substream);
	}

err_irq:
	/* clear irq */
	regmap_write(afe->regmap,
		     AFE_IRQ_MCU_CLR,
		     status_mcu);

	return IRQ_HANDLED;
}

static void mt8169_dai_tdm_config(struct mtk_base_afe *afe, int dai_id,
				  bool enable)
{
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	struct mtk_clk_ao_attr *attr = &(afe_priv->clk_ao_data[dai_id]);
	struct device *dev = afe->dev;
	unsigned int rate = attr->fix_lrck_rate;
	bool low_jitter = attr->fix_low_jitter;
	/* slave:1 master:0 */
	bool slave_mode = attr->fix_tdm_slave_mode;
	/* I2S:0 LJ:1 RJ:2 EIAJ:3 DSPA:4 DSPB:5 */
	unsigned int tdm_mode = attr->fix_tdm_mode;
	/* ONE_PIN:0 MULTI_PIN:1 */
	unsigned int data_mode = attr->fix_tdm_data_mode;
	/* num of channels */
	unsigned int channels = attr->fix_tdm_channels;
	/* bclk = channel * fs * bit_width */
	unsigned int bit_width = attr->fix_bclk_width;
	/* default lrck no inverse */
	bool lrck_inv = TDM_LRCK_NON_INV;
	/* default bclk inverse */
	bool bck_inv = TDM_BCK_INV;
	/* tdm channel */
	unsigned int tdm_channels = (data_mode == TDM_DATA_ONE_PIN) ?
		mt8169_get_tdm_ch_per_sdata(tdm_mode, channels) : 2;
	unsigned int tdm_con = 0;
	unsigned int ctrl_reg;
	unsigned int ctrl_mask;
	unsigned int tran_rate;
	unsigned int tran_relatch_rate;

	tran_rate = mt8169_rate_transform(dev, rate, dai_id);
	tran_relatch_rate = mt8169_tdm_relatch_rate_transform(afe->dev, rate);

	/* ETDM_IN1_CON0 */
	tdm_con |= slave_mode << ETDM_IN1_CON0_REG_SLAVE_MODE_SFT;
	tdm_con |= tdm_mode << ETDM_IN1_CON0_REG_FMT_SFT;
	tdm_con |= (bit_width - 1) << ETDM_IN1_CON0_REG_BIT_LENGTH_SFT;
	tdm_con |= (bit_width - 1) << ETDM_IN1_CON0_REG_WORD_LENGTH_SFT;
	tdm_con |= (tdm_channels - 1) << ETDM_IN1_CON0_REG_CH_NUM_SFT;
	/* default disable sync mode */
	tdm_con |= 0 << ETDM_IN1_CON0_REG_SYNC_MODE_SFT;
	/* relatch fix to h26m */
	tdm_con |= 0 << ETDM_IN1_CON0_REG_RELATCH_1X_EN_SEL_DOMAIN_SFT;

	ctrl_reg = ETDM_IN1_CON0;
	ctrl_mask = ETDM_IN_CON0_CTRL_MASK;
	regmap_update_bits(afe->regmap, ctrl_reg, ctrl_mask, tdm_con);

	/* ETDM_IN1_CON3 */
	tdm_con = 0;
	tdm_con = ETDM_IN_CON3_FS(tran_rate);

	ctrl_reg = ETDM_IN1_CON3;
	ctrl_mask = ETDM_IN_CON3_CTRL_MASK;
	regmap_update_bits(afe->regmap, ctrl_reg, ctrl_mask, tdm_con);

	/* ETDM_IN1_CON4 */
	tdm_con = 0;
	tdm_con = ETDM_IN_CON4_FS(tran_relatch_rate);
	if (slave_mode) {
		if (lrck_inv)
			tdm_con |= ETDM_IN_CON4_CON0_SLAVE_LRCK_INV;
		if (bck_inv == TDM_BCK_INV)
			tdm_con |= ETDM_IN_CON4_CON0_SLAVE_BCK_INV;
	} else {
		if (lrck_inv)
			tdm_con |= ETDM_IN_CON4_CON0_MASTER_LRCK_INV;
		if (bck_inv == TDM_BCK_NON_INV)
			tdm_con |= ETDM_IN_CON4_CON0_MASTER_BCK_INV;
	}

	ctrl_reg = ETDM_IN1_CON4;
	ctrl_mask = ETDM_IN_CON4_CTRL_MASK;
	regmap_update_bits(afe->regmap, ctrl_reg, ctrl_mask, tdm_con);

	/* ETDM_IN1_CON2 */
	tdm_con = 0;
	if (data_mode == TDM_DATA_MULTI_PIN) {
		tdm_con |= ETDM_IN_CON2_MULTI_IP_2CH_MODE;
		tdm_con |= ETDM_IN_CON2_MULTI_IP_CH(channels);
	} else {
		tdm_con |= ETDM_IN_CON2_MULTI_IP_CH(2);
	}

	ctrl_reg = ETDM_IN1_CON2;
	ctrl_mask = ETDM_IN_CON2_CTRL_MASK;
	regmap_update_bits(afe->regmap, ctrl_reg, ctrl_mask, tdm_con);

	/* ETDM_IN1_CON8 */
	tdm_con = 0;
	if (slave_mode) {
		tdm_con |= 1 << ETDM_IN1_CON8_REG_ETDM_USE_AFIFO_SFT;
		tdm_con |= 0 << ETDM_IN1_CON8_REG_AFIFO_CLOCK_DOMAIN_SEL_SFT;
		tdm_con |= ETDM_IN_CON8_FS(tran_relatch_rate);
	} else
		tdm_con |= 0 << ETDM_IN1_CON8_REG_ETDM_USE_AFIFO_SFT;

	ctrl_reg = ETDM_IN1_CON8;
	ctrl_mask = ETDM_IN_CON8_CTRL_MASK;
	regmap_update_bits(afe->regmap, ctrl_reg, ctrl_mask, tdm_con);

	/* ETDM_0_3_COWORK_CON1 */
	tdm_con = 0;
	ctrl_mask = 0;
	ctrl_reg = ETDM_0_3_COWORK_CON1;
	tdm_con |= 2 << ETDM_0_3_COWORK_CON1_ETDM_IN1_SDATA0_SEL_SFT;
	ctrl_mask |= ETDM_0_3_COWORK_CON1_ETDM_IN1_SDATA0_SEL_MASK_SFT;
	tdm_con |= 2 << ETDM_0_3_COWORK_CON1_ETDM_IN1_SDATA1_15_SEL_SFT;
	ctrl_mask |= ETDM_0_3_COWORK_CON1_ETDM_IN1_SDATA1_15_SEL_MASK_SFT;
	if (slave_mode) {
		tdm_con |= 3 << ETDM_0_3_COWORK_CON1_ETDM_IN1_SLAVE_SEL_SFT;
		ctrl_mask |= ETDM_0_3_COWORK_CON1_ETDM_IN1_SLAVE_SEL_MASK_SFT;
	}
	regmap_update_bits(afe->regmap, ctrl_reg, ctrl_mask, tdm_con);

	/* ETDM IN hd mode */
	tdm_con |= low_jitter << ETDM_IN1_CON2_REG_CLOCK_SOURCE_SEL_SFT;

	ctrl_reg = ETDM_IN1_CON2;
	ctrl_mask = ETDM_IN1_CON2_REG_CLOCK_SOURCE_SEL_MASK_SFT;
	regmap_update_bits(afe->regmap, ctrl_reg, ctrl_mask, tdm_con);

	/* ETDM IN enable  */
	tdm_con |= enable << ETDM_IN1_CON0_REG_ETDM_IN_EN_SFT;

	ctrl_reg = ETDM_IN1_CON0;
	ctrl_mask = ETDM_IN1_CON0_REG_ETDM_IN_EN_MASK_SFT;
	regmap_update_bits(afe->regmap, ctrl_reg, ctrl_mask, tdm_con);
}

static void mt8169_dai_i2s_config(struct mtk_base_afe *afe, int dai_id,
					 bool enable)
{
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	struct mtk_clk_ao_attr *attr = &(afe_priv->clk_ao_data[dai_id]);
	struct device *dev = afe->dev;
	unsigned int rate = attr->fix_lrck_rate;
	bool low_jitter = attr->fix_low_jitter;
	unsigned int format = attr->fix_bclk_width == 16 ? 0 : 1;
	unsigned int reg_con = 0;
	unsigned int tran_rate = mt8169_rate_transform(dev, rate, dai_id);

	switch (dai_id) {
	case MT8169_DAI_I2S_0:
		reg_con = 1 << I2SIN_PAD_SEL_SFT;
		reg_con |= tran_rate << I2S_OUT_MODE_SFT;
		reg_con |= 1 << I2S_FMT_SFT;
		reg_con |= format << I2S_WLEN_SFT;
		reg_con |= low_jitter << I2S1_HD_EN_SFT;
		reg_con |= enable << I2S_EN_SFT;
		regmap_update_bits(afe->regmap, AFE_I2S_CON,
				   0xfffffffb, reg_con);
		break;
	case MT8169_DAI_I2S_1:
		reg_con = 0 << I2S2_SEL_O03_O04_SFT;
		reg_con |= tran_rate << I2S2_OUT_MODE_SFT;
		reg_con |= 1 << I2S2_FMT_SFT;
		reg_con |= format << I2S2_WLEN_SFT;
		reg_con |= low_jitter << I2S2_HD_EN_SFT;
		reg_con |= enable << I2S2_EN_SFT;
		regmap_update_bits(afe->regmap, AFE_I2S_CON1,
				   0xfffffffb, reg_con);
		break;
	case MT8169_DAI_I2S_2:
		reg_con = 8 << I2S3_UPDATE_WORD_SFT;
		reg_con |= tran_rate << I2S3_OUT_MODE_SFT;
		reg_con |= 1 << I2S3_FMT_SFT;
		reg_con |= format << I2S3_WLEN_SFT;
		reg_con |= low_jitter << I2S3_HD_EN_SFT;
		reg_con |= enable << I2S3_EN_SFT;
		regmap_update_bits(afe->regmap, AFE_I2S_CON2,
				   0xfffffffb, reg_con);
		break;
	case MT8169_DAI_I2S_3:
		reg_con = tran_rate << I2S4_OUT_MODE_SFT;
		reg_con |= 1 << I2S4_FMT_SFT;
		reg_con |= format << I2S4_WLEN_SFT;
		reg_con |= low_jitter << I2S4_HD_EN_SFT;
		reg_con |= enable << I2S4_EN_SFT;
		regmap_update_bits(afe->regmap, AFE_I2S_CON3,
				   0xfffffffb, reg_con);
		break;
	}
}

static int mtk_dai_apll_always_on(struct mtk_base_afe *afe, int dai_id,
				  bool enable)
{
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	struct mtk_clk_ao_attr *attr = &(afe_priv->clk_ao_data[dai_id]);
	unsigned int lrck_rate = attr->fix_lrck_rate;
	int need_apll;
	int ret = 0;

	dev_info(afe->dev, "%s()\n", __func__);

	/* choose APLL from lrck rate */
	need_apll = mt8169_get_apll_by_rate(afe, lrck_rate);

	if (enable) {
		if (need_apll == MT8169_APLL1)
			ret = mt8169_apll1_enable(afe);
		else
			ret = mt8169_apll2_enable(afe);
	} else {
		if (need_apll == MT8169_APLL1)
			mt8169_apll1_disable(afe);
		else
			mt8169_apll2_disable(afe);
	}

	afe_priv->use_apll = enable;

	return ret;
}

static int mtk_dai_mclk_always_on(struct mtk_base_afe *afe, int dai_id,
				  bool enable)
{
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	struct mtk_clk_ao_attr *attr = &(afe_priv->clk_ao_data[dai_id]);
	unsigned int mclk_rate = attr->fix_mclk_ratio * attr->fix_lrck_rate;
	unsigned int mclk_id;
	int ret = 0;

	dev_info(afe->dev, "%s()\n", __func__);

	if (!attr->apll_ao)
		dev_info(afe->dev, "%s(), error! mclk depends on apll\n",
			 __func__);

	switch (dai_id) {
	case MT8169_DAI_I2S_0:
		mclk_id = MT8169_I2S0_MCK;
		break;
	case MT8169_DAI_I2S_1:
		mclk_id = MT8169_I2S1_MCK;
		break;
	case MT8169_DAI_I2S_2:
		mclk_id = MT8169_I2S2_MCK;
		break;
	case MT8169_DAI_I2S_3:
		mclk_id = MT8169_I2S4_MCK;
		break;
	case MT8169_DAI_TDM_IN:
		mclk_id = MT8169_TDM_MCK;
		break;
	default:
		dev_info(afe->dev, "%s(), dai id %u invalid, use I2S0!!!\n",
			 __func__, dai_id);
		mclk_id = MT8169_I2S0_MCK;
		break;
	}

	if (enable)
		ret = mt8169_mck_enable(afe, mclk_id, mclk_rate);
	else
		mt8169_mck_disable(afe, mclk_id);

	return ret;
}

static int mtk_dai_bclk_always_on(struct mtk_base_afe *afe, int dai_id,
				  bool enable)
{
	dev_info(afe->dev, "%s()\n", __func__);

	if (dai_id >= MT8169_DAI_I2S_0 && dai_id <= MT8169_DAI_I2S_3)
		mt8169_dai_i2s_config(afe, dai_id, enable);
	else if (dai_id == MT8169_DAI_TDM_IN)
		mt8169_dai_tdm_config(afe, dai_id, enable);
	else
		dev_info(afe->dev, "%s(), id %d not support clk ao\n",
			 __func__, dai_id);

	return 0;
}

static int mt8169_afe_set_clk_always_on(struct mtk_base_afe *afe,
					bool enable)
{
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	struct mtk_clk_ao_attr *dai_attr;
	struct device *dev = afe->dev;
	bool clk_ao;
	int ret;
	int id;

	dev_info(dev, "%s(), ++\n", __func__);

	for (id = MT8169_MEMIF_NUM; id < MT8169_DAI_NUM; id++) {
		dai_attr = &afe_priv->clk_ao_data[id];
		if (enable && dai_attr->clk_ao_enable) {
			dev_info(dev, "%s(), dai %d clk has ao\n",
				 __func__, id);
			continue;
		} else if (!enable && !dai_attr->clk_ao_enable) {
			dev_info(dev, "%s(), dai %d clk has not ao\n",
				 __func__, id);
			continue;
		}

		clk_ao = dai_attr->apll_ao || dai_attr->mclk_ao ||
			dai_attr->bclk_ao || dai_attr->lrck_ao;

		if (!clk_ao)
			continue;

		/* enable clock for clock always on */
		if (enable) {
			ret = pm_runtime_get_sync(dev);
			if (ret)
				dev_info(dev, "get_ret:%d, rpm_error:%d\n",
					 ret, dev->power.runtime_error);
		}

		if (dai_attr->apll_ao)
			mtk_dai_apll_always_on(afe, id, enable);

		if (dai_attr->mclk_ao)
			mtk_dai_mclk_always_on(afe, id, enable);

		if (dai_attr->bclk_ao || dai_attr->lrck_ao)
			mtk_dai_bclk_always_on(afe, id, enable);

		/* disable clock for clock always on */
		if (!enable) {
			ret = pm_runtime_put_sync(dev);
			if (ret)
				dev_info(dev, "put_ret:%d, rpm_error:%d\n",
					 ret, dev->power.runtime_error);
		}

		dai_attr->clk_ao_enable = enable;
	}

	dev_info(dev, "%s(), --\n", __func__);

	return 0;
}

static int mt8169_afe_runtime_suspend(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	unsigned int value = 0;
	int ret;

	dev_info(afe->dev, "%s() ++\n", __func__);

	if (!afe->regmap)
		goto skip_regmap;

	/* disable AFE */
	regmap_update_bits(afe->regmap, AFE_DAC_CON0, 0x1, 0x0);

	ret = regmap_read_poll_timeout(afe->regmap,
				       AFE_DAC_MON,
				       value,
				       (value & AFE_ON_RETM_MASK_SFT) == 0,
				       20,
				       1 * 1000 * 1000);
	if (ret)
		dev_info(afe->dev, "%s(), ret %d\n", __func__, ret);

	/* make sure all irq status are cleared */
	regmap_write(afe->regmap, AFE_IRQ_MCU_CLR, 0xffffffff);
	regmap_write(afe->regmap, AFE_IRQ_MCU_CLR, 0xffffffff);

	/* reset sgen */
	regmap_write(afe->regmap, AFE_SINEGEN_CON0, 0x0);
	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON2,
			   INNER_LOOP_BACK_MODE_MASK_SFT,
			   0x3f << INNER_LOOP_BACK_MODE_SFT);

	/* cache only */
	regcache_cache_only(afe->regmap, true);
	regcache_mark_dirty(afe->regmap);

skip_regmap:
	mt8169_afe_disable_cgs(afe);
	mt8169_afe_disable_clock(afe);

	dev_info(afe->dev, "%s() --\n", __func__);

	return 0;
}

static int mt8169_afe_runtime_resume(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	int ret;

	dev_info(afe->dev, "%s() ++\n", __func__);

	ret = mt8169_afe_enable_clock(afe);
	if (ret)
		return ret;

	/* enable cgs through ccf, so not limited to regcache true */
	ret = mt8169_afe_enable_cgs(afe);
	if (ret)
		return ret;

	if (!afe->regmap) {
		dev_info(afe->dev, "%s() skip_regmap --\n", __func__);
		goto skip_regmap;
	}

	regcache_cache_only(afe->regmap, false);
	regcache_sync(afe->regmap);

	/* enable audio sys DCM for power saving */
	regmap_update_bits(afe_priv->infracfg_ao,
			   PERI_BUS_DCM_CTRL, 0x1 << 29, 0x1 << 29);
	regmap_update_bits(afe->regmap, AUDIO_TOP_CON0, 0x1 << 29, 0x1 << 29);

	/* force cpu use 8_24 format when writing 32bit data */
	regmap_update_bits(afe->regmap, AFE_MEMIF_CON0,
			   CPU_HD_ALIGN_MASK_SFT, 0 << CPU_HD_ALIGN_SFT);

	/* set all output port to 24bit */
	regmap_write(afe->regmap, AFE_CONN_24BIT, 0xffffffff);
	regmap_write(afe->regmap, AFE_CONN_24BIT_1, 0xffffffff);

	/* enable AFE */
	regmap_update_bits(afe->regmap, AFE_DAC_CON0, 0x1, 0x1);

	dev_info(afe->dev, "%s() --\n", __func__);

skip_regmap:
	return 0;
}

static int mt8169_afe_pcm_dev_suspend(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	int ret;
#if IS_ENABLED(CONFIG_MTK_HIFIXDSP_SUPPORT)
	struct mt8169_adsp_data *adsp_data = &(afe_priv->adsp_data);

	/* adsp pcm is active, do nothing */
	if (adsp_data->adsp_on && adsp_data->adsp_active()) {
		dev_info(dev, "%s(), adsp is active, do nothing\n", __func__);
		return 0;
	}
#endif
	dev_info(dev, "%s()\n", __func__);

	/* alsa card device suspend  */
	snd_soc_suspend(afe_priv->soc_card->dev);

	/* disable clock always on */
	ret = mt8169_afe_set_clk_always_on(afe, false);
	if (ret)
		dev_info(dev, "%s(), set clk ao(false) fail\n", __func__);

	/* check whether is runtime resume status */
	if (!pm_runtime_status_suspended(afe->dev))
		mt8169_afe_runtime_suspend(afe->dev);

	return 0;
}

static int mt8169_afe_pcm_dev_resume(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	int ret;
#if IS_ENABLED(CONFIG_MTK_HIFIXDSP_SUPPORT)
	struct mt8169_adsp_data *adsp_data = &(afe_priv->adsp_data);
	/* adsp pcm is active, do nothing */
	if (adsp_data->adsp_on && adsp_data->adsp_active()) {
		dev_info(dev, "%s(), adsp is active, do nothing\n", __func__);
		return 0;
	}
#endif
	dev_info(dev, "%s()\n", __func__);

	/*  check whether is runtime resume status */
	if (!pm_runtime_status_suspended(afe->dev))
		mt8169_afe_runtime_resume(afe->dev);

	/* enable clock always on */
	ret = mt8169_afe_set_clk_always_on(afe, true);
	if (ret)
		dev_info(dev, "%s(), set clk ao(true) fail\n", __func__);

	/* alsa card device resume  */
	snd_soc_resume(afe_priv->soc_card->dev);

	return 0;
}

static int mt8169_afe_pcm_copy(struct snd_pcm_substream *substream,
			       int channel, unsigned long hwoff,
			       void *buf, unsigned long bytes,
			       mtk_sp_copy_f sp_copy)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	int ret = 0;

	mt8169_set_audio_int_bus_parent(afe, CLK_TOP_MAINPLL_D2_D4);

	ret = sp_copy(substream, channel, hwoff, buf, bytes);

	mt8169_set_audio_int_bus_parent(afe, CLK_CLK26M);

	return ret;
}

static int mt8169_set_memif_sram_mode(struct device *dev,
				      enum mtk_audio_sram_mode sram_mode)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	int reg_bit = sram_mode == MTK_AUDIO_SRAM_NORMAL_MODE ? 1 : 0;

	regmap_update_bits(afe->regmap, AFE_DL1_CON0,
			   DL1_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL1_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL2_CON0,
			   DL2_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL2_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL3_CON0,
			   DL3_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL3_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL4_CON0,
			   DL4_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL4_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL5_CON0,
			   DL5_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL5_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL6_CON0,
			   DL6_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL6_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL7_CON0,
			   DL7_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL7_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL8_CON0,
			   DL8_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL8_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL12_CON0,
			   DL12_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL12_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_AWB_CON0,
			   AWB_NORMAL_MODE_MASK_SFT,
			   reg_bit << AWB_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_AWB2_CON0,
			   AWB2_NORMAL_MODE_MASK_SFT,
			   reg_bit << AWB2_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL12_CON0,
			   VUL12_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL12_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL_CON0,
			   VUL_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL2_CON0,
			   VUL2_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL2_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL3_CON0,
			   VUL3_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL3_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL4_CON0,
			   VUL4_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL4_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL5_CON0,
			   VUL5_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL5_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL6_CON0,
			   VUL6_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL6_NORMAL_MODE_SFT);
	return 0;
}

static int mt8169_set_sram_mode(struct device *dev,
				enum mtk_audio_sram_mode sram_mode)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);

	/* set memif sram mode */
	mt8169_set_memif_sram_mode(dev, sram_mode);

	if (sram_mode == MTK_AUDIO_SRAM_COMPACT_MODE)
		/* cpu use compact mode when access sram data */
		regmap_update_bits(afe->regmap, AFE_MEMIF_CON0,
				   CPU_COMPACT_MODE_MASK_SFT,
				   0x1 << CPU_COMPACT_MODE_SFT);
	else
		/* cpu use normal mode when access sram data */
		regmap_update_bits(afe->regmap, AFE_MEMIF_CON0,
				   CPU_COMPACT_MODE_MASK_SFT,
				   0x0 << CPU_COMPACT_MODE_SFT);

	return 0;
}

static const struct mtk_audio_sram_ops mt8169_sram_ops = {
	.set_sram_mode = mt8169_set_sram_mode,
};

static ssize_t mt8169_debugfs_read(struct file *file, char __user *buf,
				   size_t count, loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	const int size = 32768;
	char *buffer = NULL; /* for reduce kernel stack */
	int n = 0;
	int ret = 0;
	unsigned int value;
	int i;

	buffer = kmalloc(size, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	n += scnprintf(buffer + n, size - n,
		       "mtkaif calibration phase %d, %d, %d, %d\n",
		       afe_priv->mtkaif_chosen_phase[0],
		       afe_priv->mtkaif_chosen_phase[1],
		       afe_priv->mtkaif_chosen_phase[2],
		       afe_priv->mtkaif_chosen_phase[3]);

	n += scnprintf(buffer + n, size - n,
		       "mtkaif calibration cycle %d, %d, %d, %d\n",
		       afe_priv->mtkaif_phase_cycle[0],
		       afe_priv->mtkaif_phase_cycle[1],
		       afe_priv->mtkaif_phase_cycle[2],
		       afe_priv->mtkaif_phase_cycle[3]);

	for (i = 0; i < afe->memif_size; i++) {
		n += scnprintf(buffer + n, size - n,
			       "memif[%d], irq_usage %d\n",
			       i, afe->memif[i].irq_usage);
	}

	regmap_read(afe_priv->topckgen, CLK_CFG_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_CFG_4 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_CFG_5, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_CFG_5 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_AUDDIV_0 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_AUDDIV_2 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_3, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_AUDDIV_3 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, AP_PLL_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "AP_PLL_CON3 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL1_CON0 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL1_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL1_CON1 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL1_CON2 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL1_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL1_CON4 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL2_CON0 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL2_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL2_CON1 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL2_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL2_CON2 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL2_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL2_CON4 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL1_TUNER_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL1_TUNER_CON0 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL2_TUNER_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL2_TUNER_CON0 = 0x%x\n", value);

	regmap_read(afe_priv->infracfg_ao, PERI_BUS_DCM_CTRL, &value);
	n += scnprintf(buffer + n, size - n,
		       "PERI_BUS_DCM_CTRL = 0x%x\n", value);
	regmap_read(afe_priv->infracfg_ao, MODULE_SW_CG_1_STA, &value);
	n += scnprintf(buffer + n, size - n,
		       "MODULE_SW_CG_1_STA = 0x%x\n", value);
	regmap_read(afe_priv->infracfg_ao, MODULE_SW_CG_2_STA, &value);
	n += scnprintf(buffer + n, size - n,
		       "MODULE_SW_CG_2_STA = 0x%x\n", value);

	regmap_read(afe->regmap, AUDIO_TOP_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AUDIO_TOP_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AUDIO_TOP_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AUDIO_TOP_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "AUDIO_TOP_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DAC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DAC_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_I2S_CON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_I2S_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN3, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN4, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_I2S_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_I2S_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_I2S_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_I2S_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_I2S_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_I2S_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN5, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_24BIT, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN_24BIT = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL1_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL1_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL1_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL1_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL1_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL1_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL3_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL3_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL3_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL3_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL3_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL3_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL3_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN6, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL4_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL4_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL4_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL4_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL4_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL4_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL4_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL12_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL12_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL12_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL12_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL12_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL12_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL12_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL12_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL12_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL12_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL12_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL12_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL12_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL12_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_DL_SRC2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC2_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_DL_SRC2_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL_SRC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_UL_SRC_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL_SRC_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_UL_SRC_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_TOP_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_TOP_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL_DL_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_UL_DL_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_SRC_DEBUG, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_SRC_DEBUG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_SRC_DEBUG_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_SRC_DEBUG_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_SRC_DEBUG_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_SRC_DEBUG_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL_SRC_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_UL_SRC_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL_SRC_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_UL_SRC_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SRAM_BOUND, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SRAM_BOUND = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_CONN0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_CONN0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SIDETONE_DEBUG, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SIDETONE_DEBUG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SIDETONE_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SIDETONE_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SINEGEN_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SINEGEN_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SIDETONE_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SIDETONE_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SIDETONE_COEFF, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SIDETONE_COEFF = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SIDETONE_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SIDETONE_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SIDETONE_GAIN, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SIDETONE_GAIN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SINEGEN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SINEGEN_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_TOP_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_TOP_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL3_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL3_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL3_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL3_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL3_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL3_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL3_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_BUSY, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_BUSY = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_BUS_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_BUS_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_PREDIS_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_PREDIS_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_PREDIS_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_PREDIS_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_I2S_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_I2S_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_IIR_COEF_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_IIR_COEF_02_01 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_IIR_COEF_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_IIR_COEF_04_03 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_IIR_COEF_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_IIR_COEF_06_05 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_IIR_COEF_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_IIR_COEF_08_07 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_IIR_COEF_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_IIR_COEF_10_09 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DAC_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DAC_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT6, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT8, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_DSP2_EN, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_DSP2_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ0_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ0_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ6_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ6_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL4_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL4_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL4_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL4_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL4_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL4_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL4_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL12_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL12_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL12_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL12_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL12_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL12_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL12_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL12_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL12_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL12_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL12_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL12_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL12_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL12_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ3_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ3_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ4_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ4_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_STATUS, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_STATUS = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CLR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CLR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_EN, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_MON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_MON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT5, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ1_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ1_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ2_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ2_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ5_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ5_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_DSP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_DSP_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_SCP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_SCP_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT7, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ7_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ7_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT3, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT4, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT11, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_APLL1_TUNER_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_APLL1_TUNER_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_APLL2_TUNER_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_APLL2_TUNER_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_MISS_CLR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_MISS_CLR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN33, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN33 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT12, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GAIN1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GAIN1_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GAIN1_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GAIN1_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN7, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GAIN1_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GAIN2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GAIN2_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GAIN2_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GAIN2_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN8, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GAIN2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN9, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN10, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN11, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN12, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN13, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN14, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN15, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN15 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN16, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN16 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN17, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN17 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN18, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN18 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN19, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN19 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN20, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN20 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN21, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN21 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN22, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN22 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN23, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN23 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN24, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN24 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_RS, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN_RS = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_DI, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN_DI = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN25, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN25 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN26, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN26 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN27, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN27 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN28, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN28 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN29, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN29 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN30, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN30 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN31, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN31 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN32, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN32 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SRAM_DELSEL_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SRAM_DELSEL_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN56, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN56 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN57, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN57 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN56_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN56_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN57_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN57_1 = 0x%x\n", value);
	regmap_read(afe->regmap, PCM_INTF_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "PCM_INTF_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, PCM_INTF_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "PCM_INTF_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN34, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN34 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_DBG_CON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AUDIO_TOP_DBG_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_DBG_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AUDIO_TOP_DBG_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_DBG_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AUDIO_TOP_DBG_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ8_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ8_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ11_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ11_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ12_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ12_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT9, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT10, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT13, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT14, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT15, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT15 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT16, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT16 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT17, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT17 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT18, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT18 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT19, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT19 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT20, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT20 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT21, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT21 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT22, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT22 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT23, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT23 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT24, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT24 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT25, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT25 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT26, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ_MCU_CNT26 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ9_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ9_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ10_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ10_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ13_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ13_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ14_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ14_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ15_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ15_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ16_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ16_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ17_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ17_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ18_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ18_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ19_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ19_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ20_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ20_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ21_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ21_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ22_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ22_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ23_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ23_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ24_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ24_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ25_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ25_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ26_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ26_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ31_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_IRQ31_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL_REG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL_REG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL_REG2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG3, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL_REG3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG4, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL_REG4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG5, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL_REG5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG6, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL_REG6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG7, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL_REG7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG8, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL_REG8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG9, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL_REG9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG10, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL_REG10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG11, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL_REG11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG12, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL_REG12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG13, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL_REG13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG14, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL_REG14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG15, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL_REG15 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CBIP_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CBIP_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_MUX_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CBIP_SLV_MUX_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_DECODER_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CBIP_SLV_DECODER_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_MTKAIF_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_MTKAIF_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_MTKAIF_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_MTKAIF_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_AWB_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_AWB_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_AWB_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_AWB_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_AWB_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_AWB_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_AWB_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_AWB2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_AWB2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_AWB2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_AWB2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_AWB2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_AWB2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_AWB2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_MEMIF_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN0_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN0_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN1_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN1_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN2_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN2_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN3_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN3_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN4_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN4_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN5_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN5_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN6_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN6_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN7_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN7_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN8_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN8_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN9_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN9_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN10_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN10_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN11_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN11_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN12_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN12_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN13_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN13_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN14_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN14_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN15_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN15_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN16_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN16_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN17_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN17_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN18_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN18_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN19_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN19_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN20_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN20_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN21_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN21_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN22_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN22_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN23_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN23_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN24_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN24_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN25_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN25_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN26_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN26_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN27_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN27_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN28_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN28_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN29_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN29_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN30_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN30_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN31_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN31_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN32_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN32_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN33_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN33_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN34_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN34_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_RS_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN_RS_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_DI_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN_DI_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN_24BIT_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_REG, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN_REG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN35, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN35 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN36, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN36 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN37, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN37 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN38, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN38 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN35_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN35_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN36_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN36_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN37_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN37_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN38_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN38_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN39, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN39 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN40, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN40 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN41, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN41 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN42, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN42 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN39_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN39_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN40_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN40_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN41_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN41_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN42_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN42_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_I2S_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_I2S_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_TOP_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_TOP_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_UL_SRC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_UL_SRC_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_UL_SRC_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_UL_SRC_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_SRC_DEBUG, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_SRC_DEBUG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_SRC_DEBUG_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_SRC_DEBUG_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_ULCF_CFG_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_ULCF_CFG_02_01 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_ULCF_CFG_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_ULCF_CFG_04_03 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_ULCF_CFG_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_ULCF_CFG_06_05 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_ULCF_CFG_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_ULCF_CFG_08_07 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_ULCF_CFG_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_ULCF_CFG_10_09 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_ULCF_CFG_12_11, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_ULCF_CFG_12_11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_ULCF_CFG_14_13, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_ULCF_CFG_14_13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_ULCF_CFG_16_15, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_ULCF_CFG_16_15 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_ULCF_CFG_18_17, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_ULCF_CFG_18_17 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_ULCF_CFG_20_19, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_ULCF_CFG_20_19 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_ULCF_CFG_22_21, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_ULCF_CFG_22_21 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_ULCF_CFG_24_23, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_ULCF_CFG_24_23 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_ULCF_CFG_26_25, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_ULCF_CFG_26_25 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_ULCF_CFG_28_27, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_ULCF_CFG_28_27 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_ULCF_CFG_30_29, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_ULCF_CFG_30_29 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADD6A_UL_SRC_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADD6A_UL_SRC_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_UL_SRC_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_UL_SRC_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN43, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN43 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN43_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN43_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_AWB_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_AWB_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL12_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL12_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL12_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL12_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL2_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL2_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_AWB2_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_AWB2_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL3_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL3_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL4_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL4_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL5_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL5_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL6_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL6_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL1_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL1_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL2_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL2_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL12_RCH1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL12_RCH1_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL12_LCH1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL12_LCH1_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL12_RCH2_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL12_RCH2_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL12_LCH2_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL12_LCH2_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL3_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL3_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL4_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL4_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL5_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL5_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL6_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL6_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL7_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL7_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL8_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL8_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL5_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL5_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL5_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL5_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL5_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL5_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL5_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL6_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL6_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL6_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL6_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL6_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL6_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_VUL6_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_DCCOMP_CON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_DL_SDM_DCCOMP_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_TEST, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_DL_SDM_TEST = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_DC_COMP_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_DL_DC_COMP_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_DC_COMP_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_DL_DC_COMP_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_FIFO_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_DL_SDM_FIFO_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_DL_SRC_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_DL_SRC_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_OUT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_DL_SDM_OUT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_DITHER_CON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_DL_SDM_DITHER_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_AUTO_RESET_CON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_DL_SDM_AUTO_RESET_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_CON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONNSYS_I2S_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONNSYS_I2S_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ASRC_2CH_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ASRC_2CH_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ASRC_2CH_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ASRC_2CH_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ASRC_2CH_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ASRC_2CH_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ASRC_2CH_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ASRC_2CH_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ASRC_2CH_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ASRC_2CH_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ASRC_2CH_CON10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ASRC_2CH_CON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ASRC_2CH_CON13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_IIR_COEF_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_IIR_COEF_02_01 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_IIR_COEF_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_IIR_COEF_04_03 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_IIR_COEF_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_IIR_COEF_06_05 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_IIR_COEF_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_IIR_COEF_08_07 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_IIR_COEF_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_IIR_COEF_10_09 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SE_PROT_SIDEBAND = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SE_DOMAIN_SIDEBAND0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_PREDIS_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_PREDIS_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_PREDIS_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_PREDIS_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SE_DOMAIN_SIDEBAND1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SE_DOMAIN_SIDEBAND2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND3, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SE_DOMAIN_SIDEBAND3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN44, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN44 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN45, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN45 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN46, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN46 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN47, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN47 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN44_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN44_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN45_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN45_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN46_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN46_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN47_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN47_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_HD_ENGEN_ENABLE, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_HD_ENGEN_ENABLE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_NLE_FIFO_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_DL_NLE_FIFO_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_MTKAIF_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_SYNCWORD_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_MTKAIF_SYNCWORD_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_RX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_MTKAIF_RX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_RX_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_MTKAIF_RX_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_RX_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_MTKAIF_RX_CFG2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_MTKAIF_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_MTKAIF_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AUD_PAD_TOP, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_AUD_PAD_TOP = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_R_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL_NLE_R_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_R_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL_NLE_R_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_L_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL_NLE_L_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_L_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL_NLE_L_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_R_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL_NLE_R_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_R_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL_NLE_R_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_R_MON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL_NLE_R_MON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_L_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL_NLE_L_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_L_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL_NLE_L_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_L_MON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL_NLE_L_MON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_GAIN_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL_NLE_GAIN_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_MTKAIF_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_MTKAIF_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_MTKAIF_RX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_MTKAIF_RX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_MTKAIF_RX_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_MTKAIF_RX_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_MTKAIF_RX_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA6_MTKAIF_RX_CFG2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL1_ASRC_2CH_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL1_ASRC_2CH_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL1_ASRC_2CH_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL1_ASRC_2CH_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL1_ASRC_2CH_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL1_ASRC_2CH_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL1_ASRC_2CH_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL1_ASRC_2CH_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL1_ASRC_2CH_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL1_ASRC_2CH_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL1_ASRC_2CH_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL1_ASRC_2CH_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL1_ASRC_2CH_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL1_ASRC_2CH_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL1_ASRC_2CH_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL1_ASRC_2CH_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL1_ASRC_2CH_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL1_ASRC_2CH_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL1_ASRC_2CH_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL1_ASRC_2CH_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL1_ASRC_2CH_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL1_ASRC_2CH_CON10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL1_ASRC_2CH_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL1_ASRC_2CH_CON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL1_ASRC_2CH_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL1_ASRC_2CH_CON13 = 0x%x\n", value);
	regmap_read(afe->regmap, GENERAL_ASRC_MODE, &value);
	n += scnprintf(buffer + n, size - n,
		       "GENERAL_ASRC_MODE = 0x%x\n", value);
	regmap_read(afe->regmap, GENERAL_ASRC_EN_ON, &value);
	n += scnprintf(buffer + n, size - n,
		       "GENERAL_ASRC_EN_ON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN48, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN48 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN49, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN49 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN50, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN50 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN51, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN51 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN52, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN52 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN53, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN53 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN54, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN54 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN55, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN55 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN48_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN48_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN49_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN49_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN50_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN50_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN51_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN51_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN52_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN52_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN53_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN53_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN54_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN54_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN55_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN55_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL2_ASRC_2CH_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL2_ASRC_2CH_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL2_ASRC_2CH_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL2_ASRC_2CH_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL2_ASRC_2CH_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL2_ASRC_2CH_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL2_ASRC_2CH_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL2_ASRC_2CH_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL2_ASRC_2CH_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL2_ASRC_2CH_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL2_ASRC_2CH_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL2_ASRC_2CH_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL2_ASRC_2CH_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL2_ASRC_2CH_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL2_ASRC_2CH_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL2_ASRC_2CH_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL2_ASRC_2CH_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL2_ASRC_2CH_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL2_ASRC_2CH_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL2_ASRC_2CH_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL2_ASRC_2CH_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL2_ASRC_2CH_CON10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL2_ASRC_2CH_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL2_ASRC_2CH_CON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL2_ASRC_2CH_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_GENERAL2_ASRC_2CH_CON13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL5_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL5_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL5_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL5_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL5_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL5_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL5_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL6_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL6_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL6_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL6_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL6_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL6_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL6_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL7_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL7_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL7_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL7_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL7_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL7_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL7_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL8_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL8_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL8_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL8_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL8_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL8_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_END, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DL8_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_SECURE_CON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SE_SECURE_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_PROT_SIDEBAND_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_PROT_SIDEBAND_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DOMAIN_SIDEBAND0_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DOMAIN_SIDEBAND1_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND2_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DOMAIN_SIDEBAND2_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND3_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_DOMAIN_SIDEBAND3_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN0, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN2, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN3, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN4, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN5, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN6, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN7, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN8, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN9, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN10, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN11, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN12, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN13, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN14, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN15, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN15 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN16, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN16 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN17, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN17 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN18, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN18 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN19, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN19 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN20, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN20 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN21, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN21 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN22, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN22 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN23, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN23 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN24, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN24 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN25, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN25 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN26, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN26 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN27, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN27 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN28, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN28 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN29, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN29 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN30, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN30 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN31, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN31 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN32, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN32 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN33, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN33 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN34, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN34 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN35, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN35 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN36, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN36 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN37, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN37 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN38, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN38 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN39, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN39 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN40, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN40 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN41, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN41 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN42, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN42 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN43, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN43 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN44, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN44 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN45, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN45 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN46, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN46 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN47, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN47 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN48, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN48 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN49, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN49 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN50, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN50 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN51, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN51 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN52, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN52 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN53, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN53 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN54, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN54 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN55, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN55 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN56, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN56 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN57, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN57 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN0_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN0_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN1_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN1_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN2_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN2_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN3_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN3_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN4_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN4_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN5_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN5_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN6_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN6_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN7_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN7_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN8_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN8_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN9_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN9_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN10_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN10_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN11_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN11_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN12_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN12_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN13_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN13_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN14_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN14_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN15_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN15_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN16_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN16_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN17_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN17_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN18_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN18_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN19_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN19_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN20_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN20_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN21_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN21_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN22_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN22_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN23_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN23_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN24_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN24_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN25_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN25_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN26_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN26_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN27_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN27_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN28_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN28_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN29_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN29_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN30_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN30_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN31_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN31_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN32_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN32_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN33_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN33_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN34_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN34_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN35_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN35_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN36_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN36_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN37_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN37_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN38_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN38_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN39_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN39_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN40_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN40_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN41_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN41_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN42_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN42_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN43_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN43_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN44_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN44_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN45_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN45_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN46_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN46_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN47_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN47_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN48_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN48_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN49_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN49_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN50_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN50_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN51_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN51_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN52_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN52_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN53_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN53_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN54_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN54_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN55_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN55_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_MASK_CONN56_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_SECURE_MASK_CONN56_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN60_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN60_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN61_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN61_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN62_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN62_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN63_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN63_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN64_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN64_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN65_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN65_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN66_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "AFE_CONN66_1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_IN1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_IN1_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_IN1_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_IN1_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_IN1_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_IN1_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_IN1_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_IN1_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_IN1_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_OUT1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_OUT1_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_OUT1_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_OUT1_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_OUT1_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_OUT1_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_OUT1_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_OUT1_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_OUT1_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_IN1_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_OUT1_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_0_3_COWORK_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_0_3_COWORK_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_0_3_COWORK_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_0_3_COWORK_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_0_3_COWORK_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "ETDM_0_3_COWORK_CON3 = 0x%x\n", value);

	ret = simple_read_from_buffer(buf, count, pos, buffer, n);
	kfree(buffer);

	return ret;
}

static const struct mtk_afe_debug_cmd mt8169_debug_cmds[] = {
	MTK_AFE_DBG_CMD("write_reg", mtk_afe_debug_write_reg),
	{}
};

static const struct file_operations mt8169_debugfs_ops = {
	.open = mtk_afe_debugfs_open,
	.write = mtk_afe_debugfs_write,
	.read = mt8169_debugfs_read,
};

static int mt8169_afe_component_probe(struct snd_soc_component *component)
{
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;

	afe_priv->soc_card = component->card;
	mtk_afe_add_sub_dai_control(component);
	mt8169_add_misc_control(component);

	return 0;
}

static void mt8169_afe_component_remove(struct snd_soc_component *component)
{
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;

	afe_priv->soc_card = NULL;
}

static snd_pcm_uframes_t mt8169_afe_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	if (rtd->cpu_dai->id >= MT8169_MEMIF_NUM ||
	    rtd->cpu_dai->id < MT8169_MEMIF_DL1)
		return 0;

	return mtk_afe_pcm_pointer(substream);
}

static int mt8169_afe_pcm_silence(struct snd_pcm_substream *substream,
				   int channel, unsigned long pos,
				   unsigned long bytes)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
			snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	int id = rtd->cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[id];

	if (substream->runtime->access == SNDRV_PCM_ACCESS_RW_INTERLEAVED ||
	    substream->runtime->access == SNDRV_PCM_ACCESS_MMAP_INTERLEAVED) {
		struct snd_pcm_runtime *runtime = substream->runtime;
		void *hwbuf = runtime->dma_area + pos +
				channel * (runtime->dma_bytes / runtime->channels);

		if (memif->using_sram)
			memset_io(hwbuf, 0, bytes);
		else
			memset(hwbuf, 0, bytes);
	}
	return 0;
}

static const struct snd_pcm_ops mt8169_afe_pcm_ops = {
	.ioctl = snd_pcm_lib_ioctl,
	.pointer = mt8169_afe_pcm_pointer,
	.ack = mtk_afe_pcm_ack,
	.copy_user = mtk_afe_pcm_copy_user,
	.fill_silence = mt8169_afe_pcm_silence,
};

static const struct snd_soc_component_driver mt8169_afe_component = {
	.name = AFE_PCM_NAME,
	.ops = &mt8169_afe_pcm_ops,
	.pcm_new = mtk_afe_pcm_new,
	.pcm_free = mtk_afe_pcm_free,
	.probe = mt8169_afe_component_probe,
	.remove = mt8169_afe_component_remove,
};

static int mt8169_dai_memif_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mt8169_memif_dai_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mt8169_memif_dai_driver);

	dai->controls = mt8169_pcm_kcontrols;
	dai->num_controls = ARRAY_SIZE(mt8169_pcm_kcontrols);
	dai->dapm_widgets = mt8169_memif_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mt8169_memif_widgets);
	dai->dapm_routes = mt8169_memif_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mt8169_memif_routes);
	return 0;
}

typedef int (*dai_register_cb)(struct mtk_base_afe *);
static const dai_register_cb dai_register_cbs[] = {
	mt8169_dai_adda_register,
	mt8169_dai_i2s_register,
	mt8169_dai_tdm_register,
	mt8169_dai_hw_gain_register,
	mt8169_dai_src_register,
	mt8169_dai_pcm_register,
	mt8169_dai_hostless_register,
	mt8169_dai_memif_register,
};

static void mt8169_afe_parse_of(struct mtk_base_afe *afe,
				      struct device_node *np)

{
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	size_t i;
	int ret;
	char prop[128];
	unsigned int val[9];
	u32 use_sram_force_cnt = 0;
	u32 memif_index[MT8169_MEMIF_NUM];
	struct {
		char *name;
		unsigned int val;
	} of_be_table[] = {
		{ "i2s0", MT8169_DAI_I2S_0 },
		{ "i2s1", MT8169_DAI_I2S_1 },
		{ "i2s2", MT8169_DAI_I2S_2 },
		{ "i2s3", MT8169_DAI_I2S_3 },
		{ "tdmin", MT8169_DAI_TDM_IN },
	};

	ret = of_property_read_u32(np, "mediatek,memif_force_sram_cnt",
				   &use_sram_force_cnt);
	if (!ret) {
		dev_info(afe->dev, "use_sram_force_cnt:%u\n", use_sram_force_cnt);
		if (use_sram_force_cnt > MT8169_MEMIF_NUM)
			use_sram_force_cnt = MT8169_MEMIF_NUM;

		ret = of_property_read_u32_array(np, "mediatek,memif_force_sram",
						 &memif_index[0],
						 use_sram_force_cnt);
		if (!ret) {
			for (i = 0; i < afe->memif_size; i++) {
				afe->memif[i].use_dram_only = 1;
				afe_priv->use_sram_force[i] = 0;
			}

			for (i = 0; i < use_sram_force_cnt; i++) {
				if (memif_index[i] >= MT8169_MEMIF_NUM)
					continue;

				afe->memif[memif_index[i]].use_dram_only = 0;
				afe_priv->use_sram_force[memif_index[i]] = 1;
				dev_info(afe->dev, "memif %u use sram\n", memif_index[i]);
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(of_be_table); i++) {
		struct mtk_clk_ao_attr *data;

		memset(val, 0, sizeof(val));

		ret = snprintf(prop, sizeof(prop), "mediatek,%s-clk-always-on",
			 of_be_table[i].name);
		if (ret < 0 || ret > sizeof(prop)) {
			dev_info(afe->dev, "mediatek,%s-clk-always-on parse fail",
				 of_be_table[i].name);
			continue;
		}
		ret = of_property_read_u32_array(np, prop, &val[0], 9);
		if (ret)
			continue;

		dev_info(afe->dev, "%s %s 0x%x %d %d %d %d %d %d %d %d",
			 __func__, of_be_table[i].name, val[0], val[1],
			 val[2], val[3], val[4], val[5], val[6], val[7],
			 val[8]);

		/*
		 * level dependence is apll->mclk->bclk->lrck->gpio
		 * 0x8 means apll always on
		 * 0x4 means mclk always on
		 * 0x2 means bclk always on
		 * 0x1 means lrck always on
		 * example1:
		 *      set 0x8 + 0x4 = 0xc to keep mclk always on
		 * example2:
		 *      set 0x8 + 0x4 + 0x2 = 0xe to keep mclk && bclk  always on
		 */
		data = &afe_priv->clk_ao_data[of_be_table[i].val];
		data->ao_level = val[0];
		data->apll_ao = APLL_AO(data->ao_level);
		data->mclk_ao = MCLK_AO(data->ao_level);
		data->bclk_ao = BCLK_AO(data->ao_level);
		data->lrck_ao = LRLK_AO(data->ao_level);
		data->fix_low_jitter = val[1];
		data->fix_lrck_rate = val[2];
		data->fix_mclk_ratio = val[3];
		data->fix_bclk_width = val[4];
		data->fix_tdm_slave_mode = val[5];
		data->fix_tdm_channels = val[6];
		data->fix_tdm_mode = val[7];
		data->fix_tdm_data_mode = val[8];
	}
}

static int mt8169_afe_pcm_dev_probe(struct platform_device *pdev)
{
	int ret, i;
	int irq_id;
	struct mtk_base_afe *afe;
	struct mt8169_afe_private *afe_priv;
	struct resource *res;
	struct device *dev;

	dev_info(&pdev->dev, "%s(), ++\n", __func__);

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));
	if (ret)
		return ret;

	afe = devm_kzalloc(&pdev->dev, sizeof(*afe), GFP_KERNEL);
	if (!afe)
		return -ENOMEM;
	platform_set_drvdata(pdev, afe);
	mt8169_set_local_afe(afe);

	afe->platform_priv = devm_kzalloc(&pdev->dev, sizeof(*afe_priv),
					  GFP_KERNEL);
	if (!afe->platform_priv)
		return -ENOMEM;
	afe_priv = afe->platform_priv;

	afe->dev = &pdev->dev;
	dev = afe->dev;

	dev_info(dev, "%s(), mt8169_init_clock\n", __func__);

	/* init audio related clock */
	ret = mt8169_init_clock(afe);
	if (ret) {
		dev_info(dev, "init clock error\n");
		return ret;
	}

	pm_runtime_enable(dev);
	if (!pm_runtime_enabled(dev))
		goto err_pm_disable;

	/* regmap init */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	afe->base_addr = devm_ioremap_resource(dev, res);
	if (IS_ERR(afe->base_addr))
		return PTR_ERR(afe->base_addr);

	/* enable clock for regcache get default value from hw */
	ret = pm_runtime_get_sync(dev);
	if (ret)
		dev_info(dev, "get_ret:%d, rpm_error:%d\n",
			 ret, dev->power.runtime_error);

	afe->regmap = devm_regmap_init_mmio(dev, afe->base_addr,
					    &mt8169_afe_regmap_config);
	if (IS_ERR(afe->regmap))
		return PTR_ERR(afe->regmap);

	ret = pm_runtime_put_sync(dev);
	if (ret)
		dev_info(dev, "put_ret:%d, rpm_error:%d\n",
			 ret, dev->power.runtime_error);

	/* init sram */
	afe->sram = devm_kzalloc(dev, sizeof(struct mtk_audio_sram),
				 GFP_KERNEL);
	if (!afe->sram)
		return -ENOMEM;

	dev_info(dev, "%s(), mtk_audio_sram_init\n", __func__);

	ret = mtk_audio_sram_init(dev, afe->sram, &mt8169_sram_ops);
	if (ret)
		return ret;

	/* init memif */
	afe->memif_32bit_supported = 0;
	afe->memif_size = MT8169_MEMIF_NUM;
	afe->memif = devm_kcalloc(dev, afe->memif_size, sizeof(*afe->memif),
				  GFP_KERNEL);
	if (!afe->memif)
		return -ENOMEM;

	for (i = 0; i < afe->memif_size; i++) {
		afe->memif[i].data = &memif_data[i];
		afe->memif[i].irq_usage = memif_irq_usage[i];
		afe->memif[i].const_irq = 1;
		afe->memif[i].gnt_info = &gnt_memif_data[i];
	}
	afe->memif[MT8169_DEEP_MEMIF].ack = mtk_sp_clean_written_buffer_ack;

	mutex_init(&afe->irq_alloc_lock);	/* needed when dynamic irq */

	dev_info(dev, "%s(), init irq\n", __func__);

	/* init irq */
	afe->irqs_size = MT8169_IRQ_NUM;
	afe->irqs = devm_kcalloc(dev, afe->irqs_size, sizeof(*afe->irqs),
				 GFP_KERNEL);

	if (!afe->irqs)
		return -ENOMEM;

	for (i = 0; i < afe->irqs_size; i++)
		afe->irqs[i].irq_data = &irq_data[i];


	dev_info(dev, "%s(), devm_request_irq\n", __func__);

	/* request irq */
	irq_id = platform_get_irq(pdev, 0);
	if (irq_id <= 0) {
		dev_info(dev, "%pOFn no irq found\n", dev->of_node);
		return irq_id < 0 ? irq_id : -ENXIO;
	}
	ret = devm_request_irq(dev, irq_id, mt8169_afe_irq_handler,
			       IRQF_TRIGGER_NONE,
			       "Afe_ISR_Handle", (void *)afe);
	if (ret) {
		dev_info(dev, "could not request_irq for Afe_ISR_Handle\n");
		return ret;
	}

	ret = enable_irq_wake(irq_id);
	if (ret < 0)
		dev_info(dev, "enable_irq_wake %d err: %d\n", irq_id, ret);

	/* init sub_dais */
	INIT_LIST_HEAD(&afe->sub_dais);

	for (i = 0; i < ARRAY_SIZE(dai_register_cbs); i++) {
		ret = dai_register_cbs[i](afe);
		if (ret) {
			dev_info(dev, "dai register i %d fail, ret %d\n",
				 i, ret);
			goto err_pm_disable;
		}
	}

	dev_info(dev, "%s(), mtk_afe_combine_sub_dai\n", __func__);

	/* init dai_driver and component_driver */
	ret = mtk_afe_combine_sub_dai(afe);
	if (ret) {
		dev_info(dev, "mtk_afe_combine_sub_dai fail, ret %d\n",
			 ret);
		goto err_pm_disable;
	}

	/* others */
	afe->mtk_afe_hardware = &mt8169_afe_hardware;
	afe->memif_fs = mt8169_memif_fs;
	afe->irq_fs = mt8169_irq_fs;
	afe->get_dai_fs = mt8169_get_dai_fs;
	afe->get_memif_pbuf_size = mt8169_get_memif_pbuf_size;

	afe->runtime_resume = mt8169_afe_runtime_resume;
	afe->runtime_suspend = mt8169_afe_runtime_suspend;

	afe->request_dram_resource = mt8169_afe_dram_request;
	afe->release_dram_resource = mt8169_afe_dram_release;

	afe->copy = mt8169_afe_pcm_copy;

	/* debugfs */
	afe->debug_cmds = mt8169_debug_cmds;
	afe->debugfs = debugfs_create_file("mtksocaudio",
					   S_IFREG | 0444, NULL,
					   afe, &mt8169_debugfs_ops);

	dev_info(dev, "%s(), mt8169_afe_parse_of\n", __func__);
	mt8169_afe_parse_of(afe, dev->of_node);

	/* register platform */
	dev_info(dev, "%s(), devm_snd_soc_register_component\n", __func__);

	ret = devm_snd_soc_register_component(dev,
					      &mt8169_afe_component,
					      afe->dai_drivers,
					      afe->num_dai_drivers);
	if (ret) {
		dev_info(dev, "err_dai_component\n");
		goto err_pm_disable;
	}

	dev_info(dev, "%s(), --\n", __func__);

#if IS_ENABLED(CONFIG_MTK_HIFIXDSP_SUPPORT)
	g_priv = afe;

	afe_priv->adsp_data.afe_memif_set = mt8169_adsp_set_afe_memif;
	afe_priv->adsp_data.afe_memif_enable = mt8169_adsp_afe_memif_enable;
	afe_priv->adsp_data.afe_init = mt8169_adsp_afe_init;
	afe_priv->adsp_data.afe_uninit = mt8169_adsp_afe_uninit;
	afe_priv->adsp_data.afe_memif_init = mt8169_afe_memif_init;
	afe_priv->adsp_data.afe_memif_uninit = mt8169_afe_memif_uninit;
	afe_priv->adsp_data.afe_sram_get = mt8169_afe_sram_get;
	afe_priv->adsp_data.afe_sram_put = mt8169_afe_sram_put;
#endif

	ret = mt8169_afe_set_clk_always_on(afe, true);
	if (ret)
		dev_info(dev, "%s(), set clk ao(true) fail\n", __func__);

	return 0;

err_pm_disable:
	pm_runtime_disable(dev);

	return ret;
}

static int mt8169_afe_pcm_dev_remove(struct platform_device *pdev)
{
	struct mtk_base_afe *afe = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	int ret;

	ret = mt8169_afe_set_clk_always_on(afe, false);
	if (ret)
		dev_info(dev, "%s(), set clk ao(false) fail\n", __func__);

	pm_runtime_disable(dev);
	if (!pm_runtime_status_suspended(dev))
		mt8169_afe_runtime_suspend(dev);

	/* disable afe clock */
	mt8169_afe_disable_clock(afe);

	return 0;
}

static const struct of_device_id mt8169_afe_pcm_dt_match[] = {
	{ .compatible = "mediatek,mt8169-sound", },
	{},
};
MODULE_DEVICE_TABLE(of, mt8169_afe_pcm_dt_match);

static const struct dev_pm_ops mt8169_afe_pm_ops = {
#if IS_ENABLED(CONFIG_PM)
	SET_RUNTIME_PM_OPS(mt8169_afe_runtime_suspend,
			   mt8169_afe_runtime_resume, NULL)
#endif
#if IS_ENABLED(CONFIG_PM_SLEEP)
	SET_SYSTEM_SLEEP_PM_OPS(mt8169_afe_pcm_dev_suspend,
				mt8169_afe_pcm_dev_resume)
#endif
};

static struct platform_driver mt8169_afe_pcm_driver = {
	.driver = {
		   .name = "mt8169-audio",
		   .of_match_table = mt8169_afe_pcm_dt_match,
		   .pm = &mt8169_afe_pm_ops,
	},
	.probe = mt8169_afe_pcm_dev_probe,
	.remove = mt8169_afe_pcm_dev_remove,
};

module_platform_driver(mt8169_afe_pcm_driver);

MODULE_DESCRIPTION("Mediatek ALSA SoC AFE platform driver for 8169");
MODULE_AUTHOR("Jiaxin Yu <jiaxin.yu@mediatek.com>");
MODULE_LICENSE("GPL v2");
