/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_CMO_H__
#define __MTK_CMO_H__

#if IS_ENABLED(CONFIG_MEDIATEK_SOLUTION)
extern void inner_dcache_flush_all(void);
extern void inner_dcache_flush_L1(void);
extern void inner_dcache_flush_L2(void);
extern void inner_dcache_disable(void);
extern void smp_inner_dcache_flush_all(void);
#endif

#endif /* __MTK_CMO_H__ */
