/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/** Commands for TA SYSTEM **/

#ifndef __TRUSTZONE_TA_SYSTEM__
#define __TRUSTZONE_TA_SYSTEM__

#if IS_ENABLED(CONFIG_OPTEE)

#define MTEE_SESSION_HANDLE_SYSTEM sys_session
#define PTA_SYSTEM_UUID_STRING "75bb9d8e-1570-11e9-ab14-d663bd873d93"
#define PTA_SYSTEM_UUID {0x75bb9d8e, 0x1570, 0x11e9, \
		{0xab, 0x14, 0xd6, 0x63, 0xbd, 0x87, 0x3d, 0x93 } }

#include "kree/system.h"
extern KREE_SESSION_HANDLE MTEE_SESSION_HANDLE_SYSTEM;

#else
/* Special handle for system connect. */
/* NOTE: Handle manager guarantee normal handle will have bit31=0. */
#define MTEE_SESSION_HANDLE_SYSTEM 0xFFFF1234
#endif

/* Session Management */
#define TZCMD_SYS_INIT                0
#define TZCMD_SYS_SESSION_CREATE      1
#define TZCMD_SYS_SESSION_CLOSE       2
#define TZCMD_SYS_IRQ                 3
#define TZCMD_SYS_THREAD_CREATE       4
#define TZCMD_SYS_SESSION_CREATE_WITH_TAG 5
#define TZCMD_SYS_REE_CALLBACK        6

#endif	/* __TRUSTZONE_TA_SYSTEM__ */
