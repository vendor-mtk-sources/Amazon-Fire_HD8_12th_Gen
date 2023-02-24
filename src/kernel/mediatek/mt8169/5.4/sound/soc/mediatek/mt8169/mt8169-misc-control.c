// SPDX-License-Identifier: GPL-2.0
/*
 *  MediaTek ALSA SoC Audio Misc Control
 *
 *  Copyright (c) 2021 MediaTek Inc.
 *  Author: Jiaxin Yu <jiaxin.yu@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/regmap.h>
#include <sound/soc.h>

#include "../common/mtk-afe-fe-dai.h"
#include "../common/mtk-afe-platform-driver.h"
#include "mt8169-afe-common.h"

#define SGEN_MUTE_CH1_KCONTROL_NAME "Audio_SineGen_Mute_Ch1"
#define SGEN_MUTE_CH2_KCONTROL_NAME "Audio_SineGen_Mute_Ch2"

static const char * const mt8169_sgen_mode_str[] = {
	"I0I1",   "I2",     "I3I4",   "I5I6",
	"I7I8",   "I9I22",  "I10I11", "I12I13",
	"I14I21", "I15I16", "I17I18", "I19I20",
	"I23I24", "I25I26", "I27I28", "I33",
	"I34I35", "I36I37", "I38I39", "I40I41",
	"I42I43", "I44I45", "I46I47", "I48I49",
	"I56I57", "I58I59", "I60I61", "I62I63",
	"O0O1",   "O2",     "O3O4",   "O5O6",
	"O7O8",   "O9O10",  "O11",    "O12",
	"O13O14", "O15O16", "O17O18", "O19O20",
	"O21O22", "O23O24", "O25",    "O28O29",
	"O34",    "O35",    "O32O33", "O36O37",
	"O38O39", "O30O31", "O40O41", "O42O43",
	"O44O45", "O46O47", "O48O49", "O50O51",
	"O58O59", "O60O61", "O62O63", "O64O65",
	"O66O67", "O68O69", "O26O27", "OFF",
};

static const int mt8169_sgen_mode_idx[] = {
	0, 2, 4, 6,
	8, 22, 10, 12,
	14, -1, 18, 20,
	24, 26, 28, 33,
	34, 36, 38, 40,
	42, 44, 46, 48,
	56, 58, 60, 62,
	128, 130, 132, 134,
	135, 138, 139, 140,
	142, 144, 166, 148,
	150, 152, 153, 156,
	162, 163, 160, 164,
	166, -1, 168, 170,
	172, 174, 176, 178,
	186, 188, 190, 192,
	194, 196, -1, -1,
};

static const char * const mt8169_sgen_rate_str[] = {
	"8K", "11K", "12K", "16K",
	"22K", "24K", "32K", "44K",
	"48K", "88k", "96k", "176k",
	"192k"
};

static const int mt8169_sgen_rate_idx[] = {
	0, 1, 2, 4,
	5, 6, 8, 9,
	10, 11, 12, 13,
	14
};

/* this order must match reg bit amp_div_ch1/2 */
static const char * const mt8169_sgen_amp_str[] = {
	"1/128", "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1" };
static const char * const mt8169_sgen_mute_str[] = {
	"Off", "On"
};

static int mt8169_sgen_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->sgen_mode;
	return 0;
}

static int mt8169_sgen_set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int mode;
	int mode_idx;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	mode = ucontrol->value.integer.value[0];
	mode_idx = mt8169_sgen_mode_idx[mode];

	dev_info(afe->dev, "%s(), mode %d, mode_idx %d\n",
		 __func__, mode, mode_idx);

	if (mode_idx >= 0) {
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON2,
				   INNER_LOOP_BACK_MODE_MASK_SFT,
				   mode_idx << INNER_LOOP_BACK_MODE_SFT);
		//regmap_write(afe->regmap, AFE_SINEGEN_CON0, 0x04ac2ac1);
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
				   DAC_EN_MASK_SFT,
				   0x1 << DAC_EN_SFT);
	} else {
		/* disable sgen */
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
				   DAC_EN_MASK_SFT,
				   0x0);
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON2,
				   INNER_LOOP_BACK_MODE_MASK_SFT,
				   0x3f << INNER_LOOP_BACK_MODE_SFT);
	}

	afe_priv->sgen_mode = mode;
	return 0;
}

static int mt8169_sgen_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->sgen_rate;
	return 0;
}

static int mt8169_sgen_rate_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int rate;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	rate = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), rate %d\n", __func__, rate);

	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
			   SINE_MODE_CH1_MASK_SFT,
			   mt8169_sgen_rate_idx[rate] << SINE_MODE_CH1_SFT);

	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
			   SINE_MODE_CH2_MASK_SFT,
			   mt8169_sgen_rate_idx[rate] << SINE_MODE_CH2_SFT);

	afe_priv->sgen_rate = rate;
	return 0;
}

static int mt8169_sgen_amplitude_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->sgen_amplitude;
	return 0;
}

static int mt8169_sgen_amplitude_set(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int amplitude;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	amplitude = ucontrol->value.integer.value[0];
	if (amplitude > AMP_DIV_CH1_MASK) {
		dev_info(afe->dev, "%s(), amplitude %d invalid\n",
			 __func__, amplitude);
		return -EINVAL;
	}

	dev_info(afe->dev, "%s(), amplitude %d\n", __func__, amplitude);

	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
			   AMP_DIV_CH1_MASK_SFT,
			   amplitude << AMP_DIV_CH1_SFT);
	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
			   AMP_DIV_CH2_MASK_SFT,
			   amplitude << AMP_DIV_CH2_SFT);

	afe_priv->sgen_amplitude = amplitude;

	return 0;
}

static int mt8169_sgen_mute_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int mute = 0;

	regmap_read(afe->regmap, AFE_SINEGEN_CON0, &mute);

	if (strcmp(kcontrol->id.name, SGEN_MUTE_CH1_KCONTROL_NAME) == 0)
		return (mute >> MUTE_SW_CH1_SFT) & MUTE_SW_CH1_MASK;
	else
		return (mute >> MUTE_SW_CH2_SFT) & MUTE_SW_CH2_MASK;
}

static int mt8169_sgen_mute_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int mute;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	mute = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), kcontrol name %s, mute %d\n",
		 __func__, kcontrol->id.name, mute);

	if (strcmp(kcontrol->id.name, SGEN_MUTE_CH1_KCONTROL_NAME) == 0) {
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
				   MUTE_SW_CH1_MASK_SFT,
				   mute << MUTE_SW_CH1_SFT);
	} else {
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
				   MUTE_SW_CH2_MASK_SFT,
				   mute << MUTE_SW_CH2_SFT);
	}

	return 0;
}

#ifdef CONFIG_KPD_VOLUME_KEY_SWAP
enum mt8168_afe_volume_key_switch {
	VOLKEY_NORMAL = 0,
	VOLKEY_SWAP
};

static const char *const mt8168_afe_volume_key_switch[] = { "VOLKEY_NORMAL", "VOLKEY_SWAP" };

extern void set_kpd_swap_vol_key(bool flag);
extern bool get_kpd_swap_vol_key(void);

static int mt8168_afe_volkey_switch_get(
				struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	if (get_kpd_swap_vol_key())
		ucontrol->value.integer.value[0] = VOLKEY_SWAP;
	else
		ucontrol->value.integer.value[0] = VOLKEY_NORMAL;

	return 0;
}

static int mt8168_afe_volkey_switch_set(
				struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.integer.value[0] == VOLKEY_NORMAL)
		set_kpd_swap_vol_key(false);
	else
		set_kpd_swap_vol_key(true);

	return 0;
}

static const struct soc_enum mt8168_afe_volkey_control_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt8168_afe_volume_key_switch),
		mt8168_afe_volume_key_switch),
};
#endif

static const struct soc_enum mt8169_afe_sgen_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt8169_sgen_mode_str),
			    mt8169_sgen_mode_str),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt8169_sgen_rate_str),
			    mt8169_sgen_rate_str),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt8169_sgen_amp_str),
			    mt8169_sgen_amp_str),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt8169_sgen_mute_str),
			    mt8169_sgen_mute_str),
};

static const struct snd_kcontrol_new mt8169_afe_sgen_controls[] = {
	SOC_ENUM_EXT("Audio_SineGen_Switch", mt8169_afe_sgen_enum[0],
		     mt8169_sgen_get, mt8169_sgen_set),
	SOC_ENUM_EXT("Audio_SineGen_SampleRate", mt8169_afe_sgen_enum[1],
		     mt8169_sgen_rate_get, mt8169_sgen_rate_set),
	SOC_ENUM_EXT("Audio_SineGen_Amplitude", mt8169_afe_sgen_enum[2],
		     mt8169_sgen_amplitude_get, mt8169_sgen_amplitude_set),
	SOC_ENUM_EXT(SGEN_MUTE_CH1_KCONTROL_NAME, mt8169_afe_sgen_enum[3],
		     mt8169_sgen_mute_get, mt8169_sgen_mute_set),
	SOC_ENUM_EXT(SGEN_MUTE_CH2_KCONTROL_NAME, mt8169_afe_sgen_enum[3],
		     mt8169_sgen_mute_get, mt8169_sgen_mute_set),
	SOC_SINGLE("Audio_SineGen_Freq_Div_Ch1", AFE_SINEGEN_CON0,
		   FREQ_DIV_CH1_SFT, FREQ_DIV_CH1_MASK, 0),
	SOC_SINGLE("Audio_SineGen_Freq_Div_Ch2", AFE_SINEGEN_CON0,
		   FREQ_DIV_CH2_SFT, FREQ_DIV_CH2_MASK, 0),
#ifdef CONFIG_KPD_VOLUME_KEY_SWAP
	SOC_ENUM_EXT("VOLKEY_SWITCH", mt8168_afe_volkey_control_enum[0],
			mt8168_afe_volkey_switch_get,
			mt8168_afe_volkey_switch_set),
#endif
};

static long long mt8169_afe_get_next_write_timestamp(struct mtk_base_afe *afe,
				    struct mtk_base_afe_memif *memif)
{
	struct mt8169_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_card *card = afe_priv->soc_card;
	struct snd_pcm_substream *substream;
	struct snd_soc_pcm_runtime *rtd = NULL;
	struct snd_pcm_runtime *runtime;
	snd_pcm_sframes_t delay;
	unsigned long flag;
	int ret;
	static long long lastValidTimestamp = -1;
	long long timestamp = 0;
	uint64_t temp;
	int real_remain_size;
	int rate;
	int i = 0;

	list_for_each_entry(rtd, &card->rtd_list, list) {
		i++;
		if (!strcmp(rtd->cpu_dai->name, memif->gnt_info->dai_name))
			break;
	}

	if (i == card->num_rtd) {
		dev_info(afe->dev, "%s() can not find the target substream\n",
			 __func__);
		return lastValidTimestamp;
	}

	substream = rtd->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	if (substream == NULL) {
		dev_info(afe->dev, "%s() substream == NULL\n", __func__);
		return -EINVAL;
	}

	/* get target sub stream information */
	snd_pcm_stream_lock_irqsave(substream, flag);
	runtime = substream->runtime;
	if (runtime == NULL) {
		dev_info(afe->dev, "%s() playback substream not opened(%d)\n",
			 __func__, substream->hw_opened);
		snd_pcm_stream_unlock_irqrestore(substream, flag);
		goto output_from_regs;
	}

	if ((runtime->status->state == SNDRV_PCM_STATE_OPEN) ||
	    (runtime->status->state == SNDRV_PCM_STATE_DISCONNECTED) ||
	    (runtime->status->state == SNDRV_PCM_STATE_SUSPENDED)) {
		dev_info(afe->dev, "%s() playback state(%d) is not right\n",
			 __func__, runtime->status->state);
		snd_pcm_stream_unlock_irqrestore(substream, flag);
		goto output_from_regs;
	}

	rate = (int)(runtime->rate);

	if (runtime->status->state == SNDRV_PCM_STATE_XRUN) {
		real_remain_size = 0;
		timestamp = (long long)mtk_timer_get_cnt(6);
		snd_pcm_stream_unlock_irqrestore(substream, flag);
		dev_info(afe->dev, "%s() playback state is xrun state\n",
			 __func__);
		goto output_cal;
	}

	if ((runtime->status->state != SNDRV_PCM_STATE_XRUN) &&
		  (!snd_pcm_running(substream))) {
		real_remain_size = (int)(runtime->buffer_size
			- snd_pcm_playback_avail(runtime)
			+ runtime->delay);
		timestamp = (long long)mtk_timer_get_cnt(6);
		snd_pcm_stream_unlock_irqrestore(substream, flag);
		goto output_cal;
	}
	snd_pcm_stream_unlock_irqrestore(substream, flag);

	ret = snd_pcm_kernel_ioctl(substream, SNDRV_PCM_IOCTL_DELAY, (void *)&delay);
	if (ret < 0) {
		pr_info("%s SNDRV_PCM_IOCTL_DELAY fail ret:%d\n",
			__func__, ret);
		goto output_from_regs;
	}

	snd_pcm_stream_lock_irqsave(substream, flag);
	timestamp = memif->timestamp;
	real_remain_size = snd_pcm_playback_hw_avail(runtime) + runtime->delay;
	snd_pcm_stream_unlock_irqrestore(substream, flag);

	dev_info(afe->dev, "%s() real_remain_size(%d), delay(%d)\n",
		 __func__, real_remain_size, runtime->delay);

output_cal:

	temp = (13000000LL) * (uint64_t)real_remain_size;
	do_div(temp, (uint64_t)rate);
	timestamp += (long long)temp;
	lastValidTimestamp = timestamp;
	return timestamp;

output_from_regs:

	timestamp = (long long)mtk_timer_get_cnt(6);
	lastValidTimestamp = timestamp;
	return timestamp;
}

static int mt8169_afe_dl1_timestamp_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_base_afe_memif *memif = &afe->memif[MT8169_MEMIF_DL1];

	ucontrol->value.integer64.value[0] =
	    mt8169_afe_get_next_write_timestamp(afe, memif);

	return 0;
}

static const struct snd_kcontrol_new mt8169_afe_gnt_controls[] = {
	SND_SOC_BYTES_EXT("DL1 Timestamp", 8,
			  mt8169_afe_dl1_timestamp_get, NULL),
};

int mt8169_add_misc_control(struct snd_soc_component *component)
{
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);

	dev_info(afe->dev, "%s()\n", __func__);

	snd_soc_add_component_controls(component,
				      mt8169_afe_sgen_controls,
				      ARRAY_SIZE(mt8169_afe_sgen_controls));

	snd_soc_add_component_controls(component,
				      mt8169_afe_gnt_controls,
				      ARRAY_SIZE(mt8169_afe_gnt_controls));

	return 0;
}

