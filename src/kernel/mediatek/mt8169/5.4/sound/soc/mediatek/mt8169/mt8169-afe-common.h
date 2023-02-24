/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt8169-afe-common.h  --  Mediatek 8169 audio driver definitions
 *
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Jiaxin Yu <jiaxin.yu@mediatek.com>
 */

#ifndef _MT_8169_AFE_COMMON_H_
#define _MT_8169_AFE_COMMON_H_
#include <sound/soc.h>
#include <linux/list.h>
#include <linux/regmap.h>
#include <linux/pm_qos.h>
#include "mt8169-reg.h"
#include "../common/mtk-base-afe.h"

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define AUDIO_AEE(message) \
	(aee_kernel_exception_api(__FILE__, \
				  __LINE__, \
				  DB_OPT_FTRACE, message, \
				  "audio assert"))
#else
#define AUDIO_AEE(message) WARN_ON(true)
#endif

enum {
	MT8169_MEMIF_DL1 = 0,
	MT8169_MEMIF_DL12,
	MT8169_MEMIF_DL2,
	MT8169_MEMIF_DL3,
	MT8169_MEMIF_DL4,
	MT8169_MEMIF_DL5,
	MT8169_MEMIF_DL6,
	MT8169_MEMIF_DL7,
	MT8169_MEMIF_DL8,
	MT8169_MEMIF_VUL12,
	MT8169_MEMIF_VUL2,
	MT8169_MEMIF_VUL3,
	MT8169_MEMIF_VUL4,
	MT8169_MEMIF_VUL5,
	MT8169_MEMIF_VUL6,
	MT8169_MEMIF_AWB,
	MT8169_MEMIF_AWB2,
	MT8169_MEMIF_NUM,
	MT8169_DAI_ADDA = MT8169_MEMIF_NUM,
	MT8169_DAI_AP_DMIC,
	MT8169_DAI_CONNSYS_I2S,
	MT8169_DAI_I2S_0,
	MT8169_DAI_I2S_1,
	MT8169_DAI_I2S_2,
	MT8169_DAI_I2S_3,
	MT8169_DAI_HW_GAIN_1,
	MT8169_DAI_HW_GAIN_2,
	MT8169_DAI_SRC_1,
	MT8169_DAI_SRC_2,
	MT8169_DAI_PCM,
	MT8169_DAI_TDM_IN,
	MT8169_DAI_HOSTLESS_LPBK,
	MT8169_DAI_HOSTLESS_FM,
	MT8169_DAI_HOSTLESS_HW_GAIN_AAUDIO,
	MT8169_DAI_HOSTLESS_SRC_AAUDIO,
	MT8169_DAI_HOSTLESS_SRC_1,
	MT8169_DAI_HOSTLESS_SRC_2,
	MT8169_DAI_HOSTLESS_SRC_BARGEIN,
	MT8169_DAI_HOSTLESS_UL1,
	MT8169_DAI_HOSTLESS_UL2,
	MT8169_DAI_HOSTLESS_UL3,
	MT8169_DAI_HOSTLESS_UL5,
	MT8169_DAI_HOSTLESS_UL6,
	MT8169_DAI_HOSTLESS_DL5,
	MT8169_DAI_NUM,
};

#define MT8169_RECORD_MEMIF MT8169_MEMIF_VUL12
#define MT8169_ECHO_REF_MEMIF MT8169_MEMIF_AWB
#define MT8169_PRIMARY_MEMIF MT8169_MEMIF_DL1
#define MT8169_FAST_MEMIF MT8169_MEMIF_DL2
#define MT8169_DEEP_MEMIF MT8169_MEMIF_DL3
#define MT8169_VOIP_MEMIF MT8169_MEMIF_DL12
#define MT8169_MMAP_DL_MEMIF MT8169_MEMIF_DL5
#define MT8169_MMAP_UL_MEMIF MT8169_MEMIF_VUL5
#define MT8169_BARGEIN_MEMIF MT8169_MEMIF_AWB

enum {
	MT8169_IRQ_0,
	MT8169_IRQ_1,
	MT8169_IRQ_2,
	MT8169_IRQ_3,
	MT8169_IRQ_4,
	MT8169_IRQ_5,
	MT8169_IRQ_6,
	MT8169_IRQ_7,
	MT8169_IRQ_8,
	MT8169_IRQ_9,
	MT8169_IRQ_10,
	MT8169_IRQ_11,
	MT8169_IRQ_12,
	MT8169_IRQ_13,
	MT8169_IRQ_14,
	MT8169_IRQ_15,
	MT8169_IRQ_16,
	MT8169_IRQ_17,
	MT8169_IRQ_18,
	MT8169_IRQ_19,
	MT8169_IRQ_20,
	MT8169_IRQ_21,
	MT8169_IRQ_22,
	MT8169_IRQ_23,
	MT8169_IRQ_24,
	MT8169_IRQ_25,
	MT8169_IRQ_26,
	MT8169_IRQ_NUM,
};

enum {
	MT8169_AFE_IRQ_DIR_MCU = 0,
	MT8169_AFE_IRQ_DIR_DSP,
	MT8169_AFE_IRQ_DIR_BOTH,
};

enum {
	MTKAIF_PROTOCOL_1 = 0,
	MTKAIF_PROTOCOL_2,
	MTKAIF_PROTOCOL_2_CLK_P2,
};

enum {
	MTK_AFE_ADDA_DL_GAIN_MUTE = 0,
	MTK_AFE_ADDA_DL_GAIN_NORMAL = 0xf74f,
	/* SA suggest apply -0.3db to audio/speech path */
};

#define MTK_SPK_NOT_SMARTPA_STR "MTK_SPK_NOT_SMARTPA"
#define MTK_SPK_RICHTEK_RT5509_STR "MTK_SPK_RICHTEK_RT5509"
#define MTK_SPK_MEDIATEK_MT6660_STR "MTK_SPK_MEDIATEK_MT6660"
#define MTK_SPK_GOODIX_TFA9874_STR "MTK_SPK_GOODIX_TFA9874"

#define MTK_SPK_I2S_0_STR "MTK_SPK_I2S_0"
#define MTK_SPK_I2S_1_STR "MTK_SPK_I2S_1"
#define MTK_SPK_I2S_2_STR "MTK_SPK_I2S_2"
#define MTK_SPK_I2S_3_STR "MTK_SPK_I2S_3"
#define MTK_SPK_I2S_5_STR "MTK_SPK_I2S_5"
#define MTK_SPK_I2S_6_STR "MTK_SPK_I2S_6"
#define MTK_SPK_I2S_7_STR "MTK_SPK_I2S_7"
#define MTK_SPK_I2S_8_STR "MTK_SPK_I2S_8"
#define MTK_SPK_I2S_9_STR "MTK_SPK_I2S_9"

#define APLL_AO(_level)	(((_level) & MTK_APLL_AO) >> 3)
#define MCLK_AO(_level)	(((_level) & MTK_MCLK_AO) >> 2)
#define BCLK_AO(_level)	(((_level) & MTK_BCLK_AO) >> 1)
#define LRLK_AO(_level)	(((_level) & MTK_LRCK_AO) >> 0)

/* MCLK */
enum {
	MT8169_I2S0_MCK = 0,
	MT8169_I2S1_MCK,
	MT8169_I2S2_MCK,
	MT8169_I2S4_MCK,
	MT8169_TDM_MCK,
	MT8169_MCK_NUM,
};

enum mtk_spk_type {
	MTK_SPK_NOT_SMARTPA = 0,
	MTK_SPK_RICHTEK_RT5509,
	MTK_SPK_MEDIATEK_MT6660,
	MTK_SPK_GOODIX_TFA9874,
	MTK_SPK_TYPE_NUM
};

enum mtk_spk_i2s_type {
	MTK_SPK_I2S_TYPE_INVALID = -1,
	MTK_SPK_I2S_0,
	MTK_SPK_I2S_1,
	MTK_SPK_I2S_2,
	MTK_SPK_I2S_3,
	MTK_SPK_I2S_TYPE_NUM
};

/* SMC CALL Operations */
enum mtk_audio_smc_call_op {
	MTK_AUDIO_SMC_OP_INIT = 0,
	MTK_AUDIO_SMC_OP_DRAM_REQUEST,
	MTK_AUDIO_SMC_OP_DRAM_RELEASE,
	MTK_AUDIO_SMC_OP_FM_REQUEST,
	MTK_AUDIO_SMC_OP_FM_RELEASE,
	MTK_AUDIO_SMC_OP_ADSP_REQUEST,
	MTK_AUDIO_SMC_OP_ADSP_RELEASE,
	MTK_AUDIO_SMC_OP_NUM
};

enum mtk_clk_always_on_level {
	MTK_LRCK_AO = 0x1,
	MTK_BCLK_AO = 0x2,
	MTK_MCLK_AO = 0x4,
	MTK_APLL_AO = 0x8,
};

enum {
	TDM_IN_I2S = 0,
	TDM_IN_LJ = 1,
	TDM_IN_RJ = 2,
	TDM_IN_EIAJ = 3,
	TDM_IN_DSP_A = 4,
	TDM_IN_DSP_B = 5,
};

enum {
	TDM_DATA_ONE_PIN = 0,
	TDM_DATA_MULTI_PIN,
};

enum {
	TDM_BCK_NON_INV = 0,
	TDM_BCK_INV = 1,
};

enum {
	TDM_LRCK_NON_INV = 0,
	TDM_LRCK_INV = 1,
};

#if IS_ENABLED(CONFIG_MTK_HIFIXDSP_SUPPORT)
struct mt8169_adsp_data {
	/* information adsp supply */
	bool adsp_on;
	int (*adsp_active)(void);
	/* information afe supply */
	int (*afe_memif_set)(struct mtk_base_afe *afe,
				       int memif_id,
				       unsigned int rate,
				       unsigned int channels,
				       snd_pcm_format_t format);
	int (*afe_memif_enable)(struct mtk_base_afe *afe,
				       int memif_id,
				       unsigned int rate,
				       unsigned int period_size,
				       int enable);
	int (*afe_memif_init)(struct mtk_base_afe *afe, int memif_id);
	int (*afe_memif_uninit)(struct mtk_base_afe *afe, int memif_id);
	int (*afe_sram_get)(struct mtk_base_afe *afe,
			    dma_addr_t *paddr,
			    unsigned char **vaddr,
			    unsigned int size,
			    void *user);
	int (*afe_sram_put)(struct mtk_base_afe *afe,
			    dma_addr_t *paddr,
			    void *user);
	void (*afe_init)(struct mtk_base_afe *afe);
	void (*afe_uninit)(struct mtk_base_afe *afe);
};
#endif


struct snd_pcm_substream;
struct mtk_base_irq_data;
struct clk;

struct mtk_clk_ao_attr {
	bool apll_ao;
	bool mclk_ao;
	bool bclk_ao;
	bool lrck_ao;
	bool clk_ao_enable;
	bool fix_low_jitter;
	int ao_level;
	int fix_lrck_rate;
	int fix_bclk_width;
	int fix_mclk_ratio;
	int fix_tdm_channels;
	int fix_tdm_mode;
	int fix_tdm_data_mode;
	int fix_tdm_slave_mode;
};

struct mt8169_afe_private {
	struct clk **clk;
	struct regmap *topckgen;
	struct regmap *apmixed;
	struct regmap *infracfg_ao;
	struct snd_soc_card *soc_card;
	int irq_cnt[MT8169_MEMIF_NUM];
	int use_sram_force[MT8169_MEMIF_NUM];
	int stf_positive_gain_db;
	int dram_resource_counter;
	int sgen_mode;
	int sgen_rate;
	int sgen_amplitude;
	/* usb call */
	int usb_call_echo_ref_enable;
	int usb_call_echo_ref_size;
	bool usb_call_echo_ref_reallocate;
	/* deep buffer playback */
	int deep_playback_state;
	/* fast playback */
	int fast_playback_state;
	/* mmap playback */
	int mmap_playback_state;
	/* mmap record */
	int mmap_record_state;
	/* primary playback */
	int primary_playback_state;
	/* voip rx */
	int voip_rx_state;
	/* xrun assert */
	int xrun_assert[MT8169_MEMIF_NUM];

	int runtime_suspend;
	/* dai */
	bool dai_on[MT8169_DAI_NUM];
	void *dai_priv[MT8169_DAI_NUM];

	/* adda */
	bool mtkaif_calibration_ok;
	int mtkaif_protocol;
	int mtkaif_chosen_phase[4];
	int mtkaif_phase_cycle[4];
	int mtkaif_calibration_num_phase;
	int mtkaif_dmic;
	int mtkaif_looback0;
	int mtkaif_looback1;

	/* mck */
	int mck_rate[MT8169_MCK_NUM];
#if IS_ENABLED(CONFIG_MTK_HIFIXDSP_SUPPORT)
	struct mt8169_adsp_data adsp_data;
#endif
	struct pm_qos_request qos_request;

	/* clk always on */
	struct mtk_clk_ao_attr clk_ao_data[MT8169_DAI_NUM];
	bool use_apll;

	/* hw src mch mode */
	bool asrc_mch_mode;
};

struct gnt_memif_info {
	int dai_id;
	char dai_name[32];
	unsigned int pbuf_size_reg;
	unsigned int pbuf_size_shift;
	unsigned int pbuf_size_mask;
	unsigned int pbuf_size_unit;
	unsigned int irq_mon_reg;
	unsigned int irq_mon_sft_mask;
	int cur_tolerance;
};

int mt8169_dai_adda_register(struct mtk_base_afe *afe);
int mt8169_dai_i2s_register(struct mtk_base_afe *afe);
int mt8169_dai_tdm_register(struct mtk_base_afe *afe);
int mt8169_dai_hw_gain_register(struct mtk_base_afe *afe);
int mt8169_dai_src_register(struct mtk_base_afe *afe);
int mt8169_dai_pcm_register(struct mtk_base_afe *afe);

int mt8169_dai_hostless_register(struct mtk_base_afe *afe);

int mt8169_add_misc_control(struct snd_soc_component *component);

int mt8169_set_local_afe(struct mtk_base_afe *afe);

unsigned int mt8169_general_rate_transform(struct device *dev,
					   unsigned int rate);
unsigned int mt8169_rate_transform(struct device *dev,
				   unsigned int rate,
				   int aud_blk);
unsigned int mt8169_tdm_relatch_rate_transform(struct device *dev,
					       unsigned int rate);
unsigned int mt8169_get_tdm_ch_per_sdata(unsigned int mode,
					 unsigned int channels);

int mt8169_enable_dc_compensation(bool enable);
int mt8169_set_lch_dc_compensation(int value);
int mt8169_set_rch_dc_compensation(int value);
int mt8169_adda_dl_gain_control(bool mute);

int mt8169_dai_set_priv(struct mtk_base_afe *afe, int id,
			int priv_size, const void *priv_data);

#if IS_ENABLED(CONFIG_MTK_HIFIXDSP_SUPPORT)
struct mtk_base_afe *mt8169_afe_pcm_get_info(void);
#endif
extern u64 mtk_timer_get_cnt(u8 timer);

#endif
