// SPDX-License-Identifier: GPL-2.0
/******************** (C) COPYRIGHT 2021~2022 SILAN ********************
 *
 * File Name			: sc7a20.c
 * Description			: SC7A20 accelerometer sensor API
 *
 *******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
 * OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
 * PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
 * AS A RESULT, SILAN SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
 * INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
 * CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
 * INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * THIS SOFTWARE IS SPECIFICALLY DESIGNED FOR EXCLUSIVE USE WITH SILAN PARTS.
 *

 ******************************************************************************/

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/time.h>
#include <linux/kernel.h>

#include "accel.h"
#include "cust_acc.h"
#include "sensors_io.h"
#include "sc7a20.h"
#include <hwmsen_helper.h>

#define I2C_DRIVERID_SC7A20 120
#define SW_CALIBRATION
#define DRIVER_VERSION "V1.0"
#define SC7A20_BUF_SIZE 256

#define SILAN_SC7A20_FILTER 1
#ifdef SILAN_SC7A20_FILTER

struct FilterChannel {
	s16 sample_l;
	s16 sample_h;

	s16 flag_l;
	s16 flag_h;

};

struct Silan_core_channel {
	s16 filter_param_l;
	s16 filter_param_h;
	s16 filter_threhold;
	struct FilterChannel sl_channel[3];
};

#endif /*__SILAN_SC7A20_FILTER__*/

#ifdef SILAN_SC7A20_FILTER
struct Silan_core_channel core_channel;
#endif

/**************************
 *** DEBUG
 **************************/
#define GSE_TAG					"[SC7A20] "
#define GSE_INFO(fmt, args...)	pr_info(GSE_TAG fmt "\n", ##args)
#define GSE_ERR(fmt, args...)	pr_err(GSE_TAG" %s %d" fmt "\n", __func__, __LINE__, ##args)
#define GSE_DEBUG(fmt, args...)	pr_debug(GSE_TAG"DEBUG [%d]" fmt,  __LINE__, ##args)
#define GSE_DEBUG_FUNC(fmt, args...)	pr_debug(GSE_TAG"%s/%d\n" fmt, __func__, __LINE__, ##args)

#define SC7A20_AXIS_X 0
#define SC7A20_AXIS_Y 1
#define SC7A20_AXIS_Z 2
#define SC7A20_AXES_NUM 3
#define SC7A20_DATA_LEN 6
#define C_MAX_FIR_LENGTH (32)
#define USE_DELAY
#define SC7A20_RANGE_DEF SC7A20_RANGE_4G

/* add for diag */
#define ACCEL_SELF_TEST_MIN_VAL 0
#define ACCEL_SELF_TEST_MAX_VAL 13000
#define SC7A20_ACC_CALI_FILE "/data/inv_cal_data.bin"
#define SC7A20_DATA_BUF_NUM 3
#define CALI_SIZE 3 /*CALI_SIZE should not less than 3*/

static int accel_self_test[SC7A20_AXES_NUM] = { 0 };
static s16 accel_xyz_offset[SC7A20_AXES_NUM] = { 0 };
/* default tolenrance is 20% */
static int accel_cali_tolerance = 20;

struct acc_hw accel_cust;
struct acc_hw *hw = &accel_cust;
#ifdef USE_DELAY
static int delay_state;
#endif
static atomic_t open_flag = ATOMIC_INIT(0);

static const struct i2c_device_id sc7a20_i2c_id[] = {
	{ SC7A20_DEV_NAME, 0 },
	{},
};
static int sc7a20_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id);
static int sc7a20_i2c_remove(struct i2c_client *client);
static int sc7a20_suspend(struct device *dev);
static int sc7a20_resume(struct device *dev);
static int sc7a20_local_init(void);
static int sc7a20_remove(void);
static int sc7a20_flush(void);

#if IS_ENABLED(CONFIG_IDME)
extern unsigned int idme_get_sensorcal(s16 *data, uint8_t size);
#endif

enum {
	ADX_TRC_FILTER = 0x01,
	ADX_TRC_RAWDATA = 0x02,
	ADX_TRC_IOCTL = 0x04,
	ADX_TRC_CALI = 0X08,
	ADX_TRC_INFO = 0X10,
} ADX_TRC;

struct scale_factor {
	u8 whole;
	u8 fraction;
};

struct data_resolution {
	struct scale_factor scalefactor;
	int sensitivity;
};

struct data_filter {
	s16 raw[C_MAX_FIR_LENGTH][SC7A20_AXES_NUM];
	int sum[SC7A20_AXES_NUM];
	int num;
	int idx;
};
static int sc7a20_init_flag = -1;
/*----------------------------------------------------------------------------*/
static struct acc_init_info sc7a20_init_info = {
	.name = SC7A20_DEV_NAME,
	.init = sc7a20_local_init,
	.uninit = sc7a20_remove,
};
struct sc7a20_i2c_data {
	struct i2c_client *client;
	struct acc_hw *hw;
	struct hwmsen_convert cvt;
	atomic_t layout;
	/*misc*/
	struct data_resolution *reso;
	atomic_t trace;
	atomic_t suspend;
	atomic_t selftest;
	atomic_t filter;
	s16 cali_sw[SC7A20_AXES_NUM + 1];

	/*data*/
	s8 offset[SC7A20_AXES_NUM + 1]; /*+1: for 4-byte alignment*/
	s16 data[SC7A20_AXES_NUM + 1];

	bool flush;
};

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops sc7a20_i2c_pm_ops = { SET_SYSTEM_SLEEP_PM_OPS(
	sc7a20_suspend, sc7a20_resume) };
#endif

#ifdef CONFIG_OF
static const struct of_device_id accel_of_match[] = {
	{.compatible = "mediatek,sc7a20"},
	{},
};
#endif

static struct i2c_driver sc7a20_i2c_driver = {
	 .driver = {
		 .name			 = SC7A20_DEV_NAME,
#if IS_ENABLED(CONFIG_PM_SLEEP)
		.pm = &sc7a20_i2c_pm_ops,
#endif
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = accel_of_match,
#endif
	 },
	 .probe = sc7a20_i2c_probe,
	 .remove = sc7a20_i2c_remove,
	 .id_table = sc7a20_i2c_id,
};

struct i2c_client *sc7a20_i2c_client;
static struct sc7a20_i2c_data *obj_i2c_data;
static bool sensor_power;
static struct GSENSOR_VECTOR3D gsensor_gain;
static struct mutex sc7a20_mutex;

static struct data_resolution sc7a20_data_resolution[] = {
	{ { 0, 9 }, 1024 }, /*+/-2g  in 12-bit resolution:  0.9 mg/LSB*/
	{ { 1, 9 }, 512 }, /*+/-4g  in 12-bit resolution:  1.9 mg/LSB*/
	{ { 3, 9 }, 256 }, /*+/-8g  in 12-bit resolution: 3.9 mg/LSB*/
};
static struct data_resolution sc7a20_offset_resolution = {
	{ 0, 9 },
	1024
};

#ifdef SILAN_SC7A20_FILTER
static s16 filter_average(s16 preAve, s16 sample, s16 Filter_num, s16 *flag)
{
	if (*flag == 0) {
		preAve = sample;
		*flag = 1;
	}

	return preAve + (sample - preAve) / Filter_num;
}

static s16 silan_filter_process(struct FilterChannel *fac, s16 sample)
{
	if (fac == NULL)
		return 0;

	fac->sample_l =
		filter_average(fac->sample_l, sample,
				   core_channel.filter_param_l, &fac->flag_l);
	fac->sample_h =
		filter_average(fac->sample_h, sample,
				   core_channel.filter_param_h, &fac->flag_h);
	if (abs(fac->sample_l - fac->sample_h) > core_channel.filter_threhold)
		fac->sample_h = fac->sample_l;

	return fac->sample_h;
}
#endif /* ! SILAN_SC7A20_FILTER */

/* normal return value is 0 */
static int sc7a20_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data,
				 u8 len)
{
	u8 beg = addr;
	struct i2c_msg msgs[2] = {
		{ .addr = client->addr, .flags = 0, .len = 1, .buf = &beg },
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = data,
		}
	};
	int err = 0;

	if (!client)
		return -EINVAL;
	else if (len > C_I2C_FIFO_SIZE) {
		GSE_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	err = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (err != 2) {
		GSE_ERR("i2c_transfer error: (%d %p %d) %d\n", addr, data, len,
			err);
		err = -EIO;
	} else {
		err = 0;
	}
	return err;
}

static void sc7a20_power(struct acc_hw *hw, unsigned int on)
{
	static unsigned int power_on;

	power_on = on;
}

static int SC7A20_SetDataResolution(struct sc7a20_i2c_data *obj)
{
	switch(SC7A20_RANGE_DEF) {
	case SC7A20_RANGE_2G:
		obj->reso = &sc7a20_data_resolution[0];
		break;
	case SC7A20_RANGE_4G:
		obj->reso = &sc7a20_data_resolution[1];
		break;
	case SC7A20_RANGE_8G:
		obj->reso = &sc7a20_data_resolution[2];
		break;
	}
	return SC7A20_SUCCESS;
}

static int SC7A20_ReadData(struct i2c_client *client, s16 data[SC7A20_AXES_NUM])
{
	u8 addr = SC7A20_REG_X | 0x80;
	u8 buf[SC7A20_DATA_LEN] = { 0 };
	int err = 0;

#ifdef USE_DELAY
	if (delay_state) {
		msleep(300);
		delay_state = 0;
	}
#endif

	if (client == NULL) {
		GSE_ERR("client is null\n");
		err = -EINVAL;
	}
	err = sc7a20_i2c_read_block(client, addr, buf, SC7A20_DATA_LEN);
	if (err) {
		GSE_ERR("error: %d\n", err);
	} else {
		data[SC7A20_AXIS_X] =
			((s16)(((u8)buf[1] * 256) + (u8)buf[0]) >> 4);
		data[SC7A20_AXIS_Y] =
			((s16)(((u8)buf[3] * 256) + (u8)buf[2]) >> 4);
		data[SC7A20_AXIS_Z] =
			((s16)(((u8)buf[5] * 256) + (u8)buf[4]) >> 4);

#ifdef SILAN_SC7A20_FILTER
		data[SC7A20_AXIS_X] = silan_filter_process(
			&core_channel.sl_channel[0], data[SC7A20_AXIS_X]);
		data[SC7A20_AXIS_Y] = silan_filter_process(
			&core_channel.sl_channel[1], data[SC7A20_AXIS_Y]);
		data[SC7A20_AXIS_Z] = silan_filter_process(
			&core_channel.sl_channel[2], data[SC7A20_AXIS_Z]);
#endif
	}
	return err;
}

static int SC7A20_ReadOffset(struct i2c_client *client, s8 ofs[SC7A20_AXES_NUM])
{
	int err = 0;
#ifdef SW_CALIBRATION
	ofs[0] = ofs[1] = ofs[2] = 0x0;
#endif
	return err;
}

static int SC7A20_ResetCalibration(struct i2c_client *client)
{
	struct sc7a20_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	memset(obj->offset, 0x00, sizeof(obj->offset));
	return err;
}

static int SC7A20_ReadCalibration(struct i2c_client *client,
				  int dat[SC7A20_AXES_NUM])
{
	struct sc7a20_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	int mul = 0;

#ifdef SW_CALIBRATION
	mul = 0; /* only SW Calibration, disable HW Calibration */
#else
	err = SC7A20_ReadOffset(client, obj->offset);
	if (err) {
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}
	mul = obj->reso->sensitivity / sc7a20_offset_resolution.sensitivity;
#endif

	dat[obj->cvt.map[SC7A20_AXIS_X]] = obj->cvt.sign[SC7A20_AXIS_X] *
					   (obj->offset[SC7A20_AXIS_X] * mul +
						obj->cali_sw[SC7A20_AXIS_X]);
	dat[obj->cvt.map[SC7A20_AXIS_Y]] = obj->cvt.sign[SC7A20_AXIS_Y] *
					   (obj->offset[SC7A20_AXIS_Y] * mul +
						obj->cali_sw[SC7A20_AXIS_Y]);
	dat[obj->cvt.map[SC7A20_AXIS_Z]] = obj->cvt.sign[SC7A20_AXIS_Z] *
					   (obj->offset[SC7A20_AXIS_Z] * mul +
						obj->cali_sw[SC7A20_AXIS_Z]);

	return err;
}

static int SC7A20_ReadCalibrationEx(struct i2c_client *client,
					int act[SC7A20_AXES_NUM],
					int raw[SC7A20_AXES_NUM])
{
	int mul = 0;

	/*raw: the raw calibration data; act: the actual calibration data*/
	struct sc7a20_i2c_data *obj = i2c_get_clientdata(client);

#ifdef SW_CALIBRATION
	mul = 0; /* only SW Calibration, disable HW Calibration */
#else
	int err = SC7A20_ReadOffset(client, obj->offset);

	if (err) {
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}
	mul = obj->reso->sensitivity / sc7a20_offset_resolution.sensitivity;
#endif

	raw[SC7A20_AXIS_X] =
		obj->offset[SC7A20_AXIS_X] * mul + obj->cali_sw[SC7A20_AXIS_X];
	raw[SC7A20_AXIS_Y] =
		obj->offset[SC7A20_AXIS_Y] * mul + obj->cali_sw[SC7A20_AXIS_Y];
	raw[SC7A20_AXIS_Z] =
		obj->offset[SC7A20_AXIS_Z] * mul + obj->cali_sw[SC7A20_AXIS_Z];

	act[obj->cvt.map[SC7A20_AXIS_X]] =
		obj->cvt.sign[SC7A20_AXIS_X] * raw[SC7A20_AXIS_X];
	act[obj->cvt.map[SC7A20_AXIS_Y]] =
		obj->cvt.sign[SC7A20_AXIS_Y] * raw[SC7A20_AXIS_Y];
	act[obj->cvt.map[SC7A20_AXIS_Z]] =
		obj->cvt.sign[SC7A20_AXIS_Z] * raw[SC7A20_AXIS_Z];

	return 0;
}

/*********************************************************
 *** SC7A20_CaliConvert
 * 6 direction calibration
 * xyz threshold:(-300mg ~ +300mg) can be calibrated
 * GRAVITY_EARTH_1000		   9807   about (9.80665f)*1000
 * 9807-3000-->6807   9807+3000 -->12807
 *********************************************************/
static int SC7A20_CaliConvert(struct SENSOR_DATA *cali_data)
{
	struct SENSOR_DATA local_data;

	local_data.x = 0 - cali_data->x;
	local_data.y = 0 - cali_data->y;
	local_data.z = GRAVITY_EARTH_1000 - cali_data->z;
	GSE_INFO("no convert data  %d %d %d\n", local_data.x, local_data.y,
			local_data.z);
	if (((local_data.x >= 6807) && (local_data.x <= 12807))) {
		cali_data->x = GRAVITY_EARTH_1000 - local_data.x;
		cali_data->y = 0 - local_data.y;
		cali_data->z = 0 - local_data.z;
	} else if (((0 - local_data.x) > 6807) && (0 - local_data.x) <= 12807) {
		cali_data->x = 0 - (GRAVITY_EARTH_1000 + local_data.x);
		cali_data->y = 0 - local_data.y;
		cali_data->z = 0 - local_data.z;
	} else if (((local_data.y >= 6807) && (local_data.y <= 12807))) {
		cali_data->x = 0 - local_data.x;
		cali_data->y = GRAVITY_EARTH_1000 - local_data.y;
		cali_data->z = 0 - local_data.z;
	} else if (((0 - local_data.y) > 6807) && (0 - local_data.y) <= 12807) {
		cali_data->x = 0 - local_data.x;
		cali_data->y = 0 - (GRAVITY_EARTH_1000 + local_data.y);
		cali_data->z = 0 - local_data.z;
	} else if (((local_data.x >= 6807) && (local_data.x <= 12807))) {
		cali_data->x = 0 - local_data.x;
		cali_data->y = 0 - local_data.y;
		cali_data->z = GRAVITY_EARTH_1000 - local_data.z;
	} else if (((0 - local_data.z) > 6807) && (0 - local_data.z) <= 12807) {
		cali_data->x = 0 - local_data.x;
		cali_data->y = 0 - local_data.y;
		cali_data->z = 0 - (GRAVITY_EARTH_1000 + local_data.z);
	} else {
		GSE_INFO("the xyz threshold over:(-300mg ~ +300mg)\n");
		return -EINVAL;
	}
	GSE_INFO("convert data  %d %d %d\n", cali_data->x, cali_data->y,
			cali_data->z);
	return 0;
}

static int SC7A20_WriteCalibration(struct i2c_client *client,
				   int dat[SC7A20_AXES_NUM])
{
	struct sc7a20_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	int cali[SC7A20_AXES_NUM], raw[SC7A20_AXES_NUM];
#ifdef SW_CALIBRATION
#else
	int lsb = sc7a20_offset_resolution.sensitivity;
	int divisor = obj->reso->sensitivity / lsb;
#endif
	err = SC7A20_ReadCalibrationEx(
		client, cali, raw); /*offset will be updated in obj->offset*/
	if (err) {
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}

	GSE_DEBUG(
		"OLDOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n",
		raw[SC7A20_AXIS_X], raw[SC7A20_AXIS_Y], raw[SC7A20_AXIS_Z],
		obj->offset[SC7A20_AXIS_X], obj->offset[SC7A20_AXIS_Y],
		obj->offset[SC7A20_AXIS_Z], obj->cali_sw[SC7A20_AXIS_X],
		obj->cali_sw[SC7A20_AXIS_Y], obj->cali_sw[SC7A20_AXIS_Z]);

	/*calculate the real offset expected by caller*/
	cali[SC7A20_AXIS_X] += dat[SC7A20_AXIS_X];
	cali[SC7A20_AXIS_Y] += dat[SC7A20_AXIS_Y];
	cali[SC7A20_AXIS_Z] += dat[SC7A20_AXIS_Z];

	GSE_DEBUG("UPDATE: (%+3d %+3d %+3d)\n", dat[SC7A20_AXIS_X],
		  dat[SC7A20_AXIS_Y], dat[SC7A20_AXIS_Z]);

#ifdef SW_CALIBRATION
	obj->cali_sw[SC7A20_AXIS_X] = obj->cvt.sign[SC7A20_AXIS_X] *
					  (cali[obj->cvt.map[SC7A20_AXIS_X]]);
	obj->cali_sw[SC7A20_AXIS_Y] = obj->cvt.sign[SC7A20_AXIS_Y] *
					  (cali[obj->cvt.map[SC7A20_AXIS_Y]]);
	obj->cali_sw[SC7A20_AXIS_Z] = obj->cvt.sign[SC7A20_AXIS_Z] *
					  (cali[obj->cvt.map[SC7A20_AXIS_Z]]);
#else
	int divisor = obj->reso->sensitivity / lsb;

	obj->offset[SC7A20_AXIS_X] =
		(s8)(obj->cvt.sign[SC7A20_AXIS_X] *
			 (cali[obj->cvt.map[SC7A20_AXIS_X]]) / (divisor));
	obj->offset[SC7A20_AXIS_Y] =
		(s8)(obj->cvt.sign[SC7A20_AXIS_Y] *
			 (cali[obj->cvt.map[SC7A20_AXIS_Y]]) / (divisor));
	obj->offset[SC7A20_AXIS_Z] =
		(s8)(obj->cvt.sign[SC7A20_AXIS_Z] *
			 (cali[obj->cvt.map[SC7A20_AXIS_Z]]) / (divisor));

	/*convert software calibration using standard calibration*/
	obj->cali_sw[SC7A20_AXIS_X] = obj->cvt.sign[SC7A20_AXIS_X] *
					  (cali[obj->cvt.map[SC7A20_AXIS_X]]) %
					  (divisor);
	obj->cali_sw[SC7A20_AXIS_Y] = obj->cvt.sign[SC7A20_AXIS_Y] *
					  (cali[obj->cvt.map[SC7A20_AXIS_Y]]) %
					  (divisor);
	obj->cali_sw[SC7A20_AXIS_Z] = obj->cvt.sign[SC7A20_AXIS_Z] *
					  (cali[obj->cvt.map[SC7A20_AXIS_Z]]) %
					  (divisor);

	GSE_DEBUG(
		"NEWOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n",
		obj->offset[SC7A20_AXIS_X] * divisor +
			obj->cali_sw[SC7A20_AXIS_X],
		obj->offset[SC7A20_AXIS_Y] * divisor +
			obj->cali_sw[SC7A20_AXIS_Y],
		obj->offset[SC7A20_AXIS_Z] * divisor +
			obj->cali_sw[SC7A20_AXIS_Z],
		obj->offset[SC7A20_AXIS_X], obj->offset[SC7A20_AXIS_Y],
		obj->offset[SC7A20_AXIS_Z], obj->cali_sw[SC7A20_AXIS_X],
		obj->cali_sw[SC7A20_AXIS_Y], obj->cali_sw[SC7A20_AXIS_Z]);

#endif
	msleep(20);
	return err;
}

static int SC7A20_CheckDeviceID(struct i2c_client *client)
{
	u8 addr = SC7A20_REG_ID;
	u8 databuf[10] = { 0 };
	int res = 0;

	memset(databuf, 0, sizeof(u8) * 10);
	res = sc7a20_i2c_read_block(client, addr, databuf,
					1); /* normal return value is 0 */
	if (res < 0)
		goto exit_SC7A20_CheckDeviceID;

	databuf[0] = (databuf[0] & 0xff);

	if (databuf[0] == SC7A20_ID_1) {
		GSE_INFO("check device id success");
	} else {
		GSE_ERR("%s %d done!\n ", __func__, databuf[0]);
		goto exit_SC7A20_CheckDeviceID;
	}

	return SC7A20_SUCCESS;

exit_SC7A20_CheckDeviceID:
	if (res < 0) {
		GSE_ERR("%s %d failed!\n ", __func__, SC7A20_ERR_I2C);
		return SC7A20_ERR_I2C;
	}
	return SC7A20_ERR_I2C;
}

static int SC7A20_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2] = { 0 };
	int res = 0, i = 0;
	u8 addr = SC7A20_REG_CTL_REG1;

	res = sc7a20_i2c_read_block(client, addr, databuf,
					1); /* normal return value is 0 */
	if (res < 0)
		return SC7A20_ERR_I2C;

	databuf[0] &= ~SC7A20_POWER_MODE_MASK;
	if (enable == 1)
		databuf[0] |= SC7A20_AWAKE;
	else
		databuf[0] |= SC7A20_SLEEP;

	databuf[1] = databuf[0];
	databuf[0] = SC7A20_REG_CTL_REG1;
	while (i++ < 3) {
		res = i2c_master_send(client, databuf, 0x2);
		if (res > 0)
			break;
		msleep(5);
	}

	if (res <= 0) {
		GSE_ERR("silan set power mode failed!\n");
		return SC7A20_ERR_I2C;
	}
	sensor_power = enable;
#ifdef USE_DELAY
	delay_state = enable;
#else
	msleep(300);
#endif
	if (obj_i2c_data->flush) {
		if (sensor_power) {
			GSE_ERR("remain flush");
			sc7a20_flush();
		} else
			obj_i2c_data->flush = false;
	}

	return SC7A20_SUCCESS;
}

static int SC7A20_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	struct sc7a20_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[10] = { 0 };
	u8 addr = SC7A20_REG_CTL_REG4;
	int res = 0;

	memset(databuf, 0, sizeof(u8) * 10);

	res = sc7a20_i2c_read_block(client, addr, databuf,
					1); /* normal return value is 0 */
	if (res < 0)
		return SC7A20_ERR_I2C;

	databuf[0] &= ~SC7A20_RANGE;

	switch (dataformat) {
	case SC7A20_RANGE_2G:
		databuf[0] |= SC7A20_RANGE_2G;
		break;
	case SC7A20_RANGE_4G:
		databuf[0] |= SC7A20_RANGE_4G;
		break;
	case SC7A20_RANGE_8G:
		databuf[0] |= SC7A20_RANGE_8G;
		break;
}

	databuf[0] |= 0x88;
	databuf[1] = databuf[0];
	databuf[0] = SC7A20_REG_CTL_REG4;

	res = i2c_master_send(client, databuf, 0x2);
	if (res <= 0) {
		GSE_ERR("set power mode failed!\n");
		return SC7A20_ERR_I2C;
	}

	return SC7A20_SetDataResolution(obj);
}

static int SC7A20_SetBWRate(struct i2c_client *client, u8 bwrate)
{
	u8 databuf[10] = { 0 };
	u8 addr = SC7A20_REG_CTL_REG1;
	int res = 0;

	memset(databuf, 0, sizeof(u8) * 10);

	res = sc7a20_i2c_read_block(client, addr, databuf,
					1);
	if (res < 0)
		return SC7A20_ERR_I2C;

	databuf[0] &= ~SC7A20_BW;

	if (bwrate == SC7A20_BW_400HZ)
		databuf[0] |= SC7A20_BW_400HZ;
	else
		databuf[0] |= SC7A20_BW_100HZ;

	databuf[1] = databuf[0];
	databuf[0] = SC7A20_REG_CTL_REG1;

	res = i2c_master_send(client, databuf, 0x2);

	if (res <= 0) {
		GSE_ERR("%s failed!\n", __func__);
		return SC7A20_ERR_I2C;
	}

	return SC7A20_SUCCESS;
}

static int sc7a20_init_client(struct i2c_client *client, int reset_cali)
{
	struct sc7a20_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

	GSE_DEBUG_FUNC();
	res = SC7A20_SetPowerMode(client, true);
	if (res != SC7A20_SUCCESS)
		return res;

	res = SC7A20_SetBWRate(client, SC7A20_BW_50HZ);
	if (res != SC7A20_SUCCESS) {
		GSE_ERR("SC7A20 Set BWRate failed\n");
		return res;
	}

	res = SC7A20_SetDataFormat(
		client,
		SC7A20_RANGE_DEF);
	if (res != SC7A20_SUCCESS)
		return res;

	gsensor_gain.x = gsensor_gain.y = gsensor_gain.z =
		obj->reso->sensitivity;

	if (reset_cali != 0) {
		/*reset calibration only in power on*/
		res = SC7A20_ResetCalibration(client);
		if (res != SC7A20_SUCCESS)
			return res;
	}
	GSE_INFO("%s OK!\n", __func__);

	msleep(20);

	return SC7A20_SUCCESS;
}

static int SC7A20_ReadChipInfo(struct i2c_client *client, char *buf,
				   int bufsize)
{
	if ((buf == NULL) || (bufsize <= 30))
		return -1;

	if (client == NULL) {
		*buf = 0;
		return -2;
	}

	sprintf(buf, "SC7A20 Chip");
	return 0;
}

static int SC7A20_ReadSensorData(struct i2c_client *client, int *buf,
				 int bufsize)
{
	struct sc7a20_i2c_data *obj =
		(struct sc7a20_i2c_data *)i2c_get_clientdata(client);
	int acc[SC7A20_AXES_NUM] = { 0 };
	int data_buf[SC7A20_AXES_NUM] = {0};
	int res = 0;
	int err = 0;

	if (buf == NULL) {
		GSE_ERR("buf is null !!!\n");
		return SC7A20_ERR_STATUS;
	}
	if (client == NULL) {
		*buf = 0;
		GSE_ERR("client is null !!!\n");
		return SC7A20_ERR_STATUS;
	}

	if (atomic_read(&obj->suspend)) {
		GSE_DEBUG("sensor in suspend read not data!\n");
		return SC7A20_ERR_GETGSENSORDATA;
	}

	if (false == sensor_power) {
		if (SC7A20_SetPowerMode(client, true)
			!= SC7A20_SUCCESS) {
			GSE_ERR("ERR: fail to set power mode!\n");
			err = SC7A20_ERR_I2C;
			goto out;
		}
	}

	res = SC7A20_ReadData(client, obj->data);
	if (res) {
		GSE_ERR("I2C error: ret value=%d", res);
		return SC7A20_ERR_I2C;
	}

	/* add for sw cali */
	data_buf[SC7A20_AXIS_X] =
	obj->data[SC7A20_AXIS_X] * GRAVITY_EARTH_1000 / gsensor_gain.x;
	data_buf[SC7A20_AXIS_Y] =
	obj->data[SC7A20_AXIS_Y] * GRAVITY_EARTH_1000 / gsensor_gain.y;
	data_buf[SC7A20_AXIS_Z] =
	obj->data[SC7A20_AXIS_Z] * GRAVITY_EARTH_1000 / gsensor_gain.z;

	data_buf[SC7A20_AXIS_X] += obj->cali_sw[SC7A20_AXIS_X];
	data_buf[SC7A20_AXIS_Y] += obj->cali_sw[SC7A20_AXIS_Y];
	data_buf[SC7A20_AXIS_Z] += obj->cali_sw[SC7A20_AXIS_Z];

	if (atomic_read(&obj->trace) & ADX_TRC_RAWDATA)
		GSE_INFO("[%s] raw data: %d, %d, %d\n", __func__,
			obj->data[SC7A20_AXIS_X],
			obj->data[SC7A20_AXIS_Y],
			obj->data[SC7A20_AXIS_Z]);

	/*remap coordinate*/
	acc[obj->cvt.map[SC7A20_AXIS_X]] =
		obj->cvt.sign[SC7A20_AXIS_X] * data_buf[SC7A20_AXIS_X];
	acc[obj->cvt.map[SC7A20_AXIS_Y]] =
		obj->cvt.sign[SC7A20_AXIS_Y] * data_buf[SC7A20_AXIS_Y];
	acc[obj->cvt.map[SC7A20_AXIS_Z]] =
		obj->cvt.sign[SC7A20_AXIS_Z] * data_buf[SC7A20_AXIS_Z];

	if (atomic_read(&obj->trace) & ADX_TRC_RAWDATA) {
		GSE_INFO("[%s] map data: %d, %d, %d!\n", __func__,
			acc[SC7A20_AXIS_X],
			acc[SC7A20_AXIS_Y],
			acc[SC7A20_AXIS_Z]);
		GSE_INFO("[%s] cali data: %d, %d, %d!\n", __func__,
			obj->cali_sw[SC7A20_AXIS_X],
			obj->cali_sw[SC7A20_AXIS_Y],
			obj->cali_sw[SC7A20_AXIS_Z]);
		GSE_INFO("[%s] gain: %d, %d, %d!\n", __func__,
			gsensor_gain.x,
			gsensor_gain.y,
			gsensor_gain.z);
	}

	buf[0] = acc[SC7A20_AXIS_X];
	buf[1] = acc[SC7A20_AXIS_Y];
	buf[2] = acc[SC7A20_AXIS_Z];

	return res;

out:
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	return err;
}

static int SC7A20_ReadRawData(struct i2c_client *client, char *buf)
{
	struct sc7a20_i2c_data *obj =
		(struct sc7a20_i2c_data *)i2c_get_clientdata(client);
	int res = 0;

	if (!buf || !client) {
		GSE_ERR(" buf or client is null !!\n");
		return -EINVAL;
	}

	if (sensor_power == false) {
		res = SC7A20_SetPowerMode(client, true);
		if (res) {
			GSE_ERR("Power on SC7A20 error %d!\n", res);
			return SC7A20_ERR_I2C;
		}
	}

	res = SC7A20_ReadData(client, obj->data);
	if (res) {
		GSE_ERR("I2C error: ret value=%d\n", res);
		return -EIO;
	}
	buf[0] = (int)obj->data[SC7A20_AXIS_X];
	buf[1] = (int)obj->data[SC7A20_AXIS_Y];
	buf[2] = (int)obj->data[SC7A20_AXIS_Z];

	return 0;
}

static ssize_t chipinfo_show(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = sc7a20_i2c_client;
	char strbuf[SC7A20_BUFSIZE] = { 0 };

	if (client == NULL) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	SC7A20_ReadChipInfo(client, strbuf, SC7A20_BUFSIZE);

	return scnprintf(buf, PAGE_SIZE, "%s\n", (char *)strbuf);
}

static ssize_t sensordata_show(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = sc7a20_i2c_client;
	int err = 0;
	int strbuf[SC7A20_BUFSIZE] = { 0 };
	struct sc7a20_i2c_data *obj = NULL;

	if (client == NULL) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);
	if (false == sensor_power) {
		err = SC7A20_SetPowerMode(client, true);
		if (err) {
			GSE_ERR("[Sensor] enable power fail!! err code %d!\n", err);
			return scnprintf(buf, PAGE_SIZE, "[Sensor] enable power fail\n");
		}
		msleep(200);
	}

	SC7A20_ReadSensorData(client, strbuf, SC7A20_BUFSIZE);

	return scnprintf(buf, PAGE_SIZE, "%d %d %d\n", strbuf[0], strbuf[1], strbuf[2]);
}

static ssize_t cali_show(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = sc7a20_i2c_client;
	struct sc7a20_i2c_data *obj = NULL;
	int err, mul, err1, len = 0;
	int tmp[SC7A20_AXES_NUM] = { 0 };

	if (client == NULL) {
		GSE_ERR("i2c client is null!!\n");
		return SC7A20_ERR_STATUS;
	}

	obj = i2c_get_clientdata(client);
	err = SC7A20_ReadOffset(client, obj->offset);
	err1 = SC7A20_ReadCalibration(client, tmp);
	if (err)
		return -EINVAL;
	else if ((err1))
		return -EINVAL;
	mul = obj->reso->sensitivity /
		  sc7a20_offset_resolution.sensitivity;
	len += scnprintf(
		buf + len, PAGE_SIZE - len,
		"[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n",
		mul, obj->offset[SC7A20_AXIS_X],
		obj->offset[SC7A20_AXIS_Y], obj->offset[SC7A20_AXIS_Z],
		obj->offset[SC7A20_AXIS_X], obj->offset[SC7A20_AXIS_Y],
		obj->offset[SC7A20_AXIS_Z]);
	len += scnprintf(buf + len, PAGE_SIZE - len,
			"[SW ][%d] (%+3d, %+3d, %+3d)\n", 1,
			obj->cali_sw[SC7A20_AXIS_X],
			obj->cali_sw[SC7A20_AXIS_Y],
			obj->cali_sw[SC7A20_AXIS_Z]);

	len += scnprintf(
		buf + len, PAGE_SIZE - len,
		"[ALL]	(%+3d, %+3d, %+3d) : (%+3d, %+3d, %+3d)\n",
		obj->offset[SC7A20_AXIS_X] * mul +
			obj->cali_sw[SC7A20_AXIS_X],
		obj->offset[SC7A20_AXIS_Y] * mul +
			obj->cali_sw[SC7A20_AXIS_Y],
		obj->offset[SC7A20_AXIS_Z] * mul +
			obj->cali_sw[SC7A20_AXIS_Z],
		tmp[SC7A20_AXIS_X], tmp[SC7A20_AXIS_Y],
		tmp[SC7A20_AXIS_Z]);

	return len;
}

static ssize_t cali_store(struct device_driver *ddri, const char *buf,
			  size_t count)
{
	struct i2c_client *client = sc7a20_i2c_client;
	int err, x = 0, y = 0, z = 0;
	int dat[SC7A20_AXES_NUM] = { 0 };

	if (!strncmp(buf, "rst", 3)) {
		err = SC7A20_ResetCalibration(client);
		if (err)
			GSE_ERR("reset offset err = %d\n", err);
	} else if (sscanf(buf, "0x%02X 0x%02X 0x%02X", &x, &y, &z) == 3) {
		dat[SC7A20_AXIS_X] = x;
		dat[SC7A20_AXIS_Y] = y;
		dat[SC7A20_AXIS_Z] = z;
		err = SC7A20_WriteCalibration(client, dat);
		if (err)
			GSE_ERR("write calibration err = %d\n", err);
	} else {
		GSE_ERR("invalid format\n");
	}

	return 0;
}

static ssize_t trace_show(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;
	struct sc7a20_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	res = scnprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

static ssize_t trace_store(struct device_driver *ddri, const char *buf,
			   size_t count)
{
	struct sc7a20_i2c_data *obj = obj_i2c_data;
	int trace = 0;

	if (obj == NULL) {
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (sscanf(buf, "0x%x", &trace) == 1) {
		atomic_set(&obj->trace, trace);
	} else {
		GSE_ERR("invalid content: '%s', length = %d\n", buf,
			(int)count);
	}
	return count;
}

static ssize_t status_show(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct sc7a20_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (obj->hw) {
		len += scnprintf(buf + len, PAGE_SIZE - len,
				"CUST: %d %d (%d %d)\n", obj->hw->i2c_num,
				obj->hw->direction, obj->hw->power_id,
				obj->hw->power_vol);
	} else {
		len += scnprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");
	}
	return len;
}

static ssize_t powerstatus_show(struct device_driver *ddri, char *buf)
{
	if (sensor_power)
		GSE_INFO("G sensor is in work mode, sensor_power = %d\n",
			 sensor_power);
	else
		GSE_INFO("G sensor is in standby mode, sensor_power = %d\n",
			 sensor_power);

	return scnprintf(buf, PAGE_SIZE, "%x\n", sensor_power);
}

static ssize_t powerstatus_store(struct device_driver *ptDevDrv,
								const char *pbBuf, size_t tCount)
{
	int power_mode = 0;
	bool power_enable = false;
	int ret = 0;

	struct i2c_client *client = sc7a20_i2c_client;

	if (client == NULL)
		return 0;

	ret = kstrtoint(pbBuf, 10, &power_mode);

	power_enable = (power_mode ? true : false);

	if (ret == 0)
		ret = SC7A20_SetPowerMode(client, power_enable);

	if (ret) {
		GSE_ERR("set power %s failed %d\n", (power_enable ? "on" : "off"), ret);
		return 0;
	}
	GSE_ERR("set power %s ok\n", (power_enable ? "on" : "off"));

	return tCount;
}

static ssize_t layout_show(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = sc7a20_i2c_client;
	struct sc7a20_i2c_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
			   data->hw->direction, atomic_read(&data->layout),
			   data->cvt.sign[0], data->cvt.sign[1], data->cvt.sign[2],
			   data->cvt.map[0], data->cvt.map[1], data->cvt.map[2]);
}

static ssize_t layout_store(struct device_driver *ddri, const char *buf,
				size_t count)
{
	struct i2c_client *client = sc7a20_i2c_client;
	struct sc7a20_i2c_data *data = i2c_get_clientdata(client);
	int layout = 0;

	if (kstrtoint(buf, 10, &layout) == 1) {
		atomic_set(&data->layout, layout);
		if (!hwmsen_get_convert(layout, &data->cvt)) {
			GSE_ERR("HWMSEN_GET_CONVERT function error!\r\n");
		} else if (!hwmsen_get_convert(data->hw->direction,
						   &data->cvt)) {
			GSE_ERR("invalid layout: %d, restore to %d\n", layout,
				data->hw->direction);
		} else {
			GSE_ERR("invalid layout: (%d, %d)\n", layout,
				data->hw->direction);
			hwmsen_get_convert(0, &data->cvt);
		}
	} else {
		GSE_ERR("invalid format = '%s'\n", buf);
	}

	return count;
}

static ssize_t reg_show(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = sc7a20_i2c_client;
	int count = 0;

	char i = 0;
	char buffer = 0;

	i = 0x0f;
	/*buffer = i;*/
	sc7a20_i2c_read_block(client, 0xf, &buffer, 1);
	count += sprintf(buf, "0x%x: 0x%x\n", i, buffer);
	for (i = 0x0e; i < 0x5b; i++) {
		sc7a20_i2c_read_block(client, i, &buffer, 1);
		count += sprintf(&buf[count], "0x%x: 0x%x\n", i, buffer);
	}
	return count;
}

static ssize_t reg_store(struct device_driver *ddri, const char *buf,
			 size_t count)
{
	struct i2c_client *client = sc7a20_i2c_client;
	int address, value = 0;
	int result = 0, scanf_result;
	char databuf[2] = { 0 };

	scanf_result = sscanf(buf, "0x%x=0x%x", &address, &value);

	databuf[1] = value;
	databuf[0] = address;

	result = i2c_master_send(client, databuf, 0x2);

	if (result)
		GSE_ERR("%s:fail to write sensor_register\n", __func__);

	return count;
}

static ssize_t accelsetselftest_show(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = sc7a20_i2c_client;
	struct sc7a20_i2c_data *obj = NULL;
	int avg[3] = { 0 }, out_nost[3] = { 0 };
	int err = -1, num = 0, count = 5;
	int data_x = 0, data_y = 0, data_z = 0;
	int buff[SC7A20_BUFSIZE] = { 0 };

	accel_self_test[0] = accel_self_test[1] = accel_self_test[2] = 0;
	obj = i2c_get_clientdata(client);

	if (obj == NULL) {
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	err = SC7A20_SetPowerMode(obj_i2c_data->client, true);
	if (err) {
		GSE_ERR("enable gsensor fail: %d\n", err);
		goto SC7A20_accel_self_test_exit;
	}

	msleep(200);

	while (num < count) {
		/* read gsensor data */
		err = SC7A20_ReadSensorData(client, buff, SC7A20_BUFSIZE);
		if (err) {
			GSE_ERR("read data fail: %d\n", err);
			goto SC7A20_accel_self_test_exit;
		}
		data_x = buff[0];
		data_y = buff[1];
		data_z = buff[2];

		avg[SC7A20_AXIS_X] = data_x + avg[SC7A20_AXIS_X];
		avg[SC7A20_AXIS_Y] = data_y + avg[SC7A20_AXIS_Y];
		avg[SC7A20_AXIS_Z] = data_z + avg[SC7A20_AXIS_Z];

		num++;
		msleep(10);
	}

	out_nost[0] = avg[SC7A20_AXIS_X] / count;
	out_nost[1] = avg[SC7A20_AXIS_Y] / count;
	out_nost[2] = avg[SC7A20_AXIS_Z] / count;

	accel_self_test[0] = abs(out_nost[0]);
	accel_self_test[1] = abs(out_nost[1]);
	accel_self_test[2] = abs(out_nost[2]);

	/* disable sensor */
	err = SC7A20_SetPowerMode(obj_i2c_data->client, false);
	if (err < 0)
		goto SC7A20_accel_self_test_exit;

	return scnprintf(buf, PAGE_SIZE,
			 "[G Sensor] set_accel_self_test PASS\n");

SC7A20_accel_self_test_exit:
	SC7A20_SetPowerMode(obj_i2c_data->client, false);

	return scnprintf(buf, PAGE_SIZE, "[G Sensor] exit - Fail , err=%d\n",
			 err);
}

/******************************* add for calibration ***********************************/
static int acc_store_offset_in_file(const char *filename, s16 *offset,
					int data_valid)
{
	struct file *cali_file = NULL;
	char w_buf[SC7A20_DATA_BUF_NUM * sizeof(s16) * 2 + 1] = { 0 };
	char r_buf[SC7A20_DATA_BUF_NUM * sizeof(s16) * 2 + 1] = { 0 };
	int i = 0;
	char *dest = w_buf;
	mm_segment_t fs;

	cali_file = filp_open(filename, O_CREAT | O_RDWR, 0777);
	if (IS_ERR(cali_file)) {
		GSE_ERR("open error! exit!\n");
		return -1;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	for (i = 0; i < SC7A20_DATA_BUF_NUM; i++) {
		sprintf(dest, "%02X", offset[i] & 0x00FF);
		dest += 2;
		sprintf(dest, "%02X", (offset[i] >> 8) & 0x00FF);
		dest += 2;
	};
	GSE_INFO("w_buf: %s\n", w_buf);
	kernel_write(cali_file, (void *)w_buf,
			 SC7A20_DATA_BUF_NUM * sizeof(s16) * 2, &cali_file->f_pos);
	cali_file->f_pos = 0x00;
	kernel_read(cali_file, (void *)r_buf,
			SC7A20_DATA_BUF_NUM * sizeof(s16) * 2, &cali_file->f_pos);
	for (i = 0; i < SC7A20_DATA_BUF_NUM * sizeof(s16) * 2; i++) {
		if (r_buf[i] != w_buf[i]) {
			set_fs(fs);
			filp_close(cali_file, NULL);
			GSE_ERR("read back error! exit!\n");
			return -1;
		}
	}
	set_fs(fs);

	filp_close(cali_file, NULL);
	GSE_INFO("store_offset_in_file ok exit\n");
	return 0;
}

static ssize_t accelgetselftest_show(struct device_driver *ddri, char *buf)
{
	if (accel_self_test[0] < ACCEL_SELF_TEST_MIN_VAL ||
		accel_self_test[0] > ACCEL_SELF_TEST_MAX_VAL)
		return sprintf(buf, "X=%d , out of range\nFail\n",
				   accel_self_test[0]);

	if (accel_self_test[1] < ACCEL_SELF_TEST_MIN_VAL ||
		accel_self_test[1] > ACCEL_SELF_TEST_MAX_VAL)
		return sprintf(buf, "Y=%d , out of range\nFail\n",
				   accel_self_test[1]);

	if (accel_self_test[2] < ACCEL_SELF_TEST_MIN_VAL ||
		accel_self_test[2] > ACCEL_SELF_TEST_MAX_VAL)
		return sprintf(buf, "Z=%d , out of range\nFail\n",
				   accel_self_test[2]);
	else
		return sprintf(buf, "%d , %d , %d\nPass\n", accel_self_test[0],
				   accel_self_test[1], accel_self_test[2]);
}

static ssize_t accelsetcali_show(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = sc7a20_i2c_client;
	struct sc7a20_i2c_data *obj = NULL;
	int avg[3] = { 0, 0, 0 };
	int cali[3] = { 0, 0, 0 };
	int golden_x = 0;
	int golden_y = 0;
	int golden_z = -9800;
	int cali_last[3] = { 0, 0, 0 };
	int err = -1, num = 0, times = 20;
	int data_x = 0, data_y = 0, data_z = 0;
	int buff[SC7A20_BUFSIZE] = { 0 };

	if (client == NULL) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);
	obj->cali_sw[SC7A20_AXIS_X] = 0;
	obj->cali_sw[SC7A20_AXIS_Y] = 0;
	obj->cali_sw[SC7A20_AXIS_Z] = 0;

	err = SC7A20_SetPowerMode(client, true);
	if (err) {
		GSE_ERR("[Sensor] enable power fail!! err code %d!\n", err);
		return scnprintf(buf, PAGE_SIZE,
				 "[Sensor] enable power fail\n");
	}

	/* wait 200 for stable output */
	msleep(200);

	err = SC7A20_ResetCalibration(client);
	if (err) {
		GSE_ERR("ResetCalibration Fail: %d\n", err);
		return scnprintf(buf, PAGE_SIZE,
				 "[Sensor] ResetCalibration Fail\n");
	}

	while (num < times) {
		msleep(10);

		/* read gsensor data */
		err = SC7A20_ReadSensorData(client, buff, SC7A20_BUFSIZE);
		if (err) {
			GSE_ERR("read data fail: %d\n", err);
			return scnprintf(buf, PAGE_SIZE,
					 "[Sensor] Read data fail\n");
		}

		data_x = buff[0];
		data_y = buff[1];
		data_z = buff[2];

		if (data_z > 8500)
			golden_z = 9800;
		else if (data_z < -8500)
			golden_z = -9800;
		else
			return 0;

		avg[SC7A20_AXIS_X] = data_x + avg[SC7A20_AXIS_X];
		avg[SC7A20_AXIS_Y] = data_y + avg[SC7A20_AXIS_Y];
		avg[SC7A20_AXIS_Z] = data_z + avg[SC7A20_AXIS_Z];

		num++;
	}

	avg[SC7A20_AXIS_X] /= times;
	avg[SC7A20_AXIS_Y] /= times;
	avg[SC7A20_AXIS_Z] /= times;

	cali[SC7A20_AXIS_X] = golden_x - avg[SC7A20_AXIS_X];
	cali[SC7A20_AXIS_Y] = golden_y - avg[SC7A20_AXIS_Y];
	cali[SC7A20_AXIS_Z] = golden_z - avg[SC7A20_AXIS_Z];

	if ((abs(cali[SC7A20_AXIS_X]) >
		 abs(accel_cali_tolerance * golden_z / 100)) ||
		(abs(cali[SC7A20_AXIS_Y]) >
		 abs(accel_cali_tolerance * golden_z / 100)) ||
		(abs(cali[SC7A20_AXIS_Z]) >
		 abs(accel_cali_tolerance * golden_z / 100))) {
		GSE_INFO(
			"X/Y/Z out of range  tolerance:[%d] avg_x:[%d] avg_y:[%d] avg_z:[%d]\n",
			accel_cali_tolerance, avg[SC7A20_AXIS_X],
			avg[SC7A20_AXIS_Y], avg[SC7A20_AXIS_Z]);

		return scnprintf(
			buf, PAGE_SIZE,
			"place the Pad to a horizontal level.\ntolerance:[%d] avg_x:[%d] avg_y:[%d] avg_z:[%d]\n",
			accel_cali_tolerance, avg[SC7A20_AXIS_X],
			avg[SC7A20_AXIS_Y], avg[SC7A20_AXIS_Z]);
	}

	cali_last[0] = cali[SC7A20_AXIS_X];
	cali_last[1] = cali[SC7A20_AXIS_Y];
	cali_last[2] = cali[SC7A20_AXIS_Z];

	err = SC7A20_WriteCalibration(client, cali_last);

	if (err) {
		GSE_ERR("SC7A20_WriteCalibration!! err code %d!\n", err);
		return scnprintf(buf, PAGE_SIZE,
				 "[Sensor] SC7A20_WriteCalibration fail\n");
	}

	accel_xyz_offset[0] = (s16)cali_last[SC7A20_AXIS_X];
	accel_xyz_offset[1] = (s16)cali_last[SC7A20_AXIS_Y];
	accel_xyz_offset[2] = (s16)cali_last[SC7A20_AXIS_Z];

	err = SC7A20_SetPowerMode(client, false);
	if (err) {
		GSE_ERR("disable power fail!! err code %d!\n", err);
		return scnprintf(buf, PAGE_SIZE,
				 "[Sensor] disable power fail\n");
	}

	if (acc_store_offset_in_file(SC7A20_ACC_CALI_FILE, accel_xyz_offset,
					 1)) {
		return scnprintf(buf, PAGE_SIZE,
				 "[G Sensor] set_accel_cali ERROR %d, %d, %d\n",
				 accel_xyz_offset[0], accel_xyz_offset[1],
				 accel_xyz_offset[2]);
	}

	return scnprintf(buf, PAGE_SIZE,
			 "[G Sensor] set_accel_cali PASS  %d, %d, %d\n",
			 accel_xyz_offset[0], accel_xyz_offset[1],
			 accel_xyz_offset[2]);
}

static ssize_t accelgetcali_show(struct device_driver *ddri, char *buf)
{
	return scnprintf(
		buf, PAGE_SIZE,
		"x=%d , y=%d , z=%d\nx=0x%04x , y=0x%04x , z=0x%04x\nPass\n",
		accel_xyz_offset[0], accel_xyz_offset[1], accel_xyz_offset[2],
		accel_xyz_offset[0], accel_xyz_offset[1], accel_xyz_offset[2]);
}

static void get_accel_idme_cali(void)
{
	s16 idmedata[CALI_SIZE] = { 0 };
#if IS_ENABLED(CONFIG_IDME)
	idme_get_sensorcal(idmedata, CALI_SIZE);
#endif
	accel_xyz_offset[0] = idmedata[0];
	accel_xyz_offset[1] = idmedata[1];
	accel_xyz_offset[2] = idmedata[2];

	GSE_INFO("accel_xyz_offset =%d, %d, %d\n", accel_xyz_offset[0],
		 accel_xyz_offset[1], accel_xyz_offset[2]);
}

static ssize_t accelgetidme_show(struct device_driver *ddri, char *buf)
{
	get_accel_idme_cali();
	return scnprintf(buf, PAGE_SIZE,
			 "offset_x=%d , offset_y=%d , offset_z=%d\nPass\n",
			 accel_xyz_offset[0], accel_xyz_offset[1],
			 accel_xyz_offset[2]);
}

static ssize_t cali_tolerance_show(struct device_driver *ddri, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "accel_cali_tolerance=%d\n",
			 accel_cali_tolerance);
}

static ssize_t cali_tolerance_store(struct device_driver *ddri, const char *buf,
					size_t tCount)
{
	int temp_cali_tolerance = 0;
	int ret = 0;

	ret = kstrtoint(buf, 10, &temp_cali_tolerance);

	if (ret == 0) {
		if (temp_cali_tolerance > 100)
			temp_cali_tolerance = 100;
		if (temp_cali_tolerance <= 0)
			temp_cali_tolerance = 1;

		accel_cali_tolerance = temp_cali_tolerance;
	}

	if (ret) {
		GSE_ERR("set accel_cali_tolerance failed %d\n", ret);
		return 0;
	}
	GSE_ERR("set accel_cali_tolerance %d ok\n", accel_cali_tolerance);

	return tCount;
}

static DRIVER_ATTR_RO(chipinfo);
static DRIVER_ATTR_RO(sensordata);
static DRIVER_ATTR_RW(cali);
static DRIVER_ATTR_RW(trace);
static DRIVER_ATTR_RW(layout);
static DRIVER_ATTR_RO(status);
static DRIVER_ATTR_RW(powerstatus);
static DRIVER_ATTR_RW(reg);

/*add for diag*/
static DRIVER_ATTR_RO(accelsetselftest);
static DRIVER_ATTR_RO(accelgetselftest);
static DRIVER_ATTR_RO(accelgetcali);
static DRIVER_ATTR_RO(accelsetcali);
static DRIVER_ATTR_RO(accelgetidme);
static DRIVER_ATTR_RW(cali_tolerance);

static struct driver_attribute *sc7a20_attr_list[] = {
	&driver_attr_chipinfo, /*chip information*/
	&driver_attr_sensordata, /*dump sensor data*/
	&driver_attr_cali, /*show calibration data*/
	&driver_attr_trace, /*trace log*/
	&driver_attr_layout,
	&driver_attr_status,
	&driver_attr_powerstatus,
	&driver_attr_reg,

	/*add for diag */
	&driver_attr_accelsetselftest,
	&driver_attr_accelgetselftest,
	&driver_attr_accelsetcali,
	&driver_attr_accelgetcali,
	&driver_attr_accelgetidme,
	&driver_attr_cali_tolerance,
};

static int sc7a20_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(sc7a20_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, sc7a20_attr_list[idx]);
		if (err) {
			GSE_ERR("driver_create_file (%s) = %d\n",
				sc7a20_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int sc7a20_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(sc7a20_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, sc7a20_attr_list[idx]);

	return err;
}

static int sc7a20_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sc7a20_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	GSE_INFO("%s\n", __func__);
	if (obj == NULL) {
		GSE_ERR("null sc7a20!!\n");
		return -EINVAL;
	}
	mutex_lock(&sc7a20_mutex);
	acc_driver_pause_polling(1);
	atomic_set(&obj->suspend, 1);
	err = SC7A20_SetPowerMode(client, false);
	if (err) {
		acc_driver_pause_polling(0);
		atomic_set(&obj->suspend, 0);
		GSE_ERR("write power control fail!!\n");
		mutex_unlock(&sc7a20_mutex);
		return -EINVAL;
	}
	mutex_unlock(&sc7a20_mutex);
	return err;
}

static int sc7a20_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sc7a20_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	GSE_INFO("%s\n", __func__);
	if (obj == NULL) {
		GSE_ERR("null sc7a20!!\n");
		return -EINVAL;
	}
	mutex_lock(&sc7a20_mutex);
	if (acc_driver_query_polling_state() == 1) {
		err = SC7A20_SetPowerMode(client, true);
		if (err != SC7A20_SUCCESS) {
			GSE_ERR("Set PowerMode fail!!\n");
			mutex_unlock(&sc7a20_mutex);
			return -EINVAL;
		}
	}
	atomic_set(&obj->suspend, 0);
	acc_driver_pause_polling(0);
	mutex_unlock(&sc7a20_mutex);
	return err;
}

/* if use  this typ of enable , Gsensor should report inputEvent(x, y, z ,stats, div) to HAL */
static int sc7a20_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

/* if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL */

static int sc7a20_enable_nodata(int en)
{
	int res = 0;
	bool power = false;

	if (en == 1) {
		power = true;
		atomic_set(&open_flag, 1);
	}
	if (en == 0) {
		power = false;
		atomic_set(&open_flag, 0);
	}
	res = SC7A20_SetPowerMode(obj_i2c_data->client, power);
	if (res != SC7A20_SUCCESS) {
		GSE_ERR("SC7A20_SetPowerMode fail!\n");
		return -1;
	}
	GSE_DEBUG("%s OK en = %d sensor_power = %d\n", __func__, en,
		  sensor_power);
	return 0;
}

static int sc7a20_set_delay(u64 ns)
{
	return 0;
}

static int sc7a20_get_data(int *x, int *y, int *z, int *status)
{
	int buff[SC7A20_BUFSIZE] = { 0 };

	SC7A20_ReadSensorData(obj_i2c_data->client, buff, SC7A20_BUFSIZE);
	*x = buff[0];
	*y = buff[1];
	*z = buff[2];

	*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	return 0;
}

static int sc7a20_batch(int flag, int64_t samplingPeriodNs,
			int64_t maxBatchReportLatencyNs)
{
	return 0;
}

static int sc7a20_flush(void)
{
	int err = 0;
	/*Only flush after sensor was enabled*/
	if (!sensor_power) {
		obj_i2c_data->flush = true;
		return 0;
	}
	err = acc_flush_report();
	if (err >= 0)
		obj_i2c_data->flush = false;
	return err;
}

static int sc7a20_factory_enable_sensor(bool enabledisable,
					int64_t sample_periods_ms)
{
	int err = 0;

	err = sc7a20_enable_nodata(enabledisable == true ? 1 : 0);
	if (err) {
		GSE_ERR("%s enable sensor failed!\n", __func__);
		return -1;
	}
	err = sc7a20_batch(0, sample_periods_ms * 1000000, 0);
	if (err) {
		GSE_ERR("%s enable set batch failed!\n", __func__);
		return -1;
	}
	return 0;
}

static int sc7a20_factory_get_data(int32_t data[3], int *status)
{
	return sc7a20_get_data(&data[0], &data[1], &data[2], status);
}

static int sc7a20_factory_get_raw_data(int32_t data[3])
{
	char strbuf[SC7A20_BUF_SIZE] = { 0 };

	SC7A20_ReadRawData(sc7a20_i2c_client, strbuf);
	GSE_ERR("support %s!\n", __func__);
	return 0;
}

static int sc7a20_factory_enable_calibration(void)
{
	return 0;
}

static int sc7a20_factory_clear_cali(void)
{
	int err = 0;

	err = SC7A20_ResetCalibration(sc7a20_i2c_client);
	if (err) {
		GSE_ERR("SC7A20_ResetCalibration failed!\n");
		return -1;
	}
	return 0;
}

static int sc7a20_factory_set_cali(int32_t data[3])
{
	int err = 0;
	int cali[3] = { 0 };
	struct SENSOR_DATA nvram_cali_data = { 0 };

	nvram_cali_data.x = data[0];
	nvram_cali_data.y = data[1];
	nvram_cali_data.z = data[2];

	if (!sc7a20_i2c_client) {
		GSE_ERR("mc34xx client is NULL\n");
		return -1;
	}

	err = SC7A20_CaliConvert(&nvram_cali_data);
	if (err != 0)
		return -1;

	cali[SC7A20_AXIS_X] =
		nvram_cali_data.x * gsensor_gain.x / GRAVITY_EARTH_1000;
	cali[SC7A20_AXIS_Y] =
		nvram_cali_data.y * gsensor_gain.y / GRAVITY_EARTH_1000;
	cali[SC7A20_AXIS_Z] =
		nvram_cali_data.z * gsensor_gain.z / GRAVITY_EARTH_1000;
	err = SC7A20_WriteCalibration(sc7a20_i2c_client, cali);
	if (err) {
		GSE_ERR("SC7A20_WriteCalibration failed!\n");
		return -1;
	}
	return 0;
}

static int sc7a20_factory_get_cali(int32_t data[3])
{
	data[0] = obj_i2c_data->cali_sw[SC7A20_AXIS_X];
	data[1] = obj_i2c_data->cali_sw[SC7A20_AXIS_Y];
	data[2] = obj_i2c_data->cali_sw[SC7A20_AXIS_Z];
	return 0;
}

static int sc7a20_factory_do_self_test(void)
{
	return 0;
}

static struct accel_factory_fops sc7a20_factory_fops = {
	.enable_sensor = sc7a20_factory_enable_sensor,
	.get_data = sc7a20_factory_get_data,
	.get_raw_data = sc7a20_factory_get_raw_data,
	.enable_calibration = sc7a20_factory_enable_calibration,
	.clear_cali = sc7a20_factory_clear_cali,
	.set_cali = sc7a20_factory_set_cali,
	.get_cali = sc7a20_factory_get_cali,
	.do_self_test = sc7a20_factory_do_self_test,
};

static struct accel_factory_public sc7a20_factory_device = {
	.gain = 1,
	.sensitivity = 1,
	.fops = &sc7a20_factory_fops,
};

/*----------------------------------------------------------------------------*/
static int sc7a20_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct i2c_client *new_client = NULL;
	struct sc7a20_i2c_data *obj = NULL;
	struct acc_control_path ctl = { 0 };
	struct acc_data_path data = { 0 };
	int err = 0;

	s16 idmedata[CALI_SIZE] = {0};
	int cali_last[CALI_SIZE] = {0};

	GSE_DEBUG_FUNC();
	GSE_INFO("driver version = %s\n", DRIVER_VERSION);

	err = get_accel_dts_func(client->dev.of_node, hw);
	if (err < 0) {
		GSE_ERR("get cust_baro dts info fail\n");
		goto exit;
	}
	err = SC7A20_CheckDeviceID(client);
	if (err != SC7A20_SUCCESS) {
		err = -ENODEV;
		goto exit;
	}


#ifdef SILAN_SC7A20_FILTER
	/* configure default filter param */
	core_channel.filter_param_l = 2;
	core_channel.filter_param_h = 8;
	core_channel.filter_threhold = 50; /*4G scale: 25; 2G scale: 50*/

	{
		int j = 0;

		for (j = 0; j < 3; j++) {
			core_channel.sl_channel[j].sample_l = 0;
			core_channel.sl_channel[j].sample_h = 0;
			core_channel.sl_channel[j].flag_l = 0;
			core_channel.sl_channel[j].flag_h = 0;
		}
	}
#endif
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!(obj)) {
		err = -ENOMEM;
		goto exit;
	}

	obj->hw = hw;
	atomic_set(&obj->layout, obj->hw->direction);

	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if (err) {
		GSE_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit_init_failed;
	}

	obj_i2c_data = obj;
	obj->client = client;
	new_client = obj->client;
	i2c_set_clientdata(new_client, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);

	sc7a20_i2c_client = new_client;

	err = sc7a20_init_client(new_client, 1);
	if (err)
		goto exit_init_failed;
	err = accel_factory_device_register(&sc7a20_factory_device);
	if (err) {
		GSE_ERR("sc7a20_device register failed\n");
		goto exit_misc_device_register_failed;
	}
	ctl.is_use_common_factory = false;
	err = sc7a20_create_attr(&sc7a20_init_info.platform_diver_addr->driver);
	if (err) {
		GSE_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data = sc7a20_open_report_data;
	ctl.enable_nodata = sc7a20_enable_nodata;
	ctl.set_delay = sc7a20_set_delay;
	ctl.batch = sc7a20_batch;
	ctl.flush = sc7a20_flush;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = obj->hw->is_batch_supported;
	err = acc_register_control_path(&ctl);
	if (err) {
		GSE_ERR("register acc control path err\n");
		goto exit_kfree;
	}

	data.get_data = sc7a20_get_data;
	data.vender_div = 1000;
	err = acc_register_data_path(&data);
	if (err) {
		GSE_ERR("register acc data path err\n");
		goto exit_kfree;
	}

#if IS_ENABLED(CONFIG_IDME)
	err = idme_get_sensorcal(idmedata, CALI_SIZE);
	if (err)
		GSE_ERR("Get gsensor offset fail,default offset x=y=z=0\n");
#endif

	cali_last[0] = idmedata[0];
	cali_last[1] = idmedata[1];
	cali_last[2] = idmedata[2];

	accel_xyz_offset[0] = cali_last[0];
	accel_xyz_offset[1] = cali_last[1];
	accel_xyz_offset[2] = cali_last[2];
	if ((idmedata[0] != 0) || (idmedata[1] != 0) || (idmedata[2] != 0)) {
		err = SC7A20_WriteCalibration(client, cali_last);

		if (err)
			GSE_ERR("SC7A20_WriteCalibration err= %d\n", err);
	}

	sc7a20_init_flag = 0;
	GSE_INFO("%s: OK\n", __func__);
	return 0;

exit_kfree:
	sc7a20_delete_attr(
		&(sc7a20_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
	accel_factory_device_deregister(&sc7a20_factory_device);
exit_misc_device_register_failed:
exit_init_failed:
	kfree(obj);
	obj = NULL;
exit:
	GSE_ERR("%s: err = %d\n", __func__, err);
	new_client = NULL;
	obj_i2c_data = NULL;
	sc7a20_i2c_client = NULL;
	sc7a20_init_flag = -1;
	return err;
}

static int sc7a20_i2c_remove(struct i2c_client *client)
{
	int err = sc7a20_delete_attr(
		&sc7a20_init_info.platform_diver_addr->driver);
	if (err)
		GSE_ERR("sc7a20_delete_attr fail: %d\n", err);

	sc7a20_i2c_client = NULL;
	i2c_unregister_device(client);
	accel_factory_device_deregister(&sc7a20_factory_device);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static int sc7a20_local_init(void)
{
	GSE_DEBUG_FUNC();
	sc7a20_power(hw, 1);

	if (i2c_add_driver(&sc7a20_i2c_driver)) {
		GSE_ERR("add driver error\n");
		return -1;
	}
	if (-1 == sc7a20_init_flag) {
		GSE_ERR("%s failed sc7a20_init_flag=%d\n", __func__,
			sc7a20_init_flag);
		return -1;
	}
	return 0;
}

static int sc7a20_remove(void)
{
	GSE_DEBUG_FUNC();
	sc7a20_power(hw, 0);
	i2c_del_driver(&sc7a20_i2c_driver);
	return 0;
}

static int __init sc7a20_driver_init(void)
{
	GSE_DEBUG_FUNC();
	acc_driver_add(&sc7a20_init_info);

	mutex_init(&sc7a20_mutex);

	return 0;
}

static void __exit sc7a20_driver_exit(void)
{
	GSE_DEBUG_FUNC();
	mutex_destroy(&sc7a20_mutex);
}

module_init(sc7a20_driver_init);
module_exit(sc7a20_driver_exit);

MODULE_AUTHOR("Jianqing Dong<dongjianqing@silan.com.cn>");
MODULE_DESCRIPTION("SILAN SC7A20 Accelerometer Sensor Driver");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
