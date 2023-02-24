/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#ifndef _MTK_SECURE_API_H_
#define _MTK_SECURE_API_H_

#if IS_ENABLED(CONFIG_ARM64)
#define MTK_SIP_SMC_AARCH_BIT			0x40000000
#else
#define MTK_SIP_SMC_AARCH_BIT			0x00000000
#endif

/*	0x82000200 -	0x820003FF &	0xC2000300 -	0xC20003FF */
/* Debug feature and ATF related */
#define MTK_SIP_KERNEL_WDT \
	(0x82000200 | MTK_SIP_SMC_AARCH_BIT)

#endif				/* _MTK_SECURE_API_H_ */
