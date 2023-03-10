/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef _PERF_TRACKER_INTERNAL_H
#define _PERF_TRACKER_INTERNAL_H

#include <linux/types.h>
#include <linux/module.h>
#include <linux/cpufreq.h>

/* get OPP table */
struct ppm_cluster_info {
	struct cpufreq_frequency_table *dvfs_tbl;
};

struct ppm_data {
	struct ppm_cluster_info *cluster_info;
};

extern void __iomem *csram_base;
extern struct ppm_data ppm_main_info;
extern int cluster_nr;

#if IS_ENABLED(CONFIG_MTK_PERF_TRACKER)
#if IS_ENABLED(CONFIG_MTK_BLOCK_TAG)
#include <mt-plat/mtk_blocktag.h>
#else
struct mtk_btag_mictx_iostat_struct {
	__u64 duration;  /* duration time for below performance data (ns) */
	__u32 tp_req_r;  /* throughput (per-request): read  (KB/s) */
	__u32 tp_req_w;  /* throughput (per-request): write (KB/s) */
	__u32 tp_all_r;  /* throughput (overlapped) : read  (KB/s) */
	__u32 tp_all_w;  /* throughput (overlapped) : write (KB/s) */
	__u32 reqsize_r; /* request size : read  (Bytes) */
	__u32 reqsize_w; /* request size : write (Bytes) */
	__u32 reqcnt_r;  /* request count: read */
	__u32 reqcnt_w;  /* request count: write */
	__u16 wl;        /* storage device workload (%) */
	__u16 q_depth;   /* storage cmdq queue depth */
};
#endif
extern struct kobj_attribute perf_tracker_enable_attr;

extern void perf_tracker(u64 wallclock,
			 bool hit_long_check);

extern struct kobj_attribute perf_fuel_gauge_enable_attr;
extern struct kobj_attribute perf_fuel_gauge_period_attr;
#if IS_ENABLED(CONFIG_MTK_CHARGER)
extern struct kobj_attribute perf_charger_enable_attr;
extern struct kobj_attribute perf_charger_period_attr;
#endif
#if IS_ENABLED(CONFIG_MTK_GPU_SWPM_SUPPORT)
extern struct kobj_attribute perf_gpu_pmu_enable_attr;
extern struct kobj_attribute perf_gpu_pmu_period_attr;
#endif

#else
static inline void perf_tracker(u64 wallclock,
				bool hit_long_check) {}
#endif /* CONFIG_MTK_PERF_TRACKER */
#endif /* _PERF_TRACKER_INTERNAL_H */
