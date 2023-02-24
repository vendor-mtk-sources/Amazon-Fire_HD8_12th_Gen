// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include "sspm_define.h"


#if SSPM_EMI_PROTECTION_SUPPORT
#include <soc/mediatek/emi.h>

void sspm_set_emi_mpu(phys_addr_t base, phys_addr_t size)
{
	struct emimpu_region_t rg_info;

	mtk_emimpu_init_region(&rg_info, SSPM_MPU_REGION_ID);

	mtk_emimpu_set_addr(&rg_info, base, base + size - 1);

	mtk_emimpu_set_apc(&rg_info, 0, MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_set_apc(&rg_info, 8, MTK_EMIMPU_NO_PROTECTION);

	mtk_emimpu_set_protection(&rg_info);

	mtk_emimpu_free_region(&rg_info);
}

#endif
