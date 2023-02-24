// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.

#include <linux/bitops.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/property.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>

enum {
	MA5150_IDX_CH1 = 1,
	MA5150_IDX_CH2,
	MA5150_IDX_CH3,
	MA5150_IDX_CH4,
	MA5150_IDX_CH5
};

#define MA5150_MODE_AUTO	0
#define MA5150_MODE_FORCEPWM	1

#define MA5150_REG_PGINFO	0x03
#define MA5150_REG_CH1VID	0x06
#define MA5150_REG_ENABLE	0x08
#define MA5150_REG_PWMMODECTL	0x09
#define MA5150_REG_SPMMODECTL	0x0C
#define MA5150_REG_SEQCFG	0x0D
#define MA5150_REG_CH1TIMECFG	0x0E
#define MA5150_REG_EN1TIMECFG	0x13
#define MA5150_REG_RSTDLYTIME	0x1C
#define MA5150_REG_INTSTAT	0x1E
#define MA5150_REG_ACTDISCHG	0x1F
#define MA5150_REG_CH2VOFFS	0x20
#define MA5150_REG_FZCMODE	0x44

#define MA5150_CH1VID_MASK	GENMASK(6, 0)
#define MA5150_CHEN_MASK(_id)	BIT(_id)
#define MA5150_CHAD_MASK(_id)	BIT(_id)
#define MA5150_CH2VOFFS_MASK	GENMASK(1, 0)
#define MA5150_HDSTAT_MASK	BIT(1)
#define MA5150_SLEEPSEL_MASK	BIT(7)
#define MA5150_SLEEPSEQ_MASK	BIT(1)

#define MA5150_CH1_MINUV	568750
#define MA5150_CH1_STEPUV	6250
#define MA5150_CH1_MAXUV	1362500
#define MA5150_CH1_NUM_VOLTS	((MA5150_CH1_MAXUV - MA5150_CH1_MINUV) / MA5150_CH1_STEPUV + 1)
#define MA5150_CH3_FIXED_VOLT	1800000
#define MA5150_CH4_FIXED_VOLT	3300000
#define MA5150_CH5_FIXED_VOLT	750000
#define MA5150_OFFSEQ_REGNUM	6


struct ma5150_data {
	struct device *dev;
	struct regmap *regmap;
	u8 power_off_seq[MA5150_OFFSEQ_REGNUM];
	struct notifier_block reboot_notifier;
};

static int ma5150_get_error_flags(struct regulator_dev *rdev, unsigned int *flags)
{
	struct regmap *regmap = rdev_get_regmap(rdev);
	int err_mask = BIT(rdev_get_id(rdev));
	unsigned int val, events = 0;
	u8 stat_raw[3], stat;
	int i, ret;
	const struct {
		bool inverted;
		unsigned int event;
	} stat_conv[3] = {
		{ true, REGULATOR_ERROR_FAIL },
		{ false, REGULATOR_ERROR_UNDER_VOLTAGE },
		{ false, REGULATOR_ERROR_REGULATION_OUT }
	};

	ret = regmap_raw_read(regmap, MA5150_REG_PGINFO, stat_raw, sizeof(stat_raw));
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(stat_conv); i++) {
		stat = stat_conv[i].inverted ? ~stat_raw[i] : stat_raw[i];
		if (!(stat & err_mask))
			continue;

		events |= stat_conv[i].event;
	}

	ret = regmap_read(regmap, MA5150_REG_INTSTAT, &val);
	if (ret)
		return ret;

	if (val & MA5150_HDSTAT_MASK)
		events |= REGULATOR_ERROR_OVER_TEMP;

	*flags = events;
	return 0;
}

static int ma5150_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct regmap *regmap = rdev_get_regmap(rdev);
	unsigned int mode_mask = BIT(rdev_get_id(rdev)), mode_val = 0;

	switch (mode) {
	case REGULATOR_MODE_FAST:
		mode_val = mode_mask;
		fallthrough;
	case REGULATOR_MODE_NORMAL:
		break;
	default:
		return -EINVAL;
	}

	return regmap_update_bits(regmap, MA5150_REG_PWMMODECTL, mode_mask, mode_val);
}

static unsigned int ma5150_get_mode(struct regulator_dev *rdev)
{
	struct regmap *regmap = rdev_get_regmap(rdev);
	unsigned int mode_mask = BIT(rdev_get_id(rdev)), mode_val;
	int ret;

	ret = regmap_read(regmap, MA5150_REG_PWMMODECTL, &mode_val);
	if (ret)
		return ret;

	if (mode_val & mode_mask)
		return REGULATOR_MODE_FAST;

	return REGULATOR_MODE_NORMAL;
}

static const struct regulator_ops buck1_regulator_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_active_discharge = regulator_set_active_discharge_regmap,
	.get_error_flags = ma5150_get_error_flags,
	.set_mode = ma5150_set_mode,
	.get_mode = ma5150_get_mode,
};

static const struct regulator_ops buck2_regulator_ops = {
	.list_voltage = regulator_list_voltage_table,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_active_discharge = regulator_set_active_discharge_regmap,
	.get_error_flags = ma5150_get_error_flags,
	.set_mode = ma5150_set_mode,
	.get_mode = ma5150_get_mode,
};

static const struct regulator_ops buck34_regulator_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_active_discharge = regulator_set_active_discharge_regmap,
	.get_error_flags = ma5150_get_error_flags,
	.set_mode = ma5150_set_mode,
	.get_mode = ma5150_get_mode,
};

static const struct regulator_ops ldo1_regulator_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_active_discharge = regulator_set_active_discharge_regmap,
	.get_error_flags = ma5150_get_error_flags,
};

static unsigned int ma5150_of_map_mode(unsigned int mode)
{
	if (mode == MA5150_MODE_AUTO)
		return REGULATOR_MODE_NORMAL;
	else if (mode == MA5150_MODE_FORCEPWM)
		return REGULATOR_MODE_FAST;

	return REGULATOR_MODE_INVALID;
}

static const unsigned int buck2_volt_table[] = { 600000, 576000, 652000 };

#define MA5150_REG_DESC(_id, _minuV, _uVstep, _nvolts, _fixuV, _vtbl, _vselreg, _vselmask, _ops,\
			_map_mode) \
{ \
	.name = "ma5150-chan" #_id, \
	.of_match = of_match_ptr("chan" #_id), \
	.regulators_node = of_match_ptr("regulators"), \
	.of_map_mode = _map_mode, \
	.id = MA5150_IDX_CH##_id, \
	.type = REGULATOR_VOLTAGE, \
	.owner = THIS_MODULE, \
	.min_uV = _minuV, \
	.uV_step = _uVstep, \
	.volt_table = _vtbl, \
	.n_voltages = _nvolts, \
	.fixed_uV = _fixuV, \
	.ops = &_ops##_regulator_ops, \
	.vsel_reg = _vselreg, \
	.vsel_mask = _vselmask, \
	.enable_reg = MA5150_REG_ENABLE, \
	.enable_mask = MA5150_CHEN_MASK(_id), \
	.active_discharge_reg = MA5150_REG_ACTDISCHG, \
	.active_discharge_mask = MA5150_CHAD_MASK(_id), \
}

static const struct regulator_desc ma5150_regulator_descs[] = {
	MA5150_REG_DESC(1, MA5150_CH1_MINUV, MA5150_CH1_STEPUV, MA5150_CH1_NUM_VOLTS, 0, NULL,
			MA5150_REG_CH1VID, MA5150_CH1VID_MASK, buck1, ma5150_of_map_mode),
	MA5150_REG_DESC(2, 0, 0, ARRAY_SIZE(buck2_volt_table), 0, buck2_volt_table,
			MA5150_REG_CH2VOFFS, MA5150_CH2VOFFS_MASK, buck2, ma5150_of_map_mode),
	MA5150_REG_DESC(3, 0, 0, 1, MA5150_CH3_FIXED_VOLT, NULL, 0, 0, buck34, ma5150_of_map_mode),
	MA5150_REG_DESC(4, 0, 0, 1, MA5150_CH4_FIXED_VOLT, NULL, 0, 0, buck34, ma5150_of_map_mode),
	MA5150_REG_DESC(5, 0, 0, 1, MA5150_CH5_FIXED_VOLT, NULL, 0, 0, ldo1, NULL)
};

static int ma5150_reboot_notifier(struct notifier_block *nb, unsigned long action, void *data)
{
	struct ma5150_data *md = container_of(nb, struct ma5150_data, reboot_notifier);
	int ret;

	if (action != SYS_HALT && action != SYS_POWER_OFF)
		return NOTIFY_DONE;

	/* Enable EXT_EN */
	ret = regmap_write(md->regmap, MA5150_REG_ENABLE, 0x6E);
	if (ret)
		dev_info(md->dev, "Failed to enable EXT_EN\n");

	/* Config SleepSel to suspend pin, the others are 0 */
	ret = regmap_write(md->regmap, MA5150_REG_SPMMODECTL, MA5150_SLEEPSEL_MASK);
	if (ret)
		dev_err(md->dev, "Failed to configure sleep mode\n");

	/* Config Sleep by sequence */
	ret = regmap_update_bits(md->regmap, MA5150_REG_SEQCFG, MA5150_SLEEPSEQ_MASK, 0);
	if (ret)
		dev_err(md->dev, "Failed to configure sleep by sequence\n");

	/* Apply sequence time slot setting */
	ret = regmap_raw_write(md->regmap, MA5150_REG_CH1TIMECFG, md->power_off_seq,
			       sizeof(md->power_off_seq));
	if (ret)
		dev_err(md->dev, "Failed to apply sequence\n");

	return NOTIFY_DONE;
}

static bool ma5150_is_accessible_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MA5150_REG_PGINFO ... MA5150_REG_EN1TIMECFG:
	case MA5150_REG_RSTDLYTIME ... MA5150_REG_CH2VOFFS:
	case MA5150_REG_FZCMODE:
		return true;
	}

	return false;
}

static const struct regmap_config ma5150_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = MA5150_REG_FZCMODE,

	.writeable_reg = ma5150_is_accessible_reg,
	.readable_reg = ma5150_is_accessible_reg,
};

static int ma5150_probe(struct i2c_client *i2c)
{
	struct ma5150_data *data;
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	int i, ret;

	data = devm_kzalloc(&i2c->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = &i2c->dev;

	data->regmap = devm_regmap_init_i2c(i2c, &ma5150_regmap_config);
	if (IS_ERR(data->regmap))
		return PTR_ERR(data->regmap);

	ret = device_property_read_u8_array(&i2c->dev, "mediatek,power-off-sequence",
					    data->power_off_seq, sizeof(data->power_off_seq));
	if (ret) {
		dev_warn(&i2c->dev, "No power-off-sequence specified\n");
		data->power_off_seq[0] = 4;
		data->power_off_seq[1] = 13;
		data->power_off_seq[2] = 0;
		data->power_off_seq[3] = 0;
		data->power_off_seq[4] = 9;
		data->power_off_seq[5] = 15;
	}

	config.dev = &i2c->dev;
	config.regmap = data->regmap;

	for (i = 0; i < ARRAY_SIZE(ma5150_regulator_descs); i++) {
		rdev = devm_regulator_register(&i2c->dev, ma5150_regulator_descs + i, &config);
		if (IS_ERR(rdev)) {
			dev_err(&i2c->dev, "Failed to register [%d] regulator\n", i);
			return PTR_ERR(rdev);
		}
	}

	data->reboot_notifier.notifier_call = ma5150_reboot_notifier;
	return devm_register_reboot_notifier(&i2c->dev, &data->reboot_notifier);
}

static const struct of_device_id __maybe_unused ma5150_of_device_table[] = {
	{ .compatible = "mediatek,ma5150", },
	{}
};
MODULE_DEVICE_TABLE(of, ma5150_of_device_table);

static struct i2c_driver ma5150_driver = {
	.driver = {
		.name = "mediatek,ma5150",
		.of_match_table = ma5150_of_device_table,
	},
	.probe_new = ma5150_probe,
};
module_i2c_driver(ma5150_driver);

MODULE_AUTHOR("ChiYuan Hwang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MA5150 Regulator Driver");
MODULE_LICENSE("GPL v2");
