/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef _KTCH_H
#define _KTCH_H

#include <linux/proc_fs.h>
#include <linux/seq_file.h>


#include "perf_ioctl.h"

#define TOUCH_BOOST_OPP 0

/* mtk_perf_ioctl.ko */
extern struct proc_dir_entry *perfmgr_root;

#endif /* _KTCH_H */
