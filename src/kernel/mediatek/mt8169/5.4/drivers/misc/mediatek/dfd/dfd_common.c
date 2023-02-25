// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/of_fdt.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/soc/mediatek/mtk_sip_svc.h> /* for SMC ID table */
#include <linux/arm-smccc.h>
#include "dfd.h"

static struct dfd_drv *drv;

/* return -1 for error indication */
int dfd_setup(int version)
{
	int ret = 0;
	int dfd_doe;
	struct arm_smccc_res res;

	if (drv && (drv->enabled == 1) && (drv->base_addr > 0)) {
		/* check support or not first */
		if (check_dfd_support() == 0) {
			pr_notice("DFD is not supported.\n");
			return -1;
		}

		if (drv->mem_reserve && drv->cachedump_en) {
			dfd_doe = DFD_CACHE_DUMP_ENABLE;
			if (drv->l2c_trigger)
				dfd_doe |= DFD_PARITY_ERR_TRIGGER;

			if (version == DFD_EXTENDED_DUMP)
				arm_smccc_smc(MTK_SIP_KERNEL_DFD,
					DFD_SMC_MAGIC_SETUP,
					(u64) drv->base_addr,
					drv->chain_length, dfd_doe,
					0, 0, 0, &res);
			else
				arm_smccc_smc(MTK_SIP_KERNEL_DFD,
					DFD_SMC_MAGIC_SETUP,
					(u64) drv->base_addr,
					drv->chain_length, 0, 0, 0, 0, &res);
		} else {
			arm_smccc_smc(MTK_SIP_KERNEL_DFD, DFD_SMC_MAGIC_SETUP,
				(u64) drv->base_addr, drv->chain_length,
				0, 0, 0, 0, &res);
		}

		if (res.a0) {
			pr_err("DFD setup failed. ret = %lu\n", res.a0);
			return -1;
		}

		return ret;
	}

	pr_notice("DFD is disabled\n");
	return -2;
}

static struct device_node __init *fdt_get_chosen(void)
{
	struct device_node *chosen_node;

	chosen_node = of_find_node_by_path("/chosen");
	if (!chosen_node)
		chosen_node = of_find_node_by_path("/chosen@0");

	return chosen_node;
}

static int __init dfd_init(void)
{
	struct device_node *dev_node, *infra_node, *node;
	const void *prop;
	unsigned int val;

	drv = kzalloc(sizeof(struct dfd_drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	/* get dfd settings */
	dev_node = of_find_compatible_node(NULL, NULL, "mediatek,dfd");
	if (dev_node) {
		if (of_property_read_u32(dev_node, "mediatek,enabled", &val))
			drv->enabled = 0;
		else
			drv->enabled = val;

		if (of_property_read_u32(dev_node,
					"mediatek,chain_length", &val))
			drv->chain_length = 0;
		else
			drv->chain_length = val;

		if (of_property_read_u32(dev_node,
					"mediatek,rg_dfd_timeout", &val))
			drv->rg_dfd_timeout = 0;
		else
			drv->rg_dfd_timeout = val;
	} else
		return -ENODEV;

	/* for cachedump enable */
	dev_node = of_find_compatible_node(NULL, NULL,
		"mediatek,dfd_cache");
	if (dev_node) {
		if (of_property_read_u32(dev_node, "mediatek,enabled", &val))
			drv->cachedump_en = 0;
		else
			drv->cachedump_en = val;

		if (drv->cachedump_en) {
			if (!of_property_read_u32(dev_node,
				"mediatek,rg_dfd_timeout", &val))
				drv->rg_dfd_timeout = val;
			if (!of_property_read_u32(dev_node,
				"mediatek,l2c_trigger", &val))
				drv->l2c_trigger = val;
		}
	}

	if (drv->enabled == 0)
		return 0;

	node = fdt_get_chosen();
	if (node) {
		prop = of_get_property(node, "dfd,base_addr_msb", NULL);
		drv->base_addr_msb = (prop) ? of_read_number(prop, 1) : 0;
	} else {
		drv->base_addr_msb = 0;
	}

	if (node) {
		prop = of_get_property(node, "dfd,cache_dump_support", NULL);
		drv->mem_reserve = (prop) ? of_read_number(prop, 1) : 0;
	} else {
		drv->mem_reserve = 0;
	}

	infra_node = of_find_compatible_node(NULL, NULL,
			"mediatek,infracfg_ao");
	if (infra_node) {
		void __iomem *infra = of_iomap(infra_node, 0);

		if (infra && drv->base_addr_msb) {
			infra += dfd_infra_base();
			writel(readl(infra)
				| (drv->base_addr_msb >> dfd_ap_addr_offset()),
				infra);
		}
	}

	/* get base address if enabled */
	if (node) {
		prop = of_get_property(node, "dfd,base_addr", NULL);
		drv->base_addr = (prop) ? (u64) of_read_number(prop, 2) : 0;
	} else
		return -ENODEV;

	return dfd_setup(0);
}
module_init(dfd_init);

static void dfd_exit(void)
{
}
module_exit(dfd_exit);

MODULE_DESCRIPTION("MediaTek DFD Driver");
MODULE_LICENSE("GPL v2");
