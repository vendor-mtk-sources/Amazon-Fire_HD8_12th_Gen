// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/syscore_ops.h>

#include <lpm.h>
#include <lpm_spm_comm.h>
#include <mtk_lpm_sysfs.h>
#include "mt8169_lpm_compat.h"

/* compatible with lpm_dbg_logger.c */

#include <lpm_module.h>

static struct lpm_dbg_plat_ops _lpm_dbg_plat_ops;

void __iomem *lpm_spm_base;
EXPORT_SYMBOL(lpm_spm_base);

static void lpm_check_cg_pll(void)
{
	int i;
	u32 block;
	u32 blkcg;

	block = (u32)
		lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RC_DUMP_PLL,
				MT_LPM_SMC_ACT_GET, 0, 0);
	if (block != 0) {
		for (i = 0 ; i < spm_cond.pll_cnt ; i++) {
			if (block & 1 << (16+i))
				pr_info("suspend warning: pll: %s not closed\n"
					, spm_cond.pll_str[i]);
		}
	}

	/* Definition about SPM_COND_CHECK_BLOCKED
	 * bit [00 ~ 15]: cg blocking index
	 * bit [16 ~ 29]: pll blocking index
	 * bit [30]     : pll blocking information
	 * bit [31]	: idle condition check fail
	 */

	for (i = 1 ; i < spm_cond.cg_cnt ; i++) {
		blkcg = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_BLOCK_DETAIL, MT_LPM_SMC_ACT_GET, 0, i);
		if (blkcg != 0)
			pr_info("suspend warning: CG: %6s = 0x%08x\n"
				, spm_cond.cg_str[i], blkcg);
	}
}

int lpm_dbg_plat_ops_register(struct lpm_dbg_plat_ops *lpm_dbg_plat_ops)
{
	if (!lpm_dbg_plat_ops)
		return -1;

	_lpm_dbg_plat_ops.lpm_show_message = lpm_dbg_plat_ops->lpm_show_message;
	_lpm_dbg_plat_ops.lpm_save_sleep_info = lpm_dbg_plat_ops->lpm_save_sleep_info;
	_lpm_dbg_plat_ops.lpm_get_spm_wakesrc_irq = lpm_dbg_plat_ops->lpm_get_spm_wakesrc_irq;
	_lpm_dbg_plat_ops.lpm_get_wakeup_status = lpm_dbg_plat_ops->lpm_get_wakeup_status;

	return 0;
}
EXPORT_SYMBOL(lpm_dbg_plat_ops_register);

static int lpm_issuer_func(int type, const char *prefix, void *data)
{
	if (!_lpm_dbg_plat_ops.lpm_get_wakeup_status)
		return -1;

	_lpm_dbg_plat_ops.lpm_get_wakeup_status();

	if (type == LPM_ISSUER_SUSPEND)
		lpm_check_cg_pll();

	if (_lpm_dbg_plat_ops.lpm_show_message)
		return _lpm_dbg_plat_ops.lpm_show_message(
			 type, prefix, data);
	else
		return -1;
}

struct lpm_issuer issuer = {
	.log = lpm_issuer_func,
	.log_type = 0,
};

static void lpm_suspend_save_sleep_info_func(void)
{
	if (_lpm_dbg_plat_ops.lpm_save_sleep_info)
		_lpm_dbg_plat_ops.lpm_save_sleep_info();
}

static struct syscore_ops lpm_suspend_save_sleep_info_syscore_ops = {
	.resume = lpm_suspend_save_sleep_info_func,
};

int lpm_logger_init(void)
{
	struct device_node *node = NULL;
	int ret = 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");

	if (node) {
		lpm_spm_base = of_iomap(node, 0);
		of_node_put(node);
	}

	if (lpm_spm_base)
		lpm_issuer_register(&issuer);
	else
		pr_info("[name:mtk_lpm][P] - Don't register the issue by error! (%s:%d)\n",
			__func__, __LINE__);

	if (_lpm_dbg_plat_ops.lpm_get_spm_wakesrc_irq)
		_lpm_dbg_plat_ops.lpm_get_spm_wakesrc_irq();

	mtk_lpm_sysfs_root_entry_create();

	ret = spm_cond_init();
	if (ret)
		pr_info("[%s:%d] - spm_cond_init failed\n",
			__func__, __LINE__);

	register_syscore_ops(&lpm_suspend_save_sleep_info_syscore_ops);

	return 0;
}
EXPORT_SYMBOL(lpm_logger_init);

void __exit lpm_logger_deinit(void)
{
	spm_cond_deinit();
}
EXPORT_SYMBOL(lpm_logger_deinit);

/* compatible with lpm_module.c */

#include <lpm_registry.h>

#define LPM_MODULE_MAGIC	0xFCD03DC3
#define LPM_CPUIDLE_DRIVER	0xD0
#define LPM_MODLE		0xD1
#define LPM_SYS_ISSUER		0xD2

struct lpm_model_info {
	const char *name;
	void *mod;
};

struct lpm_module_reg {
	int magic;
	int type;
	union {
		struct lpm_issuer *issuer;
		struct lpm_model_info info;
	} data;
};

struct mtk_lpm {
	struct lpm_model suspend;
	struct lpm_issuer *issuer;
};

static struct mtk_lpm lpm_system;

static DEFINE_SPINLOCK(lpm_mod_locker);
static DEFINE_SPINLOCK(lpm_sys_locker);

void lpm_system_spin_lock(unsigned long *irqflag)
{
	if (irqflag) {
		unsigned long flag;

		spin_lock_irqsave(&lpm_sys_locker, flag);
		*irqflag = flag;
	} else
		spin_lock(&lpm_sys_locker);
}
void lpm_system_spin_unlock(unsigned long *irqflag)
{
	if (irqflag) {
		unsigned long flag = *irqflag;

		spin_unlock_irqrestore(&lpm_sys_locker, flag);
	} else
		spin_unlock(&lpm_sys_locker);
}

static int __lpm_issuer_register(struct lpm_issuer *issuer)
{
	lpm_system.issuer = issuer;

	return 0;
}

int lpm_issuer_register(struct lpm_issuer *issuer)
{
	return __lpm_issuer_register(issuer);
}
EXPORT_SYMBOL(lpm_issuer_register);

static int lpm_suspend_enter(void)
{
	return 0;
}

static void lpm_suspend_resume(void)
{
	unsigned long flags;
	struct lpm_issuer *issuer = lpm_system.issuer;

	if (issuer) {
		spin_lock_irqsave(&lpm_mod_locker, flags);
		issuer->log(LPM_ISSUER_SUSPEND, "suspend", (void *)issuer);
		spin_unlock_irqrestore(&lpm_mod_locker, flags);
	}
}

struct syscore_ops lpm_suspend = {
	.suspend = lpm_suspend_enter,
	.resume = lpm_suspend_resume,
};

int __init lpm_init(void)
{
	register_syscore_ops(&lpm_suspend);

	return 0;
}
