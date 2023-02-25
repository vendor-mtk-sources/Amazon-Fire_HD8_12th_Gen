/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CAM_CAL_LIST_H
#define __CAM_CAL_LIST_H
#include <linux/i2c.h>

#define DEFAULT_MAX_EEPROM_SIZE_8K 0x2000

typedef unsigned int (*cam_cal_cmd_func) (struct i2c_client *client,
	unsigned int addr, unsigned char *data, unsigned int size);

struct stCAM_CAL_LIST_STRUCT {
	unsigned int sensorID;
	unsigned int slaveID;
	cam_cal_cmd_func readCamCalData;
	unsigned int maxEepromSize;
	cam_cal_cmd_func writeCamCalData;
};

unsigned int cam_cal_front_read_region(
	struct i2c_client *client, unsigned int addr,
	unsigned char *data, unsigned int size);

unsigned int cam_cal_rear_read_region(
	struct i2c_client *client, unsigned int addr,
	unsigned char *data, unsigned int size);

#ifdef CONFIG_abc123_SENSOR_OTP
unsigned int hi556_tung_lce_r_read_region(
	struct i2c_client *client, unsigned int addr,
	unsigned char *data, unsigned int size);

unsigned int hi556_tung_lce_f_read_region(
	struct i2c_client *client, unsigned int addr,
	unsigned char *data, unsigned int size);

unsigned int gc05a2_tung_hlt_r_read_region(struct i2c_client *client,
				unsigned int addr,
				unsigned char *data,
				unsigned int size);

unsigned int gc05a2_tung_txd_f_read_region(struct i2c_client *client,
				unsigned int addr,
				unsigned char *data,
				unsigned int size);

unsigned int hi556_tung_lce3_r_read_region(
	struct i2c_client *client, unsigned int addr,
	unsigned char *data, unsigned int size);

unsigned int hi556_tung_lce3_f_read_region(
	struct i2c_client *client, unsigned int addr,
	unsigned char *data, unsigned int size);
#endif //CONFIG_abc123_SENSOR_OTP

unsigned int cam_cal_get_sensor_list
		(struct stCAM_CAL_LIST_STRUCT **ppCamcalList);

#endif				/* __CAM_CAL_LIST_H */
