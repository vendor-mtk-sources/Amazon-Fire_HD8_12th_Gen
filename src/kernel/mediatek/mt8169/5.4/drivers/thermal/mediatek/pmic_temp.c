// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <dt-bindings/iio/mt635x-auxadc.h>
#include <linux/bits.h>
#include <linux/delay.h>
#include <linux/iio/consumer.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/types.h>
#include "../thermal_hwmon.h"
#include <charger_class.h>

#if IS_ENABLED(CONFIG_AMAZON_SIGN_OF_LIFE)
#include <linux/amzn_sign_of_life.h>
#endif

/*=============================================================
 *Local variable definition
 *=============================================================
 */

struct pmic_tz_cali_data {
	int cali_factor;
	int slope1;
	int slope2;
	int intercept;
	int o_vts;
	struct iio_channel *iio_chan;
	char *iio_chan_name;
};

struct pmic_tz_data {
	int degc_cali;
	int adc_cali_en;
	int o_slope;
	int o_slope_sign;
	int id;
	int sensor_num;
	struct pmic_tz_cali_data *cali_data;
	int (*get_cali_data)(struct device *dev,
				struct pmic_tz_data *tz_data);
};

struct pmic_temp_info {
	struct device *dev;
	struct pmic_tz_data *efuse_data;
};

struct pmic_temp_tz {
	int tz_id;
	struct pmic_temp_info *pmic_tz_info;
};

#define PMIC_TEMP_TZ_NUM 3
static struct pmic_temp_tz *gpmic_tz[PMIC_TEMP_TZ_NUM];

/*=============================================================*/

static int pmic_raw_to_temp(struct pmic_tz_cali_data *cali_data, int tz_id,
						int val)
{
	int t_current;
	int y_curr = val;

	t_current = cali_data[tz_id].intercept +
		((cali_data[tz_id].slope1 * y_curr) / (cali_data[tz_id].slope2));

	return t_current;
}
static void pmic_get_temp_convert_params(struct pmic_temp_info *data)
{
	int vbe_t;
	int factor;
	int i = 0;
	struct pmic_temp_info *temp_info = data;
	struct pmic_tz_data *tz_data = temp_info->efuse_data;
	struct pmic_tz_cali_data *cali = tz_data->cali_data;

	for (i = 0; i < tz_data->sensor_num; i++) {

		factor = cali[i].cali_factor;

		cali[i].slope1 = (100 * 1000 * 10);	/* 1000 is for 0.001 degree */

		if (tz_data->o_slope_sign == 0)
			cali[i].slope2 = -(factor + tz_data->o_slope);
		else
			cali[i].slope2 = -(factor - tz_data->o_slope);

		vbe_t = (-1) * ((((cali[i].o_vts) * 1800)) / 4096) * 1000;


		if (tz_data->o_slope_sign == 0)
			cali[i].intercept = (vbe_t * 1000) / (-(factor + tz_data->o_slope * 10));
		/*0.001 degree */
		else
			cali[i].intercept = (vbe_t * 1000) / (-(factor - tz_data->o_slope * 10));
		/*0.001 degree */

		cali[i].intercept = cali[i].intercept + (tz_data->degc_cali * (1000 / 2));
		/* 1000 is for 0.1 degree */
	}
}

#ifdef CONFIG_THERMAL_SHUTDOWN_LAST_KMESG
static void last_kmsg_thermal_shutdown(const char* msg)
{
	int rc = 0;
	char *argv[] = {
		"/system_ext/bin/crashreport",
		"thermal_shutdown",
		NULL
	};

	pr_err("%s:%s start to save last kmsg\n", __func__, msg);
	/* UMH_WAIT_PROC UMH_WAIT_EXEC */
	rc = call_usermodehelper(argv[0], argv, NULL, UMH_WAIT_EXEC);
	pr_err("%s: save last kmsg finish\n", __func__);

	if (rc < 0)
		pr_err("call crashreport failed, rc = %d\n", rc);
	else
		msleep(6000);	/* 6000ms */
}
#else
static inline void last_kmsg_thermal_shutdown(const char* msg) {}
#endif

static int pmic_thermal_notify(struct thermal_zone_device *thermal,
			       int trip, enum thermal_trip_type type)
{
	int trip_temp = 0;

	if (!thermal) {
		pr_err("%s thermal:%p is NULL!\n", __func__, thermal);
		return -EINVAL;
	}

	if (type == THERMAL_TRIP_CRITICAL) {
		if (thermal->ops->get_trip_temp)
			thermal->ops->get_trip_temp(thermal, trip, &trip_temp);

		pr_err("[%s][%s]type:[%s] Thermal shutdown, "
			"current temp=%d, trip=%d, trip_temp=%d\n",
			__func__, dev_name(&thermal->device), thermal->type,
			thermal->temperature, trip, trip_temp);

#if IS_ENABLED(CONFIG_AMAZON_SIGN_OF_LIFE)
		life_cycle_set_thermal_shutdown_reason
			(THERMAL_SHUTDOWN_REASON_PMIC);
#endif

		last_kmsg_thermal_shutdown(dev_name(&thermal->device));
		set_shutdown_enable_dcap();
	}

	return 0;
}

static int pmic_get_temp(void *data, int *temp)
{
	int val = 0;
	int ret;
	struct pmic_temp_tz *pmic_tz = (struct pmic_temp_tz *)data;
	struct pmic_temp_info *temp_info = pmic_tz->pmic_tz_info;
	struct pmic_tz_data *tz_data = temp_info->efuse_data;
	struct pmic_tz_cali_data *cali_data = tz_data->cali_data;

	ret = iio_read_channel_processed(cali_data[pmic_tz->tz_id].iio_chan, &val);
	if (ret < 0) {
		pr_notice("pmic_chip_temp read fail, ret=%d\n", ret);
		*temp = THERMAL_TEMP_INVALID;
		dev_info(temp_info->dev, "temp = %d\n", *temp);
		return -EINVAL;
	}

	*temp = pmic_raw_to_temp(cali_data, pmic_tz->tz_id, val);

	return 0;
}

int get_pmic_thermal_temp(int id)
{
	int temp = 0;

	if (id >= PMIC_TEMP_TZ_NUM) {
		pr_err("%s Error id:%d\n", __func__, id);
		return -1;
	}
	if (gpmic_tz[id]) {
		pmic_get_temp(gpmic_tz[id], &temp);
	} else {
		pr_err("%s gpmic_tz[%d] is NULL!\n", __func__, id);
	}

	return temp;
}
EXPORT_SYMBOL_GPL(get_pmic_thermal_temp);

static const struct thermal_zone_of_device_ops pmic_temp_ops = {
	.get_temp = pmic_get_temp,
};
static int pmic_temp_parse_iio_channel(struct device *dev,
					struct pmic_temp_info *pmic_info)
{
	int ret;
	struct pmic_tz_data *tz_data = pmic_info->efuse_data;
	struct pmic_tz_cali_data *cali_data = tz_data->cali_data;
	int i = 0;

	for (i = 0; i < tz_data->sensor_num; i++) {
		cali_data[i].iio_chan = devm_iio_channel_get(dev, cali_data[i].iio_chan_name);
		ret = PTR_ERR_OR_ZERO(cali_data[i].iio_chan);
		if (ret) {
			if (ret != -EPROBE_DEFER)
				pr_info("pmic_chip_temp auxadc get fail, ret=%d\n", ret);
				return ret;
		}
	}
	return 0;
}

static int pmic_register_thermal_zones(struct pmic_temp_info *pmic_info)
{
	struct pmic_tz_data *tz_data = pmic_info->efuse_data;
	struct thermal_zone_device *tzdev;
	struct device *dev = pmic_info->dev;
	struct pmic_temp_tz *pmic_tz;
	int i, ret;

	for (i = 0; i < tz_data->sensor_num; i++) {
		pmic_tz = devm_kzalloc(dev, sizeof(*pmic_tz), GFP_KERNEL);
		if (!pmic_tz)
			return -ENOMEM;

		pmic_tz->tz_id = i;
		pmic_tz->pmic_tz_info = pmic_info;

		tzdev = devm_thermal_zone_of_sensor_register(dev, pmic_tz->tz_id,
				pmic_tz, &pmic_temp_ops);

		if (IS_ERR(tzdev)) {
			ret = PTR_ERR(tzdev);
			dev_info(dev,
				"Error: Failed to register pmic tz %d, ret = %d\n",
				pmic_tz->tz_id, ret);
			return ret;
		} else {
			tzdev->ops->notify = pmic_thermal_notify;
		}
		if (i < PMIC_TEMP_TZ_NUM)
			gpmic_tz[i] = pmic_tz;

		ret = thermal_add_hwmon_sysfs(tzdev);
		if (ret)
			dev_warn(dev, "Error: thermal_add_hwmon_sysfs fail\n");

	}
	return 0;
}

static int pmic_temp_probe(struct platform_device *pdev)
{
	struct pmic_temp_info *pmic_info;
	int ret;

	if (!pdev->dev.of_node) {
		dev_info(&pdev->dev, "Only DT based supported\n");
		return -ENODEV;
	}

	pmic_info = devm_kzalloc(&pdev->dev, sizeof(*pmic_info), GFP_KERNEL);
	if (!pmic_info)
		return -ENOMEM;

	pmic_info->dev = &pdev->dev;
	pmic_info->efuse_data = (struct pmic_tz_data *)
		of_device_get_match_data(&pdev->dev);
	ret = pmic_temp_parse_iio_channel(&pdev->dev, pmic_info);
	if (ret < 0)
		return ret;

	ret = pmic_info->efuse_data->get_cali_data(&pdev->dev, pmic_info->efuse_data);
	if (ret < 0)
		return ret;

	pmic_get_temp_convert_params(pmic_info);

	platform_set_drvdata(pdev, pmic_info);

	ret = pmic_register_thermal_zones(pmic_info);
	if (ret)
		return ret;
	return 0;
}


static int mt6359_get_cali_data(struct device *dev, struct pmic_tz_data *tz_data)
{
	size_t len = 0;
	unsigned short *efuse_buff;
	struct nvmem_cell *cell_1;
	int i = 0;
	struct pmic_tz_cali_data *cali_data = tz_data->cali_data;

	cell_1 = devm_nvmem_cell_get(dev, "e_data1");
	if (IS_ERR(cell_1)) {
		dev_info(dev, "Error: Failed to get nvmem cell %s\n",
			"e_data1");
		return PTR_ERR(cell_1);
	}
	efuse_buff = (unsigned short *)nvmem_cell_read(cell_1, &len);
	nvmem_cell_put(cell_1);

	if (IS_ERR(efuse_buff))
		return PTR_ERR(efuse_buff);
	if (len != 2 * (tz_data->sensor_num + 1)) {
		kfree(efuse_buff);
		return -EINVAL;
	}

	for (i = 0; i < tz_data->sensor_num; i++)
		cali_data[i].o_vts = efuse_buff[i+1] & GENMASK(12, 0);

	tz_data->adc_cali_en = ((efuse_buff[0] & BIT(14)) >> 14);
	if (tz_data->adc_cali_en == 0)
		goto out;
	tz_data->degc_cali = ((efuse_buff[0] & GENMASK(13, 8)) >> 8);
	tz_data->o_slope_sign = ((efuse_buff[0] & BIT(7)) >> 7);
	tz_data->o_slope = ((efuse_buff[0] & GENMASK(6, 1)) >> 1);
	tz_data->id = (efuse_buff[0] & BIT(0));

	if (tz_data->id == 0)
		tz_data->o_slope = 0;

	if (tz_data->degc_cali < 38 || tz_data->degc_cali > 60)
		tz_data->degc_cali = 50;
out:
	for (i = 0; i < tz_data->sensor_num; i++)
		dev_info(dev, "[pmic_debug] tz_id=%d, o_vts = 0x%x\n", i, cali_data[i].o_vts);
	dev_info(dev, "[pmic_debug] degc_cali= 0x%x\n", tz_data->degc_cali);
	dev_info(dev, "[pmic_debug] adc_cali_en        = 0x%x\n", tz_data->adc_cali_en);
	dev_info(dev, "[pmic_debug] o_slope        = 0x%x\n", tz_data->o_slope);
	dev_info(dev, "[pmic_debug] o_slope_sign        = 0x%x\n", tz_data->o_slope_sign);
	dev_info(dev, "[pmic_debug] id        = 0x%x\n", tz_data->id);
	kfree(efuse_buff);

	return 0;
}

static struct pmic_tz_cali_data mt6359_cali_data[] = {
	[0] = {
		.cali_factor = 1681,
		.slope1 = 0x0,
		.slope2 = 0x0,
		.intercept = 0x0,
		.o_vts = 1600,
		.iio_chan = NULL,
		.iio_chan_name = "pmic_chip_temp",
	},
	[1] = {
		.cali_factor = 1863,
		.slope1 = 0x0,
		.slope2 = 0x0,
		.intercept = 0x0,
		.o_vts = 1600,
		.iio_chan = NULL,
		.iio_chan_name = "pmic_buck1_temp",
	},
	[2] = {
		.cali_factor = 1863,
		.slope1 = 0x0,
		.slope2 = 0x0,
		.intercept = 0x0,
		.o_vts = 1600,
		.iio_chan = NULL,
		.iio_chan_name = "pmic_buck2_temp",
	},
	[3] = {
		.cali_factor = 1863,
		.slope1 = 0x0,
		.slope2 = 0x0,
		.intercept = 0x0,
		.o_vts = 1600,
		.iio_chan = NULL,
		.iio_chan_name = "pmic_buck3_temp",
	}
};

static struct pmic_tz_data mt6359_pmic_tz_data = {
	.degc_cali = 50,
	.adc_cali_en = 0,
	.o_slope = 0,
	.o_slope_sign = 0,
	.id = 0,
	.sensor_num = 4,
	.cali_data = mt6359_cali_data,
	.get_cali_data = mt6359_get_cali_data,
};

static int mt6358_get_cali_data(struct device *dev, struct pmic_tz_data *tz_data)
{
	size_t len = 0;
	unsigned short *efuse_buff;
	struct nvmem_cell *cell_1;
	int i = 0;
	struct pmic_tz_cali_data *cali_data = tz_data->cali_data;

	cell_1 = devm_nvmem_cell_get(dev, "e_data1");
	if (IS_ERR(cell_1)) {
		dev_err(dev, "Error: Failed to get nvmem cell %s\n",
			"e_data1");
		return PTR_ERR(cell_1);
	}

	efuse_buff = (unsigned short *)nvmem_cell_read(cell_1, &len);
	nvmem_cell_put(cell_1);
	if (IS_ERR(efuse_buff))
		return PTR_ERR(efuse_buff);

	if (len != 2 * (tz_data->sensor_num + 2)) {
		kfree(efuse_buff);
		return -EINVAL;
	}

	cali_data[0].o_vts = efuse_buff[1] & GENMASK(12, 0);
	cali_data[1].o_vts = efuse_buff[4] & GENMASK(12, 0);
	cali_data[2].o_vts = efuse_buff[5] & GENMASK(12, 0);
	cali_data[3].o_vts = (efuse_buff[3] & GENMASK(15, 5)) >> 5;

	tz_data->adc_cali_en = ((efuse_buff[0] & BIT(8)) >> 8);
	if (tz_data->adc_cali_en == 0)
		goto out;
	tz_data->degc_cali = (efuse_buff[0] & GENMASK(5, 0));
	tz_data->o_slope_sign = ((efuse_buff[2] & BIT(8)) >> 8);
	tz_data->o_slope = (efuse_buff[2] & GENMASK(5, 0));
	tz_data->id = ((efuse_buff[3] & BIT(4)) >> 4);

	if (tz_data->id == 0)
		tz_data->o_slope = 0;

	if (tz_data->degc_cali < 38 || tz_data->degc_cali > 60)
		tz_data->degc_cali = 53;
out:
	for (i = 0; i < tz_data->sensor_num; i++)
		dev_info(dev, "[pmic_debug] tz_id=%d, o_vts = 0x%x\n", i, cali_data[i].o_vts);
	dev_info(dev, "[pmic_debug] degc_cali= 0x%x\n", tz_data->degc_cali);
	dev_info(dev, "[pmic_debug] adc_cali_en        = 0x%x\n", tz_data->adc_cali_en);
	dev_info(dev, "[pmic_debug] o_slope        = 0x%x\n", tz_data->o_slope);
	dev_info(dev, "[pmic_debug] o_slope_sign        = 0x%x\n", tz_data->o_slope_sign);
	dev_info(dev, "[pmic_debug] id        = 0x%x\n", tz_data->id);
	kfree(efuse_buff);

	return 0;
}

static struct pmic_tz_cali_data mt6358_cali_data[] = {
	[0] = {
		.cali_factor = 1681,
		.slope1 = 0x0,
		.slope2 = 0x0,
		.intercept = 0x0,
		.o_vts = 1600,
		.iio_chan = NULL,
		.iio_chan_name = "pmic_chip_temp",
	},
	[1] = {
		.cali_factor = 1863,
		.slope1 = 0x0,
		.slope2 = 0x0,
		.intercept = 0x0,
		.o_vts = 1600,
		.iio_chan = NULL,
		.iio_chan_name = "pmic_buck1_temp",
	},
	[2] = {
		.cali_factor = 1863,
		.slope1 = 0x0,
		.slope2 = 0x0,
		.intercept = 0x0,
		.o_vts = 1600,
		.iio_chan = NULL,
		.iio_chan_name = "pmic_buck2_temp",
	},
	[3] = {
		.cali_factor = 1863,
		.slope1 = 0x0,
		.slope2 = 0x0,
		.intercept = 0x0,
		.o_vts = 1600,
		.iio_chan = NULL,
		.iio_chan_name = "pmic_buck3_temp",
	}
};

static struct pmic_tz_data mt6358_pmic_tz_data = {
	.degc_cali = 50,
	.adc_cali_en = 0,
	.o_slope = 0,
	.o_slope_sign = 0,
	.id = 0,
	.sensor_num = 4,
	.cali_data = mt6358_cali_data,
	.get_cali_data = mt6358_get_cali_data,
};

static int mt6357_get_cali_data(struct device *dev, struct pmic_tz_data *tz_data)
{
	size_t len = 0;
	unsigned short *efuse_buff;
	struct nvmem_cell *cell_1;
	int i = 0;
	struct pmic_tz_cali_data *cali_data = tz_data->cali_data;

	cell_1 = devm_nvmem_cell_get(dev, "e_data1");
	if (IS_ERR(cell_1)) {
		dev_info(dev, "Error: Failed to get nvmem cell %s\n",
			"e_data1");
		return PTR_ERR(cell_1);
	}

	efuse_buff = (unsigned short *)nvmem_cell_read(cell_1, &len);
	nvmem_cell_put(cell_1);
	if (IS_ERR(efuse_buff))
		return PTR_ERR(efuse_buff);

	if (len != 2 * (tz_data->sensor_num + 3)) {
		kfree(efuse_buff);
		return -EINVAL;
	}

	tz_data->adc_cali_en = ((efuse_buff[0] & BIT(8)) >> 8);
	if (tz_data->adc_cali_en == 0)
		goto out;

	cali_data[0].o_vts = efuse_buff[1] & GENMASK(12, 0);
	cali_data[1].o_vts = efuse_buff[4] & GENMASK(12, 0);
	cali_data[2].o_vts = efuse_buff[5] & GENMASK(12, 0);

	tz_data->degc_cali = (efuse_buff[0] & GENMASK(5, 0));
	tz_data->o_slope_sign = ((efuse_buff[2] & BIT(8)) >> 8);
	tz_data->o_slope = (efuse_buff[2] & GENMASK(5, 0));
	tz_data->id = ((efuse_buff[3] & BIT(4)) >> 4);

	if (tz_data->id == 0)
		tz_data->o_slope = 0;

	if (tz_data->degc_cali < 38 || tz_data->degc_cali > 60)
		tz_data->degc_cali = 53;
out:
	for (i = 0; i < tz_data->sensor_num; i++)
		dev_info(dev, "[pmic_debug] tz_id=%d, o_vts = 0x%x\n", i, cali_data[i].o_vts);
	dev_info(dev, "[pmic_debug] degc_cali= 0x%x\n", tz_data->degc_cali);
	dev_info(dev, "[pmic_debug] adc_cali_en        = 0x%x\n", tz_data->adc_cali_en);
	dev_info(dev, "[pmic_debug] o_slope        = 0x%x\n", tz_data->o_slope);
	dev_info(dev, "[pmic_debug] o_slope_sign        = 0x%x\n", tz_data->o_slope_sign);
	dev_info(dev, "[pmic_debug] id        = 0x%x\n", tz_data->id);
	kfree(efuse_buff);

	return 0;
}

static struct pmic_tz_cali_data mt6357_cali_data[] = {
	[0] = {
		.cali_factor = 1681,
		.slope1 = 0x0,
		.slope2 = 0x0,
		.intercept = 0x0,
		.o_vts = 1600,
		.iio_chan = NULL,
		.iio_chan_name = "pmic_chip_temp",
	},
	[1] = {
		.cali_factor = 1863,
		.slope1 = 0x0,
		.slope2 = 0x0,
		.intercept = 0x0,
		.o_vts = 1600,
		.iio_chan = NULL,
		.iio_chan_name = "pmic_buck1_temp",
	},
	[2] = {
		.cali_factor = 1863,
		.slope1 = 0x0,
		.slope2 = 0x0,
		.intercept = 0x0,
		.o_vts = 1600,
		.iio_chan = NULL,
		.iio_chan_name = "pmic_buck2_temp",
	},
};

static struct pmic_tz_data mt6357_pmic_tz_data = {
	.degc_cali = 50,
	.adc_cali_en = 0,
	.o_slope = 0,
	.o_slope_sign = 0,
	.id = 0,
	.sensor_num = 3,
	.cali_data = mt6357_cali_data,
	.get_cali_data = mt6357_get_cali_data,
};


/*==================================================
 * Support chips
 *==================================================
 */

static const struct of_device_id pmic_temp_of_match[] = {
	{
		.compatible = "mediatek,mt6359-pmic-temp",
		.data = (void *)&mt6359_pmic_tz_data,
	},
	{
		.compatible = "mediatek,mt6357-pmic-temp",
		.data = (void *)&mt6357_pmic_tz_data,
	},
	{
		.compatible = "mediatek,mt6358-pmic-temp",
		.data = (void *)&mt6358_pmic_tz_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, pmic_temp_of_match);

static struct platform_driver pmic_temp_driver = {
	.probe = pmic_temp_probe,
	.driver = {
		.name = "mtk-pmic-temp",
		.of_match_table = pmic_temp_of_match,
	},
};

module_platform_driver(pmic_temp_driver);

MODULE_AUTHOR("Henry Huang <henry.huang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek pmic temp sensor driver");
MODULE_LICENSE("GPL v2");
