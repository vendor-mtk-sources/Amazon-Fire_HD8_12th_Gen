/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CUST_ACC_H__
#define __CUST_ACC_H__

#include <linux/of.h>
#include <linux/types.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#define G_CUST_I2C_ADDR_NUM 2
#define HVT_U2604_DIRECTION 5  /* U2604 atch direction index */
#define HVT_U2600_DIRECTION 6  /* U2600 atch direction index */

struct acc_hw {
	int i2c_num;   /*!< the i2c bus used by the chip */
	int direction; /*!< the direction of the chip */
	int power_id;  /*!< the VDD LDO ID of the chip */
	int power_vol; /*!< the VDD Power Voltage used by the chip */
	int firlen;    /*!< the length of low pass filter */
	int (*power)(struct acc_hw *hw, unsigned int on, char *devname);
	/*!< i2c address list,for chips which has different addresses with
	 * different HW layout.
	 */
	unsigned char i2c_addr[G_CUST_I2C_ADDR_NUM];
	int power_vio_id;  /*!< the VIO LDO ID of the chip */
	int power_vio_vol; /*!< the VIO Power Voltage used by the chip */
	bool is_batch_supported;
};

extern int get_accel_dts_func(struct device_node *node, struct acc_hw *hw);
extern int check_direction_by_gpio(int *direction,struct device_node *node);
#endif
