// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef ___MT_GPUFREQ_INTERNAL_PLAT_H___
#define ___MT_GPUFREQ_INTERNAL_PLAT_H___

/**************************************************
 *  0:     all on when mtk probe init (freq/ Vgpu/ Vsram_gpu)
 *         disable DDK power on/off callback
 **************************************************/
//MT8169_TODO disable bringup  enable POWER CTL & DVFS
#define MT_GPUFREQ_POWER_CTL_ENABLE	1

/**************************************************
 * (DVFS_ENABLE, CUST_CONFIG)
 * (1, 1) -> DVFS enable and init to CUST_INIT_OPP
 * (1, 0) -> DVFS enable
 * (0, 1) -> DVFS disable but init to CUST_INIT_OPP (do DVFS only onces)
 * (0, 0) -> DVFS disable
 **************************************************/
//MT8169_TODO disable bringup  enable POWER CTL & DVFS
#define MT_GPUFREQ_DVFS_ENABLE          1
#define MT_GPUFREQ_CUST_CONFIG          0
#define MT_GPUFREQ_CUST_INIT_OPP        (g_opp_table_segment_1[16].gpufreq_khz)

/**************************************************
 * DVFS Setting
 **************************************************/
#define NUM_OF_OPP_IDX (sizeof(g_opp_table_segment_1) / \
			sizeof(g_opp_table_segment_1[0]))

/* On opp table, low vgpu will use the same vsram.
 * And hgih vgpu will have the same diff with vsram.
 *
 * if (vgpu <= FIXED_VSRAM_VOLT_THSRESHOLD) {
 *     vsram = FIXED_VSRAM_VOLT;
 * } else {
 *     vsram = vgpu + FIXED_VSRAM_VOLT_DIFF;
 * }
 */
#define FIXED_VSRAM_VOLT_M1             (85000)
#define FIXED_VSRAM_VOLT_P1             (90000)
#define FIXED_VSRAM_VOLT_THSRESHOLD     (75000)
#define FIXED_VSRAM_VOLT_DIFF           (10000)

/**************************************************
 * PMIC Setting
 **************************************************/
/* MT6357 PMIC hardware range:
 * vgpu      500000 ~ 1193750
 * vsram_gpu 518750 ~ 1312500
 * MT6358 PMIC hardware range:
 * vgpu      500000 ~ 1293750
 * vsram_gpu 500000 ~ 1293750
 * sign off table 0.6 ~ 0.95V
 * mssv required: 0.5 ~ 1.0
 */
#define VGPU_MAX_VOLT                   (119375)        /* mV x 100 */
#define VGPU_MIN_VOLT                   (50000)         /* mV x 100 */
#define VSRAM_GPU_MAX_VOLT              (129375)        /* mV x 100 */
#define VSRAM_GPU_MIN_VOLT              (50000)         /* mV x 100 */
#define PMIC_STEP                       (625)           /* mV x 100 */
/*
 * (100)mv <= (VSRAM - VGPU) <= (250)mV
 */
#define BUCK_DIFF_MAX                   (25000)         /* mV x 100 */
#define BUCK_DIFF_MIN                   (10000)             /* mV x 100 */

/**************************************************
 * Clock Setting
 **************************************************/
#define GPU_MAX_FREQ                    (1000000)        /* KHz */
#define GPU_MIN_FREQ                    (299000)        /* KHz */

/**************************************************
 * Reference Power Setting
 **************************************************/
#define GPU_ACT_REF_POWER               (1285)                /* mW  */
#define GPU_ACT_REF_FREQ                (900000)              /* KHz */
#define GPU_ACT_REF_VOLT                (90000)               /* mV x 100 */

// MT8169_PORTING_TODO check below power settings @{
/**************************************************
 * Battery Over Current Protect
 **************************************************/
 //MT8169_TODO disable bringup 
#define MT_GPUFREQ_BATT_OC_PROTECT              0
#define MT_GPUFREQ_BATT_OC_LIMIT_FREQ           (485000)        /* KHz */

/**************************************************
 * Battery Percentage Protect
 **************************************************/
 //MT8169_TODO disable bringup 
#define MT_GPUFREQ_BATT_PERCENT_PROTECT         0
#define MT_GPUFREQ_BATT_PERCENT_LIMIT_FREQ      (485000)        /* KHz */

/**************************************************
 * Low Battery Volume Protect
 **************************************************/
 //MT8169_TODO disable bringup
#define MT_GPUFREQ_LOW_BATT_VOLT_PROTECT        0
#define MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ     (485000)        /* KHz */

//@}

/**************************************************
 * Register Manipulations
 **************************************************/
#define READ_REGISTER_UINT32(reg)	\
	(*(unsigned int * const)(reg))
#define WRITE_REGISTER_UINT32(reg, val)	\
	((*(unsigned int * const)(reg)) = (val))
#define INREG32(x)	\
	READ_REGISTER_UINT32((unsigned int *)((void *)(x)))
#define OUTREG32(x, y)	\
	WRITE_REGISTER_UINT32((unsigned int *)((void *)(x)), (unsigned int)(y))
#define SETREG32(x, y)	\
	OUTREG32(x, INREG32(x)|(y))
#define CLRREG32(x, y)	\
	OUTREG32(x, INREG32(x)&~(y))
#define MASKREG32(x, y, z)	\
	OUTREG32(x, (INREG32(x)&~(y))|(z))
#define DRV_Reg32(addr)				INREG32(addr)
#define DRV_WriteReg32(addr, data)	OUTREG32(addr, data)
#define DRV_SetReg32(addr, data)	SETREG32(addr, data)
#define DRV_ClrReg32(addr, data)	CLRREG32(addr, data)

/**************************************************
 * Proc Node Definition
 **************************************************/
#ifdef CONFIG_PROC_FS
#define PROC_FOPS_RW(name)	\
	static int mt_ ## name ## _proc_open(	\
			struct inode *inode,	\
			struct file *file)	\
	{	\
		return single_open(	\
				file,	\
				mt_ ## name ## _proc_show,	\
				PDE_DATA(inode));	\
	}	\
	static const struct file_operations mt_ ## name ## _proc_fops =	\
	{	\
		.owner = THIS_MODULE,	\
		.open = mt_ ## name ## _proc_open,	\
		.read = seq_read,	\
		.llseek = seq_lseek,	\
		.release = single_release,	\
		.write = mt_ ## name ## _proc_write,	\
	}
#define PROC_FOPS_RO(name)	\
	static int mt_ ## name ## _proc_open(	\
			struct inode *inode,	\
			struct file *file)	\
	{	\
		return single_open(	\
				file,	\
				mt_ ## name ## _proc_show,	\
				PDE_DATA(inode));	\
	}	\
	static const struct file_operations mt_ ## name ## _proc_fops =	\
	{	\
		.owner = THIS_MODULE,	\
		.open = mt_ ## name ## _proc_open,	\
		.read = seq_read,	\
		.llseek = seq_lseek,	\
		.release = single_release,	\
	}
#define PROC_ENTRY(name) \
	{__stringify(name), &mt_ ## name ## _proc_fops}
#endif

/**************************************************
 * Operation Definition
 **************************************************/
#define VOLT_NORMALIZATION(volt)	\
	((volt % 625) ? (volt - (volt % 625) + 625) : volt)
#ifndef MIN
#define MIN(x, y)	(((x) < (y)) ? (x) : (y))
#endif

#define GPUOP(khz, vgpu, vsram, aging_margin)	\
	{							\
		.gpufreq_khz = khz,				\
		.gpufreq_vgpu = vgpu,				\
		.gpufreq_vsram = vsram,				\
		.gpufreq_aging_margin = aging_margin,		\
	}

/**************************************************
 * Enumerations
 **************************************************/
/*MT8169_PORTING_TODO: check the segment settings*/
enum g_segment_id_enum {
	MT8186_SEGMENT = 1,
	MT8169A_SEGMENT,
	MT8169B_SEGMENT,
};


enum g_segment_table_enum {
	SEGMENT_TABLE_1 = 1,
};

enum g_clock_source_enum  {
	CLOCK_MAIN = 0,
	CLOCK_SUB,
};

enum g_limit_enable_enum  {
	LIMIT_DISABLE = 0,
	LIMIT_ENABLE,
};

enum {
	GPUFREQ_LIMIT_PRIO_NONE,	/* the lowest priority */
	GPUFREQ_LIMIT_PRIO_1,
	GPUFREQ_LIMIT_PRIO_2,
	GPUFREQ_LIMIT_PRIO_3,
	GPUFREQ_LIMIT_PRIO_4,
	GPUFREQ_LIMIT_PRIO_5,
	GPUFREQ_LIMIT_PRIO_6,
	GPUFREQ_LIMIT_PRIO_7,
	GPUFREQ_LIMIT_PRIO_8		/* the highest priority */
};

struct gpudvfs_limit {
	unsigned int kicker;
	char *name;
	unsigned int prio;
	unsigned int upper_idx;
	unsigned int upper_enable;
	unsigned int lower_idx;
	unsigned int lower_enable;
};

#define LIMIT_IDX_DEFAULT -1
#define GPU_CORE_NUM 2

struct gpudvfs_limit limit_table[] = {
	{KIR_STRESS,		"STRESS",	GPUFREQ_LIMIT_PRIO_8,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_PROC,			"PROC",		GPUFREQ_LIMIT_PRIO_7,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_PTPOD,			"PTPOD",	GPUFREQ_LIMIT_PRIO_6,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_THERMAL,		"THERMAL",	GPUFREQ_LIMIT_PRIO_5,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_BATT_OC,		"BATT_OC",	GPUFREQ_LIMIT_PRIO_5,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_BATT_LOW,		"BATT_LOW",	GPUFREQ_LIMIT_PRIO_5,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_BATT_PERCENT,	"BATT_PERCENT",	GPUFREQ_LIMIT_PRIO_5,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_PBM,			"PBM",		GPUFREQ_LIMIT_PRIO_5,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_POLICY,		"POLICY",	GPUFREQ_LIMIT_PRIO_4,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
};

/**************************************************
 * Structures
 **************************************************/
struct opp_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_vgpu;
	unsigned int gpufreq_vsram;
	unsigned int gpufreq_aging_margin;
};
struct g_clk_info {
	struct clk *clk_mux;
	struct clk *clk_pll_src;
	struct clk *clk_main_parent;
	struct clk *clk_sub_parent;
	struct clk *subsys_mfg_cg;
};
struct g_pmic_info {
	struct regulator *reg_vgpu;
	struct regulator *reg_vsram_gpu;
};
struct g_mtcmos_info {
	unsigned int num_domains;
	struct device *pm_domain_devs[GPU_CORE_NUM];
};

/**************************************************
 * External functions declaration
 **************************************************/
extern bool mtk_get_gpu_loading(unsigned int *pLoading);

/**************************************************
 * global value definition
 **************************************************/
struct opp_table_info *g_opp_table;

/**************************************************
 * PTPOD definition
 **************************************************/
unsigned int g_ptpod_opp_idx_table_segment[] = {
	0, 2, 4, 6,
	8, 10, 12, 14,
	16, 18, 20, 23,
	25, 27, 29, 31
};

/**************************************************
 * GPU OPP table definition
 **************************************************/
// MT8169_PORTING_TODO: check aging settings
struct opp_table_info g_opp_table_segment_1[] = {
	GPUOP(1000000, 95000, 105000, 1875), /* 0 sign off */
	GPUOP(950000, 90000, 100000, 1875), /* 2 */
	GPUOP(900000, 85000, 95000, 1875), /* 4 */
	GPUOP(850000, 80000, 90000, 1875), /* 6 sign off */
	GPUOP(796000, 78125, 88125, 1875), /* 8 */
	GPUOP(743000, 75625, 85625, 1250), /* 10 */
	GPUOP(690000, 73750, 85000, 1250), /*12 */
	GPUOP(637000, 71250, 85000, 1250), /*14 sign off */
	GPUOP(586000, 70000, 85000, 1250), /*16*/
	GPUOP(535000, 68750, 85000, 625), /*18 */
	GPUOP(484000, 66875, 85000, 625), /*20 */
	GPUOP(434000, 65625, 85000, 625), /*23 */
	GPUOP(400000, 64375, 85000, 625), /*25 */
	GPUOP(366000, 63750, 85000, 625), /*27 */
	GPUOP(332000, 62500, 85000, 625), /*29 */
	GPUOP(299000, 61250, 85000, 625), /*31 sign off */
};

struct opp_table_info g_opp_table_segment_2[] = {
	GPUOP(600000, 70625, 90000, 1250), /*15*/
	GPUOP(586000, 70000, 90000, 1250), /*16*/
	GPUOP(560000, 69375, 90000, 625), /*17 */
	GPUOP(535000, 68750, 90000, 625), /*18 */
	GPUOP(509000, 68125, 90000, 625), /*19 */
	GPUOP(484000, 66875, 90000, 625), /*20 */
	GPUOP(467000, 66875, 90000, 625), /*21 */
	GPUOP(434000, 65625, 90000, 625), /*23 */
	GPUOP(417000, 65000, 90000, 625), /*24 */
	GPUOP(400000, 65000, 90000, 625), /*25 */
	GPUOP(383000, 65000, 90000, 625), /*26 */
	GPUOP(366000, 65000, 90000, 625), /*27 */
	GPUOP(349000, 65000, 90000, 625), /*28 */
	GPUOP(332000, 65000, 90000, 625), /*29 */
	GPUOP(315000, 65000, 90000, 625), /*30 */
	GPUOP(299000, 65000, 90000, 625), /*31 sign off */
};

#endif /* ___MT_GPUFREQ_INTERNAL_PLAT_H___ */
