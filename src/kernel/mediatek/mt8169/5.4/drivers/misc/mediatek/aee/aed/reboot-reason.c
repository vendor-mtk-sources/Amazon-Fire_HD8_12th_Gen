// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <asm/memory.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#include <mt-plat/aee.h>
#include <mt-plat/mboot_params.h>
#include <mt-plat/mrdump.h>
#include "aed.h"

#define RR_PROC_NAME "reboot-reason"

static struct proc_dir_entry *aee_rr_file;

static int aee_rr_reboot_reason_proc_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, aee_rr_reboot_reason_show, NULL);
}

static const struct file_operations aee_rr_reboot_reason_proc_fops = {
	.open = aee_rr_reboot_reason_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


void aee_rr_proc_init(struct proc_dir_entry *aed_proc_dir)
{
	aee_rr_file = proc_create(RR_PROC_NAME, 0440, aed_proc_dir,
			&aee_rr_reboot_reason_proc_fops);
	if (!aee_rr_file)
		pr_notice("%s: Can't create rr proc entry\n", __func__);
}
EXPORT_SYMBOL(aee_rr_proc_init);

void aee_rr_proc_done(struct proc_dir_entry *aed_proc_dir)
{
	remove_proc_entry(RR_PROC_NAME, aed_proc_dir);
}
EXPORT_SYMBOL(aee_rr_proc_done);
