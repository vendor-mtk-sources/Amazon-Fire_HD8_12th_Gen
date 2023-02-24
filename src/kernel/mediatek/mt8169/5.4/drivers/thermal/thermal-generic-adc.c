// SPDX-License-Identifier: GPL-2.0-only
/*
 * Generic ADC thermal driver
 *
 * Copyright (C) 2016 NVIDIA CORPORATION. All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 */
#include <linux/iio/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include "thermal_hwmon.h"
#include <charger_class.h>
#include <linux/delay.h>

#if IS_ENABLED(CONFIG_AMAZON_SIGN_OF_LIFE)
#include <linux/amzn_sign_of_life.h>
#endif

struct gadc_thermal_info {
	struct device *dev;
	struct thermal_zone_device *tz_dev;
	struct iio_channel *channel;
	s32 *lookup_table;
	int nlookup_table;
};

#define GADC_NTC_NUM 5
static struct gadc_thermal_info *gti_ntc[GADC_NTC_NUM];
static int gti_ntc_num = 0;

static int gadc_thermal_adc_to_temp(struct gadc_thermal_info *gti, int val)
{
	int temp, temp_hi, temp_lo, adc_hi, adc_lo;
	int i;

	if (!gti->lookup_table)
		return val;

	for (i = 0; i < gti->nlookup_table; i++) {
		if (val >= gti->lookup_table[2 * i + 1])
			break;
	}

	if (i == 0) {
		temp = gti->lookup_table[0];
	} else if (i >= gti->nlookup_table) {
		temp = gti->lookup_table[2 * (gti->nlookup_table - 1)];
	} else {
		adc_hi = gti->lookup_table[2 * i - 1];
		adc_lo = gti->lookup_table[2 * i + 1];

		temp_hi = gti->lookup_table[2 * i - 2];
		temp_lo = gti->lookup_table[2 * i];

		temp = temp_hi + mult_frac(temp_lo - temp_hi, val - adc_hi,
					   adc_lo - adc_hi);
	}

	return temp;
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

static int gadc_thermal_notify(struct thermal_zone_device *thermal,
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
			(THERMAL_SHUTDOWN_REASON_BTS);
#endif
		last_kmsg_thermal_shutdown(dev_name(&thermal->device));
		set_shutdown_enable_dcap();
	}

	return 0;
}

static int gadc_thermal_get_temp(void *data, int *temp)
{
	struct gadc_thermal_info *gti = data;
	int val;
	int ret;

	ret = iio_read_channel_processed(gti->channel, &val);
	if (ret < 0) {
		dev_err(gti->dev, "IIO channel read failed %d\n", ret);
		return ret;
	}
	*temp = gadc_thermal_adc_to_temp(gti, val);

	return 0;
}

int get_gadc_thermal_temp(const char* name)
{
	struct gadc_thermal_info *gti = NULL;
	int temp = 0;
	int i;

	for (i = 0; i < GADC_NTC_NUM; i++) {
		if(gti_ntc[i] &&
		!strncasecmp(name, gti_ntc[i]->tz_dev->type, THERMAL_NAME_LENGTH)) {
			gti = gti_ntc[i];
			break;
		}
	}
	if (gti) {
		gadc_thermal_get_temp(gti, &temp);
	} else {
		pr_err("%s name:%s get temp failed!\n", __func__, name);
	}

	return temp;
}
EXPORT_SYMBOL_GPL(get_gadc_thermal_temp);

static const struct thermal_zone_of_device_ops gadc_thermal_ops = {
	.get_temp = gadc_thermal_get_temp,
};

static int gadc_thermal_read_linear_lookup_table(struct device *dev,
						 struct gadc_thermal_info *gti)
{
	struct device_node *np = dev->of_node;
	int ntable;
	int ret;

	ntable = of_property_count_elems_of_size(np, "temperature-lookup-table",
						 sizeof(u32));
	if (ntable <= 0) {
		dev_notice(dev, "no lookup table, assuming DAC channel returns milliCelcius\n");
		return 0;
	}

	if (ntable % 2) {
		dev_err(dev, "Pair of temperature vs ADC read value missing\n");
		return -EINVAL;
	}

	gti->lookup_table = devm_kcalloc(dev,
					 ntable, sizeof(*gti->lookup_table),
					 GFP_KERNEL);
	if (!gti->lookup_table)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, "temperature-lookup-table",
					 (u32 *)gti->lookup_table, ntable);
	if (ret < 0) {
		dev_err(dev, "Failed to read temperature lookup table: %d\n",
			ret);
		return ret;
	}

	gti->nlookup_table = ntable / 2;

	return 0;
}

static int gadc_thermal_probe(struct platform_device *pdev)
{
	struct gadc_thermal_info *gti;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "Only DT based supported\n");
		return -ENODEV;
	}

	gti = devm_kzalloc(&pdev->dev, sizeof(*gti), GFP_KERNEL);
	if (!gti)
		return -ENOMEM;

	ret = gadc_thermal_read_linear_lookup_table(&pdev->dev, gti);
	if (ret < 0)
		return ret;

	gti->dev = &pdev->dev;
	platform_set_drvdata(pdev, gti);

	gti->channel = devm_iio_channel_get(&pdev->dev, "sensor-channel");
	if (IS_ERR(gti->channel)) {
		ret = PTR_ERR(gti->channel);
		dev_err(&pdev->dev, "IIO channel not found: %d\n", ret);
		return ret;
	}

	gti->tz_dev = devm_thermal_zone_of_sensor_register(&pdev->dev, 0, gti,
							   &gadc_thermal_ops);
	if (IS_ERR(gti->tz_dev)) {
		ret = PTR_ERR(gti->tz_dev);
		dev_err(&pdev->dev, "Thermal zone sensor register failed: %d\n",
			ret);
		return ret;
	} else {
		gti->tz_dev->ops->notify = gadc_thermal_notify;
	}

	if (gti_ntc_num < GADC_NTC_NUM)
		gti_ntc[gti_ntc_num++] = gti;

	ret = thermal_add_hwmon_sysfs(gti->tz_dev);
	if (ret)
		dev_warn(&pdev->dev, "Error: thermal_add_hwmon_sysfs fail\n");

	return 0;
}

static const struct of_device_id of_adc_thermal_match[] = {
	{ .compatible = "generic-adc-thermal", },
	{},
};
MODULE_DEVICE_TABLE(of, of_adc_thermal_match);

static struct platform_driver gadc_thermal_driver = {
	.driver = {
		.name = "generic-adc-thermal",
		.of_match_table = of_adc_thermal_match,
	},
	.probe = gadc_thermal_probe,
};

module_platform_driver(gadc_thermal_driver);

MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_DESCRIPTION("Generic ADC thermal driver using IIO framework with DT");
MODULE_LICENSE("GPL v2");
