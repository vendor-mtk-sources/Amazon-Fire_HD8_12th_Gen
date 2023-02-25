/************************************************************
 *
 * file: idtp922x_wireless_power.c
 *
 * Description: P922x Wireless Power Charger Driver
 *
 *------------------------------------------------------------
 *
 * Copyright (c) 2022, Integrated Device Technology Co., Ltd.
 * Copyright (C) 2022 Amazon.com Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *************************************************************/

#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/switch.h>
#include <linux/uaccess.h>
#include <asm/setup.h>
#include <linux/reboot.h>
#include <linux/rtc.h>
#include <charger_class.h>
#include <mtk_charger.h>
#include "idtp922x_wireless_power.h"
#include "P9221_amazon_v25.1.0.2_otp.h"
#include "P9221_amazon_v25.1.1.5_sram.h"
#include <linux/thermal.h>
#include "mtk_boot_common.h"
#include "thermal_core.h"

#if IS_ENABLED(CONFIG_AMAZON_SIGN_OF_LIFE)
#include <linux/amzn_sign_of_life.h>
#endif

#define WPC_COOLER_NAME  "wpc_bcct2"
#define WPC_CHARGER_NAME "Wireless"
/* The corresponding NTC temp is 0C */
#define DEFAULT_NTC_ADC 2741
#define THERMAL_SENSOR_NUM 2
#if IS_ENABLED(CONFIG_AMAZON_METRICS_LOG)
#include <linux/metricslog.h>
#define BATTERY_METRICS_BUFF_SIZE 512
static char metrics_buf[BATTERY_METRICS_BUFF_SIZE];

#define p922x_metrics_log(domain, fmt, ...)				\
	do {									\
	memset(metrics_buf, 0, BATTERY_METRICS_BUFF_SIZE);		\
	snprintf(metrics_buf, sizeof(metrics_buf), fmt, ##__VA_ARGS__);\
	log_to_metrics(ANDROID_LOG_INFO, domain, metrics_buf);	\
} while (0)
#else
static inline void p922x_metrics_log(void) {}
#endif

struct p922x_dev *idt;
struct thermal_zone_p922x_data {
	struct p922x_dev *tz_chip;
	int id;
};

static void p922x_fast_charge_enable(struct p922x_dev *chip, bool en);
static int p922x_enable_charge_flow(struct charger_device *chg_dev, bool en);
static bool p922x_get_pg_irq_status(struct p922x_dev *chip);
static int p922x_set_ovp_en(struct p922x_dev *chip, bool en);
static void p922x_init_adaptive_current_limit_work(struct p922x_dev *chip,
	int initial_delay);
static bool p922x_skip_adaptive_work(struct p922x_dev *chip);

static struct p922x_iout_cal_data cal_data = {
	.threshold = {
		{REG_CAL_THR0_ADDR, CAL_THR0_VAL},
		{REG_CAL_THR1_ADDR, CAL_THR1_VAL},
		{REG_CAL_THR2_ADDR, CAL_THR2_VAL},
		{REG_CAL_THR3_ADDR, CAL_THR3_VAL},
	},
	.gain = {
		{REG_CAL_GAIN0_ADDR, GAIN_DEFAULT_VAL},
		{REG_CAL_GAIN1_ADDR, GAIN_DEFAULT_VAL},
		{REG_CAL_GAIN2_ADDR, GAIN_DEFAULT_VAL},
		{REG_CAL_GAIN3_ADDR, GAIN_DEFAULT_VAL},
		{REG_CAL_GAIN4_ADDR, GAIN_DEFAULT_VAL},
	},
	.offset = {
		{REG_CAL_OFFSET0_ADDR, OFFSET_DEFAULT_VAL},
		{REG_CAL_OFFSET1_ADDR, OFFSET_DEFAULT_VAL},
		{REG_CAL_OFFSET2_ADDR, OFFSET_DEFAULT_VAL},
		{REG_CAL_OFFSET3_ADDR, OFFSET_DEFAULT_VAL},
		{REG_CAL_OFFSET4_ADDR, OFFSET_DEFAULT_VAL},
	},
};

#ifdef MODULE
static char __chg_cmdline[COMMAND_LINE_SIZE];
static char *chg_cmdline = __chg_cmdline;

static const char *p922x_chg_get_cmd(void)
{
	struct file *fd;
	mm_segment_t fs;
	loff_t pos = 0;

	if (__chg_cmdline[0] != 0)
		return chg_cmdline;

	fs = get_fs();
	set_fs(KERNEL_DS);
	fd = filp_open("/proc/cmdline", O_RDONLY, 0);
	if (IS_ERR(fd)) {
		chr_info("kedump: Unable to open /proc/cmdline (%ld)",
			PTR_ERR(fd));
		set_fs(fs);
		return chg_cmdline;
	}
	kernel_read(fd, (void *)chg_cmdline, COMMAND_LINE_SIZE, &pos);
	filp_close(fd, NULL);
	fd = NULL;
	set_fs(fs);

	return chg_cmdline;
}
#else
static const char *p922x_chg_get_cmd(void)
{
	return saved_command_line;
}
#endif

static bool p922x_get_wpc_support(void)
{
	char keyword[] = "androidboot.wpc.support=1";

	if (strstr(p922x_chg_get_cmd(), keyword))
		return true;

	return false;
}

#if IS_ENABLED(CONFIG_IDME)
static s8 castChar(s8 c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return (s8)255;
}

static void hexToBytes(s8 *inhex, s8 *outbytes)
{
	s8 *p;
	int len, i;

	len = strlen(inhex) / 2;

	for (i = 0, p = (uint8_t *) inhex; i < len; i++) {
		outbytes[i] = (castChar(*p) << 4) | castChar(*(p+1));
		p += 2;
	}
}

static int idme_get_wpc_iout_cal(s16 *data, uint8_t size)
{
	struct device_node *ap = NULL;
	char *wpc_iout_cal = NULL;
	int ret = 0;
	uint8_t i = 0;
	s8 *retdata = NULL;

	pr_debug("%s enter\n", __func__);
	retdata = kzalloc(IOUT_CAL_IDME_SIZE * sizeof(s16), GFP_KERNEL);
	if (!retdata) {
		pr_err("retdata is NULL!\n");
		return -ENOMEM;
	}

	ap = of_find_node_by_path(IDME_OF_WPCIOUTCAL);
	if (ap) {
		wpc_iout_cal = (char *)of_get_property(ap, "value", NULL);
		pr_notice("wpc_iout_cal %s\n", wpc_iout_cal);
	} else {
		pr_err("of_find_node_by_path failed\n");
		ret = -EINVAL;
		goto out;
	}

	if(wpc_iout_cal) {
		hexToBytes(wpc_iout_cal, retdata);
	} else {
		pr_err("wpc_iout_cal is NULL!\n");
		ret = -EINVAL;
		goto out;
	}

	pr_notice("retdata %02x %02x, %02x %02x, %02x %02x, %02x %02x,"
		"%02x %02x, %02x %02x, %02x %02x, %02x %02x, %02x %02x,"
		"%02x %02x, %02x %02x, %02x %02x, %02x %02x, %02x %02x\n",
		retdata[0], retdata[1], retdata[2], retdata[3],
		retdata[4], retdata[5], retdata[6], retdata[7],
		retdata[8], retdata[9], retdata[10], retdata[11],
		retdata[12], retdata[13], retdata[14], retdata[15],
		retdata[16], retdata[17], retdata[18], retdata[19],
		retdata[20], retdata[21], retdata[22], retdata[23],
		retdata[24], retdata[25], retdata[26], retdata[27]);

	if (size > IOUT_CAL_IDME_SIZE)
		size = IOUT_CAL_IDME_SIZE;
	for (i = 0; i < size; i++)
		data[i] = (s16)((retdata[i*2] & 0x00FF) | (retdata[i*2+1] << 8));

out:
	pr_debug("%s exit\n", __func__);
	kfree(retdata);

	return ret;
}
#endif

static int p922x_read(struct p922x_dev *data, u16 reg, u8 *val)
{
	unsigned int temp = 0;
	int rc = 0;
	struct p922x_dev *di = data;

	rc = regmap_read(di->regmap, reg, &temp);
	if (rc >= 0)
		*val = (u8)temp;

	return rc;
}

static int p922x_write(struct p922x_dev *data, u16 reg, u8 val)
{
	int rc = 0;
	struct p922x_dev *di = data;

	rc = regmap_write(di->regmap, reg, val);
	if (rc < 0) {
		dev_err(di->dev,
				"%s: error: %d, reg: %04x, val: %02x\n",
				__func__, rc, reg, val);
	}

	return rc;
}

static int p922x_read_buffer(struct p922x_dev *data, u16 reg, u8 *buf, u32 size)
{
	int ret = 0;
	u32 offset = 0, length = 0;
	struct p922x_dev *di = data;

	do {
		length = size / I2C_BYTES_MAX ? I2C_BYTES_MAX : size % I2C_BYTES_MAX;
		ret = regmap_bulk_read(di->regmap, reg + offset, buf + offset, length);
		if (ret) {
			dev_err(di->dev, "%s: Failed to read, ret: %d\n",
				__func__,  ret);
			return ret;
		}
		size -= length;
		offset += length;
	} while (size > 0);

	return ret;
}

static int p922x_write_buffer(struct p922x_dev *data, u16 reg, u8 *buf, u32 size)
{
	int ret = 0;
	u32 offset = 0, length = 0;
	struct p922x_dev *di = data;

	do {
		length = size / I2C_BYTES_MAX ? I2C_BYTES_MAX : size % I2C_BYTES_MAX;
		ret = regmap_bulk_write(di->regmap, reg + offset, buf + offset, length);
		if (ret) {
			dev_err(di->dev, "%s: Failed to write, ret: %d\n",
				__func__, ret);
			return ret;
		}
		size -= length;
		offset += length;
	} while (size > 0);

	return ret;
}

static u16 p922x_get_vout(struct p922x_dev *chip)
{
	u8 value = 0;
	u16 vout = 0;

	p922x_read(chip, REG_VOUT_SET, &value);
	/* vout = value * 0.1V + 3.5V */
	vout = value * 100 + 3500;

	return vout;
}

static int p922x_set_vout(struct p922x_dev *chip, int vout)
{
	int ret = 0;

	if ((vout >= SET_VOUT_MIN) && (vout <= SET_VOUT_MAX)) {
		dev_info(chip->dev, "%s: Set vout: %dmv\n", __func__, vout);
		ret = p922x_write(chip, REG_VOUT_SET, vout/100 - 35);
	} else {
		dev_err(chip->dev, "%s: Set vout parameter error!\n", __func__);
		ret = -EINVAL;
	}

	if (ret)
		dev_err(chip->dev, "%s: failed!\n", __func__);

	return ret;
}

static u16 p922x_get_iout_adc(struct p922x_dev *chip)
{
	u8 buf[2] = {0};
	u16 iout = 0;

	p922x_read_buffer(chip, REG_RX_LOUT, buf, 2);
	iout = buf[0] | (buf[1] << 8);
	dev_dbg(chip->dev, "%s: iout:%04x\n", __func__, iout);

	return iout;
}

static u16 p922x_get_iout_raw(struct p922x_dev *chip)
{
	u8 buf[2] = {0};
	u16 iout_raw = 0;
	int ret = 0;

	ret = p922x_read_buffer(chip, REG_IOUT_RAW, buf, 2);
	if (ret) {
		dev_err(chip->dev, "%s: read iout raw data error!\n", __func__);
		goto out;
	}

	iout_raw = buf[0] | (buf[1] << 8);
	dev_dbg(chip->dev, "%s: iout raw:%04x\n", __func__, iout_raw);

out:
	return iout_raw;
}

static u16 p922x_get_vout_adc(struct p922x_dev *chip)
{
	u8 buf[2] = {0};
	u32 vout = 0;

	p922x_read_buffer(chip, REG_ADC_VOUT, buf, 2);
	/* vout = val/4095*6*2.1 */
	vout = (buf[0] | (buf[1] << 8)) * 6 * 21 * 1000 / 40950;
	dev_dbg(chip->dev, "%s: vout:%d MV\n", __func__, vout);

	return vout;
}

static u16 p922x_get_vrect_adc(struct p922x_dev *chip)
{
	u8 buf[2] = {0};
	u32 vrect = 0;

	p922x_read_buffer(chip, REG_ADC_VRECT, buf, 2);
	/* vrect = val/4095*10*2.1 , val = REG_ADC_VRECT bit0-11 */
	buf[1] &= 0xf;
	vrect = (buf[0] | (buf[1] << 8)) * 10 * 21 * 1000 / 40950;
	dev_dbg(chip->dev, "%s: vrect:%d MV\n", __func__, vrect);

	return vrect;
}

static u16 p922x_get_prx(struct p922x_dev *chip)
{
	u8 buf[2] = {0};
	u16 power_rx = 0;

	p922x_read_buffer(chip, REG_PRX, buf, 2);
	power_rx = buf[0] | (buf[1] << 8);
	dev_dbg(chip->dev, "%s: power_rx:%d MW\n", __func__, power_rx);

	return power_rx;
}

static int p922x_get_temp_adc(struct p922x_dev *chip)
{
	u8 buf[2] = {0};
	int ret = 0;
	/* The default temp value is -273°C */
	int temp = P922X_DIE_TEMP_DEFAULT;

	if (p922x_get_pg_irq_status(chip))
		ret = p922x_read_buffer(chip, REG_ADC_TEMP, buf, 2);
	else
		goto out;

	if (ret) {
		dev_err(chip->dev, "%s: Failed to read temp: %d\n",
			__func__, ret);
		goto out;
	}

	/* temp = (val-1350)*83/444-273 */
	temp = ((buf[0] | (buf[1] << 8)) - 1350) * 83 / 444 - 273;

	dev_dbg(chip->dev, "%s: temp:%d degrees C\n", __func__, temp);

out:
	return temp;
}

static u8 p922x_get_tx_signal_strength(struct p922x_dev *chip)
{
	int ret = 0;
	u8 ss = 0;

	ret = p922x_read(chip, REG_SS, &ss);
	if (ret) {
		dev_err(chip->dev, "%s: Failed to read tx signal strength: %d\n",
			__func__, ret);
		goto out;
	}

	dev_info(chip->dev, "%s: tx signal strength:%d\n", __func__, ss);

out:
	return ss;
}

static int p922x_metrics_tx_signal_strength(struct p922x_dev *chip)
{
	u8 ss = 0;

	ss = p922x_get_tx_signal_strength(chip);

	if (ss)
		p922x_metrics_log("wpc", "wpc:def:tx_signal_strength=%d;CT;1:NR", ss);

	return 0;
}

static int p922x_metrics_abnormal_reconnection(struct p922x_dev *chip)
{
	s64 disconnect_duration = 0;

	disconnect_duration =
		ktime_to_ms(ktime_sub(chip->pg_time[1], chip->pg_time[0]));
	/* Judge abnormal reconnection time(ms) */
	dev_info(chip->dev, "%s: disconnect_duration=%d\n", __func__, disconnect_duration);
	if (disconnect_duration < 5000)
		p922x_metrics_log("wpc", "wpc:def:abnormal_reconnection_ms=%d;CT;1:NR",
			disconnect_duration);

	return 0;
}

static u16 p922x_get_ntc_adc(struct p922x_dev *chip)
{
	u8 buf[2] = {0};
	u16 ntc_adc = DEFAULT_NTC_ADC;
	int ret = 0;

	ret = p922x_read_buffer(chip, REG_ADC_NTC, buf, 2);
	if (ret) {
		dev_err(chip->dev, "%s: Failed to read ntc adc: %d\n",
			__func__, ret);
		goto out;
	}

	ntc_adc = buf[0] | (buf[1] << 8);

	if (ntc_adc == 0) {
		ntc_adc = DEFAULT_NTC_ADC;
		dev_dbg(chip->dev, "%s: ntc adc is not ready, use default value\n",
			__func__);
	}

	dev_dbg(chip->dev, "%s: ntc adc:%04x\n", __func__, ntc_adc);

out:
	return ntc_adc;
}

static signed int p922x_get_ntc_temp(struct p922x_dev *chip)
{
	int low = 0, mid = 0, high = 0;
	signed int ntc_temp = -200, ntc_volt = 0, ntc_adc = 0;

	high = ARRAY_SIZE(ntc_temperature_table) - 1;

	ntc_adc = p922x_get_ntc_adc(chip);

	/* Voltage=2.1*1000*ADC/4096 */
	ntc_volt = 2100 * ntc_adc / 4096;

	while (low <= high) {
		mid = (low + high) / 2;
		if (ntc_volt > ntc_temperature_table[mid].voltage)
			high = mid - 1;
		else if (ntc_volt < ntc_temperature_table[mid].voltage)
			low = mid + 1;
		else
			break;
	}

	/*
	 * If a matching value is not found from the table,
	 * we will return a similar voltage value.
	 */
	ntc_temp = ntc_temperature_table[mid].temperature;
	dev_dbg(chip->dev, "%s: ntc adc: %d, vol: %d, temp: %d\n",
			__func__, ntc_adc, ntc_volt, ntc_temp);

	return ntc_temp;
}

static void p922x_power_switch(struct p922x_dev *chip, ushort mv)
{
	p922x_write_buffer(chip, REG_FC_VOLTAGE, (u8 *)&mv, 2);
	p922x_write(chip, REG_COMMAND, VSWITCH);
	dev_info(chip->dev, "%s: %dmv\n", __func__, mv);
}

static ssize_t p922x_fwver(struct p922x_dev *chip)
{
	u8 id[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	u8 ver[4] = { 0xff, 0xff, 0xff, 0xff };

	p922x_read_buffer(chip, REG_CHIP_ID, id, 8);
	p922x_read_buffer(chip, REG_CHIP_REV, ver, 4);

	pr_info("%s: ChipID: %04x\nFWVer:%02x.%02x.%02x.%02x\n",
			__func__, id[4] | (id[0] << 8), ver[3], ver[2], ver[1], ver[0]);

	return 0;
}

static ssize_t p922x_version_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	u8 id[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	u8 ver[4] = { 0xff, 0xff, 0xff, 0xff };
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	mutex_lock(&chip->sys_lock);
	p922x_read_buffer(chip, REG_CHIP_ID, id, 8);
	p922x_read_buffer(chip, REG_CHIP_REV, ver, 4);
	mutex_unlock(&chip->sys_lock);

	return sprintf(buffer, "%02x.%02x.%02x.%02x\n",
			ver[3], ver[2], ver[1], ver[0]);
}

static ssize_t p922x_id_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	u8 id[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	mutex_lock(&chip->sys_lock);
	p922x_read_buffer(chip, REG_CHIP_ID, id, 8);
	mutex_unlock(&chip->sys_lock);

	return sprintf(buffer, "%04x\n", id[4] | (id[0] << 8));
}

static ssize_t p922x_id_authen_status_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	return scnprintf(buffer, PAGE_SIZE, "%d\n", chip->tx_id_authen_status);
}

static ssize_t p922x_dev_authen_status_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	return scnprintf(buffer, PAGE_SIZE, "%d\n", chip->tx_dev_authen_status);
}

static ssize_t p922x_is_hv_adapter_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	return sprintf(buffer, "%d\n", chip->is_hv_adapter);
}

static ssize_t p922x_tx_adapter_type_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	return sprintf(buffer, "%d\n", chip->tx_adapter_type);
}

static ssize_t p922x_power_rx_mw_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	int power_rx_mw = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	mutex_lock(&chip->sys_lock);
	power_rx_mw = p922x_get_prx(chip);
	mutex_unlock(&chip->sys_lock);

	return sprintf(buffer, "%d\n", power_rx_mw);
}

static ssize_t p922x_vout_adc_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	int vout = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	mutex_lock(&chip->sys_lock);
	vout = p922x_get_vout_adc(chip);
	mutex_unlock(&chip->sys_lock);

	return sprintf(buffer, "%d\n", vout);
}

static ssize_t p922x_vrect_adc_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	int vrect = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	mutex_lock(&chip->sys_lock);
	vrect = p922x_get_vrect_adc(chip);
	mutex_unlock(&chip->sys_lock);

	return sprintf(buffer, "%d\n", vrect);
}

static ssize_t p922x_vout_set_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	int vout = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	mutex_lock(&chip->sys_lock);
	vout = p922x_get_vout(chip);
	mutex_unlock(&chip->sys_lock);

	return sprintf(buffer, "%d\n", vout);
}

static ssize_t p922x_vout_set_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int ret = 0, vout = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	mutex_lock(&chip->sys_lock);
	ret = kstrtoint(buf, 10, &vout);
	if (ret < 0) {
		dev_err(chip->dev, "%s: kstrtoint failed! ret:%d\n", __func__, ret);
		goto out;
	}
	p922x_set_vout(chip, vout);

out:
	mutex_unlock(&chip->sys_lock);

	return count;
}

static ssize_t p922x_iout_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	u16 iout = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	mutex_lock(&chip->sys_lock);
	iout = p922x_get_iout_adc(chip);
	mutex_unlock(&chip->sys_lock);

	return sprintf(buffer, "%d\n", iout);
}

static ssize_t p922x_power_switch_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);
	int wpc_power = 0;
	int chr_type = 0;
	int ret = 0;
	union power_supply_propval propval;

	struct power_supply *chg_psy = power_supply_get_by_name("mt6370_pmu_charger");

	mutex_lock(&chip->sys_lock);

	ret = power_supply_get_property(chg_psy,
				POWER_SUPPLY_PROP_TYPE, &propval);
	if (ret < 0) {
		dev_err(chip->dev, "%s: get psy type failed, ret = %d\n",
				__func__, ret);
		mutex_unlock(&chip->sys_lock);
		return ret;
	}
	chr_type = propval.intval;

	if (chr_type == POWER_SUPPLY_TYPE_WIRELESS_10W)
		wpc_power = 10;
	if (chr_type == POWER_SUPPLY_TYPE_WIRELESS_5W)
		wpc_power = 5;
	if (chr_type == POWER_SUPPLY_TYPE_WIRELESS)
		wpc_power = 1;
	mutex_unlock(&chip->sys_lock);

	return sprintf(buffer, "%d\n", wpc_power);
}

static ssize_t p922x_power_switch_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int ret = 0, power = 0;
	bool fast_switch = false;
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	mutex_lock(&chip->irq_lock);
	ret = kstrtoint(buf, 10, &power);
	if (ret < 0) {
		dev_err(chip->dev, "%s: kstrtoint failed! ret:%d\n",
			__func__, ret);
		goto out;
	}

	if (power == 5) {
		fast_switch = false;
		chip->force_switch = true;
	} else if (power == 10) {
		fast_switch = true;
		chip->force_switch = true;
	} else if (power == 0) {
		/* disable force power switch */
		chip->force_switch = false;
	} else {
		dev_err(chip->dev, "%s: invalid parameter\n", __func__);
		return -EINVAL;
	}

	if (chip->tx_authen_complete != true) {
		dev_info(chip->dev, "%s: tx authen not completed\n", __func__);
		goto out;
	}
	p922x_fast_charge_enable(chip, fast_switch);

out:
	mutex_unlock(&chip->irq_lock);

	return count;
}

static ssize_t p922x_regs_dump_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	u16 reg = 0x00;
	u8 val = 0;
	ssize_t len = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	mutex_lock(&chip->sys_lock);
	for (reg = 0x00; reg <= REG_DUMP_MAX; reg++) {
		p922x_read(chip, reg, &val);
		len += snprintf(buffer+len, PAGE_SIZE-len,
			"reg:0x%02x=0x%02x\n", reg, val);
	}
	mutex_unlock(&chip->sys_lock);

	return len;
}

static ssize_t p922x_reg_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	u8 buf[REG_DUMP_MAX] = {0};
	int ret = 0;
	int i = 0;
	int len = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	if (!p922x_get_pg_irq_status(chip))
		return -ENODEV;
	ret = p922x_read_buffer(chip, chip->reg.addr, buf, chip->reg.size);
	if (ret) {
		dev_err(chip->dev, "%s: Failed to read reg: %d\n",
			__func__,  ret);
		return ret;
	}
	for (i = 0; i < chip->reg.size; i++)
		len += scnprintf(buffer + len, PAGE_SIZE - len, "addr:0x%04x = 0x%02x\n",
			chip->reg.addr + i, buf[i]);

	return len;
}

static ssize_t p922x_reg_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int ret = 0;
	int i = 0;
	u8 regs_data[REG_DUMP_MAX] = {0};
	char *tmp_data = NULL;
	char *reg_data = NULL;
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	if ((chip->reg.size + chip->reg.addr) > REG_DUMP_MAX ||
		!chip->reg.size) {
		dev_err(chip->dev, "%s: invalid parameters\n", __func__);
		return -EINVAL;
	}
	if (!p922x_get_pg_irq_status(chip))
		return -ENODEV;
	tmp_data = kzalloc(strlen(buf) + 1, GFP_KERNEL);
	if (!tmp_data)
		return -ENOMEM;
	strncpy(tmp_data, buf, strlen(buf));
	while (tmp_data && i < chip->reg.size) {
		reg_data = strsep(&tmp_data, " ");
		if (*reg_data) {
			ret = kstrtou8(reg_data, 0, &regs_data[i]);
			if (ret)
				break;
			i++;
		}
	}

	if (i != chip->reg.size || ret) {
		ret = -EINVAL;
		goto out;
	}

	ret = p922x_write_buffer(chip, chip->reg.addr, regs_data, chip->reg.size);
	if (ret) {
		dev_err(chip->dev,
			"%s: Failed to write reg: %d\n", __func__,  ret);
		goto out;
	}
	ret = count;

out:
	kfree(tmp_data);

	return ret;
}

static ssize_t p922x_addr_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	return scnprintf(buffer, PAGE_SIZE, "addr:0x%04x size:%d\n",
			chip->reg.addr, chip->reg.size);
}

static ssize_t p922x_addr_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	unsigned int data[2] = {0};
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	if (sscanf(buf, "%x %x", &data[0], &data[1]) != 2)
		return -EINVAL;
	chip->reg.addr = data[0];
	chip->reg.size = data[1];

	return count;
}

static ssize_t p922x_over_reason_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	return scnprintf(buffer, PAGE_SIZE, "%u\n", chip->over_reason);
}

static ssize_t p922x_fod_regs_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	u16 reg = 0x00;
	u8 val = 0;
	ssize_t len = 0;
	u16 idx_coe;
	int ret = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);
	struct p922x_fodcoeftype *fod_coe;

	len += scnprintf(buffer+len, PAGE_SIZE-len, "P922X:\n");
	for (reg = REG_FOD_COEF_ADDR; reg <= REG_FOD_DUMP_MAX; reg++) {
		ret = p922x_read(chip, reg, &val);
		if (ret) {
			dev_err(idt->dev, "%s: Failed to read fod register, ret: %d\n",
					__func__, ret);
			return ret;
		}

		len += scnprintf(buffer+len, PAGE_SIZE-len,
			"FOD REG[0x%02x]=0x%02x\n", reg, val);
	}

	fod_coe = chip->bpp_5w_fod;
	len += scnprintf(buffer+len, PAGE_SIZE-len, "BPP:\n");
	for (idx_coe = 0; idx_coe < FOD_COEF_ARRY_LENGTH; idx_coe++)
		len += scnprintf(buffer+len, PAGE_SIZE-len,
					"FOD%d:0x%02x%02x\n", idx_coe,
					fod_coe[idx_coe].offs,
					fod_coe[idx_coe].gain);

	fod_coe = chip->bpp_10w_fod;
	len += scnprintf(buffer+len, PAGE_SIZE-len, "BPP Plus:\n");
	for (idx_coe = 0; idx_coe < FOD_COEF_ARRY_LENGTH; idx_coe++)
		len += scnprintf(buffer+len, PAGE_SIZE-len,
					"FOD%d:0x%02x%02x\n", idx_coe,
					fod_coe[idx_coe].offs,
					fod_coe[idx_coe].gain);

	return len;
}

static ssize_t p922x_fod_regs_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int cnt = 0, ret = 0;
	u16 idx_coe;
	enum fod_type type = TYPE_UNKNOWN;
	u16 fod[FOD_COEF_PARAM_LENGTH] = { 0 };
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);
	struct p922x_fodcoeftype *fod_coe;

	cnt = sscanf(buf, "%u %x %x %x %x %x %x %x %x",
			&type,
			&fod[0], &fod[1], &fod[2], &fod[3],
			&fod[4], &fod[5], &fod[6], &fod[7]);
	if (cnt != (FOD_COEF_ARRY_LENGTH + 1)) {
		return -EINVAL;
	}

	switch (type) {
	case TYPE_BPP:
		fod_coe = chip->bpp_5w_fod;
		break;
	case TYPE_BPP_PLUS:
		fod_coe = chip->bpp_10w_fod;
		break;
	default:
		return -EINVAL;
	}

	mutex_lock(&chip->fod_lock);

	for (idx_coe = 0; idx_coe < FOD_COEF_ARRY_LENGTH; idx_coe++) {
		fod_coe[idx_coe].gain = fod[idx_coe] & 0xFF;
		fod_coe[idx_coe].offs = (fod[idx_coe] >> 8) & 0xFF;
		dev_info(chip->dev, "%s: 0x%x 0x%x 0x%x\n", __func__,
			fod[idx_coe], fod_coe[idx_coe].offs, fod_coe[idx_coe].gain);

		ret = p922x_write_buffer(chip,
					REG_FOD_COEF_ADDR + idx_coe * 2,
					(u8 *)&fod[idx_coe], 2);
		if (ret) {
			dev_err(chip->dev, "%s: Failed to write fod data: %d\n",
				__func__, ret);
			mutex_unlock(&chip->fod_lock);
			return ret;
		}
	}

	mutex_unlock(&chip->fod_lock);

	return count;
}

static ssize_t p922x_dock_state_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	int state = 0;

	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	state = switch_get_state(&chip->dock_state)
		== TYPE_UNDOCKED ? 0 : 1;

	return scnprintf(buffer, PAGE_SIZE, "%d\n", state);
}

static ssize_t p922x_temp_adc_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	int temp;
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	temp = p922x_get_temp_adc(chip);

	return scnprintf(buffer, PAGE_SIZE, "%d\n", temp);
}

static ssize_t p922x_tx_signal_strength_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	u8 ss;
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	ss = p922x_get_tx_signal_strength(chip);

	return scnprintf(buffer, PAGE_SIZE, "%d\n", ss);
}

static ssize_t fwver(void)
{
	u8 id[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	u8 ver[4] = { 0xff, 0xff, 0xff, 0xff };

	p922x_read_buffer(idt, REG_CHIP_ID, id, 8);
	p922x_read_buffer(idt, REG_CHIP_REV, ver, 4);

	pr_info("IDT ChipID: %04x\nFWVer:%02x.%02x.%02x.%02x\n",
			id[4] | (id[0] << 8), ver[3], ver[2], ver[1], ver[0]);

	return 0;
}

static int program_bootloader(struct p922x_dev *di)
{
	int i, rc = 0;
	int len;
	len = sizeof(bootloader);

	for (i = 0; i < len; i++) {
		rc = p922x_write(di, 0x1c00 + i, bootloader[i]);
		if (rc)
			return rc;
	}

	return 0;
}

static int check_fwver(void)
{
	u16 chip_id = 0xffff;
	u16 fw_ver = 0xffff;
	u8 id[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	u8 ver[4] = { 0xff, 0xff, 0xff, 0xff };
	int i;

	p922x_read_buffer(idt, REG_CHIP_ID, id, 8);
	p922x_read_buffer(idt, REG_CHIP_REV, ver, 4);

	pr_info("IDT ChipID: %04x\nFWVer:%02x.%02x.%02x.%02x\n",
			id[4] | (id[0] << 8), ver[3], ver[2], ver[1], ver[0]);

	chip_id = id[4] | (id[0] << 8);
	if (chip_id != 0x9220) {
		dev_err(idt->dev, "ic not ready\n");
		return false;
	}

	fw_ver = ver[0] | (ver[1] << 8);
	for (i = 0; i < ARRAY_SIZE(fw_ver_list); i++) {
		if (fw_ver == fw_ver_list[i]) {
			dev_info(idt->dev, "firmware version has been included!\n");
			break;
		}
	}

	if (i < (ARRAY_SIZE(fw_ver_list) - 1)) {
		dev_warn(idt->dev, "current fw ver: %04x\n", fw_ver);
		return false;
	}

	return true;
}

static int program_fw(struct p922x_dev *di, u16 destAddr, u8 *src, u32 size)
{
	int i, j;
	u8 data = 0;

	/*
	 * === Step-1 ===
	 * Transfer 9220 boot loader code "OTPBootloader" to 9220 SRAM
	 * - Setup 9220 registers before transferring the boot loader code
	 * - Transfer the boot loader code to 9220 SRAM
	 * - Reset 9220 => 9220 M0 runs the boot loader
	 */
	p922x_read(di, 0x5870, &data);
	pr_emerg(KERN_EMERG "0x5870 %s:%d :%02x\n", __func__, __LINE__, data);
	p922x_read(di, 0x5874, &data);
	pr_emerg(KERN_EMERG "0x5874 %s:%d :%02x\n", __func__, __LINE__, data);
	/* configure the system */
	if (p922x_write(di, 0x3000, 0x5a))
		return false;
	/* write key */
	if (p922x_write(di, 0x3040, 0x10))
		return false;        /* halt M0 execution */
	if (program_bootloader(di))
		return false;
	if (p922x_write(di, 0x3048, 0x80))
		return false;        /* map RAM to OTP */

	/* ignoreNAK */
	p922x_write(di, 0x3040, 0x80);        /* reset chip and run the bootloader */
	mdelay(100);
	pr_emerg(KERN_EMERG "%s:%d\n", __func__, __LINE__);
	/*
	 * === Step-2 ===
	 * Program OTP image data to 9220 OTP memory
	 */
	for (i = destAddr; i < destAddr + size; i += 128) {        /* program pages of 128 bytes */
		/* Build a packet */

		char sBuf[136];        /* 136=8+128 --- 8-byte header plus 128-byte data */
		u16 StartAddr = (u16)i;
		u16 CheckSum = StartAddr;
		u16 CodeLength = 128;
		int retry_cnt = 0;

		memset(sBuf, 0, 136);

		/* (1) Copy the 128 bytes of the OTP image data to the packet data buffer */
		memcpy(sBuf + 8, src, 128);    /* Copy 128 bytes from srcData (starting at i+srcOffs) */
		src += 128;
		/*
		 * to sBuf (starting at 8)
		 * srcData     --- source array
		 * i + srcOffs     --- start index in source array
		 * sBuf         --- destination array
		 * 8         --- start index in destination array
		 * 128         --- elements to copy
		 */

		/* (2) Calculate the packet checksum of the 128-byte data, StartAddr, and CodeLength */
		for (j = 127; j >= 0; j--) {        /* find the 1st non zero value byte from the end of the sBuf[] buffer */
			if (sBuf[j + 8] != 0)
				break;
			else
				CodeLength--;
		}
		if (CodeLength == 0)
			continue;            /* skip programming if nothing to program */

		for (; j >= 0; j--)
			CheckSum += sBuf[j + 8];    /* add the nonzero values */
		CheckSum += CodeLength;        /* finish calculation of the check sum */

		/* (3) Fill up StartAddr, CodeLength, CheckSum of the current packet. */
		memcpy(sBuf + 2, &StartAddr, 2);
		memcpy(sBuf + 4, &CodeLength, 2);
		memcpy(sBuf + 6, &CheckSum, 2);

		/* Send the current packet to 9220 SRAM via I2C */

		/* read status is guaranteed to be != 1 at this point */
		for (j = 0; j < CodeLength + 8; j++) {
			if (p922x_write(di, 0x400 + j, sBuf[j])) {
				pr_emerg("ERROR: on writing to OTP buffer");
				return false;
			}
		}

		/*
		 * Write 1 to the Status in the SRAM. This informs the 9220 to start programming the new packet
		 * from SRAM to OTP memory
		 */
		if (p922x_write(di, 0x400, 1)) {
			pr_emerg("ERROR: on OTP buffer validation");
			return false;
		}

		/*
		 * Wait for 9220 bootloader to complete programming the current packet image data from SRAM to the OTP.
		 * The boot loader will update the Status in the SRAM as follows:
		 *     Status:
		 *     "0" - reset value (from AP)
		 *     "1" - buffer validated / busy (from AP)
		 *     "2" - finish "OK" (from the boot loader)
		 *     "4" - programming error (from the boot loader)
		 *     "8" - wrong check sum (from the boot loader)
		 *     "16"- programming not possible (try to write "0" to bit location already programmed to "1")
		 *         (from the boot loader)
		 *        DateTime startT = DateTime.Now;
		 */
		do {
			mdelay(100);
			p922x_read(di, 0x400, sBuf);
			if (sBuf[0] == 1)
				pr_err("ERROR: Programming OTP buffer status sBuf:%02x i:%d\n", sBuf[0], i);

			if (retry_cnt++ > 5)
				break;
		} while (sBuf[0] == 1); /* check if OTP programming finishes "OK" */

		if (sBuf[0] != 2) {        /* not OK */
			pr_err("ERROR: buffer write to OTP returned status:%d :%s\n", sBuf[0], "X4");
			return false;
		} else {
			pr_err(KERN_ERR "Program OTP 0x%04x\n", i);
		}
	}

	/*
	 * === Step-3 ===
	 * Restore system (Need to reset or power cycle 9220 to run the OTP code)
	 */
	if (p922x_write(di, 0x3000, 0x5a))
		return false; /* write key */

	if (p922x_write(di, 0x3048, 0x00))
		return false; /* remove code remapping */

	return true;
}

static ssize_t chip_prog_fw_show(struct device *dev,
		struct device_attribute *attr, char *buffer)
{
	int len = 0;
	disable_irq_nosync(idt->pg_num);
	disable_irq_nosync(idt->int_num);
	mutex_lock(&idt->lock);
	if (!check_fwver()) {
		len = scnprintf(buffer, PAGE_SIZE, "IDTP922x check firmware version failed!\n");
		goto out;
	}
	if (!program_fw(idt, 0x0000, idtp9220_rx_fw, sizeof(idtp9220_rx_fw))) {
		len = scnprintf(buffer, PAGE_SIZE, "IDTP922x program firmware failed!\n");
		goto out;
	}
	fwver();
	len = scnprintf(buffer, PAGE_SIZE, "IDTP922x program firmware successfully\n");

out:
	mutex_unlock(&idt->lock);
	enable_irq(idt->pg_num);
	enable_irq(idt->int_num);
	return len;
}

#define READ_BUF_LENGTH 255
static bool sram_compare_fw(struct p922x_dev *di, u8 *src, u32 size)
{
	int ret = 0, i = 0;
	u8 read_buf[READ_BUF_LENGTH] = {0};
	u32 length = 0, offset = 0;

	do {
		length = size / READ_BUF_LENGTH ? READ_BUF_LENGTH : size % READ_BUF_LENGTH;
		ret = p922x_read_buffer(di, 0x600 + offset, read_buf, length);
		if (ret) {
			dev_err(di->dev, "%s: Failed to read, ret: %d\n",
				__func__, ret);
			return false;
		}

		for (i = 0; i < length; i++) {
			if (*(src + offset + i) != read_buf[i]) {
				dev_err(di->dev, "%s: on compare SRAM buffer addr[0x%x]\n",
					__func__, 0x600 + offset + i);

				return false;
			}
		}

		memset(read_buf, 0, READ_BUF_LENGTH);
		size -= length;
		offset += length;
	} while (size > 0);

	return true;
}

#define SRAM_PROGRAM_RETRY 3
static int sram_program_fw(struct p922x_dev *di, u16 srcOffset, u8 *src, u32 size)
{
	int ret = 0, retry_cnt = 0;
	u8 val = 0;

	/* Program and compare firmware */
	do {
		ret = p922x_write_buffer(di, 0x600, src + srcOffset, size);
		if (ret) {
			dev_err(di->dev, "%s: Failed to write reg: %d\n",
				__func__,  ret);
			return false;
		}

		dev_info(di->dev, "%s: program SRAM firmware successfully\n", __func__);

		if (sram_compare_fw(di, src, size)) {
			dev_info(di->dev, "%s: compare SRAM firmware successfully\n",
				__func__);
			break;
		}

		dev_info(di->dev, "%s: compare SRAM firemware failed, retry = %d\n",
			__func__, retry_cnt);
	} while (retry_cnt++ < SRAM_PROGRAM_RETRY);

	if (retry_cnt > SRAM_PROGRAM_RETRY)
		return false;

	/* Check whether IC is ready */
	retry_cnt = 0;
	do {
		if (p922x_write(di, 0x4c, 0x5a)) {
			dev_err(di->dev, "%s: on write reg[0x4c] to SRAM buffer\n", __func__);
			return false;
		}

		if (p922x_write(di, 0x4e, 0x40)) {
			dev_err(di->dev, "%s: on write reg[0x4e] to SRAM buffer\n", __func__);
			return false;
		}

		msleep(20);

		ret = p922x_read(di, 0x4d, &val);
		if (ret) {
			dev_err(di->dev, "%s: on read to SRAM buffer\n", __func__);
			return false;
		}

		if (val == 0x41)
			break;

		dev_info(di->dev, "%s: Reg[0x4d] = 0x%x, expected value is 0x41, retry = %d\n",
			__func__, val, retry_cnt);
	} while (retry_cnt++ < SRAM_PROGRAM_RETRY);

	if (retry_cnt > SRAM_PROGRAM_RETRY)
		return false;

	di->is_sram_updated = true;

	return true;
}

static ssize_t sram_prog_fw_show(struct device *dev,
		struct device_attribute *attr, char *buffer)
{
	mutex_lock(&idt->lock);
	if (!check_fwver()) {
		mutex_unlock(&idt->lock);
		return sprintf(buffer, "IDTP922x check firmware version failed!\n");
	}
	if (!sram_program_fw(idt, 0x0000, idt_sram_firmware, ARRAY_SIZE(idt_sram_firmware))) {
		mutex_unlock(&idt->lock);
		return sprintf(buffer, "IDTP922x program sram firmware failed!\n");
	}
	fwver();
	mutex_unlock(&idt->lock);

	return sprintf(buffer, "IDTP922x program sram fw successfully!\n");
}

static ssize_t p922x_ntc_temp_show(struct device *dev,
		struct device_attribute *attr, char *buffer)
{
	signed int ntc_temp = -200;
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	mutex_lock(&chip->sys_lock);
	ntc_temp = p922x_get_ntc_temp(chip);
	mutex_unlock(&chip->sys_lock);

	return scnprintf(buffer, PAGE_SIZE, "%d\n", ntc_temp);
}

static int p922x_get_iout_cal_data(struct p922x_dev *chip)
{
	int ret = 0, i = 0;
	s16 idmedata[IOUT_CAL_IDME_SIZE] = {0};

#if IS_ENABLED(CONFIG_IDME)
	ret = idme_get_wpc_iout_cal(idmedata, IOUT_CAL_IDME_SIZE);
	if (ret) {
		dev_info(chip->dev,"%s: Get iout cal data fail\n", __func__);
		goto out;
	}
#else
	dev_info(chip->dev,"%s: IDME is not ready, skip iout calibration\n", __func__);
	ret = -EINVAL;
	goto out;
#endif

	for (i = 0; i < IOUT_CAL_AREA_MAX; i++) {
		chip->iout_cal_data->gain[i].val = idmedata[i * 2];
		chip->iout_cal_data->offset[i].val = idmedata[i * 2 + 1];
		dev_info(chip->dev, "%s: IDME to read area[%d], gain = 0x%04x, offset = 0x%04x\n",
			__func__, i, (u16)chip->iout_cal_data->gain[i].val,
			(u16)chip->iout_cal_data->offset[i].val);
	}

	for (i = 0; i < IOUT_CAL_THR_MAX; i++) {
		chip->iout_cal_data->threshold[i].val = idmedata[IOUT_CAL_AREA_MAX * 2 + i];
		dev_info(chip->dev, "%s: IDME to read threshold[%d] = 0x%04x\n",
			__func__, i, (u16)chip->iout_cal_data->threshold[i].val);
	}

	chip->is_cal_data_ready = true;
	dev_info(chip->dev, "%s: Get iout calibration data successfully!\n", __func__);

out:
	return ret;
}

static int p922x_mask_iout_cal_data(void)
{
	int ret = 0, i = 0;
	u16 mask_val = 0;

	for (i = 0; i < IOUT_CAL_THR_MAX; i++) {
		/* if threshold is equal to 0, calibration data will not be applied */
		ret = p922x_write_buffer(idt,  idt->iout_cal_data->threshold[i].addr,
					(u8 *)&mask_val, 2);
		if (ret) {
			dev_err(idt->dev, "%s: Failed to write area[%d]\n",
				__func__, i);
			return ret;
		}
	}

	dev_info(idt->dev, "%s: Mask iout calibration data successfully!\n", __func__);

	return ret;
}

static int p922x_update_iout_cal_data(void)
{
	int ret = 0, i = 0;

	if (!idt->is_cal_data_ready || !idt->is_sram_updated) {
		dev_err(idt->dev, "%s: P992X is not ready, cal_data_ready=%d, sram_updated=%d\n",
			__func__, idt->is_cal_data_ready, idt->is_sram_updated);
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < IOUT_CAL_AREA_MAX; i++) {
		ret = p922x_write_buffer(idt, idt->iout_cal_data->gain[i].addr,
					(u8 *)&idt->iout_cal_data->gain[i].val, 2);
		if (ret) {
			dev_err(idt->dev, "%s: Failed to write area[%d] cal gain: %d, ret: %d\n",
				__func__, i, idt->iout_cal_data->gain[i].val, ret);
			goto error;
		}

		ret = p922x_write_buffer(idt, idt->iout_cal_data->offset[i].addr,
					(u8 *)&idt->iout_cal_data->offset[i].val, 2);
		if (ret) {
			dev_err(idt->dev, "%s: Failed to write area[%d] cal offset: %d, ret: %d\n",
				__func__, i,  idt->iout_cal_data->offset[i].val, ret);
			goto error;
		}
	}

	for (i = 0; i < IOUT_CAL_THR_MAX; i++) {
		ret = p922x_write_buffer(idt, idt->iout_cal_data->threshold[i].addr,
					(u8 *)&idt->iout_cal_data->threshold[i].val, 2);
		if (ret) {
			dev_err(idt->dev, "%s: Failed to write area[%d] threshold: %d, ret: %d\n",
				__func__, i, idt->iout_cal_data->threshold[i].val, ret);
			goto error;
		}
	}
	dev_info(idt->dev, "%s: Update iout calibration data successfully!\n", __func__);

out:
	return ret;

error:
	/* If some errors exist, we will try to set p922x not to use incomplete calibration data */
	ret = p922x_mask_iout_cal_data();
	if (ret)
		dev_err(idt->dev, "%s: mask iout calibration data failed! ret: %d\n",
			__func__, ret);

	return ret;
}

static ssize_t p922x_iout_cal_data_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int ret = 0, val = 0;

	ret = kstrtoint(buf, 10, &val);
	if (ret < 0) {
		dev_err(idt->dev, "%s: kstrtoint failed! ret:%d\n",
			__func__, ret);
		return ret;
	}

	if (val == 0) {
		ret = p922x_mask_iout_cal_data();
		if (ret) {
			dev_err(idt->dev, "%s: mask iout calibration data failed! ret: %d\n",
				__func__, ret);
			return ret;
		}
	} else if (val == 1) {
		ret = p922x_get_iout_cal_data(idt);
		if (ret) {
			dev_err(idt->dev, "%s: get iout calibration data failed! ret: %d\n",
				__func__, ret);
			return ret;
		}
		ret = p922x_update_iout_cal_data();
		if (ret) {
			dev_err(idt->dev, "%s: update iout calibration data failed! ret: %d\n",
				__func__, ret);
			return ret;
		}
	} else {
		dev_err(idt->dev, "%s: invalid parameter: %d\n",
			__func__, val);
		return -EINVAL;
	}

	return count;
}

static ssize_t p922x_iout_cal_data_show(struct device *dev,
		struct device_attribute *attr, char *buffer)
{
	int off = 0, i = 0., ret = 0;
	u16 cal_gain = 0, cal_offs = 0, cal_thr = 0;
	u8 buf[2] = {0};

	off += scnprintf(buffer + off, PAGE_SIZE - off, "idme dump:\n");
	for (i = 0; i < IOUT_CAL_AREA_MAX; i++) {
		off += scnprintf(buffer + off, PAGE_SIZE - off,
			"area[%d], gain = 0x%04x, offset = 0x%04x\n",
			i, (u16)idt->iout_cal_data->gain[i].val,
			(u16)idt->iout_cal_data->offset[i].val);
	}

	for (i = 0; i < IOUT_CAL_THR_MAX; i++) {
		if (i < IOUT_CAL_THR_MAX)
		off += scnprintf(buffer + off, PAGE_SIZE - off, "threshold[%d] = 0x%04x\n",
			i, idt->iout_cal_data->threshold[i].val);
	}

	off += scnprintf(buffer + off, PAGE_SIZE - off, "p9221 dump:\n");
	for (i = 0; i < IOUT_CAL_AREA_MAX; i++) {
		ret = p922x_read_buffer(idt, idt->iout_cal_data->gain[i].addr, buf, 2);
		if (ret) {
			dev_err(idt->dev, "%s: Failed to read area[%d] cal gain, ret: %d\n",
				__func__, i, ret);
			return ret;
		}
		cal_gain = buf[0] | (buf[1] << 8);

		ret = p922x_read_buffer(idt, idt->iout_cal_data->offset[i].addr, buf, 2);
		if (ret) {
			dev_err(idt->dev, "%s: Failed to read area[%d] cal offset, ret: %d\n",
				__func__, i, ret);
			return ret;
		}
		cal_offs = buf[0] | (buf[1] << 8);

		off += scnprintf(buffer + off, PAGE_SIZE - off,
			"area[%d], gain = 0x%04x, offset = 0x%04x\n",
			i, cal_gain, cal_offs);
	}

	for (i = 0; i < IOUT_CAL_THR_MAX; i++) {
		ret = p922x_read_buffer(idt, idt->iout_cal_data->threshold[i].addr, buf, 2);
		if (ret) {
			dev_err(idt->dev, "%s: Failed to read threshold[%d], ret: %d\n",
				__func__, i, ret);
			return ret;
		}
		cal_thr = buf[0] | (buf[1] << 8);

		off += scnprintf(buffer + off, PAGE_SIZE - off, "threshold[%d] = 0x%04x\n",
			i, cal_thr);
	}

	return off;
}

static ssize_t p922x_iout_raw_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	return scnprintf(buffer, PAGE_SIZE, "%d\n", p922x_get_iout_raw(idt));
}

static ssize_t p922x_irq_stat_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);
	u16 irq_stat = 0;
	u8 buf[2] = {0};
	int ret = 0;

	ret = p922x_read_buffer(chip, REG_STATUS, buf, 2);
	if (ret) {
		dev_err(idt->dev, "%s: Failed to read irq status, ret: %d\n",
				__func__, ret);
		return ret;
	}
	irq_stat = buf[0] | (buf[1] << 8);

	return scnprintf(buffer, PAGE_SIZE, "0x%04x\n", irq_stat);
}

static ssize_t p922x_step_load_en_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	return scnprintf(buffer, PAGE_SIZE, "%d\n", idt->step_load_en);
}

static ssize_t p922x_step_load_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int ret = 0, val = 0;

	mutex_lock(&idt->lock);
	ret = kstrtoint(buf, 10, &val);
	if (ret < 0) {
		dev_err(idt->dev, "%s: kstrtoint failed! ret:%d\n",
			__func__, ret);
		goto out;
	}

	if (val == 0)
		idt->step_load_en = false;
	else if (val == 1)
		idt->step_load_en = true;
	else
		dev_err(idt->dev, "%s: invalid parameter: %d\n",
			__func__, val);

out:
	mutex_unlock(&idt->lock);

	return count;
}

static ssize_t p922x_force_iout_cal_data_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int ret = 0, i = 0, size = 0;
	s8 *retdata = NULL;
	char iout_cal_data[IOUT_CAL_CHAR_SIZE] = {0};

	dev_info(idt->dev, "%s: buf=%s", __func__, buf);
	size = strlen(buf) - 1;
	if (size != IOUT_CAL_CHAR_SIZE) {
		dev_err(idt->dev, "%s: parameter size: %d\n", __func__, size);
		return -EINVAL;
	}

	retdata = kzalloc(IOUT_CAL_IDME_SIZE * sizeof(s16), GFP_KERNEL);
	if (!retdata) {
		dev_err(idt->dev, "%s: retdata is NULL!\n", __func__);
		return -ENOMEM;
	}

	if (!strncpy(iout_cal_data, buf, size)) {
		dev_err(idt->dev, "%s: failed strncpy!\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	hexToBytes(iout_cal_data, retdata);

	dev_info(idt->dev, "%s: retdata %02x %02x, %02x %02x, %02x %02x, %02x %02x,"
		"%02x %02x, %02x %02x, %02x %02x, %02x %02x, %02x %02x,"
		"%02x %02x, %02x %02x, %02x %02x, %02x %02x, %02x %02x\n", __func__,
		retdata[0], retdata[1], retdata[2], retdata[3],
		retdata[4], retdata[5], retdata[6], retdata[7],
		retdata[8], retdata[9], retdata[10], retdata[11],
		retdata[12], retdata[13], retdata[14], retdata[15],
		retdata[16], retdata[17], retdata[18], retdata[19],
		retdata[20], retdata[21], retdata[22], retdata[23],
		retdata[24], retdata[25], retdata[26], retdata[27]);


	for (i = 0; i < IOUT_CAL_AREA_MAX; i++) {
		idt->iout_cal_data->gain[i].val =
			(s16)((retdata[i*4] & 0x00FF) | (retdata[i*4+1] << 8));
		idt->iout_cal_data->offset[i].val =
			(s16)((retdata[i*4+2] & 0x00FF) | (retdata[i*4+3] << 8));
		dev_info(idt->dev, "%s: read area[%d], gain = 0x%04x, offset = 0x%04x\n",
			__func__, i, (u16)idt->iout_cal_data->gain[i].val,
			(u16)idt->iout_cal_data->offset[i].val);
	}

	for (i = 0; i < IOUT_CAL_THR_MAX; i++) {
		idt->iout_cal_data->threshold[i].val =
			(s16)((retdata[IOUT_CAL_AREA_MAX*4+i*2] & 0x00FF) |
			(retdata[IOUT_CAL_AREA_MAX*4+i*2+1] << 8));
		dev_info(idt->dev, "%s: read threshold[%d] = 0x%04x\n",
			__func__, i, (u16)idt->iout_cal_data->threshold[i].val);
	}

	ret = p922x_update_iout_cal_data();
	if (ret) {
		dev_err(idt->dev, "%s: update iout calibration data failed! ret: %d\n",
			__func__, ret);
		goto out;
	}
	kfree(retdata);

	return count;
out:
	kfree(retdata);

	return ret;
}

static ssize_t p922x_force_iout_cal_data_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	int off = 0, i = 0., ret = 0;
	u16 cal_gain = 0, cal_offs = 0, cal_thr = 0;
	u8 buf[2] = {0};

	off += scnprintf(buffer + off, PAGE_SIZE - off, "cal data dump:\n");
	for (i = 0; i < IOUT_CAL_AREA_MAX; i++) {
		off += scnprintf(buffer + off, PAGE_SIZE - off,
			"area[%d], gain = 0x%04x, offset = 0x%04x\n",
			i, (u16)idt->iout_cal_data->gain[i].val,
			(u16)idt->iout_cal_data->offset[i].val);
	}

	for (i = 0; i < IOUT_CAL_THR_MAX; i++) {
		if (i < IOUT_CAL_THR_MAX)
		off += scnprintf(buffer + off, PAGE_SIZE - off, "threshold[%d] = 0x%04x\n",
			i, idt->iout_cal_data->threshold[i].val);
	}

	off += scnprintf(buffer + off, PAGE_SIZE - off, "p9221 dump:\n");
	for (i = 0; i < IOUT_CAL_AREA_MAX; i++) {
		ret = p922x_read_buffer(idt, idt->iout_cal_data->gain[i].addr, buf, 2);
		if (ret) {
			dev_err(idt->dev, "%s: Failed to read area[%d] cal gain, ret: %d\n",
				__func__, i, ret);
			return ret;
		}
		cal_gain = buf[0] | (buf[1] << 8);

		ret = p922x_read_buffer(idt, idt->iout_cal_data->offset[i].addr, buf, 2);
		if (ret) {
			dev_err(idt->dev, "%s: Failed to read area[%d] cal offset, ret: %d\n",
				__func__, i, ret);
			return ret;
		}
		cal_offs = buf[0] | (buf[1] << 8);

		off += scnprintf(buffer + off, PAGE_SIZE - off,
			"area[%d], gain = 0x%04x, offset = 0x%04x\n",
			i, cal_gain, cal_offs);
	}

	for (i = 0; i < IOUT_CAL_THR_MAX; i++) {
		ret = p922x_read_buffer(idt, idt->iout_cal_data->threshold[i].addr, buf, 2);
		if (ret) {
			dev_err(idt->dev, "%s: Failed to read threshold[%d], ret: %d\n",
				__func__, i, ret);
			return ret;
		}
		cal_thr = buf[0] | (buf[1] << 8);

		off += scnprintf(buffer + off, PAGE_SIZE - off, "threshold[%d] = 0x%04x\n",
			i, cal_thr);
	}

	return off;
}

static DEVICE_ATTR(version, 0444, p922x_version_show, NULL);
static DEVICE_ATTR(id, 0444, p922x_id_show, NULL);
static DEVICE_ATTR(id_authen_status, 0444, p922x_id_authen_status_show, NULL);
static DEVICE_ATTR(dev_authen_status, 0444, p922x_dev_authen_status_show, NULL);
static DEVICE_ATTR(is_hv_adapter, 0444, p922x_is_hv_adapter_show, NULL);
static DEVICE_ATTR(tx_adapter_type, 0444, p922x_tx_adapter_type_show, NULL);
static DEVICE_ATTR(power_rx_mw, 0444, p922x_power_rx_mw_show, NULL);
static DEVICE_ATTR(vout_adc, 0444, p922x_vout_adc_show, NULL);
static DEVICE_ATTR(vrect_adc, 0444, p922x_vrect_adc_show, NULL);
static DEVICE_ATTR(vout_set, 0644, p922x_vout_set_show,
	p922x_vout_set_store);
static DEVICE_ATTR(iout_adc, 0444, p922x_iout_show, NULL);
static DEVICE_ATTR(power_switch, 0644, p922x_power_switch_show,
	p922x_power_switch_store);
static DEVICE_ATTR(registers_dump, 0444, p922x_regs_dump_show, NULL);
static DEVICE_ATTR(reg, 0644, p922x_reg_show, p922x_reg_store);
static DEVICE_ATTR(addr, 0644, p922x_addr_show, p922x_addr_store);
static DEVICE_ATTR(over_reason, 0444, p922x_over_reason_show, NULL);
static DEVICE_ATTR(fod_regs, 0644, p922x_fod_regs_show, p922x_fod_regs_store);
static DEVICE_ATTR(dock_state, 0444, p922x_dock_state_show, NULL);
static DEVICE_ATTR(temp_adc, 0444, p922x_temp_adc_show, NULL);
static DEVICE_ATTR(tx_signal_strength, 0444, p922x_tx_signal_strength_show, NULL);
static DEVICE_ATTR(chip_prog_fw, 0444, chip_prog_fw_show, NULL);
static DEVICE_ATTR(sram_prog_fw, 0444, sram_prog_fw_show, NULL);
static DEVICE_ATTR(ntc_temp, 0444, p922x_ntc_temp_show, NULL);
static DEVICE_ATTR(iout_cal_data, 0644, p922x_iout_cal_data_show, p922x_iout_cal_data_store);
static DEVICE_ATTR(iout_raw, 0444, p922x_iout_raw_show, NULL);
static DEVICE_ATTR(irq_stat, 0444, p922x_irq_stat_show, NULL);
static DEVICE_ATTR(step_load_en, 0644, p922x_step_load_en_show, p922x_step_load_en_store);
static DEVICE_ATTR(force_iout_cal_data, 0644, p922x_force_iout_cal_data_show,
	p922x_force_iout_cal_data_store);

static struct attribute *p922x_sysfs_attrs[] = {
	&dev_attr_version.attr,
	&dev_attr_id.attr,
	&dev_attr_id_authen_status.attr,
	&dev_attr_dev_authen_status.attr,
	&dev_attr_is_hv_adapter.attr,
	&dev_attr_tx_adapter_type.attr,
	&dev_attr_power_rx_mw.attr,
	&dev_attr_vout_adc.attr,
	&dev_attr_vrect_adc.attr,
	&dev_attr_vout_set.attr,
	&dev_attr_iout_adc.attr,
	&dev_attr_power_switch.attr,
	&dev_attr_registers_dump.attr,
	&dev_attr_reg.attr,
	&dev_attr_addr.attr,
	&dev_attr_over_reason.attr,
	&dev_attr_fod_regs.attr,
	&dev_attr_dock_state.attr,
	&dev_attr_temp_adc.attr,
	&dev_attr_tx_signal_strength.attr,
	&dev_attr_chip_prog_fw.attr,
	&dev_attr_sram_prog_fw.attr,
	&dev_attr_ntc_temp.attr,
	&dev_attr_iout_cal_data.attr,
	&dev_attr_iout_raw.attr,
	&dev_attr_irq_stat.attr,
	&dev_attr_step_load_en.attr,
	&dev_attr_force_iout_cal_data.attr,
	NULL,
};

static const struct attribute_group p922x_sysfs_group_attrs = {
	.attrs = p922x_sysfs_attrs,
};

static const struct of_device_id match_table[] = {
	{.compatible = "IDT,idt_wireless_power",},
	{},
};

static const struct i2c_device_id p922x_dev_id[] = {
	{"idt_wireless_power", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, p922x_dev_id);

/* first step: define regmap_config */
static const struct regmap_config p922x_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0xFFFF,
};

/*
 * The header implicitly provides the size of the message
 * contained in the Packet. The number of bytes in a message
 * is calculated from the value contained in the header of
 * the Packet.
 * Header  Message Size  Comment
 * 0x00...0x1F  1 + (Header - 0) / 32    1 * 32 messages(size 1)
 * 0x20...0x7F  2 + (Header - 32) / 16   6 * 16 messages(size 2...7)
 * 0x80...0xDF  8 + (Header - 128) / 8   12 * 8 messages(size 8...19)
 * 0xE0...0xFF  20 + (Header - 224) / 4  8 * 4 messages(size 20...27)
 */
static int p922x_extract_packet_size(u8 hdr)
{
	if (hdr < 0x20)
		return 1;
	if (hdr < 0x80)
		return (2 + ((hdr - 0x20) >> 4));
	if (hdr < 0xe0)
		return (8 + ((hdr - 0x80) >> 3));

	return (20 + ((hdr - 0xe0) >> 2));
}

static void p922x_write_fod(struct p922x_dev *chip, int chr_type)
{
	int ret;
	u8 *fod_data = NULL;
	u8 fod_read[FOD_COEF_PARAM_LENGTH];

	if (chr_type == POWER_SUPPLY_TYPE_WIRELESS_10W &&
		chip->bpp_10w_fod_num == FOD_COEF_PARAM_LENGTH)
		fod_data = (u8 *)(chip->bpp_10w_fod);
	else if ((chr_type == POWER_SUPPLY_TYPE_WIRELESS_5W ||
		chr_type == POWER_SUPPLY_TYPE_WIRELESS) &&
		chip->bpp_5w_fod_num == FOD_COEF_PARAM_LENGTH)
		fod_data = (u8 *)(chip->bpp_5w_fod);

	if (fod_data == NULL)
		goto no_fod_data;

	/*
	 * Manual power switch or automatic power switch may call this function
	 * at the same time, so add fod mutex lock to prevent concurrent access.
	 */
	mutex_lock(&chip->fod_lock);
	dev_info(chip->dev, "%s: chr_type: %d, writing bpp fod.\n", __func__, chr_type);
	ret = p922x_write_buffer(chip, REG_FOD_COEF_ADDR, fod_data, FOD_COEF_PARAM_LENGTH);
	if (ret) {
		dev_err(chip->dev, "%s: Failed to write fod data: %d\n",
			__func__, ret);
		goto out;
	}

	ret = p922x_read_buffer(chip, REG_FOD_COEF_ADDR, fod_read, FOD_COEF_PARAM_LENGTH);
	if (ret) {
		dev_err(chip->dev, "%s: Failed to read fod data: %d\n",
			__func__, ret);
		goto out;
	}

	if (memcmp(fod_data, fod_read, FOD_COEF_PARAM_LENGTH) == 0)
		goto out;

	dev_info(chip->dev, "%s: compare error, chr_type:%d, fod read:%d %d, %d %d, %d %d, %d %d, %d %d, %d %d, %d %d, %d %d",
		__func__, chr_type, fod_read[0], fod_read[1],
		fod_read[2], fod_read[3], fod_read[4], fod_read[5],
		fod_read[6], fod_read[7], fod_read[8], fod_read[9],
		fod_read[10], fod_read[11], fod_read[12], fod_read[13],
		fod_read[14], fod_read[15]);

out:
	mutex_unlock(&chip->fod_lock);
	return;
no_fod_data:
	dev_warn(chip->dev, "%s: Fod data not set.\n", __func__);
}

static void p922x_device_auth_req(struct p922x_dev *chip)
{
	int ret;

	/* set device authentication req */
	ret = p922x_write(chip, REG_COMMAND, SEND_DEVICE_AUTH);
	if (ret)
		dev_err(chip->dev, "%s: Failed to write command reg: %d\n",
			__func__, ret);
}

static void p922x_get_tx_adapter_type(struct p922x_dev *chip)
{
	u8 header = 0;
	int length;
	u8 data_list[PACKET_SIZE_MAX] = { 0 };
	int ret = 0;

	ret = p922x_read(chip, REG_BCHEADER_ADDR, &header);
	if (ret) {
		dev_err(chip->dev, "%s: Failed to read bcheader addr: %d\n",
			__func__, ret);
		return;
	}

	length = p922x_extract_packet_size(header);
	ret = p922x_read_buffer(chip, REG_BCDATA_ADDR, data_list, length);
	if (ret) {
		dev_err(chip->dev, "%s: Failed to read bcdata addr: %d\n",
			__func__, ret);
		return;
	}

	dev_info(chip->dev, "%s: TX adapter type: %d\n", __func__, data_list[0]);
	chip->tx_adapter_type = data_list[0];

	if ((data_list[0] == DOCK_ADAPTER_QC20) || (data_list[0] == DOCK_ADAPTER_QC30)
		|| (data_list[0] == DOCK_ADAPTER_PD))
		chip->is_hv_adapter = true;
}

static void p922x_sendpkt(struct p922x_dev *chip, struct propkt_type *pkt)
{
	/* include header by +1 */
	int length = p922x_extract_packet_size(pkt->header) + 1;
	int ret = 0;

	/*  write data into proprietary packet buffer */
	ret = p922x_write_buffer(chip, REG_PROPPKT_ADDR, (u8 *)pkt, length);
	if (ret) {
		dev_err(chip->dev,
			"%s: Failed to write proprietary packet buffer: %d\n", __func__, ret);
		return;
	}

	/* send proprietary packet */
	ret = p922x_write(chip, REG_COMMAND, SENDPROPP);
	if (ret)
		dev_err(chip->dev,
		"%s: Failed to write command reg: %d\n", __func__, ret);
}

static void p922x_detect_tx_adapter_type(struct p922x_dev *chip)
{
	struct propkt_type propkt;

	propkt.header = PROPRIETARY18;
	propkt.cmd = BC_ADAPTER_TYPE;

	dev_info(chip->dev, "%s: start\n", __func__);

	p922x_sendpkt(chip, &propkt);
}

static int p922x_send_eop(struct p922x_dev *chip, u8 reason)
{
	int ret;

	dev_info(chip->dev,
			"%s: Send EOP reason = %d\n", __func__, reason);
	ret = p922x_write(chip, REG_EPT, reason);
	if (ret) {
		dev_err(chip->dev,
			"%s: Failed to write EPT: %d\n", __func__, ret);
		goto out;
	}
	ret = p922x_write(chip, REG_COMMAND, SENDEOP);
	if (ret) {
		dev_err(chip->dev,
			"%s: Failed to send EOP: %d\n", __func__, ret);
		goto out;
	}

out:
	return ret;
}

static void p922x_over_handle(struct p922x_dev *chip, u16 irq_src)
{
	u8 reason = 0;
	int ret;

	dev_err(chip->dev,
			"%s: Received OVER INT: %02x\n", __func__, irq_src);
	if (irq_src & P922X_INT_OV_TEMP)
		reason = EOP_OVER_TEMP;
	else if (irq_src & P922X_INT_OV_VOLT)
		reason = EOP_OVER_VOLT;
	else
		reason = EOP_OVER_CURRENT;

	chip->over_reason = reason;
	ret = p922x_send_eop(chip, reason);
	if (ret)
		dev_err(chip->dev,
			"%s: Failed to send EOP %d: %d\n", __func__, reason, ret);
}

static void p922x_set_cm_cap_enable(struct p922x_dev *chip, bool en)
{

	int ret = 0;

	dev_info(chip->dev, "%s: %s\n", __func__, en ? "true" : "false");

	if (chip->cm_cap_en == en) {
		dev_info(chip->dev, "%s: is_enabled status same as %d\n",
			__func__, en);
		return;
	}

	chip->cm_cap_en = en;
	if (en)
		ret = p922x_write(chip, REG_CM_CAP_EN_ADDR, EN_CM_CAP);
	else
		ret = p922x_write(chip, REG_CM_CAP_EN_ADDR, DIS_CM_CAP);
	if (ret)
		dev_err(chip->dev, "%s: Failed to enable cm cap: %d\n",
			__func__, ret);

}

static void p922x_set_tx_led_mode(struct p922x_dev *chip, enum led_mode mode)
{
	int ret = 0;

	ret = p922x_write(chip, REG_CHG_STATUS, mode);
	if (ret) {
		dev_err(chip->dev, "%s: Failed to write charge status: %d\n",
			__func__, ret);
		return;
	}
	ret = p922x_write(chip, REG_COMMAND, CHARGE_STAT);
	if (ret)
		dev_err(chip->dev, "%s: Failed to write command: %d\n",
			__func__, ret);
}

irqreturn_t p922x_int_thread(int irq, void *ptr)
{
	struct p922x_dev *chip = ptr;
	int ret = 0;
	u16 irq_src = 0, irq_status = 0, irq_clr = 0;
	u8 buf[2] = {0};

	dev_info(chip->dev, "%s\n", __func__);

	if (!chip->get_status_done) {
		dev_info(chip->dev, "%s: Get status not ready!\n", __func__);
		goto out;
	}

	ret = p922x_read_buffer(chip, REG_STATUS, buf, 2);
	if (ret) {
		dev_err(chip->dev,
			"%s: Failed to read status reg: %d\n", __func__, ret);
		goto out;
	}
	irq_status = buf[0] | (buf[1] << 8);

	dev_info(chip->dev, "%s: stat: 0x%04x\n", __func__, irq_status);

	ret = p922x_read_buffer(chip, REG_INTR, buf, 2);
	if (ret) {
		dev_err(chip->dev,
			"%s: Failed to read INT reg: %d\n", __func__, ret);
		goto out;
	}
	irq_src = buf[0] | (buf[1] << 8);
	dev_info(chip->dev, "%s: INT: 0x%04x\n", __func__, irq_src);

	ret = p922x_write_buffer(chip, REG_INT_CLEAR, (u8 *)&irq_src, 2);
	if (ret) {
		dev_err(chip->dev,
			"%s: Failed to clear INT reg: %d\n", __func__, ret);
		goto out;
	}

	ret = p922x_write(chip, REG_COMMAND, CLRINT);
	if (ret) {
		dev_err(chip->dev,
			"%s: Failed to reset INT: %d\n", __func__, ret);
		goto out;
	}

	/*
	 * When p9221 does not complete clearing interrupt immediately,
	 * it is prevented from handling repeated interrupt events.
	 */
	ret = p922x_write_buffer(chip, REG_INTR, (u8 *)&irq_clr, 2);
	if (ret) {
		dev_err(chip->dev,
			"%s: Failed to clear INT reg: %d\n", __func__, ret);
		goto out;
	}

	irq_status |= irq_src;
	ret = p922x_write_buffer(chip, REG_STATUS, (u8 *)&irq_status, 2);
	if (ret) {
		dev_err(chip->dev,
		"Failed to write IQR status: %d\n", ret);
		goto out;
	}

	if (!irq_src)
		goto out;

	if (irq_src & P922X_INT_VRECT)
		dev_info(chip->dev,
			"%s: Received VRECTON, online\n", __func__);

	if (irq_src & P922X_INT_TX_DATA_RECV)
		p922x_get_tx_adapter_type(chip);

	if ((irq_src & P922X_INT_DEVICE_AUTH_FAIL)) {
		if (chip->dev_auth_retry++ < 3) {
			dev_info(chip->dev, "%s: dev auth fail, retry = %d\n",
				__func__, chip->dev_auth_retry);
			p922x_device_auth_req(chip);
		} else
			chip->tx_authen_complete = true;
	}

	if (irq_src & P922X_INT_DEVICE_AUTH_SUCCESS) {
		chip->tx_dev_authen_status = true;
		chip->tx_authen_complete = true;
		dev_info(chip->dev,
				 "%s:P922X device auth sucess\n", __func__);
		p922x_detect_tx_adapter_type(chip);
	}

	if (irq_src & P922X_INT_ID_AUTH_FAIL)
		chip->tx_authen_complete = true;

	if (irq_src & P922X_INT_ID_AUTH_SUCCESS) {
		/* authentication success, can provide 10w charge */
		chip->tx_id_authen_status = true;
		chip->dev_auth_retry = 0;
		dev_info(chip->dev,
				 "%s:P922X ID auth sucess\n", __func__);
		p922x_device_auth_req(chip);
	}

	if (irq_src & P922X_INT_LIMIT_MASK)
		p922x_over_handle(chip, irq_src);

out:
	return IRQ_HANDLED;
}

static int p922x_attached_vbus(struct p922x_dev *chip)
{
	int ret = 0;

	atomic_set(&chip->online, true);
	chip->pg_time[1] = ktime_get_boottime();

	if (chip->support_ovp_en) {
		ret = p922x_set_ovp_en(chip, true);
		if (ret)
			dev_notice(chip->dev, "%s: set ovp_en failed, ret = %d\n",
				__func__, ret);
	}

	if (sram_program_fw(idt, 0x0000, idt_sram_firmware, ARRAY_SIZE(idt_sram_firmware)))
		dev_info(chip->dev, "%s, program sram fw successfully\n", __func__);
	else
		dev_err(chip->dev, "%s, program sram fw failed\n", __func__);

	ret = p922x_update_iout_cal_data();
	if (ret)
		dev_err(chip->dev, "%s: update iout cal data failed, ret: %d\n",
				__func__, ret);

	if (chip->use_buck) {
		/* set vout 6.5v */
		dev_info(chip->dev, "%s: use buck\n", __func__);
		ret = p922x_set_vout(chip, SET_VOUT_VAL);
		if (ret) {
			dev_err(chip->dev, "%s: Failed to set vout: %d\n", __func__, ret);
			p922x_enable_charge_flow(chip->chg_dev, false);
			return ret;
		}
	}

	p922x_enable_charge_flow(chip->chg_dev, true);

	return ret;
}

static void p922x_detached_vbus(struct p922x_dev *chip)
{
	int ret = 0;
	int boot_mode = get_boot_mode();

	dev_info(chip->dev, "%s\n", __func__);
	chip->pg_time[0] = ktime_get_boottime();
	p922x_enable_charge_flow(chip->chg_dev, false);

	if (chip->support_ovp_en) {
		ret = p922x_set_ovp_en(chip, false);
		if (ret)
			dev_notice(chip->dev, "%s: set ovp_en failed, ret = %d\n",
				__func__, ret);
	}

	if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT ||
		boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
		dev_notice(chip->dev, "%s: WPC is detached!\n", __func__);
		kernel_power_off();
	}
}

static bool p922x_get_pg_irq_status(struct p922x_dev *chip)
{
	return gpio_get_value(chip->pg_gpio);
}

static irqreturn_t p922x_pg_thread(int irq, void *ptr)
{
	bool vbus_en = false;
	struct p922x_dev *chip = ptr;

	mutex_lock(&chip->irq_lock);
	vbus_en = p922x_get_pg_irq_status(chip);
	dev_info(chip->dev, "%s: vbus_en = %d\n", __func__, vbus_en);

	if (vbus_en)
		p922x_attached_vbus(chip);
	else
		p922x_detached_vbus(chip);
	mutex_unlock(&chip->irq_lock);

	return IRQ_HANDLED;
}

static int p922x_init_switch_voltage(struct p922x_dev *chip, int *data)
{
	if (!data) {
		dev_info(chip->dev, "%s: wpc-switch-voltage config incorrect in dts, use default value\n",
			__func__);
		chip->switch_voltage[CHARGE_5W_MODE].voltage_low = SWITCH_5W_VTH_L;
		chip->switch_voltage[CHARGE_5W_MODE].voltage_target = CHARGER_VOUT_5W;
		chip->switch_voltage[CHARGE_5W_MODE].voltage_high = SWITCH_5W_VTH_H;
		chip->switch_voltage[CHARGE_10W_MODE].voltage_low = SWITCH_10W_VTH_L;
		chip->switch_voltage[CHARGE_10W_MODE].voltage_target = CHARGER_VOUT_10W;
		chip->switch_voltage[CHARGE_10W_MODE].voltage_high = SWITCH_10W_VTH_H;
	} else {
		chip->switch_voltage[CHARGE_5W_MODE].voltage_low = data[0];
		chip->switch_voltage[CHARGE_5W_MODE].voltage_target = data[1];
		chip->switch_voltage[CHARGE_5W_MODE].voltage_high = data[2];
		chip->switch_voltage[CHARGE_10W_MODE].voltage_low = data[3];
		chip->switch_voltage[CHARGE_10W_MODE].voltage_target = data[4];
		chip->switch_voltage[CHARGE_10W_MODE].voltage_high = data[5];
	}

	return 0;
}

static void p922x_pinctrl_parse_dt(struct p922x_dev *chip)
{
	int ret = 0;
	struct i2c_client *client = chip->client;

	chip->p922x_pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(chip->p922x_pinctrl)) {
		ret = PTR_ERR(chip->p922x_pinctrl);
		dev_err(chip->dev, "%s: Cannot find pinctrl, ret = %d\n", __func__, ret);
		return;
	}

	chip->support_wpc_en = true;

	chip->wpc_en0 = pinctrl_lookup_state(chip->p922x_pinctrl, "wpc_en0");
	if (IS_ERR(chip->wpc_en0)) {
		chip->support_wpc_en = false;
		ret = PTR_ERR(chip->wpc_en0);
		dev_err(chip->dev, "%s: Cannot find pinctrl wpc_en0, ret = %d\n", __func__, ret);
	}

	chip->wpc_en1 = pinctrl_lookup_state(chip->p922x_pinctrl, "wpc_en1");
	if (IS_ERR(chip->wpc_en1)) {
		chip->support_wpc_en = false;
		ret = PTR_ERR(chip->wpc_en1);
		dev_err(chip->dev, "%s: Cannot find pinctrl wpc_en1, ret = %d\n", __func__, ret);
	}

	if (chip->support_wpc_en) {
		chip->wpc_en = true;
		pinctrl_select_state(chip->p922x_pinctrl, chip->wpc_en0);
	}

	chip->support_sleep_en = true;

	chip->sleep_en0 = pinctrl_lookup_state(chip->p922x_pinctrl, "sleep_en0");
	if (IS_ERR(chip->sleep_en0)) {
		chip->support_sleep_en = false;
		ret = PTR_ERR(chip->sleep_en0);
		dev_err(chip->dev, "%s: Cannot find pinctrl sleep_en0, ret = %d\n", __func__, ret);
	}

	chip->sleep_en1 = pinctrl_lookup_state(chip->p922x_pinctrl, "sleep_en1");
	if (IS_ERR(chip->sleep_en1)) {
		chip->support_sleep_en = false;
		ret = PTR_ERR(chip->sleep_en1);
		dev_err(chip->dev, "%s: Cannot find pinctrl sleep_en1, ret = %d\n", __func__, ret);
	}

	if (chip->support_sleep_en) {
		chip->sleep_en = false;
		pinctrl_select_state(chip->p922x_pinctrl, chip->sleep_en0);
	}

	chip->support_ovp_en = true;

	chip->ovp_en0 = pinctrl_lookup_state(chip->p922x_pinctrl, "ovp_en0");
	if (IS_ERR(chip->ovp_en0)) {
		chip->support_ovp_en = false;
		ret = PTR_ERR(chip->ovp_en0);
		dev_err(chip->dev, "%s: Cannot find pinctrl ovp_en0, ret = %d\n", __func__, ret);
	}

	chip->ovp_en1 = pinctrl_lookup_state(chip->p922x_pinctrl, "ovp_en1");
	if (IS_ERR(chip->ovp_en1)) {
		chip->support_ovp_en = false;
		ret = PTR_ERR(chip->ovp_en1);
		dev_err(chip->dev, "%s: Cannot find pinctrl ovp_en1, ret = %d\n", __func__, ret);
	}

	if (chip->support_ovp_en) {
		chip->ovp_en = false;
		pinctrl_select_state(chip->p922x_pinctrl, chip->ovp_en1);
	}
}

static int p922x_parse_dt(struct p922x_dev *chip)
{
	int ret, count;
	struct device_node *dt = chip->client->dev.of_node;
	struct i2c_client *client = chip->client;
	const struct of_device_id *match = NULL;
	int data[SWITCH_VOLTAGE_COUNT];
	u8 fod_data[FOD_COEF_PARAM_LENGTH];
	struct adaptive_current_limit *adaptive =
		&chip->adaptive_current_limit;

	if (!dt) {
		dev_err(chip->dev, "%s: Device does not have associated DT data\n",
				__func__);
		return -EINVAL;
	}

	dev_err(chip->dev, "%s: Device have associated DT data\n", __func__);

	match = of_match_device(match_table, &client->dev);
	if (!match) {
		dev_err(chip->dev, "%s: Unknown device model\n", __func__);
		return -EINVAL;
	}

	p922x_pinctrl_parse_dt(chip);

	chip->int_gpio = of_get_named_gpio(dt, "p922x-irq", 0);
	if (!gpio_is_valid(chip->int_gpio))
		dev_err(chip->dev, "%s: No valid irq gpio\n", __func__);

	chip->pg_gpio = of_get_named_gpio(dt, "p922x-pg", 0);
	if (!gpio_is_valid(chip->pg_gpio))
		dev_err(chip->dev, "%s: No valid pg gpio\n", __func__);

	chip->use_buck = of_property_read_bool(dt, "use-buck");

	ret = of_property_read_u32_array(dt, "wpc-mivr", chip->wpc_mivr,
					ARRAY_SIZE(chip->wpc_mivr));
	if (ret) {
		dev_info(chip->dev, "%s: wpc_mivr %d %d",
			__func__, chip->wpc_mivr[0], chip->wpc_mivr[1]);
		return -EINVAL;
	}

	count = of_property_count_u32_elems(dt, "wpc-switch-voltage");
	ret = of_property_read_u32_array(dt, "wpc-switch-voltage", data,
					ARRAY_SIZE(data));
	if (count == SWITCH_VOLTAGE_COUNT && !ret)
		p922x_init_switch_voltage(chip, data);
	else
		p922x_init_switch_voltage(chip, NULL);

	dev_info(chip->dev, "%s: wpc_mivr (%d %d)uV, voltage (%d %d %d %d %d %d)mV",
			__func__, chip->wpc_mivr[0], chip->wpc_mivr[1],
			chip->switch_voltage[CHARGE_5W_MODE].voltage_low,
			chip->switch_voltage[CHARGE_5W_MODE].voltage_target,
			chip->switch_voltage[CHARGE_5W_MODE].voltage_high,
			chip->switch_voltage[CHARGE_10W_MODE].voltage_low,
			chip->switch_voltage[CHARGE_10W_MODE].voltage_target,
			chip->switch_voltage[CHARGE_10W_MODE].voltage_high);

	chip->bpp_5w_fod_num = of_property_count_elems_of_size(dt, "bpp-5w-fod", sizeof(u8));
	if (chip->bpp_5w_fod_num != FOD_COEF_PARAM_LENGTH) {
		dev_err(chip->dev, "%s: Incorrect num of 5w fod data", __func__);
		chip->bpp_5w_fod_num = 0;
	} else {
		ret = of_property_read_u8_array(dt, "bpp-5w-fod", fod_data, sizeof(fod_data));
		if (ret == 0) {
			memcpy(chip->bpp_5w_fod, fod_data, sizeof(fod_data));
			dev_info(chip->dev, "%s: 5w fod data:%d %d, %d %d, %d %d, %d %d, %d %d, %d %d, %d %d, %d %d",
				__func__, chip->bpp_5w_fod[0].gain, chip->bpp_5w_fod[0].offs,
				chip->bpp_5w_fod[1].gain, chip->bpp_5w_fod[1].offs,
				chip->bpp_5w_fod[2].gain, chip->bpp_5w_fod[2].offs,
				chip->bpp_5w_fod[3].gain, chip->bpp_5w_fod[3].offs,
				chip->bpp_5w_fod[4].gain, chip->bpp_5w_fod[4].offs,
				chip->bpp_5w_fod[5].gain, chip->bpp_5w_fod[5].offs,
				chip->bpp_5w_fod[6].gain, chip->bpp_5w_fod[6].offs,
				chip->bpp_5w_fod[7].gain, chip->bpp_5w_fod[7].offs);
		} else
			dev_err(chip->dev, "%s: Failed to parse bpp-5w-fod.\n",
				__func__);
	}

	chip->bpp_10w_fod_num = of_property_count_elems_of_size(dt, "bpp-10w-fod", sizeof(u8));
	if (chip->bpp_10w_fod_num != FOD_COEF_PARAM_LENGTH) {
		dev_err(chip->dev, "%s: Incorrect num of 10w fod data", __func__);
		chip->bpp_10w_fod_num = 0;
	} else {
		ret = of_property_read_u8_array(dt, "bpp-10w-fod", fod_data, sizeof(fod_data));
		if (ret == 0) {
			memcpy(chip->bpp_10w_fod, (u8 *)fod_data, sizeof(fod_data));
			dev_info(chip->dev, "%s: 10w fod data:%d %d, %d %d, %d %d, %d %d, %d %d, %d %d, %d %d, %d %d",
				__func__, chip->bpp_10w_fod[0].gain, chip->bpp_10w_fod[0].offs,
				chip->bpp_10w_fod[1].gain, chip->bpp_10w_fod[1].offs,
				chip->bpp_10w_fod[2].gain, chip->bpp_10w_fod[2].offs,
				chip->bpp_10w_fod[3].gain, chip->bpp_10w_fod[3].offs,
				chip->bpp_10w_fod[4].gain, chip->bpp_10w_fod[4].offs,
				chip->bpp_10w_fod[5].gain, chip->bpp_10w_fod[5].offs,
				chip->bpp_10w_fod[6].gain, chip->bpp_10w_fod[6].offs,
				chip->bpp_10w_fod[7].gain, chip->bpp_10w_fod[7].offs);
		} else
			dev_err(chip->dev, "%s: Failed to parse bpp-10w-fod.\n",
				__func__);
	}

	if (of_property_read_s32(dt, "adaptive_start_soc",
				&adaptive->start_soc) < 0)
		dev_err(chip->dev, "%s: Failed to parse adaptive_start_soc, use default value\n",
			__func__);

	if (of_property_read_s32(dt, "adaptive_interval",
				&adaptive->interval) < 0)
		dev_err(chip->dev, "%s: Failed to parse adaptive_interval, use default value\n",
			__func__);

	if (of_property_read_s32(dt, "adaptive_bpp_plus_max_ma",
		&adaptive->bpp_plus_max_ma) < 0)
		dev_err(chip->dev, "%s: Failed to parse adaptive_bpp_plus_max_ma,"
			"use default value\n",
			__func__);

	if (of_property_read_s32(dt, "adaptive_min_current_limit",
		&adaptive->min_current_limit) < 0)
		dev_err(chip->dev, "%s: Failed to parse adaptive_min_current_limit,"
			"use default value\n",
			__func__);

	if (of_property_read_s32(dt, "adaptive_margin",
		&adaptive->margin) < 0)
		dev_err(chip->dev, "%s: Failed to parse adaptive_margin, use default value\n",
			__func__);

	if (of_property_read_s32(dt, "adaptive_start_ma",
		&adaptive->start_ma) < 0)
		dev_err(chip->dev, "%s: Failed to parse adaptive_start_ma, use default value\n",
			__func__);

	if (of_property_read_s32(dt, "adaptive_max_current_limit_ma",
		&adaptive->max_current_limit) < 0)
		dev_err(chip->dev, "%s: Failed to parse adaptive_max_current_limit_ma, use default value\n",
			__func__);

	if (of_property_read_s32(dt, "bpp_10w_default_input_current",
		&chip->bpp_10w_default_input_current) < 0)
		dev_err(chip->dev, "%s: Failed to parse bpp_10w_default_input_current,"
			"use default value\n",
			__func__);

	if (of_property_read_s32(dt, "start_ma",
		&chip->step_load.start_ma) < 0)
		dev_err(chip->dev, "%s: Failed to parse step load start_ma, use default value\n",
			__func__);

	if (of_property_read_s32(dt, "step_ma",
		&chip->step_load.step_ma) < 0)
		dev_err(chip->dev, "%s: Failed to parse step load step_ma, use default value\n",
			__func__);

	if (of_property_read_s32(dt, "bpp_plus_max_current",
		&chip->step_load.bpp_plus_max_ua) < 0)
		dev_err(chip->dev, "%s: Failed to parse step load bpp_plus_max_current,"
			"use default value\n",
			__func__);

	if (of_property_read_u32(dt, "step_interval",
		&chip->step_load.step_interval) < 0)
		dev_err(chip->dev, "%s: Failed to parse step load step_interval,"
			"use default value\n",
			__func__);

	if (of_property_read_s32(dt, "step_max_ua",
		&chip->step_load.step_max_ua) < 0)
		dev_err(chip->dev, "%s: Failed to parse step_max_ua, use default value\n",
			__func__);

	if (of_property_read_u32(dt, "EPT_work_delay",
				&chip->EPT_work_delay) < 0)
		dev_err(chip->dev, "%s: Failed to parse EPT_work_delay, use default value\n",
			__func__);

	return 0;
}

static int p922x_set_wpc_en(struct charger_device *chg_dev, bool en)
{
	struct p922x_dev *chip = dev_get_drvdata(&chg_dev->dev);

	dev_info(chip->dev, "%s: %s\n", __func__, en ? "true" : "false");

	if (!chip->support_wpc_en) {
		dev_notice(chip->dev, "%s: not support to set wpc_en\n",
				__func__);
		return -EINVAL;
	}

	if (chip->wpc_en == en) {
		dev_info(chip->dev, "%s: is_enabled status same as %d\n",
				__func__, en);
		return 0;
	}
	chip->wpc_en = en;

	if (en) {
		pinctrl_select_state(chip->p922x_pinctrl, chip->wpc_en0);
	} else {
		msleep(10);
		pinctrl_select_state(chip->p922x_pinctrl, chip->wpc_en1);
	}

	return 0;
}

static int p922x_set_sleep_en(struct charger_device *chg_dev, bool en)
{
	struct p922x_dev *chip = dev_get_drvdata(&chg_dev->dev);

	dev_info(chip->dev, "%s: %s\n", __func__, en ? "true" : "false");

	if (!chip->support_sleep_en) {
		dev_notice(chip->dev, "%s: not support to set sleep_en\n",
				__func__);
		return -EINVAL;
	}

	if (chip->sleep_en == en) {
		dev_info(chip->dev, "%s: is_enabled status same as %d\n",
				__func__, en);
		return 0;
	}
	chip->sleep_en = en;

	if (en)
		pinctrl_select_state(chip->p922x_pinctrl, chip->sleep_en1);
	else
		pinctrl_select_state(chip->p922x_pinctrl, chip->sleep_en0);

	return 0;
}

static int p922x_set_ovp_en(struct p922x_dev *chip, bool en)
{
	dev_info(chip->dev, "%s: %s\n", __func__, en ? "true" : "false");

	if (!chip->support_ovp_en) {
		dev_notice(chip->dev, "%s: not support to set ovp_en\n", __func__);
		return -EINVAL;
	}

	if (chip->ovp_en == en) {
		dev_info(chip->dev, "%s: is_enabled status same as %d\n",
				__func__, en);
		return 0;
	}
	chip->ovp_en = en;

	if (en)
		pinctrl_select_state(chip->p922x_pinctrl, chip->ovp_en0);
	else
		pinctrl_select_state(chip->p922x_pinctrl, chip->ovp_en1);

	return 0;
}

static int p922x_request_io_port(struct p922x_dev *chip)
{
	int ret = 0;

	ret = gpio_request(chip->int_gpio, "idt_int");
	if (ret < 0) {
		dev_err(chip->dev,
			"%s: Failed to request GPIO:%d, ERRNO:%d\n",
			__func__, (s32)chip->int_gpio, ret);
		return -ENODEV;
	}
	gpio_direction_input(chip->int_gpio);
	dev_info(chip->dev, "%s: Success request int-gpio\n", __func__);

	ret = gpio_request(chip->pg_gpio, "idt_pg");
	if (ret < 0) {
		dev_err(chip->dev,
			"%s: Failed to request GPIO:%d, ERRNO:%d\n",
			__func__, (s32)chip->pg_gpio, ret);

		if (gpio_is_valid(chip->int_gpio))
			gpio_free(chip->int_gpio);

		return -ENODEV;
	}
	gpio_direction_input(chip->pg_gpio);
	dev_info(chip->dev, "%s: Success request pg-gpio\n", __func__);

	return 0;
}

static int p922x_register_irq(struct p922x_dev *chip)
{
	int ret = 0;

	chip->int_num = gpio_to_irq(chip->int_gpio);
	dev_info(chip->dev, "%s: gpio:%d, gpio_irq:%d\n",
		__func__, chip->int_gpio, chip->int_num);

	ret = request_threaded_irq(chip->int_num, NULL,
				p922x_int_thread,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT, IDT_INT,
				chip);
	if (ret == 0) {
		dev_info(chip->dev, "%s: irq disable at gpio: %d\n",
					__func__, chip->int_num);
		disable_irq_nosync(chip->int_num);
	} else
		dev_err(chip->dev, "%s: request_irq failed\n", __func__);

	chip->pg_num = gpio_to_irq(chip->pg_gpio);
	dev_info(chip->dev, "%s: gpio:%d, gpio_irq:%d\n",
			__func__, chip->pg_gpio, chip->pg_num);

	ret = request_threaded_irq(chip->pg_num, NULL, p922x_pg_thread,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			IDT_PG, chip);
	if (ret == 0) {
		dev_info(chip->dev, "%s: irq disable at gpio: %d\n",
					__func__, chip->pg_num);
		disable_irq_nosync(chip->pg_num);
	} else
		dev_err(chip->dev, "%s: request_irq failed, ret: %d\n",
					__func__, ret);

	return ret;
}

static int p922x_set_charger_mivr(struct p922x_dev *chip,
			struct charger_device *chg1_dev, u32 uV)
{
	int ret = 0;

	ret = charger_dev_set_mivr(chg1_dev, uV);
	if (ret < 0)
		dev_err(chip->dev, "%s: failed, ret = %d\n", __func__, ret);

	return ret;
}

static int p922x_set_charger_mivr_by_type(struct p922x_dev *chip,
		enum power_supply_type type)
{
	int ret = 0;
	bool is_en = false;
	bool power_path_en = !(type == POWER_SUPPLY_TYPE_UNKNOWN);
	struct charger_device *chg1_dev = get_charger_by_name("primary_chg");

	ret = charger_dev_is_powerpath_enabled(chg1_dev, &is_en);
	if (ret < 0) {
		dev_err(chip->dev, "%s: get is power path enabled failed\n", __func__);
		return ret;
	}

	if (is_en != power_path_en)
		charger_dev_enable_powerpath(chg1_dev,  power_path_en);

	switch (type) {
	case POWER_SUPPLY_TYPE_WIRELESS:
	case POWER_SUPPLY_TYPE_WIRELESS_5W:
		p922x_set_charger_mivr(chip,
			chg1_dev, chip->wpc_mivr[CHARGE_5W_MODE]);
		break;
	case POWER_SUPPLY_TYPE_WIRELESS_10W:
		p922x_set_charger_mivr(chip,
			chg1_dev, chip->wpc_mivr[CHARGE_10W_MODE]);
		break;
	default:
		break;
	}

	return ret;
}

static int p922x_update_charge_type(struct p922x_dev *chip,
			enum power_supply_type type)
{
	int ret;
	union power_supply_propval propval;

	struct power_supply *chg_psy = power_supply_get_by_name("mt6370_pmu_charger");

	ret = power_supply_get_property(chg_psy,
		POWER_SUPPLY_PROP_TYPE, &propval);
	if (ret < 0) {
		dev_err(chip->dev, "%s: get psy type failed, ret = %d\n",
			__func__, ret);
		return ret;
	}

	if (propval.intval == type) {
		dev_info(chip->dev, "%s: psy type is same as %d, not need update\n",
			__func__, type);
		return ret;
	}

	dev_info(chip->dev, "%s: psy type %d\n", __func__, type);

	p922x_set_charger_mivr_by_type(chip, type);

	chip->chr_type = type;
	propval.intval = type;
	ret = power_supply_set_property(chg_psy,
		POWER_SUPPLY_PROP_TYPE, &propval);
	if (ret < 0)
		dev_err(chip->dev, "%s: set psy type failed, ret = %d\n",
			__func__, ret);

	power_supply_changed(chip->wpc_psy);
	dev_info(chip->dev, "%s:type = %d\n", __func__, type);

	return ret;
}

static void p922x_update_current_setting(struct p922x_dev *chip)
{
	int input_current;
	struct charger_data *pdata = &chip->chg_info->chg_data[CHG1_SETTING];

	if (chip->chr_type == POWER_SUPPLY_TYPE_WIRELESS)
		input_current = chip->chg_info->data.wireless_default_charger_input_current;
	else if (chip->chr_type == POWER_SUPPLY_TYPE_WIRELESS_5W)
		input_current = chip->chg_info->data.wireless_5w_charger_input_current;
	else if (chip->chr_type == POWER_SUPPLY_TYPE_WIRELESS_10W)
		input_current = chip->chg_info->data.wireless_10w_charger_input_current;
	else
		return;

	/* Skip update if the same as previous. */
	if (input_current == chip->prev_input_current)
		return;

	dev_info(chip->dev, "%s: chr_type = %d, input_current = %d\n",
		__func__, chip->chr_type, input_current);

	if (chip->chg_info->algo.change_current_setting) {
		chip->chg_info->algo.change_current_setting(chip->chg_info);
		charger_dev_set_input_current(chip->chg1_dev,
			pdata->input_current_limit);
		charger_dev_set_charging_current(chip->chg1_dev,
			pdata->charging_current_limit);
	}

	power_supply_changed(chip->bat_psy);
	chip->prev_input_current = input_current;
}

static void p922x_power_switch_work(struct work_struct *work)
{
	static int switch_detct_count;
	int vout = 0;
	struct p922x_dev *chip = container_of(work, struct p922x_dev,
						 power_switch_work.work);
	struct adaptive_current_limit *adaptive =
		&chip->adaptive_current_limit;
	struct step_load *step_load = &chip->step_load;

	cancel_delayed_work(&chip->power_switch_work);

	vout = p922x_get_vout_adc(chip);
	dev_info(chip->dev, "%s\n", __func__);

	if (switch_detct_count > 2) {
		switch_detct_count = 0;
		return;
	}
	if (vout >= chip->switch_vth_low && vout <= chip->switch_vth_high) {
		switch_detct_count = 0;

		if (chip->chr_type == POWER_SUPPLY_TYPE_WIRELESS_10W) {
			if (get_uisoc(chip->chg_info) >= adaptive->start_soc) {
				chip->chg_info->data.wireless_10w_charger_input_current =
					adaptive->start_ma * 1000;
			} else if (chip->step_load_en) {
				chip->chg_info->data.wireless_10w_charger_input_current =
					step_load->start_ma * 1000;
				chip->on_step_charging = true;
				getrawmonotonic(&chip->start_step_chg_time);
				schedule_delayed_work(&chip->step_charging_work,
					msecs_to_jiffies(chip->step_load.step_interval));
			}

			p922x_metrics_abnormal_reconnection(chip);
		}

		p922x_update_charge_type(chip, chip->chr_type);
		p922x_update_current_setting(chip);
		p922x_write_fod(chip, chip->chr_type);

		if (chip->chr_type == POWER_SUPPLY_TYPE_WIRELESS_10W)
			p922x_init_adaptive_current_limit_work(chip, 0);

		return;
	}

	switch_detct_count++;
	schedule_delayed_work(&chip->power_switch_work, SWITCH_DETECT_TIME);

	if (chip->chr_type == POWER_SUPPLY_TYPE_WIRELESS_10W)
		p922x_power_switch(chip,
			chip->switch_voltage[CHARGE_10W_MODE].voltage_target);
	else
		p922x_power_switch(chip,
			chip->switch_voltage[CHARGE_5W_MODE].voltage_target);
}

static void p922x_EPT_work(struct work_struct *work)
{
	struct p922x_dev *chip = container_of(work, struct p922x_dev,
		EPT_work.work);

	if (p922x_get_pg_irq_status(chip)) {
		dev_info(chip->dev, "%s: Fake FOD send EPT\n", __func__);
		p922x_send_eop(chip, EOP_OVER_TEMP);
	} else {
		dev_info(chip->dev, "%s: Fake FOD skip send EPT\n", __func__);
	}
}

/* @initial_delay : Unit in ms */
static void p922x_init_adaptive_current_limit_work(struct p922x_dev *chip,
	int initial_delay)
{
	struct adaptive_current_limit *adaptive =
		&chip->adaptive_current_limit;

	adaptive->current_index = 0;

	adaptive->fill_count = 0;
	memset(adaptive->ibus, 0, sizeof(int)*IBUS_BUFFER_SIZE);

	schedule_delayed_work(&chip->adaptive_current_limit_work,
		msecs_to_jiffies(initial_delay));
}

static void dump_sw_fod_record(struct p922x_dev *chip)
{
	if (chip->sw_fod_count > 0) {
		uint16_t idx, max_count;
		struct rtc_time tm;

		dev_info(chip->dev, "%s: sw_fod_count:%d\n", __func__,
			chip->sw_fod_count);
		max_count = min(chip->sw_fod_count, SW_FOD_RECORD_SIZE);
		for (idx = 0; idx < max_count; idx++) {
			rtc_time_to_tm(chip->sw_fod_time_record[idx].tv_sec,
				&tm);
			dev_info(chip->dev, "%s: SW FOD record%d: %d-%02d-%02d %02d:%02d:%02d.%09lu UTC\n",
				__func__, idx,
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec,
				chip->sw_fod_time_record[idx].tv_nsec);
		}
	}
}

static void p922x_clear_fast_charge_switch(struct p922x_dev *chip)
{
	dev_info(chip->dev, "p922x clear fast charge switch\n");
	cancel_delayed_work(&chip->power_switch_work);
	cancel_delayed_work(&chip->adaptive_current_limit_work);
	cancel_delayed_work(&chip->step_charging_work);
	cancel_delayed_work(&chip->EPT_work);
	chip->tx_dev_authen_status = false;
	chip->tx_id_authen_status = false;
	chip->force_switch = false;
	chip->is_hv_adapter = false;
	chip->tx_authen_complete = false;
	chip->cm_cap_en = false;
	chip->tx_adapter_type = DOCK_ADAPTER_UNKNOWN;
	chip->adaptive_current_limit.on_adaptive = false;
	chip->prev_input_current = -1;
	chip->chg_info->data.wireless_10w_charger_input_current =
		chip->bpp_10w_default_input_current * 1000;
	chip->on_step_charging = false;
	chip->chr_type = POWER_SUPPLY_TYPE_UNKNOWN;

	dump_sw_fod_record(chip);
}

static void p922x_fast_charge_enable(struct p922x_dev *chip, bool en)
{
	bool stat;

	dev_info(chip->dev, "%s: en=%d\n", __func__, en);

	stat = atomic_read(&chip->online);
	if (!stat)
		return;

	cancel_delayed_work(&chip->power_switch_work);
	if (chip->tx_id_authen_status != true ||
		chip->tx_dev_authen_status != true) {
		dev_info(chip->dev, "%s: id authen:%d, dev authen:%d\n",
				__func__, chip->tx_id_authen_status,
				chip->tx_dev_authen_status);
		return;
	}

	if (en) {
		/* switch to 10w charge */
		p922x_power_switch(chip,
			chip->switch_voltage[CHARGE_10W_MODE].voltage_target);
		chip->switch_vth_low =
			chip->switch_voltage[CHARGE_10W_MODE].voltage_low;
		chip->switch_vth_high =
			chip->switch_voltage[CHARGE_10W_MODE].voltage_high;
		chip->chr_type = POWER_SUPPLY_TYPE_WIRELESS_10W;
	} else {
		/* switch to 5w charge */
		p922x_power_switch(chip,
			chip->switch_voltage[CHARGE_5W_MODE].voltage_target);
		chip->switch_vth_low =
			chip->switch_voltage[CHARGE_5W_MODE].voltage_low;
		chip->switch_vth_high =
			chip->switch_voltage[CHARGE_5W_MODE].voltage_high;
		chip->chr_type = POWER_SUPPLY_TYPE_WIRELESS_5W;
	}
	schedule_delayed_work(&chip->power_switch_work,  SWITCH_DETECT_TIME);
}

static int p922x_get_online(struct charger_device *chg_dev, bool *stat)
{
	struct p922x_dev *chip = dev_get_drvdata(&chg_dev->dev);

	*stat = atomic_read(&chip->online);
	dev_dbg(chip->dev, "%s: get online status: %d\n", __func__, *stat);

	return 0;
}

static int p922x_get_temp(struct charger_device *chg_dev)
{
	struct p922x_dev *chip = dev_get_drvdata(&chg_dev->dev);

	return p922x_get_temp_adc(chip);
}

static int p922x_do_algorithm(struct charger_device *chg_dev, void *data)
{
	struct p922x_dev *chip = dev_get_drvdata(&chg_dev->dev);
	struct mtk_charger *info = (struct mtk_charger *)data;
	int chr_type = 0;
	int ret = 0;
	union power_supply_propval propval;
	bool chg_done, stat;
	struct power_supply *chg_psy = power_supply_get_by_name("mt6370_pmu_charger");

	mutex_lock(&chip->irq_lock);

	stat = atomic_read(&chip->online);
	if (!stat)
		goto out;

	if (chip->force_switch)
		goto out;

	charger_dev_is_charging_done(info->chg1_dev, &chg_done);

	if (chip->tx_authen_complete != true) {
		dev_info(chip->dev, "%s: tx authen not completed\n", __func__);
		goto out;
	}

	ret = power_supply_get_property(chg_psy,
				POWER_SUPPLY_PROP_TYPE, &propval);
	if (ret < 0) {
		dev_err(chip->dev, "%s: get psy type failed, ret = %d\n",
				__func__, ret);
		return ret;
	}
	chr_type = propval.intval;
	dev_info(chip->dev, "%s: chr_type=%d, chg_done=%d\n",
			 __func__, chr_type, chg_done);

	if (chr_type == POWER_SUPPLY_TYPE_WIRELESS) {
		if (chip->tx_id_authen_status == true &&
			chip->tx_dev_authen_status == true) {
			p922x_fast_charge_enable(chip, true);
		} else {
			p922x_update_charge_type(chip, POWER_SUPPLY_TYPE_WIRELESS_5W);
			p922x_update_current_setting(chip);
		}

		p922x_metrics_tx_signal_strength(chip);
	}

	if (chr_type == POWER_SUPPLY_TYPE_WIRELESS_10W) {
		if (chg_done)
			p922x_set_tx_led_mode(chip, LED_CONSTANT_OFF);
		else
			p922x_set_tx_led_mode(chip, LED_CONSTANT_ON);
	}

out:
	mutex_unlock(&chip->irq_lock);

	return 0;
}

static int p922x_force_enable_charge(struct charger_device *chg_dev, bool en)
{
	struct p922x_dev *chip = dev_get_drvdata(&chg_dev->dev);

	dev_info(chip->dev, "%s\n", __func__);
	mutex_lock(&chip->irq_lock);
	p922x_enable_charge_flow(chip->chg_dev, en);
	mutex_unlock(&chip->irq_lock);

	return 0;
}

static void p922x_switch_set_state(struct p922x_dev *chip, enum dock_state_type set_state)
{
	int cur_state = 0;

	dev_info(chip->dev, "%s\n", __func__);

	cur_state = switch_get_state(&chip->dock_state);
	if (cur_state != set_state) {
		dev_info(chip->dev, "%s: state changed: %d -> %d\n",
			__func__, cur_state, set_state);
		switch_set_state(&chip->dock_state, set_state);
	} else
		dev_info(chip->dev, "%s: state is same as %d, not need update\n",
			__func__, set_state);
}

static void reset_wpc_cooler(struct thermal_zone_device *tz)
{
	struct thermal_instance *instance;

	list_for_each_entry(instance, &(tz->thermal_instances), tz_node) {
		if (!strcmp(instance->cdev->type, WPC_COOLER_NAME)) {
			unsigned long state;
			int ret;

			ret = instance->cdev->ops->get_cur_state
					(instance->cdev, &state);

			if (!ret && state > 0) {
				instance->target = 0;
				instance->cdev->ops->set_cur_state
						(instance->cdev, 0);
				pr_info("%s:%s:Reset cooler %s to restore its "
					"state to 0!\n", __FILE__, __func__,
					instance->cdev->type);
			}
			break;
		}
	}
}

static int p922x_enable_charge_flow(struct charger_device *chg_dev, bool en)
{
	struct p922x_dev *chip = dev_get_drvdata(&chg_dev->dev);
	struct thermal_zone_device *tz;

	dev_info(chip->dev, "%s: %s\n", __func__, en ? "true" : "false");

	if (chip->is_enabled == en) {
		dev_info(chip->dev, "%s: is_enabled status same as %d\n",
				__func__, en);
		return 0;
	}
	chip->is_enabled = en;

	if (en) {
		p922x_set_cm_cap_enable(chip, true);
		p922x_switch_set_state(chip, TYPE_DOCKED);
		p922x_update_charge_type(chip, POWER_SUPPLY_TYPE_WIRELESS);
		p922x_update_current_setting(chip);
		p922x_write_fod(chip, POWER_SUPPLY_TYPE_WIRELESS);
	} else {
		p922x_switch_set_state(chip, TYPE_UNDOCKED);
		p922x_clear_fast_charge_switch(chip);
		atomic_set(&chip->online, false);
		p922x_update_charge_type(chip, POWER_SUPPLY_TYPE_UNKNOWN);
		chip->is_sram_updated = false;
	}

	tz = thermal_zone_get_zone_by_name(WPC_CHARGER_NAME);
	if (IS_ERR(tz)) {
		dev_err(chip->dev, "%s:thermal:%p is NULL!\n", __func__, tz);
		tz = NULL;
	} else {
		enum thermal_device_mode mode;

		if (en) reset_wpc_cooler(tz);

		mode = en ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
		if (tz->ops->set_mode)
			tz->ops->set_mode(tz, mode);
		else
			dev_err(chip->dev, "%s:thermal %s set mode (%d) "
				"failure\n", __func__, tz->type, en);
	}

	return 0;
}

static struct charger_ops p922x_chg_ops = {
	.get_temp = p922x_get_temp,
	.get_wpc_online = p922x_get_online,
	.do_wpc_algorithm = p922x_do_algorithm,
	.force_enable_wpc_charge = p922x_force_enable_charge,
	.set_wpc_en = p922x_set_wpc_en,
	.set_sleep_en = p922x_set_sleep_en,
};

static struct p922x_desc p922x_default_desc = {
	.chg_dev_name = "wireless_chg",
	.alias_name = "p922x",
};

static int p922x_charger_device_register(struct p922x_dev *chip)
{
	int ret = 0;

	chip->desc = &p922x_default_desc;
	chip->chg_props.alias_name = chip->desc->alias_name;
	chip->chg_dev =	charger_device_register(chip->desc->chg_dev_name,
		chip->dev, chip, &p922x_chg_ops, &chip->chg_props);

	if (IS_ERR_OR_NULL(chip->chg_dev)) {
		ret = PTR_ERR(chip->chg_dev);
		return ret;
	}

	return 0;
}

static bool p922x_get_attach_status(struct p922x_dev *chip)
{
	int ret = 0;
	bool attach_status = false;
	u16 irq_status = 0;
	u8 buf[2] = {0};

	/* powergood not high, tx is not attached */
	if (!p922x_get_pg_irq_status(chip))
		goto dettached;

	dev_info(chip->dev, "%s: powergood online\n", __func__);

	/* check triggered interrupt if tx is attached */
	ret = p922x_read_buffer(chip, REG_INTR, buf, 2);
	if (ret) {
		dev_err(chip->dev, "%s: Failed to read INT reg: %d\n",
			__func__, ret);
		goto dettached;
	}
	irq_status = buf[0] | (buf[1] << 8);

	dev_info(chip->dev, "%s: IRQ status: %04x\n", __func__, irq_status);
	/* Don't need to handle interrput if irq_status is zero. */
	if (!irq_status)
		goto attached;

	/* clear interrupt register */
	ret = p922x_write_buffer(chip, REG_INT_CLEAR, (u8 *)&irq_status, 2);
	if (ret) {
		dev_err(chip->dev,
		"%s: Failed to clear INT reg: %d\n", __func__, ret);
		goto dettached;
	}

	ret = p922x_write(chip, REG_COMMAND, CLRINT);
	if (ret) {
		dev_err(chip->dev,
		"%s: Failed to reset INT: %d\n", __func__, ret);
		goto dettached;
	}

	/* handle triggered interrput */
	ret = p922x_write_buffer(chip, REG_STATUS, (u8 *)&irq_status, 2);
	if (ret) {
		dev_err(chip->dev,
		"%s: Failed to write IQR status: %d\n", __func__,  ret);
		goto dettached;
	}

	if (irq_status & P922X_INT_ID_AUTH_FAIL)
		chip->tx_authen_complete = true;

	/* authentication success, can provide 10w charge */
	if (irq_status & P922X_INT_ID_AUTH_SUCCESS)
		chip->tx_id_authen_status = true;
	if (irq_status & P922X_INT_DEVICE_AUTH_SUCCESS) {
		chip->tx_dev_authen_status = true;
		chip->tx_authen_complete = true;
	}

attached:
	/* set true if RX attached TX works well */
	atomic_set(&chip->online, true);
	attach_status = true;
	return attach_status;

dettached:
	atomic_set(&chip->online, false);
	return attach_status;

}

static void p922x_determine_init_status(struct work_struct *work)
{
	struct p922x_dev *chip = container_of(work, struct p922x_dev,
						wpc_init_work.work);

	unsigned int boot_mode = get_boot_mode();

	if ((boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
		|| boot_mode == LOW_POWER_OFF_CHARGING_BOOT)
		&& (!p922x_get_pg_irq_status(chip))) {
		dev_info(chip->dev, "%s: KPOC without WPC, not process WPC.\n",
			__func__);
		return;
	 }

	if (p922x_get_pg_irq_status(chip))
		atomic_set(&chip->online, true);
	else
		atomic_set(&chip->online, false);

	if (!is_mtk_charger_init_done())
		goto out;

	if (p922x_get_attach_status(chip))
		p922x_attached_vbus(chip);

	chip->get_status_done = true;
	enable_irq(chip->int_num);
	enable_irq(chip->pg_num);

	if (p922x_get_pg_irq_status(chip) && !chip->tx_authen_complete)
		p922x_device_auth_req(chip);

	pr_info("%s: wpc init successfully.\n", __func__);

	return;

out:
	pr_info("%s: mtk_charger not init done.\n", __func__);
	schedule_delayed_work(&chip->wpc_init_work,
				READY_DETECT_TIME);

}

static bool p922x_is_need_adaptive_current_limit(struct p922x_dev *chip)
{
	if (!p922x_get_pg_irq_status(chip) ||
		chip->chr_type != POWER_SUPPLY_TYPE_WIRELESS_10W) {
		dev_info(chip->dev, "%s: Exit adaptive current limiter.\n",
			__func__);
		return false;
	}

	return true;
}

static bool p922x_skip_adaptive_work(struct p922x_dev *chip)
{
	struct adaptive_current_limit *adaptive =
		&chip->adaptive_current_limit;

	if (adaptive->on_adaptive)
		return false;

	if (chip->on_step_charging)
		return true;

	if (get_uisoc(chip->chg_info) >= adaptive->start_soc)
		adaptive->on_adaptive = true;
	else
		return true;

	return false;
}

#define FILTER_THRESHOLD	200
static void p922x_adaptive_current_limit_work(struct work_struct *work)
{
	struct p922x_dev *chip = container_of(work, struct p922x_dev,
		adaptive_current_limit_work.work);
	struct adaptive_current_limit *adaptive =
		&chip->adaptive_current_limit;
	struct charger_device *chg_dev = chip->chg1_dev;

	uint32_t index, current_index;
	int ibus = -1, sum, target_current_limit = -1, margin;
	static int avg;

	/* check conditions should not do adaptive current limit. */
	if (!p922x_is_need_adaptive_current_limit(chip))
		return;

	/* To reset average value */
	if (adaptive->fill_count == 0)
		avg = 0;

	/* check conditions to re-schedule to next work directly. */
	if (p922x_skip_adaptive_work(chip))
		goto done;

	if (charger_dev_get_ibus(chg_dev, &ibus) < 0) {
		dev_err(chip->dev, "%s: Fail to read ibus\n", __func__);
		goto done;
	}
	ibus = ibus / 1000;
	current_index = adaptive->current_index;
	adaptive->ibus[current_index] = ibus;
	/* calculate the nex index */
	adaptive->current_index = ((current_index + 1) % IBUS_BUFFER_SIZE);

	/* To feed initial buffer data. */
	if (adaptive->fill_count < (IBUS_BUFFER_SIZE - 1)) {
		adaptive->fill_count++;
		goto done;
	}

	/* Start to filter after average value is ready. */
	if (avg != 0) {
		if (abs((ibus - avg)) > FILTER_THRESHOLD) {
			if (charger_dev_get_ibus(chg_dev, &ibus) >= 0) {
				ibus = ibus / 1000;
				/* Replace with new reading. */
				adaptive->ibus[current_index] = ibus;
			} else {
				dev_err(chip->dev, "%s: Fail to read ibus (%d).\n",
					__func__, __LINE__);
			}
		}
	}

	margin = adaptive->margin;
	for (sum = 0, index = 0; index < IBUS_BUFFER_SIZE; index++)
		sum += adaptive->ibus[index];

	avg = sum / IBUS_BUFFER_SIZE;

	target_current_limit = (avg / 100) * 100 + margin;

	if (target_current_limit > adaptive->max_current_limit)
		target_current_limit = adaptive->max_current_limit;

	if (target_current_limit < adaptive->min_current_limit)
		target_current_limit = adaptive->min_current_limit;

done:
	dev_info(chip->dev, "%s: AVG[%d] IBUS[%d] TARGET[%d]\n",
		__func__, avg, ibus, target_current_limit);

	if (p922x_get_pg_irq_status(chip)) {
		if (target_current_limit > 0) {
			chip->chg_info->data.wireless_10w_charger_input_current =
				target_current_limit * 1000;
			p922x_update_current_setting(chip);
		}
		schedule_delayed_work(&chip->adaptive_current_limit_work,
			msecs_to_jiffies(adaptive->interval));
	} else {
		dev_info(chip->dev, "%s: Exit adaptive current limiter.\n",
			__func__);
		chip->chg_info->data.wireless_10w_charger_input_current =
			adaptive->start_ma * 1000;
		adaptive->on_adaptive = false;
	}
}

static void p922x_step_charging_work(struct work_struct *work)
{
	struct p922x_dev *chip = container_of(work, struct p922x_dev,
						step_charging_work.work);
	struct step_load *step_load = &chip->step_load;
	int ua_now = 0;
	int ua_target = 0;
	int ua_max = 0;

	ua_now = chip->chg_info->data.wireless_10w_charger_input_current;
	ua_max = step_load->step_max_ua;
	if (ua_now < ua_max) {
		ua_target = ua_now + step_load->step_ma * 1000;
		if (ua_target > ua_max)
			ua_target = ua_max;
	} else {
		chip->on_step_charging = false;
		dev_info(chip->dev, "%s: Complete step charging.\n", __func__);
		return;
	}

	dev_info(chip->dev, "%s: target current:%dmA\n", __func__,
		ua_target / 1000);
	chip->chg_info->data.wireless_10w_charger_input_current = ua_target;
	p922x_update_current_setting(chip);
	if (p922x_get_pg_irq_status(chip))
		schedule_delayed_work(&chip->step_charging_work,
			msecs_to_jiffies(chip->step_load.step_interval));
	else
		dev_info(chip->dev, "%s: stop step charging.\n", __func__);
}

static void p922x_simulate_fod(struct p922x_dev *chip)
{
	uint16_t idx;
	uint16_t target_record;

	if (!atomic_read(&chip->online)) {
		dev_info(chip->dev, "%s: wireless charger is offline\n",
			__func__);
		return;
	}

	mutex_lock(&chip->fod_lock);
	target_record = chip->sw_fod_count % SW_FOD_RECORD_SIZE;
	getnstimeofday(&chip->sw_fod_time_record[target_record]);
	dev_info(chip->dev, "%s: Trigger fake FOD\n", __func__);
	for (idx = 0; idx < FOD_COEF_PARAM_LENGTH; idx++)
		p922x_write(chip, REG_FOD_COEF_ADDR + idx, 0x10);
	chip->sw_fod_count++;
	mutex_unlock(&chip->fod_lock);

	if (!delayed_work_pending(&chip->EPT_work)) {
		schedule_delayed_work(&chip->EPT_work,
			msecs_to_jiffies(chip->EPT_work_delay));
	}
}

static int p922x_psy_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	struct p922x_dev *chip = power_supply_get_drvdata(psy);
	int input_current_limit = val->intval;

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		/*  Only support zero input current limit to send fake FOD. */
		if (p922x_get_pg_irq_status(chip) && input_current_limit == 0)
			p922x_simulate_fod(chip);
		break;
	default:
		return -EINVAL;
	};

	return 0;
}

static int p922x_psy_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct p922x_dev *chip = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = atomic_read(&chip->online);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (!atomic_read(&chip->online) || !chip->is_sram_updated)
			return -ENODEV;

		val->intval = p922x_get_ntc_temp(chip);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int p922x_property_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return 1;
	default:
		return 0;
	}
}

static enum power_supply_property p922x_psy_properties[] = {
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TEMP,
};

static char *wpc_charger_supplied_to[] = {
	"battery",
	"mtk-master-charger"
};

static int p922x_pm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	if (device_may_wakeup(chip->dev))
		enable_irq_wake(chip->pg_num);
	disable_irq(chip->pg_num);
	disable_irq(chip->int_num);
	dev_info(chip->dev, "%s\n", __func__);

	return 0;
}

static int p922x_pm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct p922x_dev *chip = i2c_get_clientdata(client);

	dev_info(chip->dev, "%s\n", __func__);
	enable_irq(chip->pg_num);
	enable_irq(chip->int_num);
	if (device_may_wakeup(chip->dev))
		disable_irq_wake(chip->pg_num);

	return 0;
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

static int p922x_thermal_notify(struct thermal_zone_device *thermal,
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
		set_shutdown_enable_dcap();
		last_kmsg_thermal_shutdown(dev_name(&thermal->device));
	}

	return 0;
}

static int wpc_get_temp(void *p, int *temp)
{
	struct thermal_zone_p922x_data *data = p;

	if (!data || !data->tz_chip || !atomic_read(&data->tz_chip->online) ||
		!data->tz_chip->is_sram_updated)
		return -EINVAL;

	if (data->id == 0)
		*temp = p922x_get_temp_adc(data->tz_chip) * 1000;
	else if (data->id == 1)
		*temp = p922x_get_ntc_temp(data->tz_chip) * 1000;
	else
		return -EINVAL;

	return 0;
}

static const struct thermal_zone_of_device_ops wpc_sensor_ops = {
	.get_temp = wpc_get_temp,
};

static SIMPLE_DEV_PM_OPS(p922x_pm_ops,
			p922x_pm_suspend, p922x_pm_resume);

static int p922x_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct p922x_dev *chip;
	struct thermal_zone_device *thz_dev;
	struct thermal_zone_p922x_data *tz_data;
	int i, ret = 0;
	u8 val = 0;
	struct adaptive_current_limit *adaptive;
	struct power_supply *master_chg_psy;

	pr_info("%s: enter.\n", __func__);

	if (p922x_get_wpc_support() != true) {
		pr_err("%s: not support wireless charger!\n",
			__func__);
		return -ENODEV;
	}

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	i2c_set_clientdata(client, chip);

	chip->regmap = regmap_init_i2c(client, &p922x_regmap_config);
	if (!chip->regmap) {
		pr_err("%s parent regmap is missing\n", __func__);
		ret = -EINVAL;
		goto out_kfree;
	}

	chip->client = client;
	chip->dev = &client->dev;
	chip->use_buck = false;
	chip->force_switch = false;
	chip->is_hv_adapter = false;
	chip->tx_id_authen_status = false;
	chip->tx_dev_authen_status = false;
	chip->is_enabled = false;
	chip->over_reason = 0;
	chip->tx_adapter_type = DOCK_ADAPTER_UNKNOWN;
	chip->reg.addr = 0x0000;
	chip->reg.size = 1;
	chip->dev_auth_retry = 0;
	chip->cm_cap_en = false;
	chip->tx_authen_complete = false;
	chip->is_cal_data_ready = false;
	chip->is_sram_updated = false;
	chip->iout_cal_data = &cal_data;

	ret = p922x_get_iout_cal_data(chip);
	if (ret < 0)
		dev_err(chip->dev, "%s: get idme data failed, ret: %d\n",
			__func__, ret);

	adaptive = &chip->adaptive_current_limit;
	adaptive->start_soc = 90;
	adaptive->interval = 10000;	/* 10s */
	adaptive->bpp_plus_max_ma = 1100;	/* 1100mA */
	adaptive->max_current_limit = 1100;	/* 1100mA */
	adaptive->min_current_limit = 300;	/* 300mA */
	adaptive->margin = 200;	/* 200mA */
	adaptive->start_ma = 500;	/* 500mA */
	chip->bpp_10w_default_input_current = 1100;	/* 1100mA */
	chip->step_load.start_ma = 500;	/* 500mA */
	chip->step_load.step_ma = 200;	/* 200mA */
	chip->step_load.step_max_ua = 1100 * 1000;	/* 1100mA */
	chip->step_load.bpp_plus_max_ua = 1100 * 1000;	/* 1100mA */
	chip->step_load.step_interval = 1000;	/* 1000ms */
	chip->on_step_charging = false;
	chip->step_load_en = true;
	chip->EPT_work_delay = DEFAULT_EPT_WORK_DELAY;

	device_init_wakeup(chip->dev, true);
	mutex_init(&chip->sys_lock);
	mutex_init(&chip->irq_lock);
	mutex_init(&chip->fod_lock);

	ret = p922x_parse_dt(chip);
	if (ret < 0) {
		dev_err(chip->dev, "%s: parse dts failed, ret: %d\n",
			__func__, ret);
		goto out_regmap;
	}

	chip->wpc_desc.name = "Wireless";
	chip->wpc_desc.no_thermal = true;
	chip->wpc_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	chip->wpc_desc.properties = p922x_psy_properties;
	chip->wpc_desc.num_properties = ARRAY_SIZE(p922x_psy_properties);
	chip->wpc_desc.get_property = p922x_psy_get_property;
	chip->wpc_desc.set_property = p922x_psy_set_property;
	chip->wpc_desc.property_is_writeable = p922x_property_is_writeable,
	chip->wpc_cfg.drv_data = chip;
	chip->wpc_cfg.supplied_to = wpc_charger_supplied_to;
	chip->wpc_cfg.num_supplicants = ARRAY_SIZE(wpc_charger_supplied_to);
	chip->wpc_psy = power_supply_register(chip->dev, &chip->wpc_desc,
		&chip->wpc_cfg);
	if (IS_ERR(chip->wpc_psy)) {
		dev_err(chip->dev, "Failed to register power supply: %ld\n",
			PTR_ERR(chip->wpc_psy));
		ret = PTR_ERR(chip->wpc_psy);
		goto out_regmap;
	}

	chip->dock_state.name = "dock";
	chip->dock_state.index = 0;
	chip->dock_state.state = TYPE_UNDOCKED;
	ret = switch_dev_register(&chip->dock_state);
	if (ret) {
		dev_err(chip->dev, "%s: switch_dev_register dock_state Fail\n",
			__func__);
		goto out_wpc_psy;
	}

	ret = p922x_request_io_port(chip);
	if (ret < 0) {
		dev_err(chip->dev, "%s: Failed request IO port, ret: %d\n",
			__func__, ret);
		goto out_switch;
	}

	ret = p922x_register_irq(chip);
	if (ret < 0) {
		dev_err(chip->dev, "%s: Failed reqister irq, ret: %d\n",
			__func__, ret);
		goto out_gpio;
	}

	ret = p922x_charger_device_register(chip);
	if (ret < 0) {
		dev_err(chip->dev, "%s: charger device reqister failed, ret: %d\n",
			__func__, ret);
		goto out_irq;
	}

	idt = chip;

	INIT_DELAYED_WORK(&chip->power_switch_work, p922x_power_switch_work);
	INIT_DELAYED_WORK(&chip->wpc_init_work, p922x_determine_init_status);
	INIT_DELAYED_WORK(&chip->adaptive_current_limit_work,
		p922x_adaptive_current_limit_work);
	INIT_DELAYED_WORK(&chip->step_charging_work, p922x_step_charging_work);
	INIT_DELAYED_WORK(&chip->EPT_work, p922x_EPT_work);

	chip->chg1_dev = get_charger_by_name("primary_chg");
	if (!chip->chg1_dev)
		goto out_charger;

	chip->bat_psy = devm_power_supply_get_by_phandle(chip->dev, "gauge");
	if(!chip->bat_psy)
		goto out_charger;

	master_chg_psy = devm_power_supply_get_by_phandle(chip->dev, "charger");
	if (!master_chg_psy)
		goto out_charger;

	chip->chg_info = (struct mtk_charger *)power_supply_get_drvdata(master_chg_psy);
	if (!chip->chg_info)
		goto out_charger;

	p922x_read(chip, 0x5870, &val);
	pr_info("IDT 0x5870 %s:%d :%02x\n", __func__, __LINE__, val);

	p922x_read(chip, 0x5874, &val);
	pr_info("IDT 0x5874 %s:%d :%02x\n", __func__, __LINE__, val);

	p922x_fwver(chip);

	ret = sysfs_create_group(&client->dev.kobj, &p922x_sysfs_group_attrs);
	if (ret != 0) {
		pr_debug("[idt] %s: - ERROR: sysfs_create_group() failed.\n",
			 __func__);
		goto out_charger;
	}

	schedule_delayed_work(&chip->wpc_init_work, 0);

	for (i = 0; i < THERMAL_SENSOR_NUM; i++) {
		tz_data = devm_kzalloc(chip->dev, sizeof(*tz_data), GFP_KERNEL);
		if (!tz_data) {
			goto out_charger;
		}

		tz_data->id = i;
		tz_data->tz_chip = chip;

		thz_dev = devm_thermal_zone_of_sensor_register(chip->dev, tz_data->id,
								tz_data, &wpc_sensor_ops);

		if (IS_ERR(thz_dev)) {
			dev_err(chip->dev, "Failed to register sensor: %d id: %d\n"
				, PTR_ERR(thz_dev), tz_data->id);
		} else {
			thz_dev->ops->notify = p922x_thermal_notify;
		}
	}

	pr_info("%s: successfully.\n", __func__);
	return 0;

out_charger:
	if (chip->chg_dev)
		charger_device_unregister(chip->chg_dev);
out_irq:
	if (chip->int_num)
		free_irq(chip->int_num, chip);
	if (chip->pg_num)
		free_irq(chip->pg_num, chip);
out_gpio:
	if (gpio_is_valid(chip->int_gpio))
		gpio_free(chip->int_gpio);
	if (gpio_is_valid(chip->pg_gpio))
		gpio_free(chip->pg_gpio);
out_switch:
	switch_dev_unregister(&chip->dock_state);
out_wpc_psy:
	power_supply_unregister(chip->wpc_psy);
out_regmap:
	regmap_exit(chip->regmap);
	device_init_wakeup(chip->dev, false);
	mutex_destroy(&chip->irq_lock);
	mutex_destroy(&chip->sys_lock);
	mutex_destroy(&chip->fod_lock);
out_kfree:
	devm_kfree(&client->dev, chip);

	return ret;

}

static int p922x_remove(struct i2c_client *client)
{
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);

	if (chip->int_num)
		free_irq(chip->int_num, chip);
	if (chip->pg_num)
		free_irq(chip->pg_num, chip);

	if (gpio_is_valid(chip->int_gpio))
		gpio_free(chip->int_gpio);
	if (gpio_is_valid(chip->pg_gpio))
		gpio_free(chip->pg_gpio);

	if (chip->chg_dev)
		charger_device_unregister(chip->chg_dev);
	mutex_destroy(&chip->irq_lock);
	mutex_destroy(&chip->sys_lock);
	mutex_destroy(&chip->fod_lock);
	sysfs_remove_group(&client->dev.kobj, &p922x_sysfs_group_attrs);
	device_init_wakeup(chip->dev, false);
	regmap_exit(chip->regmap);
	switch_dev_unregister(&chip->dock_state);
	power_supply_unregister(chip->wpc_psy);

	return 0;
}

static void p922x_shutdown(struct i2c_client *client)
{
	struct p922x_dev *chip = (struct p922x_dev *)i2c_get_clientdata(client);
	unsigned int boot_mode = get_boot_mode();
	u16 irq_status = 0;
	u8 buf[2] = {0};

	dev_info(chip->dev, "%s: start\n", __func__);

	if ((boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
		    || boot_mode == LOW_POWER_OFF_CHARGING_BOOT)
		    && (!p922x_get_pg_irq_status(chip))) {
		    dev_info(chip->dev, "%s: not switch 5v in KPOC mode.\n", __func__);
		    return;
	}

	mutex_lock(&chip->irq_lock);
	if (!atomic_read(&chip->online))
		goto out;

	chip->force_switch = true;
	cancel_delayed_work(&chip->power_switch_work);
	/* force switch to 5w to prevent lk over voltage. */
	if (chip->chr_type == POWER_SUPPLY_TYPE_WIRELESS_10W) {
		p922x_power_switch(chip,
			chip->switch_voltage[CHARGE_5W_MODE].voltage_target);
		p922x_write_fod(chip, POWER_SUPPLY_TYPE_WIRELESS_5W);
	}

	disable_irq_nosync(idt->int_num);

	if (chip->tx_authen_complete) {
		if (chip->tx_dev_authen_status)
			irq_status = P922X_INT_ID_AUTH_SUCCESS | P922X_INT_DEVICE_AUTH_SUCCESS;
		else
			irq_status = P922X_INT_ID_AUTH_FAIL;
	} else {
		if (chip->tx_id_authen_status)
			irq_status = P922X_INT_ID_AUTH_SUCCESS;
	}

	if (irq_status) {
		p922x_read_buffer(chip, REG_INTR, buf, 2);
		irq_status = buf[0] | (buf[1] << 8) | irq_status;
		p922x_write_buffer(chip, REG_INTR, (u8 *)&irq_status, 2);
		dev_info(chip->dev, "%s: IRQ status: %04x\n", __func__, irq_status);
	}

out:
	mutex_unlock(&chip->irq_lock);
}

static struct i2c_driver p922x_driver = {
	.driver = {
		.name = "idt_wireless_power",
		.owner = THIS_MODULE,
		.of_match_table = match_table,
		.pm = &p922x_pm_ops,
	},
	.probe = p922x_probe,
	.remove = p922x_remove,
	.shutdown = p922x_shutdown,
	.id_table = p922x_dev_id,
};

static int __init p922x_driver_init(void)
{
	return i2c_add_driver(&p922x_driver);
}

static void __exit p922x_driver_exit(void)
{
	i2c_del_driver(&p922x_driver);
}

late_initcall(p922x_driver_init);
module_exit(p922x_driver_exit);

MODULE_AUTHOR("roy.luo@idt.com, simon.song.df@renesas.com");
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_DESCRIPTION("P922x Wireless Power Charger Monitor driver");
MODULE_LICENSE("GPL v2");
