/*
 * Copyright (C) 2022 Lab126, Inc.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include "thermal_core.h"

#define DRIVER_NAME "sysrst_cooling"
#define sysrst_cooling_dprintk(fmt, args...) \
	pr_info("%s:Line(%d) " fmt, __FILE__, __LINE__, ##args)

static unsigned int cl_dev_sysrst_state;
static struct thermal_cooling_device *cl_dev_sysrst;

static int sysrst_cooling_get_max_state(struct thermal_cooling_device *cdev,
					unsigned long *state)
{
	*state = 1;
	return 0;
}

static int sysrst_cooling_get_cur_state(struct thermal_cooling_device *cdev,
					unsigned long *state)
{
	*state = cl_dev_sysrst_state;
	return 0;
}

static int sysrst_cooling_set_cur_state(struct thermal_cooling_device *cdev,
					unsigned long state)
{
	struct thermal_instance *instance;
	int trip_temp;

	cl_dev_sysrst_state = state;

	if (cl_dev_sysrst_state == 1) {
		sysrst_cooling_dprintk("%s = 1\n", __func__);

		list_for_each_entry(instance,
				    &(cdev->thermal_instances),
				    cdev_node) {
			if (instance->tz && instance->tz->ops &&
			    instance->tz->ops->get_trip_temp) {
				instance->tz->ops->get_trip_temp(instance->tz,
								instance->trip,
								&trip_temp);
				pr_err("[%s][%s]type:[%s] Thermal reboot, "
					"current temp=%d, trip=%d, "
					"trip_temp=%d\n",
					__func__,
					dev_name(&(instance->tz->device)),
					instance->tz->type,
					instance->tz->temperature,
					instance->trip,
					trip_temp);
			}

		}

		orderly_reboot();
	}
	return 0;
}

static struct thermal_cooling_device_ops cooling_ops = {
	.get_max_state = sysrst_cooling_get_max_state,
	.get_cur_state = sysrst_cooling_get_cur_state,
	.set_cur_state = sysrst_cooling_set_cur_state,
};

void sysrst_parse_node_string_index(const struct device_node *np,
				    const char *node_name,
				    int index,
				    char *cust_string)
{
	const char *string;

	if (of_property_read_string_index(np, node_name, index, &string) == 0) {
		strncpy(cust_string, string, strlen(string));
		pr_debug("Get %s %s\n", node_name, cust_string);
	} else
		pr_notice("%s:Get %s failed\n", __func__, node_name);
}

static int sysrst_cooling_probe(struct platform_device *pdev)
{
	int ret = 0;
	char type[THERMAL_NAME_LENGTH];

	sysrst_cooling_dprintk("probe\n");

#ifdef CONFIG_OF
	pr_notice("%s:cooler custom init by DTS!\n", __func__);
	sysrst_parse_node_string_index(pdev->dev.of_node, "type", 0, type);
#endif

	cl_dev_sysrst = thermal_of_cooling_device_register(pdev->dev.of_node,
							   type,
							   NULL,
							   &cooling_ops);
	if (!cl_dev_sysrst) {
		pr_err("%s Failed to create sysrst cooling device\n",
			__func__);
		ret = -EINVAL;
	}

	return ret;
}

static int sysrst_cooling_remove(struct platform_device *pdev)
{
	sysrst_cooling_dprintk("remove\n");
	if (cl_dev_sysrst) {
		thermal_cooling_device_unregister(cl_dev_sysrst);
		cl_dev_sysrst = NULL;
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sysrst_of_match_table[] = {
	{.compatible = "amazon,sysrst_cooler", },
	{},
};
MODULE_DEVICE_TABLE(of, sysrst_of_match_table);
#endif

static struct platform_driver sysrst_driver = {
	.probe = sysrst_cooling_probe,
	.remove = sysrst_cooling_remove,
	.driver     = {
		.name  = DRIVER_NAME,
#ifdef CONFIG_OF
		.of_match_table = sysrst_of_match_table,
#endif
		.owner = THIS_MODULE,
	},
};

static int __init sysrst_cooling_init(void)
{
	int err = 0;

	sysrst_cooling_dprintk("init\n");

	err = platform_driver_register(&sysrst_driver);
	if (err) {
		pr_err("%s: Failed to register driver %s\n", __func__,
			sysrst_driver.driver.name);
		return err;
	}

	return 0;
}

static void __exit sysrst_cooling_exit(void)
{
	sysrst_cooling_dprintk("exit\n");

	platform_driver_unregister(&sysrst_driver);
}

module_init(sysrst_cooling_init);
module_exit(sysrst_cooling_exit);

MODULE_DESCRIPTION("System reboot cooling driver");
MODULE_AUTHOR("Liu Hu <liuhu5@huaqin.com>");
MODULE_LICENSE("GPL");
