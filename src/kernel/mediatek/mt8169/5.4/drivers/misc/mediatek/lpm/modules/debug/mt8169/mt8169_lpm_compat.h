/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _LPM_MT8169_COMPAT_H_
#define _LPM_MT8169_COMPAT_H_

/* compatible with lpm_dbg_common_v1.h */

struct lpm_dbg_plat_ops {
	int (*lpm_show_message)(int type, const char *prefix, void *data);
	void (*lpm_save_sleep_info)(void);
	void (*lpm_get_spm_wakesrc_irq)(void);
	int (*lpm_get_wakeup_status)(void);
};

int lpm_dbg_plat_ops_register(struct lpm_dbg_plat_ops *lpm_dbg_plat_ops);

/* compatible for mt8169_lpm_compat.c */

int __init lpm_init(void);

#endif
