#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/thermal_framework.h>

#ifdef CONFIG_THERMAL_FOD
#include <linux/power_supply.h>
#define POLICY_BPP_IDX 1
#define POLICY_MFA_IDX 2
#endif

#define DRIVER_NAME "wpc_cooler"
#define virtual_sensor_cooler_bccl_dprintk(fmt, args...) pr_debug("thermal/cooler/wpc_bcct " fmt, ##args)

/* Cancel the current limit value */
#define CHARGE_CURRENT_MAX 10000

static struct mutex bcct_update_lock;
static struct cooler_sort_list bcct_list_head;

static int virtual_sensor_get_max_state(struct thermal_cooling_device *cdev,
					unsigned long *state)
{
	struct vs_cooler_platform_data *pdata;

	if (!cdev || !(cdev->devdata)) {
		pr_err("%s cdev: %p or cdev->devdata is NULL!\n", __func__, cdev);
		return -EINVAL;
	}
	pdata = cdev->devdata;
	*state = pdata->max_state;
	return 0;
}

static int virtual_sensor_get_cur_state(struct thermal_cooling_device *cdev,
					unsigned long *state)
{
	struct vs_cooler_platform_data *pdata;

	if (!cdev || !(cdev->devdata)) {
		pr_err("%s cdev: %p or cdev->devdata is NULL!\n", __func__, cdev);
		return -EINVAL;
	}
	pdata = cdev->devdata;
	*state = pdata->state;
	return 0;
}

static int virtual_sensor_set_cur_state(struct thermal_cooling_device *cdev,
					unsigned long state)
{
	int level;
	struct vs_cooler_platform_data *pdata;
#ifdef CONFIG_THERMAL_FOD
	int ret = 0;
	union power_supply_propval propval;
	struct power_supply *chg_psy;
	int dock_type;
#endif

	if (!cdev || !(cdev->devdata)) {
		pr_err("%s cdev: %p or cdev->devdata is NULL!\n", __func__, cdev);
		return -EINVAL;
	}
	pdata = cdev->devdata;
	if (pdata->state == state)
		return 0;

#ifdef CONFIG_THERMAL_FOD
	if (!strcmp(cdev->type, "wpc_bcct1")) {
		chg_psy = power_supply_get_by_name("mt6370_pmu_charger");
		if (!chg_psy) {
			pr_err("Cannot find power supply \"mt6370_pmu_charger\"\n");
			return -ENODEV;
		}

		ret = power_supply_get_property(chg_psy, POWER_SUPPLY_PROP_TYPE, &propval);
		if (ret < 0) {
			pr_err("[FOD_INFO] : get psy type failed, ret = %d\n", __func__, ret);
			return ret;
		}

		/* -------------------------------------------------------------
		 * Type    | enum power_supply_type              | Idx of policy      |
		 * -------------------------------------------------------------
		 * BPP     | POWER_SUPPLY_TYPE_WIRELESS_5W(17)   | POLICY_BPP_IDX (1) |
		 * -------------------------------------------------------------
		 * MFA     | POWER_SUPPLY_TYPE_WIRELESS_10W(18)  | POLICY_MFA_IDX (2) |
		 * -------------------------------------------------------------
		 * Default | POWER_SUPPLY_TYPE_WIRELESS(12)      | POLICY_BPP_IDX (1) |
		 * -------------------------------------------------------------
		 *  Set up power supply type as trip point into policy.
		 *  The action of each trip point corresponds to the specified power supply type.
		 */
		switch (propval.intval) {
		case POWER_SUPPLY_TYPE_WIRELESS:
		case POWER_SUPPLY_TYPE_WIRELESS_5W:
			dock_type = POLICY_BPP_IDX;
			break;
		case POWER_SUPPLY_TYPE_WIRELESS_10W:
			dock_type = POLICY_MFA_IDX;
			break;
		default:
			dock_type = POLICY_BPP_IDX;
			pr_err("[FOD_INFO] : power_supply_type[%d] is not supported\n", propval.intval);
			break;
		}

		if (state != 0 && state < dock_type)
			return 0;
		pr_info("[FOD_INFO]:%s state = %d, dock_type = %d\n", __func__, state, dock_type);
	}
#endif

	pdata->state = (state > pdata->max_state) ? pdata->max_state : state;

	if (!pdata->state)
		level = CHARGE_CURRENT_MAX;
	else
		level = pdata->levels[pdata->state - 1];

	if (level == pdata->level) {
		goto out;
	}
	pdata->level = level;
	mutex_lock(&bcct_update_lock);
	level = thermal_level_compare(pdata, &bcct_list_head, true);
	vs_set_cooling_level(cdev, pdata->thermal_cooler_id,
		(level >= CHARGE_CURRENT_MAX) ? -1 : level);
	mutex_unlock(&bcct_update_lock);
out:
	return 0;
}

static ssize_t levels_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct thermal_cooling_device *cdev =
	    container_of(dev, struct thermal_cooling_device, device);
	struct vs_cooler_platform_data *pdata;
	int i;
	int offset = 0;
	size_t bufsz = 0;

	if (!cdev || !(cdev->devdata)) {
		pr_err("%s cdev: %p or cdev->devdata is NULL!\n", __func__, cdev);
		return -EINVAL;
	}
	pdata = cdev->devdata;
	bufsz = pdata->max_state * 20;
	for (i = 0; i < pdata->max_state; i++)
		offset +=
		    scnprintf(buf + offset, bufsz - offset, "%d %d\n", i + 1, pdata->levels[i]);
	return offset;
}

static ssize_t levels_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int level, state;
	struct thermal_cooling_device *cdev =
	    container_of(dev, struct thermal_cooling_device, device);
	struct vs_cooler_platform_data *pdata;

	if (!cdev || !(cdev->devdata)) {
		pr_err("%s cdev: %p or cdev->devdata is NULL!\n", __func__, cdev);
		return -EINVAL;
	}
	pdata = cdev->devdata;
	if (sscanf(buf, "%d %d\n", &state, &level) != 2)
		return -EINVAL;
	if (state >= pdata->max_state)
		return -EINVAL;
	pdata->levels[state] = level;
	return count;
}

static struct thermal_cooling_device_ops cooling_ops = {
	.get_max_state = virtual_sensor_get_max_state,
	.get_cur_state = virtual_sensor_get_cur_state,
	.set_cur_state = virtual_sensor_set_cur_state,
};

static DEVICE_ATTR(levels, 0644, levels_show, levels_store);

static int bcct_probe(struct platform_device *pdev)
{
	struct vs_cooler_platform_data *pdata = NULL;
	struct cooler_sort_list *tz_list = NULL;
	int ret = 0;

	virtual_sensor_cooler_bccl_dprintk("probe\n");
	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

#ifdef CONFIG_OF
	pr_notice("cooler custom init by DTS!\n");
	cooler_init_cust_data_from_dt(pdev, pdata);
#endif

	pdata->cdev = thermal_of_cooling_device_register(pdev->dev.of_node,
							pdata->type,
							pdata,
							&cooling_ops);
	if (!pdata->cdev) {
		pr_err("%s Failed to create bcct cooling device\n",
			__func__);
		ret = -EINVAL;
		goto register_err;
	}

	tz_list = kzalloc(sizeof(struct cooler_sort_list), GFP_KERNEL);
	if (!tz_list) {
		pr_err("%s Failed to allocate cooler_sort_list memory\n",
			__func__);
		ret = -ENOMEM;
		goto list_mem_err;
	}

	tz_list->pdata = pdata;
	mutex_lock(&bcct_update_lock);
	list_add(&tz_list->list, &bcct_list_head.list);
	mutex_unlock(&bcct_update_lock);
	ret = device_create_file(&pdata->cdev->device, &dev_attr_levels);
	if (ret)
		pr_err("%s Failed to create bcct cooler levels attr\n", __func__);

	platform_set_drvdata(pdev, pdata);
	kobject_uevent(&pdata->cdev->device.kobj, KOBJ_ADD);
	return 0;

list_mem_err:
	thermal_cooling_device_unregister(pdata->cdev);
register_err:
	devm_kfree(&pdev->dev, pdata);

	return ret;
}

static int bcct_remove(struct platform_device *pdev)
{
	struct vs_cooler_platform_data *pdata = platform_get_drvdata(pdev);
	struct cooler_sort_list *cooler_list, *tmp;

	virtual_sensor_cooler_bccl_dprintk("remove\n");
	if (pdata) {
		mutex_lock(&bcct_update_lock);
		list_for_each_entry_safe(cooler_list, tmp, &bcct_list_head.list, list) {
			if (cooler_list->pdata == pdata) {
				list_del(&cooler_list->list);
				kfree(cooler_list);
				cooler_list = NULL;
				break;
			}
		}
		mutex_unlock(&bcct_update_lock);
		if (pdata->cdev) {
			device_remove_file(&pdata->cdev->device, &dev_attr_levels);
			thermal_cooling_device_unregister(pdata->cdev);
		}
		devm_kfree(&pdev->dev, pdata);
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id bcct_of_match_table[] = {
	{.compatible = "amazon,wpc_cooler", },
	{},
};
MODULE_DEVICE_TABLE(of, bcct_of_match_table);
#endif

static struct platform_driver bcct_driver = {
	.probe = bcct_probe,
	.remove = bcct_remove,
	.driver     = {
		.name  = DRIVER_NAME,
#ifdef CONFIG_OF
		.of_match_table = bcct_of_match_table,
#endif
		.owner = THIS_MODULE,
	},
};

static int __init virtual_sensor_cooler_bcct_init(void)
{
	int err = 0;

	virtual_sensor_cooler_bccl_dprintk("init\n");

	INIT_LIST_HEAD(&bcct_list_head.list);
	mutex_init(&bcct_update_lock);
	err = platform_driver_register(&bcct_driver);
	if (err) {
		pr_err("%s: Failed to register driver %s\n", __func__,
			bcct_driver.driver.name);
		return err;
	}

	return 0;
}

static void __exit virtual_sensor_cooler_bcct_exit(void)
{
	virtual_sensor_cooler_bccl_dprintk("exit\n");

	platform_driver_unregister(&bcct_driver);
	mutex_destroy(&bcct_update_lock);
}

module_init(virtual_sensor_cooler_bcct_init);
module_exit(virtual_sensor_cooler_bcct_exit);

MODULE_DESCRIPTION("VIRTUAL_SENSOR wpc bcct cooler driver");
MODULE_AUTHOR("Liu Hu <liuhu5@huaqin.com>");
MODULE_LICENSE("GPL");
