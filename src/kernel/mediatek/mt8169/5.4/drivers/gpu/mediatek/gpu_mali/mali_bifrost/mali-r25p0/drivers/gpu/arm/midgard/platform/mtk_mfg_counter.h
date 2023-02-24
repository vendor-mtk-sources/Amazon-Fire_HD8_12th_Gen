// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __MTK_MFG_COUNTER_H_
#define __MTK_MFG_COUNTER_H_

#include <mtk_gpu_utility.h>

enum {
	PMU_OK = 0,
	PMU_NG = 1,
	/* reset PMU value if needed */
	PMU_RESET_VALUE = 2,
};

extern int (*mtk_get_gpu_pmu_init_fp)(struct GPU_PMU *pmus,
			int pmu_size, int *ret_size);
extern int (*mtk_get_gpu_pmu_deinit_fp)(void);
extern int (*mtk_get_gpu_pmu_swapnreset_fp)(struct GPU_PMU *pmus, int pmu_size);
extern int (*mtk_get_gpu_pmu_swapnreset_stop_fp)(void);
/* Need to get current gpu freq from GPU DVFS module */
extern unsigned int mt_gpufreq_get_cur_freq(void);

void mtk_mfg_counter_init(void);
void mtk_mfg_counter_destroy(void);


#endif
