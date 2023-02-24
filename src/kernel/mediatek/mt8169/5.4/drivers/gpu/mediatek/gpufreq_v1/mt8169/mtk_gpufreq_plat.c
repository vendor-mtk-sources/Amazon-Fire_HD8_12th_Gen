// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

/**
 * @file    mtk_gpufreq_plat.c
 * @brief   Driver for GPU-DVFS
 */

/**
 * ===============================================
 * SECTION : Include files
 * ===============================================
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>

#include <mboot_params.h>
#include "mtk_gpufreq.h"
#include "mtk_gpufreq_internal.h"
#include "mtk_gpufreq_common.h"
#include "mtk_bp_thl.h"
#include "mtk_low_battery_throttling.h"
#include "mtk_battery_oc_throttling.h"
//kkk disable for build, need enable after bringup
#ifdef CONFIG_THERMAL
#undef CONFIG_THERMAL
#endif
#if IS_ENABLED(CONFIG_THERMAL)
#include "mtk_thermal.h"
#endif
#if IS_ENABLED(CONFIG_MTK_FREQ_HOPPING)
#include "mtk_freqhopping_drv.h"
#endif
#if MT_GPUFREQ_KICKER_PBM_READY && IS_ENABLED(CONFIG_MTK_PBM)
#include "mtk_pbm.h"
#endif
#if MT_GPUFREQ_STATIC_PWR_READY2USE
#include "mtk_static_power.h"
#endif
#include "mtk_gpu_utility.h"

#if IS_ENABLED(CONFIG_MTK_DEVINFO)
#include <linux/nvmem-consumer.h>
#endif

enum gpu_dvfs_vgpu_step {
	GPU_DVFS_VGPU_STEP_1 = 0x1,
	GPU_DVFS_VGPU_STEP_2 = 0x2,
	GPU_DVFS_VGPU_STEP_3 = 0x3,
	GPU_DVFS_VGPU_STEP_4 = 0x4,
	GPU_DVFS_VGPU_STEP_5 = 0x5,
	GPU_DVFS_VGPU_STEP_6 = 0x6,
	GPU_DVFS_VGPU_STEP_7 = 0x7,
	GPU_DVFS_VGPU_STEP_8 = 0x8,
	GPU_DVFS_VGPU_STEP_9 = 0x9,
	GPU_DVFS_VGPU_STEP_A = 0xA,
	GPU_DVFS_VGPU_STEP_B = 0xB,
	GPU_DVFS_VGPU_STEP_C = 0xC,
	GPU_DVFS_VGPU_STEP_D = 0xD,
	GPU_DVFS_VGPU_STEP_E = 0xE,
	GPU_DVFS_VGPU_STEP_F = 0xF,
};

static inline void gpu_dvfs_vgpu_footprint(enum gpu_dvfs_vgpu_step step)
{
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	aee_rr_rec_gpu_dvfs_vgpu(step |
				(aee_rr_curr_gpu_dvfs_vgpu() & 0xF0));
#endif
}

static inline void gpu_dvfs_vgpu_reset_footprint(void)
{
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	aee_rr_rec_gpu_dvfs_vgpu(0);
#endif
}

static inline void gpu_dvfs_oppidx_footprint(unsigned int idx)
{
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	aee_rr_rec_gpu_dvfs_oppidx(idx);
#endif
}

static inline void gpu_dvfs_oppidx_reset_footprint(void)
{
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	aee_rr_rec_gpu_dvfs_oppidx(0xFF);
#endif
}

static inline void gpu_dvfs_power_count_footprint(int count)
{
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	aee_rr_rec_gpu_dvfs_power_count(count);
#endif
}

static inline void gpu_dvfs_power_count_reset_footprint(void)
{
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	aee_rr_rec_gpu_dvfs_power_count(0);
#endif
}

/**
 * ===============================================
 * SECTION : Local functions declaration
 * ===============================================
 */
static int __mt_gpufreq_pdrv_probe(struct platform_device *pdev);
static void __mt_gpufreq_set(
		unsigned int idx_old,
		unsigned int idx_new,
		unsigned int freq_old,
		unsigned int freq_new,
		unsigned int vgpu_old,
		unsigned int vgpu_new,
		unsigned int vsram_gpu_old,
		unsigned int vsram_gpu_new);
static void __mt_gpufreq_set_fixed_vgpu(int fixed_vgpu);
static void __mt_gpufreq_set_fixed_freq(int fixed_freq);
static unsigned int __mt_gpufreq_get_cur_vgpu(void);
static unsigned int __mt_gpufreq_get_cur_freq(void);
static unsigned int __mt_gpufreq_get_cur_vsram_gpu(void);
static unsigned int __mt_gpufreq_get_segment_id(void);
static unsigned int __mt_gpufreq_get_vsram_gpu_by_vgpu(unsigned int vgpu);
static void __mt_gpufreq_kick_pbm(int enable);
static void __mt_gpufreq_clock_switch(unsigned int freq_new);
static void __mt_gpufreq_volt_switch(
		unsigned int vgpu_old, unsigned int vgpu_new,
		unsigned int vsram_gpu_old, unsigned int vsram_gpu_new);
static void __mt_gpufreq_volt_switch_without_vsram_gpu(
		unsigned int vgpu_old, unsigned int vgpu_new);

static void __mt_gpufreq_setup_opp_power_table(int num);
static void mt_gpufreq_cal_sb_opp_index(void);

static unsigned int __calculate_vgpu_settletime(bool mode, int deltaV);
static unsigned int __calculate_vsram_settletime(bool mode, int deltaV);

static void mt_gpufreq_update_volt(unsigned int idx);

unsigned long (*mtk_devfreq_get_voltage_fp)(unsigned long) = NULL;
EXPORT_SYMBOL(mtk_devfreq_get_voltage_fp);

void (*mtk_devfreq_set_cur_freq_fp)(unsigned long) = NULL;
EXPORT_SYMBOL(mtk_devfreq_set_cur_freq_fp);

/**
 * ===============================================
 * SECTION : Local variables definition
 * ===============================================
 */

static struct platform_device *g_pdev;
static struct mt_gpufreq_power_table_info *g_power_table;
static struct g_pmic_info *g_pmic;
static struct g_clk_info *g_clk;
static struct g_mtcmos_info *g_mtcmos;

static const struct of_device_id g_gpufreq_of_match[] = {
	{ .compatible = "mediatek,mt8169-gpufreq" },
	{ /* sentinel */ }
};
static struct platform_driver g_gpufreq_pdrv = {
	.probe = __mt_gpufreq_pdrv_probe,
	.remove = NULL,
	.driver = {
		.name = "gpufreq",
		.owner = THIS_MODULE,
		.of_match_table = g_gpufreq_of_match,
	},
};

static bool g_cg_on;
static bool g_mtcmos_on;
static bool g_buck_on;
static bool g_fixed_freq_volt_state;
static bool g_probe_done;
static int g_power_count;
static unsigned int g_opp_stress_test_state;
static unsigned int g_max_opp_idx_num;
static unsigned int g_segment_max_opp_idx;
static unsigned int g_segment_min_opp_idx;
static unsigned int g_aging_enable;
static unsigned int g_cur_opp_freq;
static unsigned int g_cur_opp_vgpu;
static unsigned int g_cur_opp_vsram_gpu;
static unsigned int g_cur_opp_idx;
static unsigned int g_fixed_freq;
static unsigned int g_fixed_vgpu;
static unsigned int g_max_upper_limited_idx;
static unsigned int g_min_lower_limited_idx;
static unsigned int g_upper_kicker;
static unsigned int g_lower_kicker;
static unsigned int g_lkg_pwr;

static unsigned int g_vgpu_sfchg_rrate;
static unsigned int g_vgpu_sfchg_frate;
static unsigned int g_vsram_sfchg_rrate;
static unsigned int g_vsram_sfchg_frate;

static int g_opp_sb_idx_up[NUM_OF_OPP_IDX] = { 0 };
static int g_opp_sb_idx_down[NUM_OF_OPP_IDX] = { 0 };

static DEFINE_MUTEX(mt_gpufreq_lock);
static DEFINE_MUTEX(mt_gpufreq_power_lock);
static DEFINE_MUTEX(mt_gpufreq_limit_table_lock);

static void __iomem *g_apmixed_base;
static void __iomem *g_mfg_base;
static void __iomem *g_sleep;

unsigned int mt_gpufreq_get_shader_present(void)
{
	return MT_GPU_SHADER_PRESENT_2;
}
EXPORT_SYMBOL(mt_gpufreq_get_shader_present);

static unsigned int mt_gpufreq_return_by_condition(
			unsigned int limit_idx, enum mt_gpufreq_kicker kicker)
{
	unsigned int ret = 0;

	/* GPU DVFS disabled */
	if (!mt_gpufreq_get_dvfs_en())
		ret |= (1 << 0);

	if (limit_idx > g_segment_min_opp_idx ||
				limit_idx < g_segment_max_opp_idx) {
		ret |= (1 << 1);
		gpufreq_pr_info("@%s:out of segment opp range, %d (%d ~ %d)\n",
				__func__, limit_idx, g_segment_max_opp_idx, g_segment_min_opp_idx);
	}

	/* if /proc/gpufreq/gpufreq_fixed_freq_volt fix freq and volt */
	if (g_fixed_freq_volt_state)
		ret |= (1 << 2);

	/* update voltage in opp table from devfreq/ptp */
	mt_gpufreq_update_volt(limit_idx);

	/* the same freq && volt */
	if (g_cur_opp_freq == g_opp_table[limit_idx].gpufreq_khz &&
			g_cur_opp_vgpu == g_opp_table[limit_idx].gpufreq_vgpu)
		ret |= (1 << 3);

	gpufreq_pr_logbuf("return_by_condition: 0x%x\n", ret);

	return ret;
}

static void mt_gpufreq_update_limit_idx(unsigned int kicker,
		unsigned int t_upper_idx, unsigned int t_lower_idx)
{
	unsigned int i;
	unsigned int upper_kicker, lower_kicker;
	unsigned int upper_prio, lower_prio;
	unsigned int upper_limit_idx, lower_limit_idx;

	mutex_lock(&mt_gpufreq_limit_table_lock);

	if (limit_table[kicker].upper_idx == t_upper_idx &&
	   limit_table[kicker].lower_idx == t_lower_idx) {
		mutex_unlock(&mt_gpufreq_limit_table_lock);
		return;
	}

	limit_table[kicker].upper_idx = t_upper_idx;
	limit_table[kicker].lower_idx = t_lower_idx;

	gpufreq_pr_debug("@%s: kicker=%d t_upper_idx=%d t_lower_idx=%d\n",
			__func__, kicker, t_upper_idx, t_lower_idx);

	upper_kicker = NUM_OF_KIR;
	lower_kicker = NUM_OF_KIR;

	upper_prio = GPUFREQ_LIMIT_PRIO_NONE;
	lower_prio = GPUFREQ_LIMIT_PRIO_NONE;

	upper_limit_idx = g_segment_max_opp_idx;
	lower_limit_idx = g_segment_min_opp_idx;

	for (i = 0; i < NUM_OF_KIR; i++) {
		/* check upper limit */
		/* choose limit idx not default and limit is enable */
		if (limit_table[i].upper_idx != LIMIT_IDX_DEFAULT &&
			limit_table[i].upper_enable == LIMIT_ENABLE) {
			/* choose limit idx of higher priority */
			if (limit_table[i].prio > upper_prio) {
				upper_kicker = i;
				upper_limit_idx = limit_table[i].upper_idx;
				upper_prio = limit_table[i].prio;
			}
			/* choose big limit idx if proiority is the same */
			else if ((limit_table[i].upper_idx > upper_limit_idx) &&
				(limit_table[i].prio == upper_prio)) {
				upper_kicker = i;
				upper_limit_idx = limit_table[i].upper_idx;
				upper_prio = limit_table[i].prio;
			}
		}

		/* check lower limit */
		/* choose limit idx not default and limit is enable */
		if (limit_table[i].lower_idx != LIMIT_IDX_DEFAULT &&
			limit_table[i].lower_enable == LIMIT_ENABLE) {
			/* choose limit idx of higher priority */
			if (limit_table[i].prio > lower_prio) {
				lower_kicker = i;
				lower_limit_idx = limit_table[i].lower_idx;
				lower_prio = limit_table[i].prio;
			}
			/* choose small limit idx if proiority is the same */
			else if ((limit_table[i].lower_idx < lower_limit_idx) &&
				(limit_table[i].prio == lower_prio)) {
				lower_kicker = i;
				lower_limit_idx = limit_table[i].lower_idx;
				lower_prio = limit_table[i].prio;
			}
		}
	}

	mutex_unlock(&mt_gpufreq_limit_table_lock);

	g_upper_kicker = upper_kicker;
	g_lower_kicker = lower_kicker;

	if (upper_limit_idx > lower_limit_idx) {
		if (upper_prio >= lower_prio)
			lower_limit_idx = g_segment_min_opp_idx;
		else
			upper_limit_idx = g_segment_max_opp_idx;
	}

	g_max_upper_limited_idx = upper_limit_idx;
	g_min_lower_limited_idx = lower_limit_idx;
}

static void mt_gpufreq_update_limit_enable(unsigned int kicker,
		unsigned int t_upper_enable, unsigned int t_lower_enable)
{
	mutex_lock(&mt_gpufreq_limit_table_lock);

	if (limit_table[kicker].upper_enable == t_upper_enable &&
	   limit_table[kicker].lower_enable == t_lower_enable) {
		mutex_unlock(&mt_gpufreq_limit_table_lock);
		return;
	}

	limit_table[kicker].upper_enable = t_upper_enable;
	limit_table[kicker].lower_enable = t_lower_enable;

	gpufreq_pr_debug("@%s: kicker=%d t_upper_enable=%d t_lower_enable=%d\n",
			__func__, kicker, t_upper_enable, t_lower_enable);

	mutex_unlock(&mt_gpufreq_limit_table_lock);
}

static unsigned int mt_gpufreq_limit_idx_by_condition(unsigned int target_idx)
{
	unsigned int limit_idx;

	limit_idx = target_idx;

	/* generate random segment OPP index for stress test */
	if (g_opp_stress_test_state == 1) {
		get_random_bytes(&target_idx, sizeof(target_idx));
		limit_idx = target_idx %
			(g_segment_min_opp_idx - g_segment_max_opp_idx + 1) +
			g_segment_max_opp_idx;
		mt_gpufreq_update_limit_idx(KIR_STRESS, limit_idx, limit_idx);
	}

	if (limit_idx < g_max_upper_limited_idx)
		limit_idx = g_max_upper_limited_idx;

	if (limit_idx > g_min_lower_limited_idx)
		limit_idx = g_min_lower_limited_idx;

	gpufreq_pr_logbuf(
		"limit_idx: %d, g_upper_kicker: %d, g_max_upper_limited_idx: %d, g_lower_kicker: %d, g_min_lower_limited_idx: %d\n",
		limit_idx,
		g_upper_kicker, g_max_upper_limited_idx,
		g_lower_kicker, g_min_lower_limited_idx);

	return limit_idx;
}

unsigned int mt_gpufreq_target(unsigned int request_idx,
					enum mt_gpufreq_kicker kicker)
{
	unsigned int target_idx, limit_idx;
	unsigned int return_condition;

	mutex_lock(&mt_gpufreq_lock);

	if (kicker == KIR_POLICY)
		target_idx = request_idx + g_segment_max_opp_idx;
	else if (kicker == KIR_PTPOD) /* devfreq/ptp NOT adjust freq */
		target_idx = g_cur_opp_idx;
	else
		target_idx = request_idx;

	gpufreq_pr_logbuf("kicker: %d, target_idx: %d (%d, %d)\n",
		kicker, target_idx, request_idx, g_segment_max_opp_idx);

	limit_idx = mt_gpufreq_limit_idx_by_condition(target_idx);

	return_condition = mt_gpufreq_return_by_condition(limit_idx, kicker);

	if (return_condition) {
		mutex_unlock(&mt_gpufreq_lock);
		return 0;
	}

	__mt_gpufreq_set(g_cur_opp_idx, limit_idx,
		g_cur_opp_freq, g_opp_table[limit_idx].gpufreq_khz,
		g_cur_opp_vgpu, g_opp_table[limit_idx].gpufreq_vgpu,
		g_cur_opp_vsram_gpu, g_opp_table[limit_idx].gpufreq_vsram);

	mutex_unlock(&mt_gpufreq_lock);
	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_target);

void mt_gpufreq_set_timestamp(void)
{
	gpufreq_pr_debug("@%s\n", __func__);

	/* write 1 into 0x13fb_f130 bit 0 to enable timestamp register */
	/* timestamp will be used by clGetEventProfilingInfo*/
	writel(0x00000001, g_mfg_base + 0x130);
}
EXPORT_SYMBOL(mt_gpufreq_set_timestamp);

void mt_gpufreq_check_bus_idle(void)
{
	u32 val;

	gpufreq_pr_debug("@%s\n", __func__);

	/* MFG_QCHANNEL_CON (0x13fb_f0b4) bit [1:0] = 0x1 */
	writel(0x00000001, g_mfg_base + 0xb4);

	/* set register MFG_DEBUG_SEL (0x13fb_f170) bit [7:0] = 0x03 */
	writel(0x00000003, g_mfg_base + 0x170);

	/* polling register MFG_DEBUG_TOP (0x13fb_f178) bit 2 = 0x1 */
	/* => 1 for bus idle, 0 for bus non-idle */
	do {
		val = readl(g_mfg_base + 0x178);
	} while ((val & 0x4) != 0x4);
}
EXPORT_SYMBOL(mt_gpufreq_check_bus_idle);

static void mt_gpufreq_external_cg_control(void)
{
	u32 val;

	gpufreq_pr_debug("@%s\n", __func__);

	/* MFG_GLOBAL_CON: 0x1300_00b0 bit [8] = 0x0 */
	/* MFG_GLOBAL_CON: 0x1300_00b0 bit [10] = 0x0 */
	val = readl(g_mfg_base + 0xb0);
	val &= ~(1UL << 8);
	val &= ~(1UL << 10);
	writel(val, g_mfg_base + 0xb0);

	/* MFG_ASYNC_CON: 0x1300_0020 bit [25:22] = 0xF */
	writel(readl(g_mfg_base + 0x20) | (0xF << 22), g_mfg_base + 0x20);

	/* MFG_ASYNC_CON_1: 0x1300_0024 bit [0] = 0x1 */
	writel(readl(g_mfg_base + 0x24) | (1UL), g_mfg_base + 0x24);
}

static void mt_gpufreq_cg_control(enum mt_power_state power)
{
	gpufreq_pr_debug("@%s: power=%d", __func__, power);

	if (power == POWER_ON) {
		if (clk_prepare_enable(g_clk->subsys_mfg_cg))
			gpufreq_pr_info("@%s: failed when enable subsys_mfg_cg\n",
					__func__);

		mt_gpufreq_external_cg_control();
	} else {
		clk_disable_unprepare(g_clk->subsys_mfg_cg);
	}

	g_cg_on = power;
}

static void mt_gpufreq_mtcmos_control(enum mt_power_state power)
{
	int ret = -1;
	unsigned int shader_present = 0;

	gpufreq_pr_debug("@%s: power=%d\n", __func__, power);

	shader_present = mt_gpufreq_get_shader_present();

	if (power == POWER_ON) {
		if (shader_present & MFG2_SHADER_STACK0) {
			ret = pm_runtime_get_sync(g_mtcmos->pm_domain_devs[0]);
			if (ret != 0)
				gpufreq_pr_info("failed enable mtcmos_mfg2 (%d)\n", ret);
		}

		if (shader_present & MFG3_SHADER_STACK2) {
			ret = pm_runtime_get_sync(g_mtcmos->pm_domain_devs[1]);
			if (ret != 0)
				gpufreq_pr_info("failed enable mtcmos_mfg3 (%d)\n", ret);
		}
	} else {
		if (shader_present & MFG3_SHADER_STACK2) {
			ret = pm_runtime_put_sync(g_mtcmos->pm_domain_devs[1]);
			if (ret != 0)
				gpufreq_pr_info("failed disable mtcmos_mfg3 (%d)\n", ret);
		}

		if (shader_present & MFG2_SHADER_STACK0) {
			ret = pm_runtime_put_sync(g_mtcmos->pm_domain_devs[0]);
			if (ret != 0)
				gpufreq_pr_info("failed disable mtcmos_mfg2 (%d)\n", ret);
		}
	}

	g_mtcmos_on = power;
}

static void mt_gpufreq_buck_control(enum mt_power_state power)
{
	gpufreq_pr_debug("@%s: power=%d", __func__, power);

	if (power == POWER_ON) {
		if (regulator_enable(g_pmic->reg_vsram_gpu)) {
			gpufreq_pr_info("@%s: fail tp enable VSRAM_GPU\n",
					__func__);
			return;
		}
		if (regulator_enable(g_pmic->reg_vgpu)) {
			gpufreq_pr_info("@%s: fail to enable VGPU\n",
					__func__);
			return;
		}
	} else {
		if (regulator_disable(g_pmic->reg_vgpu)) {
			gpufreq_pr_info("@%s: fail to disable VGPU\n",
					__func__);
			return;
		}
		if (regulator_disable(g_pmic->reg_vsram_gpu)) {
			gpufreq_pr_info("@%s: fail to disable VSRAM_GPU\n",
					__func__);
			return;
		}
	}

	g_buck_on = power;
	__mt_gpufreq_kick_pbm(power);
}

void mt_gpufreq_power_control(enum mt_power_state power, enum mt_cg_state cg,
			enum mt_mtcmos_state mtcmos, enum mt_buck_state buck)
{
	mutex_lock(&mt_gpufreq_lock);

	gpufreq_pr_debug("@%s: power=%d g_power_count=%d (cg=%d, mtcmos=%d, buck=%d)\n",
			__func__, power, g_power_count, cg, mtcmos, buck);

	if (power == POWER_ON)
		g_power_count++;
	else {
		check_pending_info();

		g_power_count--;
		gpu_assert(g_power_count >= 0, GPU_FREQ_EXCEPTION,
			"power=%d g_power_count=%d (todo cg: %d, mtcmos: %d, buck: %d)\n",
			power, g_power_count, cg, mtcmos, buck);
	}
	gpu_dvfs_power_count_footprint(g_power_count);

	if (power == POWER_ON) {
		gpu_dvfs_vgpu_footprint(GPU_DVFS_VGPU_STEP_1);

		if (buck == BUCK_ON)
			mt_gpufreq_buck_control(power);

		gpu_dvfs_vgpu_footprint(GPU_DVFS_VGPU_STEP_2);

		if (mtcmos == MTCMOS_ON)
			mt_gpufreq_mtcmos_control(power);

		gpu_dvfs_vgpu_footprint(GPU_DVFS_VGPU_STEP_3);

		if (cg == CG_ON)
			mt_gpufreq_cg_control(power);

		gpu_dvfs_vgpu_footprint(GPU_DVFS_VGPU_STEP_4);
		mtk_notify_gpu_power_change(1);
	} else {
		mtk_notify_gpu_power_change(0);

		gpu_dvfs_vgpu_footprint(GPU_DVFS_VGPU_STEP_5);

		if (cg == CG_OFF)
			mt_gpufreq_cg_control(power);

		gpu_dvfs_vgpu_footprint(GPU_DVFS_VGPU_STEP_6);

		if (mtcmos == MTCMOS_OFF)
			mt_gpufreq_mtcmos_control(power);

		gpu_dvfs_vgpu_footprint(GPU_DVFS_VGPU_STEP_7);

		if (buck == BUCK_OFF)
			mt_gpufreq_buck_control(power);

		gpu_dvfs_vgpu_footprint(GPU_DVFS_VGPU_STEP_8);
	}

	mutex_unlock(&mt_gpufreq_lock);
}
EXPORT_SYMBOL(mt_gpufreq_power_control);

/*
 * mali-devfreq is probe after gpufreq,
 * gpufreq will get 0 before mali-devfreq probe done,
 * in this case, still use the original voltage.
 */
void mt_gpufreq_update_volt(unsigned int idx)
{
	unsigned int vgpu_new = 0;

	if (mtk_devfreq_get_voltage_fp != NULL) {
		vgpu_new = mtk_devfreq_get_voltage_fp(g_opp_table[idx].gpufreq_khz*1000)/10;
		if (vgpu_new) {
			if (vgpu_new != g_opp_table[idx].gpufreq_vgpu) {
				g_opp_table[idx].gpufreq_vsram =
						__mt_gpufreq_get_vsram_gpu_by_vgpu(vgpu_new);
				g_opp_table[idx].gpufreq_vgpu = vgpu_new;
			}
		}
	}
}

void mt_gpufreq_apply_aging(bool apply)
{
	int i;

	for (i = 0; i < g_max_opp_idx_num; i++) {
		if (apply) {
			g_opp_table[i].gpufreq_vgpu -=
					g_opp_table[i].gpufreq_aging_margin;
		} else {
			g_opp_table[i].gpufreq_vgpu +=
					g_opp_table[i].gpufreq_aging_margin;
		}

		g_opp_table[i].gpufreq_vsram =
				__mt_gpufreq_get_vsram_gpu_by_vgpu(
				g_opp_table[i].gpufreq_vgpu);

		gpufreq_pr_debug("@%s: apply=%d, [%02d] vgpu=%d, vsram_gpu=%d\n",
				__func__, apply, i,
				g_opp_table[i].gpufreq_vgpu,
				g_opp_table[i].gpufreq_vsram);
	}

	mt_gpufreq_cal_sb_opp_index();
}

unsigned int mt_gpufreq_bringup(void)
{
	return MT_GPUFREQ_BRINGUP;
}
EXPORT_SYMBOL(mt_gpufreq_bringup);

unsigned int mt_gpufreq_get_dvfs_en(void)
{
	return MT_GPUFREQ_DVFS_ENABLE;
}

unsigned int mt_gpufreq_not_ready(void)
{
	if (mt_gpufreq_bringup())
		return false;

	if (!g_pmic) {
		gpufreq_pr_info("gpufreq has not initialized yet\n");
		return true;
	}

	if (IS_ERR(g_pmic->reg_vgpu) || IS_ERR(g_pmic->reg_vsram_gpu)) {
		gpufreq_pr_info("@%s: VGPU(%lu)/VSRAM_GPU(%ld) was not initialized\n",
				__func__,
				PTR_ERR(g_pmic->reg_vgpu),
				PTR_ERR(g_pmic->reg_vsram_gpu));
		return true;
	} else {
		return false;
	}
}
EXPORT_SYMBOL(mt_gpufreq_not_ready);

unsigned int mt_gpufreq_power_ctl_en(void)
{
	return MT_GPUFREQ_POWER_CTL_ENABLE;
}
EXPORT_SYMBOL(mt_gpufreq_power_ctl_en);

unsigned int mt_gpufreq_get_cust_init_en(void)
{
	return MT_GPUFREQ_CUST_CONFIG;
}

/* API : get OPP table index number */
/* need to sub g_segment_max_opp_idx to map to real idx */
unsigned int mt_gpufreq_get_dvfs_table_num(void)
{
	/* prevent get wrong index */
	if (mt_gpufreq_not_ready())
		return -1;

	return g_segment_min_opp_idx - g_segment_max_opp_idx + 1;
}
EXPORT_SYMBOL(mt_gpufreq_get_dvfs_table_num);

/* API : get real OPP table index number */
unsigned int mt_gpufreq_get_real_dvfs_table_num(void)
{
	return g_max_opp_idx_num;
}

/* API : get frequency via OPP table index */
unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx)
{
	idx += g_segment_max_opp_idx;
	if (idx < g_max_opp_idx_num)
		return g_opp_table[idx].gpufreq_khz;
	else
		return 0;
}
EXPORT_SYMBOL(mt_gpufreq_get_freq_by_idx);

/* API : get frequency via OPP table real index */
unsigned int mt_gpufreq_get_freq_by_real_idx(unsigned int idx)
{
	if (idx < g_max_opp_idx_num)
		return g_opp_table[idx].gpufreq_khz;
	else
		return 0;
}
EXPORT_SYMBOL(mt_gpufreq_get_freq_by_real_idx);

/* API : get vgpu via OPP table index */
unsigned int mt_gpufreq_get_volt_by_idx(unsigned int idx)
{
	idx += g_segment_max_opp_idx;
	if (idx < g_max_opp_idx_num)
		return g_opp_table[idx].gpufreq_vgpu;
	else
		return 0;
}

/* API : get vgpu via OPP table real index */
unsigned int mt_gpufreq_get_volt_by_real_idx(unsigned int idx)
{
	if (idx < g_max_opp_idx_num)
		return g_opp_table[idx].gpufreq_vgpu;
	else
		return 0;
}
EXPORT_SYMBOL(mt_gpufreq_get_volt_by_real_idx);

/* API : get vsram via OPP table index */
unsigned int mt_gpufreq_get_vsram_by_idx(unsigned int idx)
{
	idx += g_segment_max_opp_idx;
	if (idx < g_max_opp_idx_num)
		return g_opp_table[idx].gpufreq_vsram;
	else
		return 0;
}

/* API : get vsram via OPP table index */
unsigned int mt_gpufreq_get_vsram_by_real_idx(unsigned int idx)
{
	if (idx < g_max_opp_idx_num)
		return g_opp_table[idx].gpufreq_vsram;
	else
		return 0;
}

/* API: pass GPU power table to EARA-QoS */
struct mt_gpufreq_power_table_info *pass_gpu_table_to_eara(void)
{
	return g_power_table;
}

/* API : get max power on power table */
unsigned int mt_gpufreq_get_max_power(void)
{
	return (!g_power_table) ?
			0 :
			g_power_table[g_segment_max_opp_idx].gpufreq_power;
}

/* API : get min power on power table */
unsigned int mt_gpufreq_get_min_power(void)
{
	return (!g_power_table) ?
			0 :
			g_power_table[g_segment_min_opp_idx].gpufreq_power;
}
EXPORT_SYMBOL(mt_gpufreq_get_min_power);

/* API : get idx on opp table */
int mt_gpufreq_get_opp_idx_by_freq(unsigned int freq)
{
	int i = g_segment_min_opp_idx;

	if (!g_opp_table)
		return 0;

	while (i >= (int)g_segment_max_opp_idx) {
		if (g_opp_table[i--].gpufreq_khz >= freq)
			goto EXIT;
	}

EXIT:
	return (i+1-g_segment_max_opp_idx);
}
EXPORT_SYMBOL(mt_gpufreq_get_opp_idx_by_freq);

/* API : get power on power table */
unsigned int mt_gpufreq_get_power_by_idx(int idx)
{
	if (!g_power_table)
		return 0;

	idx += g_segment_max_opp_idx;
	if (idx <= g_segment_min_opp_idx)
		return g_power_table[idx].gpufreq_power;
	else
		return 0;
}
EXPORT_SYMBOL(mt_gpufreq_get_power_by_idx);

/* API : get static leakage power */
unsigned int mt_gpufreq_get_leakage_mw(void)
{
	int temp = 0;
#if MT_GPUFREQ_STATIC_PWR_READY2USE
	unsigned int cur_vcore = __mt_gpufreq_get_cur_vgpu() / 100;
	int leak_power;
#endif

#if IS_ENABLED(CONFIG_THERMAL)
//kkk need check for 8169
	temp = get_immediate_gpu_wrap() / 1000;
#else
	temp = 40;
#endif

#if MT_GPUFREQ_STATIC_PWR_READY2USE
	leak_power = mt_spower_get_leakage(MTK_SPOWER_GPU, cur_vcore, temp);
	if (g_buck_on && leak_power > 0) {
		g_lkg_pwr = leak_power;
		return leak_power;
	} else {
		return 0;
	}
#else
	return 130;
#endif
}
EXPORT_SYMBOL(mt_gpufreq_get_leakage_mw);

unsigned int mt_gpufreq_get_dyn_power(unsigned int freq_khz, unsigned int volt)
{
	unsigned int p_dynamic = 0;
	unsigned int ref_freq = 0;
	unsigned int ref_volt = 0;

	p_dynamic = GPU_ACT_REF_POWER;
	ref_freq = GPU_ACT_REF_FREQ;
	ref_volt = GPU_ACT_REF_VOLT;

	p_dynamic = p_dynamic *
			((freq_khz * 100) / ref_freq) *
			((volt * 100) / ref_volt) *
			((volt * 100) / ref_volt) / (100 * 100 * 100);
	return p_dynamic;
}
EXPORT_SYMBOL(mt_gpufreq_get_dyn_power);

/* API : provide gpu lkg for swpm */
unsigned int mt_gpufreq_get_leakage_no_lock(void)
{
	return g_lkg_pwr;
}

/*
 * API : get current segment max opp index
 */
unsigned int mt_gpufreq_get_seg_max_opp_index(void)
{
	return g_segment_max_opp_idx;
}

/*
 * API : get current Thermal/Power/PBM limited OPP table index
 */
unsigned int mt_gpufreq_get_thermal_limit_index(void)
{
	return g_max_upper_limited_idx - g_segment_max_opp_idx;
}
EXPORT_SYMBOL(mt_gpufreq_get_thermal_limit_index);

/*
 * API : get current Thermal/Power/PBM limited OPP table frequency
 */
unsigned int mt_gpufreq_get_thermal_limit_freq(void)
{
	return g_opp_table[g_max_upper_limited_idx].gpufreq_khz;
}
EXPORT_SYMBOL(mt_gpufreq_get_thermal_limit_freq);

/*
 * API : get current OPP table conditional index
 */
unsigned int mt_gpufreq_get_cur_freq_index(void)
{
	return (g_cur_opp_idx < g_segment_max_opp_idx) ?
			0 : g_cur_opp_idx - g_segment_max_opp_idx;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_freq_index);

/*
 * API : get current OPP table frequency
 */
unsigned int mt_gpufreq_get_cur_freq(void)
{
	return g_cur_opp_freq;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_freq);

unsigned int mt_gpufreq_get_limit_user(unsigned int limit_user)
{
	if (limit_user == 0)
		return g_lower_kicker;

	if (limit_user == 1)
		return g_upper_kicker;

	return NUM_OF_KIR;
}
EXPORT_SYMBOL(mt_gpufreq_get_limit_user);

/*
 * API : get current voltage
 */
unsigned int mt_gpufreq_get_cur_volt(void)
{
	return (g_buck_on) ? g_cur_opp_vgpu : 0;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_volt);

/* API : get Thermal/Power/PBM limited OPP table index */
int mt_gpufreq_get_cur_ceiling_idx(void)
{
	return (int)mt_gpufreq_get_thermal_limit_index();
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_ceiling_idx);

static unsigned int mt_gpufreq_get_limited_idx_by_power(
		unsigned int limited_power)
{
	int i;
	unsigned int limited_idx = g_segment_min_opp_idx;

	for (i = g_segment_max_opp_idx; i <= g_segment_min_opp_idx; i++) {
		if (g_power_table[i].gpufreq_power <= limited_power) {
			limited_idx = i;
			break;
		}
	}

	gpufreq_pr_debug("@%s: limited_power = %d, limited_idx = %d\n",
		__func__, limited_power, limited_idx);

	return limited_idx;
}

#if MT_GPUFREQ_BATT_OC_PROTECT || MT_GPUFREQ_BATT_PERCENT_PROTECT || MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
static unsigned int mt_gpufreq_get_limited_idx_by_freq(
		unsigned int limited_freq)
{
	int i;
	unsigned int limited_idx = g_segment_min_opp_idx;

	for (i = g_segment_max_opp_idx; i <= g_segment_min_opp_idx; i++) {
		if (g_opp_table[i].gpufreq_khz <= limited_freq) {
			limited_idx = i;
			break;
		}
	}

	gpufreq_pr_debug("@%s: limited_freq=%d limited_idx=%d\n",
			__func__, limited_freq, limited_idx);

	return limited_idx;
}
#endif

#if MT_GPUFREQ_BATT_OC_PROTECT && IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
void mt_gpufreq_batt_oc_callback(enum BATTERY_OC_LEVEL_TAG battery_oc_level)
{
	unsigned int batt_oc_limited_idx = LIMIT_IDX_DEFAULT;

	if (battery_oc_level == BATTERY_OC_LEVEL_1) {
		batt_oc_limited_idx =
			mt_gpufreq_get_limited_idx_by_freq(
			MT_GPUFREQ_BATT_OC_LIMIT_FREQ);

		mt_gpufreq_update_limit_idx(KIR_BATT_OC,
			batt_oc_limited_idx,
			LIMIT_IDX_DEFAULT);

		if (g_cur_opp_freq >
			g_opp_table[batt_oc_limited_idx].gpufreq_khz)
			mt_gpufreq_target(batt_oc_limited_idx, KIR_BATT_OC);
	} else {
		mt_gpufreq_update_limit_idx(KIR_BATT_OC,
			LIMIT_IDX_DEFAULT,
			LIMIT_IDX_DEFAULT);
	}

	gpufreq_pr_debug("@%s: battery_oc_level=%d batt_oc_limited_idx=%d\n",
			__func__,
			battery_oc_level,
			batt_oc_limited_idx);
}
#endif

#if MT_GPUFREQ_BATT_PERCENT_PROTECT && IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
void mt_gpufreq_batt_percent_callback(
		BATTERY_PERCENT_LEVEL battery_percent_level)
{
	unsigned int batt_percent_limited_idx = LIMIT_IDX_DEFAULT;

	if (battery_percent_level == BATTERY_PERCENT_LEVEL_1) {
		batt_percent_limited_idx =
			mt_gpufreq_get_limited_idx_by_freq(
			MT_GPUFREQ_BATT_PERCENT_LIMIT_FREQ);

		mt_gpufreq_update_limit_idx(KIR_BATT_PERCENT,
			batt_percent_limited_idx,
			LIMIT_IDX_DEFAULT);

		if (g_cur_opp_freq >
			g_opp_table[batt_percent_limited_idx].gpufreq_khz)
			mt_gpufreq_target(
				batt_percent_limited_idx,
				KIR_BATT_PERCENT);
	} else {
		mt_gpufreq_update_limit_idx(KIR_BATT_PERCENT,
				LIMIT_IDX_DEFAULT,
				LIMIT_IDX_DEFAULT);
	}

	gpufreq_pr_debug("@%s: battery_percent_level=%d batt_percent_limited_idx=%d\n",
			__func__,
			battery_percent_level,
			batt_percent_limited_idx);
}
#endif

#if MT_GPUFREQ_LOW_BATT_VOLT_PROTECT && IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
void mt_gpufreq_low_batt_callback(enum LOW_BATTERY_LEVEL_TAG low_battery_level)
{
	unsigned int low_batt_limited_idx = LIMIT_IDX_DEFAULT;

	if (low_battery_level == LOW_BATTERY_LEVEL_2) {
		low_batt_limited_idx =
			mt_gpufreq_get_limited_idx_by_freq(
			MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ);

		mt_gpufreq_update_limit_idx(KIR_BATT_LOW,
			low_batt_limited_idx,
			LIMIT_IDX_DEFAULT);

		if (g_cur_opp_freq >
			g_opp_table[low_batt_limited_idx].gpufreq_khz)
			mt_gpufreq_target(low_batt_limited_idx, KIR_BATT_LOW);
	} else {
		mt_gpufreq_update_limit_idx(KIR_BATT_LOW,
			LIMIT_IDX_DEFAULT,
			LIMIT_IDX_DEFAULT);
	}

	gpufreq_pr_debug("@%s: low_battery_level=%d low_batt_limited_idx=%d\n",
			__func__, low_battery_level, low_batt_limited_idx);
}
#endif

/*
 * Thermal may limit OPP table index,
 * SVS/PTP may change the voltage for current OPP.
 */
void mt_gpufreq_devfreq_target(unsigned int limited_freq)
{
	int limited_idx;

	limited_idx = mt_gpufreq_get_opp_idx_by_freq(limited_freq);

	gpufreq_pr_debug("%s, limited_idx = %d\n", __func__, limited_idx);

	if (limited_idx == g_segment_max_opp_idx) {
		mt_gpufreq_update_limit_idx(KIR_THERMAL,
			LIMIT_IDX_DEFAULT,
			LIMIT_IDX_DEFAULT);
	} else {
		mt_gpufreq_update_limit_idx(KIR_THERMAL,
			limited_idx,
			LIMIT_IDX_DEFAULT);

		if (limited_idx > g_cur_opp_idx) {
			mt_gpufreq_target(limited_idx, KIR_THERMAL);
			return;
		}
	}

	mt_gpufreq_target(limited_idx, KIR_PTPOD);
}
EXPORT_SYMBOL(mt_gpufreq_devfreq_target);

/* API : set limited OPP table index by PBM */
void mt_gpufreq_set_power_limit_by_pbm(unsigned int limited_power)
{
	unsigned int pbm_limited_idx = LIMIT_IDX_DEFAULT;

	mutex_lock(&mt_gpufreq_power_lock);

	if (limited_power == 0) {
		mt_gpufreq_update_limit_idx(KIR_PBM,
			LIMIT_IDX_DEFAULT,
			LIMIT_IDX_DEFAULT);
	} else {
		pbm_limited_idx =
			mt_gpufreq_get_limited_idx_by_power(limited_power);

		mt_gpufreq_update_limit_idx(KIR_PBM,
			pbm_limited_idx,
			LIMIT_IDX_DEFAULT);

		if (g_cur_opp_freq >
			g_opp_table[pbm_limited_idx].gpufreq_khz)
			mt_gpufreq_target(pbm_limited_idx, KIR_PBM);
	}

	gpufreq_pr_debug("@%s: limited_power=%d pbm_limited_idx=%d\n",
			__func__, limited_power, pbm_limited_idx);

	mutex_unlock(&mt_gpufreq_power_lock);
}

/*
 * API : set GPU loading for SSPM
 */
void mt_gpufreq_set_loading(unsigned int gpu_loading)
{
	/* legacy */
}

/*
 * API : register GPU power limited notifiction callback
 */
void mt_gpufreq_power_limit_notify_registerCB(gpufreq_power_limit_notify pCB)
{
	/* legacy */
}
EXPORT_SYMBOL(mt_gpufreq_power_limit_notify_registerCB);

static unsigned int __mt_gpufreq_get_segment_id(void)
{
	static int segment_id = -1;
	struct device_node *dt_node;

	if (segment_id != -1)
		return segment_id;

	dt_node = of_find_matching_node(NULL, g_gpufreq_of_match);
	segment_id = MT8169A_SEGMENT;

	if (dt_node) {
		if (of_property_read_bool(dt_node, "gpu-max-freq"))
			segment_id = MT8186_SEGMENT;
	}

	gpufreq_pr_debug("@%s: segment_id=%d\n", __func__, segment_id);

	return segment_id;
}

#ifdef CONFIG_PROC_FS
static int mt_gpufreq_opp_dump_proc_show(struct seq_file *m, void *v)
{
	int i;
	int power = 0;

	for (i = g_segment_max_opp_idx; i <= g_segment_min_opp_idx; i++) {
		seq_printf(m, "[%02d] ",
				i - g_segment_max_opp_idx);
		seq_printf(m, "freq = %d, ",
				g_opp_table[i].gpufreq_khz);

		mt_gpufreq_update_volt(i);
		seq_printf(m, "vgpu = %d, ",
				g_opp_table[i].gpufreq_vgpu);
		seq_printf(m, "vsram = %d, ",
				g_opp_table[i].gpufreq_vsram);

		power = mt_gpufreq_get_dyn_power(g_opp_table[i].gpufreq_khz,
						g_opp_table[i].gpufreq_vgpu);
		power += mt_gpufreq_get_leakage_mw();
		seq_printf(m, "gpu_power = %d, ", power);

		seq_printf(m, "aging = %d\n",
				g_opp_table[i].gpufreq_aging_margin);
	}

	return 0;
}

static int mt_gpufreq_sb_idx_proc_show(struct seq_file *m, void *v)
{
	int i, max_opp_idx;

	max_opp_idx = g_segment_max_opp_idx;

	for (i = max_opp_idx; i <= g_segment_min_opp_idx; i++) {
		seq_printf(m,
				"[%02d] ", i - max_opp_idx);
		seq_printf(m, "g_opp_sb_idx_up = %d, ",
				g_opp_sb_idx_up[i] - max_opp_idx >= 0 ?
				g_opp_sb_idx_up[i] - max_opp_idx : 0);
		seq_printf(m, "g_opp_sb_idx_down = %d\n",
				g_opp_sb_idx_down[i] - max_opp_idx >= 0 ?
				g_opp_sb_idx_down[i] - max_opp_idx : 0);
	}

	return 0;
}

static int mt_gpufreq_var_dump_proc_show(struct seq_file *m, void *v)
{
	unsigned int gpu_loading = 0;

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
	mtk_get_gpu_loading(&gpu_loading);
#endif

	seq_printf(m, "idx: %d, freq: %d, vgpu: %d, vsram_gpu: %d\n",
			g_cur_opp_idx - g_segment_max_opp_idx,
			g_cur_opp_freq,
			g_cur_opp_vgpu,
			g_cur_opp_vsram_gpu);

	seq_printf(m, "(real) freq: %d, vgpu: %d, vsram_gpu: %d\n",
			__mt_gpufreq_get_cur_freq(),
			__mt_gpufreq_get_cur_vgpu(),
			__mt_gpufreq_get_cur_vsram_gpu());
	seq_printf(m, "segment_id = %d\n", __mt_gpufreq_get_segment_id());
	seq_printf(m, "g_power_count = %d, g_cg_on = %d, g_mtcmos_on = %d, g_buck_on = %d\n",
			g_power_count, g_cg_on, g_mtcmos_on, g_buck_on);
	seq_printf(m, "g_opp_stress_test_state = %d\n",
			g_opp_stress_test_state);
	seq_printf(m, "g_max_upper_limited_idx = %d\n",
			g_max_upper_limited_idx - g_segment_max_opp_idx);
	seq_printf(m, "g_min_lower_limited_idx = %d\n",
			g_min_lower_limited_idx - g_segment_max_opp_idx);
	seq_printf(m, "gpu_loading = %d\n",
			gpu_loading);

	return 0;
}

/*
 * PROCFS : show current opp stress test state
 */
static int mt_gpufreq_opp_stress_test_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "g_opp_stress_test_state: %d\n", g_opp_stress_test_state);
	return 0;
}

/*
 * PROCFS : opp stress test message setting
 * 0 : disable
 * 1 : enable for segment OPP table
 * 2 : enable for real OPP table
 */
static ssize_t mt_gpufreq_opp_stress_test_proc_write(
		struct file *file, const char __user *buffer,
		size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	unsigned int value = 0;
	int ret = -EFAULT;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		goto out;

	buf[len] = '\0';

	if (!kstrtouint(buf, 10, &value)) {
		if (!value || !(value-1)) {
			ret = 0;
			g_opp_stress_test_state = value;
				if (g_opp_stress_test_state == 0) {
					mt_gpufreq_update_limit_idx(
						KIR_STRESS,
						LIMIT_IDX_DEFAULT,
						LIMIT_IDX_DEFAULT);
			}
		}
	}

out:
	return (ret < 0) ? ret : count;
}

static int mt_gpufreq_aging_enable_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "g_aging_enable = %d\n", g_aging_enable);
	return 0;
}

static ssize_t mt_gpufreq_aging_enable_proc_write(
		struct file *file, const char __user *buffer,
		size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	unsigned int value = 0;
	int ret = -EFAULT;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		goto out;

	buf[len] = '\0';

	mutex_lock(&mt_gpufreq_lock);
	if (!kstrtouint(buf, 10, &value)) {
		if (!value || !(value-1)) {
			ret = 0;
			if (g_aging_enable ^ value) {
				g_aging_enable = value;
				mt_gpufreq_apply_aging(value);
			}
		}
	}
	mutex_unlock(&mt_gpufreq_lock);

out:
	return (ret < 0) ? ret : count;
}

static int mt_gpufreq_limit_table_proc_show(struct seq_file *m, void *v)
{
	int i;

	seq_puts(m, "echo [id][up_enable][low_enable] > /proc/gpufreq/gpufreq_limit_table\n");
	seq_puts(m, "ex: echo 3 0 0 > /proc/gpufreq/gpufreq_limit_table\n");
	seq_puts(m, "means disable THERMAL upper_limit_idx & lower_limit_idx\n\n");

	seq_printf(m, "%15s %5s %10s %10s %10s %10s %10s\n",
		"[name]", "[id]", "[prio]",
		"[up_idx]", "[up_enable]",
		"[low_idx]", "[low_enable]");

	mutex_lock(&mt_gpufreq_limit_table_lock);
	for (i = 0; i < NUM_OF_KIR; i++) {
		seq_printf(m, "%15s %5d %10d %10d %10d %10d %10d\n",
		limit_table[i].name,
		i,
		limit_table[i].prio,
		limit_table[i].upper_idx == LIMIT_IDX_DEFAULT
		? LIMIT_IDX_DEFAULT
		: limit_table[i].upper_idx - g_segment_max_opp_idx,
		limit_table[i].upper_enable,
		limit_table[i].lower_idx == LIMIT_IDX_DEFAULT
		? LIMIT_IDX_DEFAULT
		: limit_table[i].lower_idx - g_segment_max_opp_idx,
		limit_table[i].lower_enable
		);
	}
	mutex_unlock(&mt_gpufreq_limit_table_lock);

	return 0;
}

static ssize_t mt_gpufreq_limit_table_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	int ret = -EFAULT;
	unsigned int kicker;
	int upper_en, lower_en;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		goto out;

	buf[len] = '\0';

	if (sscanf(buf, "%d %d %d",
		&kicker, &upper_en, &lower_en) == 3) {
		if (kicker >= NUM_OF_KIR)
			goto out;
		if (upper_en != LIMIT_DISABLE &&
			upper_en != LIMIT_ENABLE)
			goto out;
		if (lower_en != LIMIT_DISABLE &&
			lower_en != LIMIT_ENABLE)
			goto out;

		ret = 0;
		mt_gpufreq_update_limit_enable(
			kicker, upper_en, lower_en);
	}

out:
	return (ret < 0) ? ret : count;
}

/*
 * PROCFS : show current keeping OPP frequency state
 */
static int mt_gpufreq_opp_freq_proc_show(struct seq_file *m, void *v)
{
	unsigned int keep_opp_freq_idx;

	mutex_lock(&mt_gpufreq_limit_table_lock);
	keep_opp_freq_idx = limit_table[KIR_PROC].upper_idx;
	mutex_unlock(&mt_gpufreq_limit_table_lock);

	if (keep_opp_freq_idx != LIMIT_IDX_DEFAULT) {
		seq_puts(m, "[GPU-DVFS] fixed OPP is enabled\n");
		seq_printf(m, "[%d] ",
				keep_opp_freq_idx - g_segment_max_opp_idx);
		seq_printf(m, "freq = %d, ",
				g_opp_table[keep_opp_freq_idx].gpufreq_khz);
		seq_printf(m, "vgpu = %d, ",
				g_opp_table[keep_opp_freq_idx].gpufreq_vgpu);
		seq_printf(m, "vsram = %d\n",
				g_opp_table[keep_opp_freq_idx].gpufreq_vsram);
	} else
		seq_puts(m, "[GPU-DVFS] fixed OPP is disabled\n");

	return 0;
}

/*
 * PROCFS : keeping OPP frequency setting
 * 0 : free run
 * 1 : keep OPP frequency
 */
static ssize_t mt_gpufreq_opp_freq_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	unsigned int value = 0;
	unsigned int i = 0;
	int ret = -EFAULT;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		goto out;

	buf[len] = '\0';

	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == 0) {
			mt_gpufreq_update_limit_idx(KIR_PROC,
				LIMIT_IDX_DEFAULT,
				LIMIT_IDX_DEFAULT);
		} else {
			for (i = g_segment_max_opp_idx;
				i <= g_segment_min_opp_idx;
				i++) {
				if (value == g_opp_table[i].gpufreq_khz) {
					ret = 0;
					mt_gpufreq_update_limit_idx(
						KIR_PROC, i, i);
					mt_gpufreq_target(i, KIR_PROC);
					break;
				}
			}
		}
	}

out:
	return (ret < 0) ? ret : count;
}

/*
 * PROCFS : show current fixed freq & volt state
 */
static int mt_gpufreq_fixed_freq_volt_proc_show(struct seq_file *m, void *v)
{
	if (g_fixed_freq_volt_state) {
		seq_puts(m, "[GPU-DVFS] fixed freq & volt is enabled\n");
		seq_printf(m, "g_fixed_freq = %d\n", g_fixed_freq);
		seq_printf(m, "g_fixed_vgpu = %d\n", g_fixed_vgpu);
	} else
		seq_puts(m, "[GPU-DVFS] fixed freq & volt is disabled\n");

	return 0;
}

/*
 * PROCFS : fixed freq & volt state setting
 */
static ssize_t mt_gpufreq_fixed_freq_volt_proc_write(
		struct file *file, const char __user *buffer,
		size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	int ret = -EFAULT;
	unsigned int fixed_freq = 0;
	unsigned int fixed_volt = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		goto out;

	buf[len] = '\0';

	if (sscanf(buf, "%d %d", &fixed_freq, &fixed_volt) == 2) {
		ret = 0;
		if (fixed_volt > VGPU_MAX_VOLT) {
			gpufreq_pr_debug("@%s: fixed_volt(%d) > VGPU_MAX_VOLT(%d)\n",
					__func__,
					fixed_volt,
					VGPU_MAX_VOLT);
			goto out;
		} else if (fixed_volt < VGPU_MIN_VOLT && fixed_volt > 0) {
			gpufreq_pr_debug("@%s: fixed_volt(%d) < VGPU_MIN_VOLT(%d)\n",
					__func__,
					fixed_volt,
					VGPU_MIN_VOLT);
			goto out;
		} else if (fixed_freq > GPU_MAX_FREQ) {
			gpufreq_pr_debug("@%s: fixed_freq(%d) > GPU_MAX_FREQ(%d)\n",
					__func__,
					fixed_freq,
					GPU_MAX_FREQ);
			goto out;
		} else if (fixed_freq < GPU_MIN_FREQ && fixed_freq > 0) {
			gpufreq_pr_debug("@%s: fixed_freq(%d) < GPU_MIN_FREQ(%d)\n",
					__func__,
					fixed_freq,
					GPU_MIN_FREQ);
			goto out;
		}

		mutex_lock(&mt_gpufreq_lock);
		if ((fixed_freq == 0) && (fixed_volt == 0)) {
			fixed_freq =
				g_opp_table[g_segment_min_opp_idx].gpufreq_khz;
			fixed_volt =
				g_opp_table[g_segment_min_opp_idx].gpufreq_vgpu;
			if (fixed_freq > g_cur_opp_freq) {
				__mt_gpufreq_set_fixed_vgpu(fixed_volt);
				__mt_gpufreq_set_fixed_freq(fixed_freq);
			} else {
				__mt_gpufreq_set_fixed_freq(fixed_freq);
				__mt_gpufreq_set_fixed_vgpu(fixed_volt);
			}
			g_fixed_freq = 0;
			g_fixed_vgpu = 0;
			g_fixed_freq_volt_state = false;
		} else {
			fixed_volt = VOLT_NORMALIZATION(fixed_volt);
			if (fixed_freq > g_cur_opp_freq) {
				__mt_gpufreq_set_fixed_vgpu(fixed_volt);
				__mt_gpufreq_set_fixed_freq(fixed_freq);
			} else {
				__mt_gpufreq_set_fixed_freq(fixed_freq);
				__mt_gpufreq_set_fixed_vgpu(fixed_volt);
			}
			g_fixed_freq_volt_state = true;
		}
		mutex_unlock(&mt_gpufreq_lock);
	}

out:
	return (ret < 0) ? ret : count;
}

/*
 * PROCFS : initialization
 */
PROC_FOPS_RW(gpufreq_opp_stress_test);
PROC_FOPS_RO(gpufreq_opp_dump);
PROC_FOPS_RW(gpufreq_opp_freq);
PROC_FOPS_RO(gpufreq_var_dump);
PROC_FOPS_RW(gpufreq_fixed_freq_volt);
PROC_FOPS_RO(gpufreq_sb_idx);
PROC_FOPS_RW(gpufreq_aging_enable);
PROC_FOPS_RW(gpufreq_limit_table);

static int __mt_gpufreq_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(gpufreq_opp_stress_test),
		PROC_ENTRY(gpufreq_opp_dump),
		PROC_ENTRY(gpufreq_opp_freq),
		PROC_ENTRY(gpufreq_var_dump),
		PROC_ENTRY(gpufreq_fixed_freq_volt),
		PROC_ENTRY(gpufreq_sb_idx),
		PROC_ENTRY(gpufreq_aging_enable),
		PROC_ENTRY(gpufreq_limit_table),
	};

	dir = proc_mkdir("gpufreq", NULL);
	if (!dir) {
		gpufreq_pr_info("@%s: fail to create /proc/gpufreq !!!\n",
				__func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0660, dir, entries[i].fops))
			gpufreq_pr_info("@%s: create /proc/gpufreq/%s failed\n",
					__func__, entries[i].name);
	}

	return 0;
}
#endif

/*
 * calculate springboard opp index to avoid buck variation,
 * the voltage between VGPU and VSRAM_GPU must be in 100mV ~ 250mV
 * that is, 100mV <= VSRAM_GPU - VGPU <= 250mV
 * (variation: VGPU / VSRAM_GPU {-6.25% / max(+6.25%, +47mV)}
 */
static void mt_gpufreq_cal_sb_opp_index(void)
{
	int i, j, diff;
	int min_vsram_idx = g_max_opp_idx_num - 1;

	for (i = 0; i < g_max_opp_idx_num; i++) {
		if (g_opp_table[i].gpufreq_vsram ==
			g_opp_table[g_max_opp_idx_num - 1].gpufreq_vsram) {
			min_vsram_idx = i;
			break;
		}
	}
	gpufreq_pr_debug("@%s: min_vsram_idx=%d\n", __func__, min_vsram_idx);

	/* build up table */
	for (i = 0; i < g_max_opp_idx_num; i++) {
		g_opp_sb_idx_up[i] = min_vsram_idx;
		for (j = 0; j <= min_vsram_idx; j++) {
			diff = g_opp_table[i].gpufreq_vgpu + BUCK_DIFF_MAX;
			if (g_opp_table[j].gpufreq_vsram <= diff) {
				g_opp_sb_idx_up[i] = j;
				break;
			}
		}
		gpufreq_pr_debug("@%s: g_opp_sb_idx_up[%d]=%d\n",
				__func__, i, g_opp_sb_idx_up[i]);
	}

	/* build down table */
	for (i = 0; i < g_max_opp_idx_num; i++) {
		if (i >= min_vsram_idx)
			g_opp_sb_idx_down[i] = g_max_opp_idx_num - 1;
		else {
			for (j = g_max_opp_idx_num - 1; j >= 0; j--) {
				diff =
				g_opp_table[i].gpufreq_vsram - BUCK_DIFF_MAX;
				if (g_opp_table[j].gpufreq_vgpu >= diff) {
					g_opp_sb_idx_down[i] = j;
					break;
				}
			}
		}
		gpufreq_pr_debug("@%s: g_opp_sb_idx_down[%d]=%d\n",
				__func__, i, g_opp_sb_idx_down[i]);
	}
}

/*
 * frequency ramp up/down handler
 * - frequency ramp up need to wait voltage settle
 * - frequency ramp down do not need to wait voltage settle
 */
static void __mt_gpufreq_set(
		unsigned int idx_old, unsigned int idx_new,
		unsigned int freq_old, unsigned int freq_new,
		unsigned int vgpu_old, unsigned int vgpu_new,
		unsigned int vsram_gpu_old, unsigned int vsram_gpu_new)
{
	unsigned int sb_idx = 0;

	gpufreq_pr_logbuf(
		"begin idx: %d -> %d, freq: %d -> %d, vgpu: %d -> %d, vsram_gpu: %d -> %d\n",
		idx_old, idx_new,
		freq_old, freq_new,
		vgpu_old, vgpu_new,
		vsram_gpu_old, vsram_gpu_new);

	if (freq_new == freq_old) {
		__mt_gpufreq_volt_switch(
				vgpu_old, vgpu_new,
				vsram_gpu_old, vsram_gpu_new);

	} else if (freq_new > freq_old) {
		while (g_cur_opp_vgpu != vgpu_new) {
			sb_idx = g_opp_sb_idx_up[g_cur_opp_idx] < idx_new ?
				idx_new : g_opp_sb_idx_up[g_cur_opp_idx];

			__mt_gpufreq_volt_switch(
			g_cur_opp_vgpu, g_opp_table[sb_idx].gpufreq_vgpu,
			g_cur_opp_vsram_gpu,
			g_opp_table[sb_idx].gpufreq_vsram);

			g_cur_opp_idx = sb_idx;
			g_cur_opp_vgpu = g_opp_table[sb_idx].gpufreq_vgpu;
			g_cur_opp_vsram_gpu = g_opp_table[sb_idx].gpufreq_vsram;
		}

		__mt_gpufreq_clock_switch(freq_new);

	} else {
		__mt_gpufreq_clock_switch(freq_new);

		while (g_cur_opp_vgpu != vgpu_new) {
			sb_idx = g_opp_sb_idx_down[g_cur_opp_idx] > idx_new ?
				idx_new : g_opp_sb_idx_down[g_cur_opp_idx];

			__mt_gpufreq_volt_switch(
			g_cur_opp_vgpu, g_opp_table[sb_idx].gpufreq_vgpu,
			g_cur_opp_vsram_gpu,
			g_opp_table[sb_idx].gpufreq_vsram);

			g_cur_opp_idx = sb_idx;
			g_cur_opp_vgpu = g_opp_table[sb_idx].gpufreq_vgpu;
			g_cur_opp_vsram_gpu = g_opp_table[sb_idx].gpufreq_vsram;
		}
	}

	/* update "g_cur_opp_idx" when "Vgpu old" and "Vgpu new" is the same */
	g_cur_opp_idx = idx_new;
	g_cur_opp_freq = freq_new;
	g_cur_opp_vgpu = __mt_gpufreq_get_cur_vgpu();
	g_cur_opp_vsram_gpu = __mt_gpufreq_get_cur_vsram_gpu();
	if (mtk_devfreq_set_cur_freq_fp)
		mtk_devfreq_set_cur_freq_fp(freq_new);

	gpu_dvfs_oppidx_footprint(idx_new);

	gpufreq_pr_logbuf(
		"end idx: %d -> %d, freq: %d, vgpu: %d, vsram_gpu: %d\n",
		idx_old, idx_new,
		__mt_gpufreq_get_cur_freq(),
		__mt_gpufreq_get_cur_vgpu(),
		__mt_gpufreq_get_cur_vsram_gpu());

	__mt_gpufreq_kick_pbm(1);
}

static void __mt_gpufreq_switch_to_clksrc(enum g_clock_source_enum clksrc)
{
	int ret;

	ret = clk_prepare_enable(g_clk->clk_mux);
	if (ret)
		gpufreq_pr_info("@%s: enable clk_mux(TOP_MUX_MFG) failed: %d\n",
				__func__, ret);

	if (clksrc == CLOCK_MAIN) {
		ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_main_parent);
		if (ret)
			gpufreq_pr_info("@%s: switch to main clock source failed: %d\n",
					__func__, ret);

	} else if (clksrc == CLOCK_SUB) {
		ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_sub_parent);
		if (ret)
			gpufreq_pr_info("@%s: switch to sub clock source failed: %d\n",
					__func__, ret);

	} else {
		gpufreq_pr_info("@%s: clock source index is not valid, clksrc: %d\n",
				__func__, clksrc);
	}

	clk_disable_unprepare(g_clk->clk_mux);
}

// MT8169_TODO check dds
/*
 * switch clock(frequency) via MFGPLL
 */
static void __mt_gpufreq_clock_switch(unsigned int freq_new)
{
	int ret = -99;

	__mt_gpufreq_switch_to_clksrc(CLOCK_SUB);

	ret = clk_set_rate(g_clk->clk_pll_src, freq_new*1000);
	if (ret)
		gpufreq_pr_info("@%s:failed set target[%ld] erro[%d]\n",
			__func__, freq_new, ret);
	udelay(20);

	__mt_gpufreq_switch_to_clksrc(CLOCK_MAIN);
}

/*
 * switch voltage and vsram via PMIC
 */
static void __mt_gpufreq_volt_switch_without_vsram_gpu(
		unsigned int vgpu_old, unsigned int vgpu_new)
{
	unsigned int vsram_gpu_new, vsram_gpu_old;

	vgpu_new = VOLT_NORMALIZATION(vgpu_new);

	vsram_gpu_new = __mt_gpufreq_get_vsram_gpu_by_vgpu(vgpu_new);
	vsram_gpu_old = __mt_gpufreq_get_vsram_gpu_by_vgpu(vgpu_old);

	__mt_gpufreq_volt_switch(
			vgpu_old,
			vgpu_new,
			vsram_gpu_old,
			vsram_gpu_new);
}

/*
 * switch voltage and vsram via PMIC
 */
static void __mt_gpufreq_volt_switch(
		unsigned int vgpu_old, unsigned int vgpu_new,
		unsigned int vsram_gpu_old, unsigned int vsram_gpu_new)
{
	unsigned int vgpu_settle_time, vsram_settle_time, final_settle_time;

	if (vgpu_new > vgpu_old) {
		/* rising */
		vgpu_settle_time = __calculate_vgpu_settletime(
			true, (vgpu_new - vgpu_old));
		vsram_settle_time = __calculate_vsram_settletime(
			true, (vsram_gpu_new - vsram_gpu_old));

		regulator_set_voltage(
				g_pmic->reg_vsram_gpu,
				vsram_gpu_new * 10,
				VSRAM_GPU_MAX_VOLT * 10 + 125);
		regulator_set_voltage(
				g_pmic->reg_vgpu,
				vgpu_new * 10,
				VGPU_MAX_VOLT * 10 + 125);

	} else if (vgpu_new < vgpu_old) {
		/* falling */
		vgpu_settle_time = __calculate_vgpu_settletime(
			false, (vgpu_old - vgpu_new));
		vsram_settle_time = __calculate_vsram_settletime(
			false, (vsram_gpu_old - vsram_gpu_new));

		regulator_set_voltage(
				g_pmic->reg_vgpu,
				vgpu_new * 10,
				VGPU_MAX_VOLT * 10 + 125);
		regulator_set_voltage(
				g_pmic->reg_vsram_gpu,
				vsram_gpu_new * 10,
				VSRAM_GPU_MAX_VOLT * 10 + 125);
	} else {
		/* voltage no change */
		return;
	}

	final_settle_time = (vgpu_settle_time > vsram_settle_time) ?
		vgpu_settle_time : vsram_settle_time;
	//kkk todo: temp disable as regulator will set the ramp-delay
	//udelay(final_settle_time);

	gpufreq_pr_logbuf("Vgpu: %d, Vsram_gpu: %d, udelay: %d\n",
		__mt_gpufreq_get_cur_vgpu(), __mt_gpufreq_get_cur_vsram_gpu(),
		final_settle_time);
}

/*
 * set fixed frequency for PROCFS: fixed_freq_volt
 */
static void __mt_gpufreq_set_fixed_freq(int fixed_freq)
{
	gpufreq_pr_debug("@%s: before, g_fixed_freq=%d g_fixed_vgpu=%d\n",
			__func__, g_fixed_freq, g_fixed_vgpu);

	g_fixed_freq = fixed_freq;
	g_fixed_vgpu = g_cur_opp_vgpu;

	gpufreq_pr_debug("@%s: now, g_fixed_freq = %d, g_fixed_vgpu = %d\n",
			__func__,
			g_fixed_freq, g_fixed_vgpu);

	__mt_gpufreq_clock_switch(g_fixed_freq);

	g_cur_opp_freq = g_fixed_freq;
	if (mtk_devfreq_set_cur_freq_fp)
		mtk_devfreq_set_cur_freq_fp(fixed_freq);
}

/*
 * set fixed voltage for PROCFS: fixed_freq_volt
 */
static void __mt_gpufreq_set_fixed_vgpu(int fixed_vgpu)
{
	gpufreq_pr_debug("@%s: before, g_fixed_freq=%d g_fixed_vgpu=%d\n",
			__func__, g_fixed_freq, g_fixed_vgpu);

	g_fixed_freq = g_cur_opp_freq;
	g_fixed_vgpu = fixed_vgpu;

	gpufreq_pr_debug("@%s: now, g_fixed_freq=%d g_fixed_vgpu=%d\n",
			__func__, g_fixed_freq, g_fixed_vgpu);

	__mt_gpufreq_volt_switch_without_vsram_gpu(
			g_cur_opp_vgpu,
			g_fixed_vgpu);

	g_cur_opp_vgpu = g_fixed_vgpu;
	g_cur_opp_vsram_gpu =
			__mt_gpufreq_get_vsram_gpu_by_vgpu(g_fixed_vgpu);
}

// MT8169_TODO check power table
/* power calculation for power table */
static void __mt_gpufreq_calculate_power(
		unsigned int idx, unsigned int freq, unsigned int volt)
{
	unsigned int p_total = 0;
	unsigned int p_dynamic = 0;
	unsigned int ref_freq = 0;
	unsigned int ref_vgpu = 0;
	int p_leakage = 0;

	p_dynamic = GPU_ACT_REF_POWER;
	ref_freq = GPU_ACT_REF_FREQ;
	ref_vgpu = GPU_ACT_REF_VOLT;

	p_dynamic = p_dynamic *
			((freq * 100) / ref_freq) *
			((volt * 100) / ref_vgpu) *
			((volt * 100) / ref_vgpu) /
			(100 * 100 * 100);

	p_leakage = mt_gpufreq_get_leakage_mw();

	p_total = p_dynamic + p_leakage;

	gpufreq_pr_debug("@%s: idx=%d dynamic=%d leakage=%d total=%d\n",
			__func__, idx, p_dynamic, p_leakage, p_total);

	g_power_table[idx].gpufreq_power = p_total;
}

static unsigned int __calculate_vgpu_settletime(bool rising, int deltaV)
{
	unsigned int settleTime, steps;

	steps = (deltaV / PMIC_STEP) + 1;

	if (rising)
		settleTime = steps*g_vgpu_sfchg_rrate + 52;
	else
		settleTime = steps*g_vgpu_sfchg_frate + 52;
	return settleTime; /* us */
}

static unsigned int __calculate_vsram_settletime(bool rising, int deltaV)
{
	unsigned int settleTime, steps;

	steps = (deltaV / PMIC_STEP) + 1;

	if (rising)
		settleTime = steps*g_vsram_sfchg_rrate + 52;
	else
		settleTime = steps*g_vsram_sfchg_frate + 52;
	return settleTime; /* us */
}

/*
 * get current frequency (KHZ)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __mt_gpufreq_get_cur_freq(void)
{
	unsigned int freq_khz = 0;

	freq_khz = clk_get_rate(g_clk->clk_pll_src)/1000;

	return freq_khz;
}

/*
 * get current vsram voltage (mV * 100)
 */
static unsigned int __mt_gpufreq_get_cur_vsram_gpu(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vsram_gpu)) {
		/* regulator_get_voltage prints volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vsram_gpu) / 10;
	}

	return volt;
}

/*
 * get current voltage (mV * 100)
 */
static unsigned int __mt_gpufreq_get_cur_vgpu(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vgpu)) {
		/* regulator_get_voltage prints volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vgpu) / 10;
	}

	return volt;
}

/*
 * calculate vsram_gpu via given vgpu
 * PTPOD only change vgpu, so we need change vsram by vgpu.
 */
static unsigned int __mt_gpufreq_get_vsram_gpu_by_vgpu(unsigned int vgpu)
{
	unsigned int vsram_gpu;

	if (vgpu > FIXED_VSRAM_VOLT_THSRESHOLD)
		vsram_gpu = vgpu + FIXED_VSRAM_VOLT_DIFF;
	else {
		if (__mt_gpufreq_get_segment_id() == MT8186_SEGMENT)
			vsram_gpu = FIXED_VSRAM_VOLT_M1;
		else
			vsram_gpu = FIXED_VSRAM_VOLT_P1;
	}

	gpufreq_pr_debug("@%s: vgpu=%d vsram_gpu=%d\n",
			__func__, vgpu, vsram_gpu);

	return vsram_gpu;
}

/*
 * kick Power Budget Manager(PBM) when OPP changed
 */
static void __mt_gpufreq_kick_pbm(int enable)
{
	unsigned int power;
	unsigned int cur_freq;
	unsigned int cur_vgpu;
	unsigned int found = 0;
	int tmp_idx = -1;
	int i;

	cur_freq = __mt_gpufreq_get_cur_freq();
	cur_vgpu = __mt_gpufreq_get_cur_vgpu();

	if (enable) {
		for (i = 0; i < g_max_opp_idx_num; i++) {
			if (g_power_table[i].gpufreq_khz == cur_freq) {
				/*
				 * record idx since
				 * current voltage may
				 * not in DVFS table
				 */
				tmp_idx = i;

				if (g_power_table[i].gpufreq_vgpu == cur_vgpu) {
					power = g_power_table[i].gpufreq_power;
					found = 1;					
#if MT_GPUFREQ_KICKER_PBM_READY && IS_ENABLED(CONFIG_MTK_PBM)
					kicker_pbm_by_gpu(true,
							power, cur_vgpu / 100);
					gpufreq_pr_debug("@%s: request GPU power = %d, cur_volt = %d uV, cur_freq = %d KHz\n",
							__func__, power, cur_vgpu * 10, cur_freq);

#endif
					return;
				}
			}
		}

		if (!found) {
			gpufreq_pr_debug("@%s: tmp_idx=%d\n",
					__func__, tmp_idx);
			if (tmp_idx != -1 && tmp_idx < g_max_opp_idx_num) {
				/*
				 * use freq to find
				 * corresponding power budget
				 */
				power = g_power_table[tmp_idx].gpufreq_power;			
#if MT_GPUFREQ_KICKER_PBM_READY && IS_ENABLED(CONFIG_MTK_PBM)
				kicker_pbm_by_gpu(true, power, cur_vgpu / 100);
				gpufreq_pr_debug("@%s: request GPU power = %d, cur_volt = %d uV, cur_freq = %d KHz\n",
						__func__, power, cur_vgpu * 10, cur_freq);

#endif
			}
		}
	} else {		
#if MT_GPUFREQ_KICKER_PBM_READY && IS_ENABLED(CONFIG_MTK_PBM)
		kicker_pbm_by_gpu(false, 0, cur_vgpu / 100);
		gpufreq_pr_debug("@%s: Cannot found request power in power table, cur_freq = %d KHz, cur_volt = %d uV\n",
			__func__, cur_freq, cur_vgpu * 10);

#endif
	}
}

static void __mt_gpufreq_init_table(void)
{
	unsigned int segment_id = __mt_gpufreq_get_segment_id();
	struct opp_table_info *opp_table = NULL;
	unsigned int i = 0;

	if (segment_id == MT8186_SEGMENT)
		opp_table = g_opp_table_segment_1;
	else if (segment_id == MT8169A_SEGMENT)
		opp_table = g_opp_table_segment_2;
	else if (segment_id == MT8169B_SEGMENT)
		opp_table = g_opp_table_segment_2;
	else
		opp_table = g_opp_table_segment_2;

	g_segment_max_opp_idx = 0;
	g_segment_min_opp_idx = NUM_OF_OPP_IDX - 1;

	g_opp_table = kzalloc((NUM_OF_OPP_IDX)*sizeof(*opp_table), GFP_KERNEL);

	if (g_opp_table == NULL)
		return;

	for (i = 0; i < NUM_OF_OPP_IDX; i++) {
		g_opp_table[i].gpufreq_khz = opp_table[i].gpufreq_khz;
		g_opp_table[i].gpufreq_vgpu = opp_table[i].gpufreq_vgpu;
		g_opp_table[i].gpufreq_vsram = opp_table[i].gpufreq_vsram;
		g_opp_table[i].gpufreq_aging_margin =
					opp_table[i].gpufreq_aging_margin;

		gpufreq_pr_debug("@%s: [%02u] freq=%u vgpu=%u vsram=%u aging=%u\n",
				__func__, i,
				opp_table[i].gpufreq_khz,
				opp_table[i].gpufreq_vgpu,
				opp_table[i].gpufreq_vsram,
				opp_table[i].gpufreq_aging_margin);
	}

	g_max_opp_idx_num = NUM_OF_OPP_IDX;
	g_max_upper_limited_idx = g_segment_max_opp_idx;
	g_min_lower_limited_idx = g_segment_min_opp_idx;

	gpufreq_pr_debug("@%s: g_segment_max_opp_idx=%u g_max_opp_idx_num=%u g_segment_min_opp_idx=%u\n",
			__func__,
			g_segment_max_opp_idx,
			g_max_opp_idx_num,
			g_segment_min_opp_idx);

	mutex_lock(&mt_gpufreq_lock);
	//MT8169_TODO check this
	mt_gpufreq_cal_sb_opp_index();
	mutex_unlock(&mt_gpufreq_lock);
	__mt_gpufreq_setup_opp_power_table(NUM_OF_OPP_IDX);
}

/*
 * OPP power table initialization
 */
static void __mt_gpufreq_setup_opp_power_table(int num)
{
	int i = 0;

	g_power_table = kzalloc(
			(num) * sizeof(struct mt_gpufreq_power_table_info),
			GFP_KERNEL);

	if (g_power_table == NULL)
		return;

	for (i = 0; i < num; i++) {
		g_power_table[i].gpufreq_khz = g_opp_table[i].gpufreq_khz;
		g_power_table[i].gpufreq_vgpu = g_opp_table[i].gpufreq_vgpu;

		__mt_gpufreq_calculate_power(i, g_power_table[i].gpufreq_khz,
				g_power_table[i].gpufreq_vgpu);

		gpufreq_pr_debug("@%s: [%02d] freq_khz=%u vgpu=%u power=%u\n",
				__func__, i,
				g_power_table[i].gpufreq_khz,
				g_power_table[i].gpufreq_vgpu,
				g_power_table[i].gpufreq_power);
	}
}

/*
 * I/O remap
 */
static void *__mt_gpufreq_of_ioremap(const char *node_name, int idx)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, node_name);

	if (node)
		return of_iomap(node, idx);

	return NULL;
}

static void __mt_gpufreq_init_volt_by_freq(void)
{
	struct opp_table_info *opp_table = g_opp_table;
	unsigned int freq, idx;

	freq = __mt_gpufreq_get_cur_freq();
	gpufreq_pr_debug("@%s: Preloader default freq is %d\n", __func__, freq);

	if (mt_gpufreq_get_cust_init_en()) {
		freq = MT_GPUFREQ_CUST_INIT_OPP;
		gpufreq_pr_info("@%s: CUST request freq to %d\n",
				__func__, freq);
	}

	/*
	 *  freq need to check lower/upper bound
	 *  because get real mfg will not correct
	 */
	if (freq >= opp_table[0].gpufreq_khz) {
		/* get Maximum opp */
		idx = 0;
	} else if (freq <= opp_table[NUM_OF_OPP_IDX - 1].gpufreq_khz) {
		/* get Minimum opp */
		idx = NUM_OF_OPP_IDX - 1;
	} else {
		for (idx = 1; idx < NUM_OF_OPP_IDX; idx++) {
			if (opp_table[idx].gpufreq_khz <= freq) {
				/* find the idx with closest freq */
				if ((freq - opp_table[idx].gpufreq_khz) >
					opp_table[idx-1].gpufreq_khz - freq)
					idx -= 1;
				break;
			}
		}
	}

	g_cur_opp_idx = idx;
	g_cur_opp_freq = freq;
	g_cur_opp_vgpu = __mt_gpufreq_get_cur_vgpu();
	g_cur_opp_vsram_gpu = __mt_gpufreq_get_cur_vsram_gpu();

	if (!mt_gpufreq_get_dvfs_en() && !mt_gpufreq_get_cust_init_en()) {
		gpufreq_pr_debug("@%s: Disable DVFS and CUST INIT !!!\n",
				__func__);
	} else {
		gpufreq_pr_debug("@%s: Enable DVFS or CUST INIT !!!\n",
				__func__);
		mutex_lock(&mt_gpufreq_lock);
		__mt_gpufreq_set(g_cur_opp_idx, idx,
			g_cur_opp_freq, opp_table[idx].gpufreq_khz,
			g_cur_opp_vgpu, opp_table[idx].gpufreq_vgpu,
			g_cur_opp_vsram_gpu, opp_table[idx].gpufreq_vsram);
		mutex_unlock(&mt_gpufreq_lock);
	}
}

static int __mt_gpufreq_init_mtcmos(struct platform_device *pdev)
{
	int i, ret;

	if (g_mtcmos == NULL)
		g_mtcmos = kzalloc(sizeof(struct g_mtcmos_info), GFP_KERNEL);
	if (g_mtcmos == NULL)
		return -ENOMEM;

	g_mtcmos->num_domains = of_count_phandle_with_args(pdev->dev.of_node,
						 "power-domains",
						 "#power-domain-cells");

	gpufreq_pr_debug("@%s: number of power domains: %d\n", __func__, g_mtcmos->num_domains);

	for (i = 0; i < g_mtcmos->num_domains; i++) {
		g_mtcmos->pm_domain_devs[i] =
			dev_pm_domain_attach_by_id(&pdev->dev, i);

		if (IS_ERR_OR_NULL(g_mtcmos->pm_domain_devs[i])) {
			ret = PTR_ERR(g_mtcmos->pm_domain_devs[i]) ? : -ENODATA;
			g_mtcmos->pm_domain_devs[i] = NULL;

			if (ret == -EPROBE_DEFER) {
				gpufreq_pr_info("@%s: Probe deferral for pm-domain-%d\n",
					__func__, i);
			} else {
				gpufreq_pr_info("@%s: Failed to get pm-domain-%d: %d\n",
					__func__,  i, ret);
			}

			goto err;
		}
	}

	return 0;

err:
	for (i = 0; i < ARRAY_SIZE(g_mtcmos->pm_domain_devs); i++) {
		if (g_mtcmos->pm_domain_devs[i])
			dev_pm_domain_detach(g_mtcmos->pm_domain_devs[i], true);
	}

	kfree(g_mtcmos);
	return ret;
}

/*
 * VGPU slew rate calculation
 * false : falling rate
 * true : rising rate
 */
static unsigned int __calculate_vgpu_sfchg_rate(bool isRising)
{
	unsigned int sfchg_rate_vgpu;

	/* [MT6366] RG_BUCK_VGPU_SFCHG_RRATE and RG_BUCK_VGPU_SFCHG_FRATE
	 * same as MT6358
	 * Rising soft change rate
	 * Ref clock = 26MHz (0.038us)
	 * Step = ( code + 1 ) * 0.038 us
	 */
//kkk need check for 8169
	if (isRising) {
		/* sfchg_rate_reg is 19, (19+1)*0.038 = 0.76us */
		sfchg_rate_vgpu = 1;
	} else {
		/* sfchg_rate_reg is 39, (39+1)*0.038 = 1.52us */
		sfchg_rate_vgpu = 2;
	}

	gpufreq_pr_debug("@%s: isRising = %d, sfchg_rate_vgpu = %d\n",
			__func__, isRising, sfchg_rate_vgpu);

	return sfchg_rate_vgpu;
}

/*
 * VSRAM slew rate calculation
 * false : falling rate
 * true : rising rate
 */
static unsigned int __calculate_vsram_sfchg_rate(bool isRising)
{
	unsigned int sfchg_rate_vsram;

	/* [MT6366] RG_LDO_VSRAM_GPU_SFCHG_RRATE and RG_LDO_VSRAM_GPU_SFCHG_FRATE
	 * same as MT6358
	 *    7'd4 : 0.19us
	 *    7'd8 : 0.34us
	 *    7'd11 : 0.46us
	 *    7'd17 : 0.69us
	 *    7'd23 : 0.92us
	 *    7'd25 : 1us
	 */
///kkk need check for 8169
	/* sfchg_rate_reg is 7 for rising, (7+1)*0.038 = 0.304us */
	/* sfchg_rate_reg is 15 for falling, (15+1)*0.038 = 0.608us */
	sfchg_rate_vsram = 1;

	gpufreq_pr_debug("@%s: isRising = %d, sfchg_rate_vsram = %d\n",
			__func__, isRising, sfchg_rate_vsram);

	return sfchg_rate_vsram;
}

static int __mt_gpufreq_init_pmic(struct platform_device *pdev)
{
	if (g_pmic == NULL)
		g_pmic = kzalloc(sizeof(struct g_pmic_info), GFP_KERNEL);
	if (g_pmic == NULL)
		return -ENOMEM;

	g_pmic->reg_vgpu =
			regulator_get_optional(&pdev->dev, "vgpu");
	if (IS_ERR(g_pmic->reg_vgpu)) {
		gpufreq_pr_info("@%s: cannot get VGPU, %ld\n",
			__func__, PTR_ERR(g_pmic->reg_vgpu));
		return PTR_ERR(g_pmic->reg_vgpu);
	}

	g_pmic->reg_vsram_gpu =
			regulator_get_optional(&pdev->dev, "vsram_gpu");
	if (IS_ERR(g_pmic->reg_vsram_gpu)) {
		gpufreq_pr_info("@%s: cannot get VSRAM_GPU, %ld\n",
			__func__, PTR_ERR(g_pmic->reg_vsram_gpu));
		return PTR_ERR(g_pmic->reg_vsram_gpu);
	}

	/* setup PMIC init value */
	g_vgpu_sfchg_rrate = __calculate_vgpu_sfchg_rate(true);
	g_vgpu_sfchg_frate = __calculate_vgpu_sfchg_rate(false);
	g_vsram_sfchg_rrate = __calculate_vsram_sfchg_rate(true);
	g_vsram_sfchg_frate = __calculate_vsram_sfchg_rate(false);

	gpufreq_pr_debug("@%s: VGPU sfchg raising rate: %d us, VGPU sfchg falling rate: %d us, \t"
			"VSRAM_GPU sfchg raising rate: %d us, VSRAM_GPU sfchg falling rate: %d us\n"
			, __func__, g_vgpu_sfchg_rrate, g_vgpu_sfchg_frate,
			g_vsram_sfchg_rrate, g_vsram_sfchg_frate);
	return 0;
}

static int __mt_gpufreq_init_clk(struct platform_device *pdev)
{
	g_apmixed_base = __mt_gpufreq_of_ioremap("mediatek,mt8169-apmixedsys", 0);
	if (!g_apmixed_base) {
		gpufreq_pr_info("@%s: ioremap failed at APMIXED\n", __func__);
		return -ENOENT;
	}

	g_sleep = __mt_gpufreq_of_ioremap("mediatek,sleep", 0);
	if (!g_sleep) {
		gpufreq_pr_info("@%s: ioremap failed at sleep\n", __func__);
		return -ENOENT;
	}

	g_mfg_base = __mt_gpufreq_of_ioremap("mediatek,mt8169-mfgsys", 0);
	if (!g_mfg_base) {
		gpufreq_pr_info("@%s: ioremap failed at mfgcfg\n", __func__);
		return -ENOENT;
	}

	if (g_clk == NULL)
		g_clk = kzalloc(sizeof(struct g_clk_info), GFP_KERNEL);
	if (g_clk == NULL)
		return -ENOMEM;

	g_clk->clk_mux = devm_clk_get(&pdev->dev, "clk_mux");
	if (IS_ERR(g_clk->clk_mux)) {
		gpufreq_pr_info("@%s: cannot get clk_mux\n", __func__);
		return PTR_ERR(g_clk->clk_mux);
	}

	g_clk->clk_pll_src = devm_clk_get(&pdev->dev, "clk_pll_src");
	if (IS_ERR(g_clk->clk_pll_src)) {
		gpufreq_pr_info("@%s: cannot get clk_pll_src\n", __func__);
		return PTR_ERR(g_clk->clk_pll_src);
	}

	g_clk->clk_main_parent = devm_clk_get(&pdev->dev, "clk_main_parent");
	if (IS_ERR(g_clk->clk_main_parent)) {
		gpufreq_pr_info("@%s: cannot get clk_main_parent\n", __func__);
		return PTR_ERR(g_clk->clk_main_parent);
	}

	g_clk->clk_sub_parent = devm_clk_get(&pdev->dev, "clk_sub_parent");
	if (IS_ERR(g_clk->clk_sub_parent)) {
		gpufreq_pr_info("@%s: cannot get clk_sub_parent\n", __func__);
		return PTR_ERR(g_clk->clk_sub_parent);
	}

	g_clk->subsys_mfg_cg = devm_clk_get(&pdev->dev, "subsys_mfg_cg");
	if (IS_ERR(g_clk->subsys_mfg_cg)) {
		gpufreq_pr_info("@%s: cannot get subsys_mfg_cg\n", __func__);
		return PTR_ERR(g_clk->subsys_mfg_cg);
	}

	return 0;
}

static void __mt_gpufreq_init_power(void)
{
#if MT_GPUFREQ_STATIC_PWR_READY2USE
	/* Initial leackage power usage */
	mt_spower_init();
#endif

#if MT_GPUFREQ_LOW_BATT_VOLT_PROTECT && IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
	register_low_battery_notify(
			&mt_gpufreq_low_batt_callback,
			LOW_BATTERY_PRIO_GPU);
#endif

#if MT_GPUFREQ_BATT_PERCENT_PROTECT && IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
	register_bp_thl_notify(
			&mt_gpufreq_batt_percent_callback,
			BATTERY_PERCENT_PRIO_GPU);
#endif

#if MT_GPUFREQ_BATT_OC_PROTECT && IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
	register_battery_oc_notify(
			&mt_gpufreq_batt_oc_callback,
			BATTERY_OC_PRIO_GPU);
#endif

#if MT_GPUFREQ_KICKER_PBM_READY && IS_ENABLED(CONFIG_MTK_PBM)
	register_pbm_notify(
		&mt_gpufreq_set_power_limit_by_pbm, PBM_PRIO_GPU);
#endif

}

void mt_gpufreq_dump_status(void)
{
	// 0x1000C000
	if (!g_apmixed_base)
		g_apmixed_base = __mt_gpufreq_of_ioremap("mediatek,mt8169-apmixedsys", 0);
	if (!g_apmixed_base) {
		gpufreq_pr_info("@%s: ioremap failed at APMIXED\n", __func__);
		return;
	}

	// 0x10006000
	if (!g_sleep)
		g_sleep = __mt_gpufreq_of_ioremap("mediatek,sleep", 0);
	if (!g_sleep) {
		gpufreq_pr_info("@%s: ioremap failed at sleep\n", __func__);
		return;
	}

	//0x1000_6308 0x1000_630C 0x1000_6310 0x1000_6314
	gpufreq_pr_info("@%s: [MFGX_PWR_CON X:0/1/2/3] 0x%08x %08x %08x %08x\n",
			__func__,
			readl(g_sleep + 0x308),
			readl(g_sleep + 0x30C),
			readl(g_sleep + 0x310),
			readl(g_sleep + 0x314));

	//0x1000_6318 0x1000_631C 0x1000_6320
	gpufreq_pr_info("@%s: [MFGX_PWR_CON X:4/5/6] 0x%08x %08x %08x\n",
			__func__,
			readl(g_sleep + 0x318),
			readl(g_sleep + 0x31C),
			readl(g_sleep + 0x320));

	// [SPM] pwr_status: pwr_ack (@0x1000_616C)
	// [SPM] pwr_status_2nd: pwr_ack_2nd (@x1000_6170)
	// [2]: MFG0, [3]: MFG1, [4]: MFG2, [5]: MFG3
	gpufreq_pr_info("@%s: [PWR_ACK] MFG0~MFG3=0x%08X(0x%08X)\n",
			__func__,
			readl(g_sleep + 0x16C) & 0x0000003C,
			readl(g_sleep + 0x170) & 0x0000003C);

	gpufreq_pr_info("@%s: [MFGPLL_CON0] RG_MFGPLL_EN=0x%08X\n",
			__func__,
			readl(g_apmixed_base + 0x314) & 0x00000001);

	gpufreq_pr_info("@%s: [MFGPLL_CON2] RG_MFGPLL_POSDIV=%d\n",
			__func__,
			(readl(g_apmixed_base + 0x318)>>24)&0x7);
}
EXPORT_SYMBOL(mt_gpufreq_dump_status);

/*
 * gpufreq driver probe
 */
static int __mt_gpufreq_pdrv_probe(struct platform_device *pdev)
{
	struct device_node *node;
	int ret;

	gpufreq_pr_debug("@%s: driver init is started\n", __func__);

	node = of_find_matching_node(NULL, g_gpufreq_of_match);
	if (!node)
		gpufreq_pr_info("@%s: find GPU node failed\n", __func__);

	g_pdev = pdev;

	/* init mtcmos */
	ret = __mt_gpufreq_init_mtcmos(pdev);
	if (ret)
		return ret;

	/* init pmic regulator */
	ret = __mt_gpufreq_init_pmic(pdev);
	if (ret)
		return ret;

	/* init clock source and mtcmos */
	ret = __mt_gpufreq_init_clk(pdev);
	if (ret)
		return ret;

	/* init opp table */
	__mt_gpufreq_init_table();

	if (!mt_gpufreq_power_ctl_en()) {
		gpufreq_pr_info("@%s: Power Control Always On !!!\n", __func__);
		mt_gpufreq_power_control(POWER_ON, CG_ON, MTCMOS_ON, BUCK_ON);
	}

	/* init Vgpu/Vsram_gpu by bootup freq index */
	__mt_gpufreq_init_volt_by_freq();

	__mt_gpufreq_init_power();

#if defined(CONFIG_ARM64) && defined(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES)
	if (strstr(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES,
						"aging") != NULL) {
		gpufreq_pr_info("@%s: AGING flavor name: %s\n",
			__func__, CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES);
		g_aging_enable = 1;
	}
#endif

	g_probe_done = true;
	gpufreq_pr_info("@%s: GPU driver init done\n", __func__);

	return 0;
}

/*
 * register the gpufreq driver
 */
static int __init __mt_gpufreq_init(void)
{
	int ret = 0;

	if (mt_gpufreq_bringup()) {
		mt_gpufreq_dump_status();
		gpufreq_pr_info("@%s: skip driver init when bringup\n",
				__func__);
		return 0;
	}

	gpufreq_pr_debug("@%s: start to initialize gpufreq driver\n",
			__func__);

#ifdef CONFIG_PROC_FS
	if (__mt_gpufreq_create_procfs())
		goto out;
#endif

	/* register platform driver */
	ret = platform_driver_register(&g_gpufreq_pdrv);
	if (ret)
		gpufreq_pr_info("@%s: fail to register gpufreq driver\n",
			__func__);

out:
	gpu_dvfs_vgpu_reset_footprint();
	gpu_dvfs_oppidx_reset_footprint();
	gpu_dvfs_power_count_reset_footprint();

	return ret;
}

/*
 * unregister the gpufreq driver
 */
static void __exit __mt_gpufreq_exit(void)
{
	platform_driver_unregister(&g_gpufreq_pdrv);
}

module_init(__mt_gpufreq_init);
module_exit(__mt_gpufreq_exit);

MODULE_DEVICE_TABLE(of, g_gpufreq_of_match);
MODULE_DESCRIPTION("MediaTek GPU-DVFS-PLAT driver");
MODULE_LICENSE("GPL");
