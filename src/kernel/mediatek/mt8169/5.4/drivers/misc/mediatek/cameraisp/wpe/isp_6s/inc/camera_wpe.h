/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author Wei-Yi Wang <Wei-Yi.Wang@mediatek.com>
 */

#ifndef _MT_WPE_H
#define _MT_WPE_H

#include <linux/ioctl.h>

/*
 *   enforce kernel log enable
 */
#define KERNEL_LOG  /* enable debug log flag if defined */

#define _SUPPORT_MAX_WPE_FRAME_REQUEST_ 32
#define _SUPPORT_MAX_WPE_REQUEST_RING_SIZE_ 32


#define SIG_ERESTARTSYS 512 /* ERESTARTSYS */
/***********************************************************************
 *
 ***********************************************************************/
#define WPE_DEV_MAJOR_NUMBER    251
/*TODO: r selected*/
#define WPE_MAGIC               'w'

/*This macro is for setting irq status represnted
 * by a local variable,WPEInfo.IrqInfo.Status[WPE_IRQ_TYPE_INT_WPE_ST]
 */
#define WPE_INT_ST                 (1<<0)



/*
 *    module's interrupt , each module should have its own isr.
 *    note:
 *        mapping to isr table,ISR_TABLE when using no device tree
 */
enum WPE_IRQ_TYPE_ENUM {
	WPE_IRQ_TYPE_INT_WPE_ST,    /* WPE */
	WPE_IRQ_TYPE_AMOUNT
};

#endif

