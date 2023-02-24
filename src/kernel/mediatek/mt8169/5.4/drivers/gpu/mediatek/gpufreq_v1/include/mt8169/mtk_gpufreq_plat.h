// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef ___MT_GPUFREQ_PLAT_H___
#define ___MT_GPUFREQ_PLAT_H___

#include <linux/module.h>
#include <linux/clk.h>

// MT8169_PORTING_TODO check below power settings @{
#define MT_GPUFREQ_BRINGUP                      0
#define MT_GPUFREQ_KICKER_PBM_READY             1
#define MT_GPUFREQ_STATIC_PWR_READY2USE         0
#define MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE   1
// @}
#define GPUFERQ_TAG	"[GPU/DVFS] "
#define gpufreq_pr_info(fmt, args...)	pr_info(GPUFERQ_TAG fmt, ##args)
#define gpufreq_pr_debug(fmt, args...)	pr_debug(GPUFERQ_TAG fmt, ##args)

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
#define gpufreq_pr_logbuf(fmt, args...)			\
do {							\
	gpufreq_pr_debug(fmt, ##args);			\
} while (0)
#else
#define gpufreq_pr_logbuf(fmt, args...)	gpufreq_pr_debug(fmt, ##args)
#endif

/* TxCy: Stack x, Core y */
#define T0C0  (1 <<  0)
#define T1C0  (1 <<  1)
#define T2C0  (1 <<  2)
#define T3C0  (1 <<  3)
#define T0C1  (1 <<  4)
#define T1C1  (1 <<  5)
#define T2C1  (1 <<  6)
#define T3C1  (1 <<  7)
#define T0C2  (1 <<  8)
#define T1C2  (1 <<  9)
#define T2C2  (1 << 10)
#define T3C2  (1 << 11)
#define T0C3  (1 << 12)
#define T1C3  (1 << 13)
#define T2C3  (1 << 14)
#define T3C3  (1 << 15)
#define T4C0  (1 << 16)
#define T5C0  (1 << 17)
#define T6C0  (1 << 18)
#define T7C0  (1 << 19)
#define T4C1  (1 << 20)
#define T5C1  (1 << 21)
#define T6C1  (1 << 22)
#define T7C1  (1 << 23)

#define MFG2_SHADER_STACK0         (T0C0)
#define MFG3_SHADER_STACK2         (T1C1)
#define MT_GPU_SHADER_PRESENT_2    (T0C0 | T1C1)

struct mt_gpufreq_power_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_vgpu;
	unsigned int gpufreq_power;
};

enum mt_gpufreq_kicker {
	KIR_STRESS,
	KIR_PROC,
	KIR_PTPOD,
	KIR_THERMAL,
	KIR_BATT_OC,
	KIR_BATT_LOW,
	KIR_BATT_PERCENT,
	KIR_PBM,
	KIR_POLICY,
	NUM_OF_KIR
};

enum mt_power_state {
	POWER_OFF = 0,
	POWER_ON,
};

enum mt_cg_state {
	CG_OFF = 0,
	CG_ON,
	CG_KEEP,
};

enum mt_mtcmos_state {
	MTCMOS_OFF = 0,
	MTCMOS_ON,
	MTCMOS_KEEP,
};

enum mt_buck_state {
	BUCK_OFF = 0,
	BUCK_ON,
	BUCK_KEEP,
};

/**
 * MTK GPUFREQ API
 */
extern unsigned int mt_gpufreq_bringup(void);
extern unsigned int mt_gpufreq_not_ready(void);
extern unsigned int mt_gpufreq_get_dvfs_en(void);
extern unsigned int mt_gpufreq_power_ctl_en(void);
extern unsigned int mt_gpufreq_get_cust_init_en(void);
extern unsigned int mt_gpufreq_get_cur_freq_index(void);
extern unsigned int mt_gpufreq_get_cur_freq(void);
extern unsigned int mt_gpufreq_get_limit_user(unsigned int limit_user);
extern unsigned int mt_gpufreq_get_cur_volt(void);
extern unsigned int mt_gpufreq_get_dvfs_table_num(void);
extern unsigned int mt_gpufreq_get_real_dvfs_table_num(void);
extern unsigned int mt_gpufreq_target(unsigned int request_idx,
		enum mt_gpufreq_kicker);
extern unsigned int mt_gpufreq_get_freq_by_real_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_volt_by_real_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_vsram_by_real_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_volt_by_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_vsram_by_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_ori_opp_idx(unsigned int idx);
extern struct mt_gpufreq_power_table_info *pass_gpu_table_to_eara(void);
extern unsigned int mt_gpufreq_get_seg_max_opp_index(void);
extern unsigned int mt_gpufreq_get_max_power(void);
extern unsigned int mt_gpufreq_get_min_power(void);
extern unsigned int mt_gpufreq_get_thermal_limit_index(void);
extern unsigned int mt_gpufreq_get_thermal_limit_freq(void);
extern void mt_gpufreq_set_power_limit_by_pbm(unsigned int limited_power);
extern unsigned int mt_gpufreq_get_leakage_mw(void);
extern unsigned int mt_gpufreq_get_dyn_power(unsigned int freq_khz, unsigned int volt);
extern unsigned int mt_gpufreq_get_leakage_no_lock(void);
extern int mt_gpufreq_get_cur_ceiling_idx(void);
extern void mt_gpufreq_set_loading(unsigned int gpu_loading); /* legacy */
extern void mt_gpufreq_power_control(enum mt_power_state, enum mt_cg_state,
		enum mt_mtcmos_state, enum mt_buck_state);
extern void mt_gpufreq_set_timestamp(void);
extern void mt_gpufreq_check_bus_idle(void);
extern unsigned int mt_gpufreq_get_shader_present(void);
extern void mt_gpufreq_dump_infra_status(void);
extern int mt_gpufreq_get_opp_idx_by_freq(unsigned int freq);
extern unsigned int mt_gpufreq_get_power_by_idx(int idx);

extern unsigned long (*mtk_devfreq_get_voltage_fp)(unsigned long target_freq);
extern void (*mtk_devfreq_set_cur_freq_fp)(unsigned long cur_freq);
extern void mt_gpufreq_devfreq_target(unsigned int limited_freq);
extern void mt_gpufreq_dump_status(void);

/**
 * power limit notification
 */
typedef void (*gpufreq_power_limit_notify)(unsigned int);
extern void mt_gpufreq_power_limit_notify_registerCB(
		gpufreq_power_limit_notify pCB);

/**
 * input boost notification
 */
typedef void (*gpufreq_input_boost_notify)(unsigned int);
extern void mt_gpufreq_input_boost_notify_registerCB(
		gpufreq_input_boost_notify pCB);

#endif /* ___MT_GPUFREQ_PLAT_H___ */
