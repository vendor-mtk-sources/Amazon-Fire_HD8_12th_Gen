// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/debugfs.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>

#include "inc/mt6370_pmu.h"

#define MT6370_PMU_I2C_DRV_VERSION	"1.0.3_MTK"

static bool dbg_log_en; /* module param to enable/disable debug log */
module_param(dbg_log_en, bool, 0644);

#if IS_ENABLED(CONFIG_RT_REGMAP)
static struct rt_regmap_fops mt6370_regmap_fops = {
	.read_device = mt6370_pmu_read_device,
	.write_device = mt6370_pmu_write_device,
};
#endif /* #ifdef CONFIG_RT_REGMAP */

static int mt_parse_dt(struct device *dev,
		       struct mt6370_pmu_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	int ret = 0;

#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
	ret = of_get_named_gpio(np, "mt6370,intr_gpio", 0);
	if (ret < 0)
		goto out_parse_dt;
	pdata->intr_gpio = ret;
#else
	ret =  of_property_read_u32(np, "mt6370,intr_gpio_num",
					&pdata->intr_gpio);
	if (ret < 0)
		goto out_parse_dt;
#endif
	return 0;
out_parse_dt:
	return ret;
}

static inline void rt_config_of_node(struct device *dev)
{
	struct device_node *np = NULL;

	np = of_find_node_by_name(NULL, "mt6370_pmu_dts");
	if (np) {
		dev_notice(dev, "find mt6370_pmu_dts node\n");
		dev->of_node = np;
	}
}

static inline int mt6370_pmu_chip_id_check(struct i2c_client *i2c)
{
	int ret = 0;
	u8 vendor_id = 0, chip_rev = 0;

	ret = i2c_smbus_read_byte_data(i2c, MT6370_PMU_REG_DEVINFO);
	if (ret < 0)
		return ret;

	vendor_id = ret & 0xF0;
	chip_rev = ret & 0x0F;

	switch (vendor_id) {
	case RT5081_VENDOR_ID:
	case MT6370_VENDOR_ID:
	case MT6371_VENDOR_ID:
	case MT6372_VENDOR_ID:
	case MT6372C_VENDOR_ID:
		dev_notice(&i2c->dev, "%s: E%d(0x%02X)\n",
				      __func__, chip_rev, vendor_id);
		break;
	default:
		dev_notice(&i2c->dev, "%s: vendor id(0x%02X) does not match\n",
				      __func__, vendor_id);
		ret = -ENODEV;
	}

	return ret;
}

static int mt6370_pmu_suspend(struct device *dev)
{
	struct mt6370_pmu_chip *chip = dev_get_drvdata(dev);

	mt_dbg(chip->dev, "%s\n", __func__);
	mt6370_pmu_irq_suspend(chip);
	disable_irq(chip->irq);
	return 0;
}

static int mt6370_pmu_resume(struct device *dev)
{
	struct mt6370_pmu_chip *chip = dev_get_drvdata(dev);

	mt_dbg(dev, "%s\n", __func__);
	enable_irq(chip->irq);
	mt6370_pmu_irq_resume(chip);
	return 0;
}

static SIMPLE_DEV_PM_OPS(mt6370_pmu_pm_ops, mt6370_pmu_suspend,
	mt6370_pmu_resume);

unsigned int g_reg_addr;
static ssize_t reg_access_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int reg_value = 0;
	struct mt6370_pmu_chip *chip = dev_get_drvdata(dev);


	reg_value = mt6370_pmu_reg_read(chip, g_reg_addr);

	pr_info("[%s] 0x%x = 0x%x\n", __func__, g_reg_addr, reg_value);
	return sprintf(buf, "0x%x = 0x%x\n", g_reg_addr, reg_value);
}

static ssize_t reg_access_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t size)
{
	int ret = 0;
	char *pvalue = NULL, *addr = NULL, *val = NULL;
	unsigned int reg_value = 0;
	struct mt6370_pmu_chip *chip = dev_get_drvdata(dev);

	pr_info("[%s]\n", __func__);
	if (buf != NULL && size != 0) {
		pr_info("[%s] size is %d, buf is %s\n"
			, __func__, (int)size, buf);

		pvalue = (char *)buf;
		addr = strsep(&pvalue, " ");
		val = strsep(&pvalue, " ");
		if (addr)
			ret = kstrtou32(addr, 16, (unsigned int *)&g_reg_addr);
		if (val) {
			ret = kstrtou32(val, 16, (unsigned int *)&reg_value);

			pr_info("[%s] write mt6370 reg 0x%x with value 0x%x !\n"
				, __func__, g_reg_addr, reg_value);
			ret = mt6370_pmu_reg_write(chip, g_reg_addr, reg_value);
		} else {
			reg_value = mt6370_pmu_reg_read(chip, g_reg_addr);
			pr_info("[%s] read mt6370 reg 0x%x with value 0x%x !\n"
				, __func__, g_reg_addr, reg_value);
		}
	}
	return size;
}
static const DEVICE_ATTR_RW(reg_access);

static int mt6370_pmu_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct mt6370_pmu_chip *chip;
	struct mt6370_pmu_platform_data *pdata = dev_get_platdata(&i2c->dev);
	bool use_dt = i2c->dev.of_node;
	u8 dev_info = 0;
	int ret = 0;
	int ret_device_file = 0;

	pr_info("%s: (%s)\n", __func__, MT6370_PMU_I2C_DRV_VERSION);

	ret = mt6370_pmu_chip_id_check(i2c);
	if (ret < 0)
		return ret;
	dev_info = ret;
	if (use_dt) {
		rt_config_of_node(&i2c->dev);
		pdata = devm_kzalloc(&i2c->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		ret = mt_parse_dt(&i2c->dev, pdata);
		if (ret < 0) {
			dev_err(&i2c->dev, "error parse platform data\n");
			devm_kfree(&i2c->dev, pdata);
			return ret;
		}
		i2c->dev.platform_data = pdata;
	} else {
		if (!pdata)
			return -EINVAL;
	}
	chip = devm_kzalloc(&i2c->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->i2c = i2c;
	chip->dev = &i2c->dev;
	chip->chip_vid = dev_info & 0xF0;
	chip->chip_rev = dev_info & 0x0F;
	mutex_init(&chip->io_lock);
	mutex_init(&chip->irq_lock);
	i2c_set_clientdata(i2c, chip);

	pm_runtime_set_active(&i2c->dev);
#if IS_ENABLED(CONFIG_RT_REGMAP)
	ret = mt6370_pmu_regmap_register(chip, &mt6370_regmap_fops);
	if (ret < 0)
		goto out_regmap;
#endif /* #ifdef CONFIG_RT_REGMAP */
	ret = mt6370_pmu_irq_register(chip);
	if (ret < 0)
		goto out_irq;
	ret = mt6370_pmu_subdevs_register(chip);
	if (ret < 0)
		goto out_subdevs;
	pm_runtime_enable(&i2c->dev);
	dev_info(&i2c->dev, "%s successfully\n", __func__);

	if (IS_ENABLED(CONFIG_DEBUG_FS)) {
		ret_device_file = device_create_file(&i2c->dev, &dev_attr_reg_access);
		if (ret_device_file)
			dev_info(&i2c->dev, "%s debugfs fail\n", __func__);
		else
			dev_dbg(&i2c->dev, "%s debugfs success\n", __func__);
	}
	return 0;
out_subdevs:
	mt6370_pmu_irq_unregister(chip);
out_irq:
#if IS_ENABLED(CONFIG_RT_REGMAP)
	mt6370_pmu_regmap_unregister(chip);
out_regmap:
#endif /* #ifdef CONFIG_RT_REGMAP */
	pm_runtime_set_suspended(&i2c->dev);
	devm_kfree(&i2c->dev, chip);
	if (use_dt)
		devm_kfree(&i2c->dev, pdata);
	return ret;
}

static int mt6370_pmu_remove(struct i2c_client *i2c)
{
	struct mt6370_pmu_chip *chip = i2c_get_clientdata(i2c);

	pm_runtime_disable(&i2c->dev);
	mt6370_pmu_subdevs_unregister(chip);
	mt6370_pmu_irq_unregister(chip);
	pm_runtime_set_suspended(&i2c->dev);
	dev_info(chip->dev, "%s successfully\n", __func__);
	return 0;
}

static const struct i2c_device_id mt6370_pmu_id_table[] = {
	{"mt6370_pmu", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, mt6370_pmu_id_table);

static const struct of_device_id mt6370_pmu_ofid_table[] = {
	{.compatible = "mediatek,mt6370_pmu",},
	{.compatible = "mediatek,subpmic_pmu",},
	{},
};
MODULE_DEVICE_TABLE(of, mt6370_pmu_ofid_table);

static struct i2c_driver mt6370_pmu = {
	.driver = {
		.name = "mt6370_pmu",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mt6370_pmu_ofid_table),
		.pm = &mt6370_pmu_pm_ops,
	},
	.probe = mt6370_pmu_probe,
	.remove = mt6370_pmu_remove,
	.id_table = mt6370_pmu_id_table,
};

module_i2c_driver(mt6370_pmu);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek MT6370 PMU");
MODULE_VERSION(MT6370_PMU_I2C_DRV_VERSION);

/*
 * Release Note
 * 1.0.3_MTK
 * (1) disable_irq()/enable_irq() in suspend()/resume()
 *
 * 1.0.2_MTK
 * (1) Add support for MT6372

 * 1.0.1_MTK
 * (1) Replace rt_mutex with mutex
 *
 * 1.0.0_MTK
 * (1) Initial Release
 */
