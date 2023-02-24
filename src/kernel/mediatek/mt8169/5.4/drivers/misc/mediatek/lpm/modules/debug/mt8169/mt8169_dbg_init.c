// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of_device.h>

#include <lpm_dbg_common_v1.h>
#include <lpm_module.h>
#include <lpm_spm_comm.h>

#include <lpm_dbg_fs_common.h>
#include <lpm_dbg_logger.h>

#include <mt8169_lpm_logger.h>
#include "mt8169_lpm_compat.h"

static int __init mt8169_dbg_fs_init(void)
{
	lpm_rc_fs_init();
	lpm_spm_fs_init();

	return 0;
}

static void __exit mt8169_dbg_fs_exit(void)
{
	lpm_spm_fs_deinit();
	lpm_rc_fs_deinit();
}

static int __init mt8169_dbg_device_initcall(void)
{
	mt8169_dbg_ops_register();
	lpm_dbg_common_fs_init();
	mt8169_dbg_fs_init();

	return 0;
}

static int __init mt8169_dbg_late_initcall(void)
{
	lpm_logger_init();

	return 0;
}

int __init mt8169_dbg_init(void)
{
	if (mt8169_dbg_device_initcall())
		goto mt8169_dbg_init_fail;

	if (mt8169_dbg_late_initcall())
		goto mt8169_dbg_init_fail;

	if (lpm_init())
		goto mt8169_dbg_init_fail;

	return 0;

mt8169_dbg_init_fail:
	return -EAGAIN;
}

void __exit mt8169_dbg_exit(void)
{
	lpm_dbg_common_fs_exit();
	mt8169_dbg_fs_exit();
	lpm_logger_deinit();
}

module_init(mt8169_dbg_init);
module_exit(mt8169_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MT8169 low power debug module");
MODULE_AUTHOR("MediaTek Inc.");
