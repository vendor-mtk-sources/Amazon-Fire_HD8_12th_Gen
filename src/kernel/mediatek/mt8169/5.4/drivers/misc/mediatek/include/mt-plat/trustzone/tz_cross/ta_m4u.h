/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

/* An example test TA implementation.
 */

#ifndef __TRUSTZONE_TA_M4U__
#define __TRUSTZONE_TA_M4U__

#if IS_ENABLED(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
#define TZ_TA_M4U_UUID   "m4u-smi-mau-spc"
#elif IS_ENABLED(CONFIG_OPTEE)
#define TZ_TA_M4U_UUID   "69fa6b1e-26b1-82ef-46a9-21f8bcc48490"
#endif

/* Data Structure for Test TA */
/* You should define data structure used both in REE/TEE here
 *  N/A for Test TA
 */

/* Command for Test TA */
#define M4U_TZCMD_TEST              0
#define M4U_TZCMD_CONFIG_PORT_ARRAY 65
#define M4U_TZCMD_CONFIG_PORT       66
#define M4U_TZCMD_REG_BACKUP        67
#define M4U_TZCMD_REG_RESTORE       68
#define M4U_TZCMD_ALLOC_MVA_SEC     70
#define M4U_TZCMD_DEALLOC_MVA_SEC   71
/*====syn nonsec pgt start*/
#define M4U_TZCMD_SEC_INIT          72
#define M4U_TZCMD_MAP_NONSEC_BUF    73
#define M4U_TZCMD_DEALLOC_MVA_SYNSEC 74
/*====syn nonsec pgt end */

#define M4U_TZCMD_SECPGTDUMP            100
#define M4U_TZCMD_LARB_REG_BACKUP       101
#define M4U_TZCMD_LARB_REG_RESTORE      102


#define M4U_TZCMD_INVALID_TLB       75
#define M4U_TZCMD_HW_INIT           76
#define M4U_TZCMD_DUMP_REG          77
#define M4U_TZCMD_WAIT_ISR          78
#define M4U_TZCMD_INVALID_CHECK     79
#define M4U_TZCMD_INSERT_SEQ        80
#define M4U_TZCMD_ERRHANGE_EN       81

#endif	/* __TRUSTZONE_TA_TEST__ */
