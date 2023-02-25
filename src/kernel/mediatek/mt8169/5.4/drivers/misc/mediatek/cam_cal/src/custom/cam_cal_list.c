// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"

#define CAM_CAL_DEBUG
#ifdef CAM_CAL_DEBUG
#include <linux/kern_levels.h>
#define PFX "cam_cal_list"

#define CAM_CALINF(format, args...) \
	pr_info(PFX "[%s] " format, __func__, ##args)
#define CAM_CALDB(format, args...) \
	pr_err(PFX "[%s] " format, __func__, ##args)

#else
#define CAM_CALINF(x, ...)
#define CAM_CALDB(x, ...)
#endif

#define MAX_EEPROM_SIZE_16K 0x4000
#define IDME_OF_FRONT_CAM_OTP "/idme/front_cam_otp"
#define IDME_OF_REAR_CAM_OTP "/idme/rear_cam_otp"

/* otp data is 2048 bytes at most,
  but needs to save as string, so need 2048*2=4096bytes */
#define CAM_OTP_DATA_LEN 2048
/* mtk otp data length */
#define CAM_MTK_OTP_LEN 1895
unsigned char front_cam_otp[CAM_OTP_DATA_LEN];
unsigned char rear_cam_otp[CAM_OTP_DATA_LEN];
struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/*Below is commom sensor */
	{GC02M1_RASPITE_CXT_REAR_SENSOR_ID, 0xA0, cam_cal_rear_read_region, MAX_EEPROM_SIZE_16K},
	{GC02M1_RASPITE_JK_REAR_SENSOR_ID, 0xA0, cam_cal_rear_read_region, MAX_EEPROM_SIZE_16K},
	{OV02B10_RASPITE_SJC_REAR_SENSOR_ID, 0xA0, cam_cal_rear_read_region, MAX_EEPROM_SIZE_16K},
	{GC5035_RASPITE_LCE_REAR_SENSOR_ID, 0xA0, cam_cal_rear_read_region, MAX_EEPROM_SIZE_16K},
	{HI556_RASPITE_HLT_REAR_SENSOR_ID, 0xA0, cam_cal_rear_read_region, MAX_EEPROM_SIZE_16K},
	{SC202CS_RASPITE_LCE_REAR_SENSOR_ID, 0xA0, cam_cal_rear_read_region, MAX_EEPROM_SIZE_16K},
	{OV02B10_RASPITE_CXT_FRONT_SENSOR_ID, 0xA0, cam_cal_front_read_region, MAX_EEPROM_SIZE_16K},
	{GC02M1_RASPITE_JK_FRONT_SENSOR_ID, 0xA0, cam_cal_front_read_region, MAX_EEPROM_SIZE_16K},
	{OV02B10_RASPITE_SJC_FRONT_SENSOR_ID, 0xA0, cam_cal_front_read_region, MAX_EEPROM_SIZE_16K},
	{OV48B_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{GC8054_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K3P9SP_SENSOR_ID, 0xA8, Common_read_region},
	{GC02M0_SENSOR_ID, 0xA8, Common_read_region},
	{IMX586_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX576_SENSOR_ID, 0xA2, Common_read_region},
	{IMX519_SENSOR_ID, 0xA0, Common_read_region},
	{IMX319_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K3M5SX_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX686_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{HI846_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5KGD1SP_SENSOR_ID, 0xA8, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K2T7SP_SENSOR_ID, 0xA4, Common_read_region},
	{IMX386_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2L7_SENSOR_ID, 0xA0, Common_read_region},
	{IMX398_SENSOR_ID, 0xA0, Common_read_region},
	{IMX350_SENSOR_ID, 0xA0, Common_read_region},
	{IMX386_MONO_SENSOR_ID, 0xA0, Common_read_region},
	{IMX499_SENSOR_ID, 0xA0, Common_read_region},
	/*  ADD before this line */
	{0, 0, 0}       /*end of list */
};

unsigned int cam_cal_get_sensor_list(
	struct stCAM_CAL_LIST_STRUCT **ppCamcalList)
{
	if (ppCamcalList == NULL)
		return 1;

	*ppCamcalList = &g_camCalList[0];
	return 0;
}

static int idme_cam_otp_checksum(unsigned char *data)
{
	int i;
	unsigned int cal_sum = 0;
	unsigned char *p = (char *)&cal_sum;

	if (data == NULL) {
		CAM_CALDB("%s data is null\n", __func__);
		return -1;
	}

	/* calculating sum, adding all otp data by byte */
	for (i = 0; i < CAM_MTK_OTP_LEN - 4; i++)
		cal_sum += data[i];

	/* checksum */
	for (i = 0; i < 4; i++) {
		if (p[i] != data[CAM_MTK_OTP_LEN - 1 - i]) {
			CAM_CALDB("%s camera otp checksum failed\n", __func__);
			return -1;
		}
		else
			CAM_CALDB("%s camera otp checksum successed\n", __func__);
	}

	return 0;
}

static int idme_get_front_cam_otp(unsigned char *data)
{
	struct device_node *ap = NULL;
	const unsigned char *idme_data = NULL;
	char buf[3] = {0};
	int i;
	int len = 0;
	int ret = 0;

	if (data == NULL) {
		CAM_CALDB("%s data is null\n",__func__);
		return -1;
	}

	ap = of_find_node_by_path(IDME_OF_FRONT_CAM_OTP);
	if (ap) {
		idme_data = (const unsigned char *)of_get_property(ap, "value", &len);
		if (likely(len > 0 && len <= CAM_OTP_DATA_LEN*2)) {
			for (i = 0; i < (CAM_MTK_OTP_LEN*2 - 1); i += 2) {
				buf[0] = idme_data[i];
				buf[1] = idme_data[i + 1];
				ret = kstrtou8(buf, 16, data+(i/2));
				if (ret)
					CAM_CALDB("%s kstrtou8 failed, i=%d\n", __func__, i);
			}
		} else {
			CAM_CALDB("%s front cam otp len err=%d\n", __func__, len);
			return -1;
		}
	} else {
		CAM_CALDB("%s of_find_node_by_path failed\n",__func__);
		return -1;
	}

	return ret;
}

static int idme_get_rear_cam_otp(unsigned char *data)
{
	struct device_node *ap = NULL;
	const unsigned char *idme_data = NULL;
	char buf[3] = {0};
	int i;
	int len = 0;
	int ret = 0;

	if (data == NULL) {
		CAM_CALDB("%s data is null\n",__func__);
		return -1;
	}

	ap = of_find_node_by_path(IDME_OF_REAR_CAM_OTP);
	if (ap) {
		idme_data = (const unsigned char *)of_get_property(ap, "value", &len);
		if (likely(len > 0 && len <= CAM_OTP_DATA_LEN*2)) {
			for (i = 0; i < (CAM_MTK_OTP_LEN*2 - 1); i += 2) {
				buf[0] = idme_data[i];
				buf[1] = idme_data[i + 1];
				ret = kstrtou8(buf, 16, data+(i/2));
				if (ret)
					CAM_CALDB("%s kstrtou8 failed, i=%d\n", __func__, i);
			}
		} else {
			CAM_CALDB("%s rear cam otp len err=%d\n", __func__, len);
			return -1;
		}
	} else {
		CAM_CALDB("%s of_find_node_by_path failed\n",__func__);
		return -1;
	}

	return ret;
}

unsigned int cam_cal_front_read_region(
	struct i2c_client *client, unsigned int addr,
	unsigned char *data, unsigned int size)
{
	int err = 0;

	if (data == NULL)
		return 0;

	if ((addr + size) > CAM_MTK_OTP_LEN)
		return 0;

	err = idme_get_front_cam_otp(front_cam_otp);
	if (err == 0 && idme_cam_otp_checksum(front_cam_otp) == 0) {
		memcpy(data, front_cam_otp+addr, size);
		return size;
	}

	return 0;
}

unsigned int cam_cal_rear_read_region(
	struct i2c_client *client, unsigned int addr,
	unsigned char *data, unsigned int size)
{
	int err = 0;

	if (data == NULL)
		return 0;

	if ((addr + size) > CAM_MTK_OTP_LEN)
		return 0;

	err = idme_get_rear_cam_otp(rear_cam_otp);
	if (err == 0 && idme_cam_otp_checksum(rear_cam_otp) == 0) {
		memcpy(data, rear_cam_otp+addr, size);
		return size;
	}

	return 0;
}