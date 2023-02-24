/*
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/sysfs.h>
#include <linux/sched/clock.h>

#include "cust_alsps.h"
#include "ltr578.h"
#include "alsps.h"
#include <hwmsen_helper.h>

#define SENSOR_ALS_ENABLED

/******************************************************************************
 * configuration
*******************************************************************************/
/*----------------------------------------------------------------------------*/
#define LTR578_DEV_NAME "ltr578"

/*----------------------------------------------------------------------------*/
#define APS_TAG "[ltr578] "
#define APS_FUN(f) pr_info(APS_TAG "%s\n", __func__)
#define APS_ERR(fmt, args...) \
	pr_info(KERN_ERR APS_TAG "%s %d : " fmt, __FUNCTION__, __LINE__, ##args)
#define APS_LOG(fmt, args...) pr_info(APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...) pr_debug(APS_TAG fmt, ##args)
#define DRIVER_ATTR(_name, _mode, _show, _store) \
	struct driver_attribute driver_attr_##_name = \
		__ATTR(_name, _mode, _show, _store)
#define IS_SPACE(CH) (((CH) == ' ') || ((CH) == '\n'))

#define DEF_HIGH_LUX_THRESHOLD (400)
#define DEF_LOW_LUX_THRESHOLD (20)

#define ALS_CAL_FILE "/data/als_cal_data.bin"
#define LTR_DATA_BUF_NUM 1
#define ENLARGE_TEN_TIMES 10
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id ltr578_i2c_id[] = { { LTR578_DEV_NAME, 0 },
							  {} };
struct alsps_hw alsps_cust;
static struct alsps_hw *hw = &alsps_cust;

/*----------------------------------------------------------------------------*/
static int ltr578_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id);
static int ltr578_i2c_remove(struct i2c_client *client);
static int ltr578_i2c_detect(struct i2c_client *client,
				 struct i2c_board_info *info);
static int ltr578_i2c_suspend(struct device *dev);
static int ltr578_i2c_resume(struct device *dev);

#if IS_ENABLED(CONFIG_IDME)
extern unsigned int idme_get_alscal_value(void);
static void get_als_cali(void);
#endif

#ifdef LTR578_DEBUG
static int ltr578_dump_reg(void);
#endif

static int als_gainrange;
static int int_fac;
static int final_lux_val;

/*----------------------------------------------------------------------------*/
typedef enum {
	CMC_BIT_ALS = 1,
	CMC_BIT_PS = 2,
} CMC_BIT;

enum ALS_TRC {
	ALS_TRC_INFO = 0X01,
	ALS_TRC_RAWDATA = 0x02,
};
/*----------------------------------------------------------------------------*/
struct ltr578_priv {
	struct alsps_hw *hw;
	struct i2c_client *client;
	struct work_struct eint_work;

	/* misc */
	u16 als_modulus;
	atomic_t i2c_retry;
	atomic_t als_suspend;
	atomic_t als_debounce; /*debounce time after enabling als*/
	atomic_t als_deb_on; /*indicates if the debounce is on*/
	atomic_t als_deb_end; /*the jiffies representing the end of debounce*/
	atomic_t trace;

#ifdef CONFIG_OF
	struct device_node *irq_node;
	int irq;
#endif

	/* data */
	u16 als;
	u8 _align;
	u16 als_level_num;
	u16 als_value_num;
	u32 als_level[C_CUST_ALS_LEVEL - 1];
	u32 als_value[C_CUST_ALS_LEVEL];

	atomic_t als_cmd_val; /*the cmd value can't be read, stored in ram*/
	atomic_t als_thd_val_high; /*the cmd value can't be read, stored in ram*/
	atomic_t als_thd_val_low; /*the cmd value can't be read, stored in ram*/
	ulong enable; /*enable mask*/
	ulong pending_intr; /*pending interrupt*/

	u32 lux_threshold;	/* The ALS calibration threshold for Diag . Default Value 400. */
	u32 als_cal_high_reading;
	u32 als_cal_low_reading;
};

static uint32_t als_cal;

static struct ltr578_priv *ltr578_obj;
static struct i2c_client *ltr578_i2c_client;

static DEFINE_MUTEX(ltr578_mutex);
static DEFINE_MUTEX(ltr578_i2c_mutex);

static const uint32_t max_uint16 = 65535;

static int ltr578_local_init(void);
static int ltr578_remove(void);
static int ltr578_init_flag = -1; /* 0<==>OK -1 <==> fail */
static int als_enabled;

static struct alsps_init_info ltr578_init_info = {
	.name = "ltr578",
	.init = ltr578_local_init,
	.uninit = ltr578_remove,
};

#ifdef CONFIG_OF
static const struct of_device_id alsps_of_match[] = {
	{ .compatible = "mediatek,alsps" },
	{},
};
#endif

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops ltr578_pm_ops = { SET_SYSTEM_SLEEP_PM_OPS(
	ltr578_i2c_suspend, ltr578_i2c_resume) };
#endif

static struct i2c_driver ltr578_i2c_driver = {
	.probe	  = ltr578_i2c_probe,
	.remove	 = ltr578_i2c_remove,
	.detect	 = ltr578_i2c_detect,
	.id_table   = ltr578_i2c_id,
	.driver = {
		.name		   = LTR578_DEV_NAME,
#ifdef CONFIG_OF
		.of_match_table = alsps_of_match,
#endif

#ifdef CONFIG_PM_SLEEP
		.pm = &ltr578_pm_ops,
#endif
	},
};

/*-----------------------------------------------------------------------------*/

/*
 * #########
 * ## I2C ##
 * #########
 */
static int ltr578_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data,
				 u8 len)
{
	int err = 0;
	u8 beg = addr;
	struct i2c_msg msgs[2] = { {
					   .addr = client->addr,
					   .flags = 0,
					   .len = 1,
					   .buf = &beg,
				   },
				   {
					   .addr = client->addr,
					   .flags = I2C_M_RD,
					   .len = len,
					   .buf = data,
				   } };

	mutex_lock(&ltr578_i2c_mutex);
	if (!client) {
		mutex_unlock(&ltr578_i2c_mutex);
		APS_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	} else if (len > C_I2C_FIFO_SIZE) {
		mutex_unlock(&ltr578_i2c_mutex);
		APS_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	err = i2c_transfer(client->adapter, msgs,
			   sizeof(msgs) / sizeof(msgs[0]));
	mutex_unlock(&ltr578_i2c_mutex);
	if (err != 2) {
		APS_ERR("i2c_transfer error: (%d %p %d) %d\n", addr, data, len,
			err);
		err = -EIO;
	} else {
		err = 0; /*no error */
	}
	return err;
}

static int ltr578_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data,
				  u8 len)
{
	int err, idx, num = 0;
	char buf[C_I2C_FIFO_SIZE];

	err = 0;
	mutex_lock(&ltr578_i2c_mutex);
	if (!client) {
		APS_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&ltr578_i2c_mutex);
		return -EINVAL;
	} else if (len >= C_I2C_FIFO_SIZE) {
		APS_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&ltr578_i2c_mutex);
		return -EINVAL;
	}

	num = 0;
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++) {
		buf[num++] = data[idx];
	}

	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		APS_ERR("send command error!!\n");
		mutex_unlock(&ltr578_i2c_mutex);
		return -EFAULT;
	}
	mutex_unlock(&ltr578_i2c_mutex);
	return err;
}

/*----------------------------------------------------------------------------*/
static int ltr578_master_recv(struct i2c_client *client, u16 addr, u8 *buf,
				  int count)
{
	int ret = 0, retry = 0;
	int trc = atomic_read(&ltr578_obj->trace);
	int max_try = atomic_read(&ltr578_obj->i2c_retry);

	while (retry++ < max_try) {
		ret = ltr578_i2c_read_block(client, addr, buf, count);
		if (ret == 0)
			break;
		udelay(100);
	}

	if (unlikely(trc)) {
		if ((retry != 1) && (trc & 0x8000)) {
			APS_LOG("(recv) %d/%d\n", retry - 1, max_try);
		}
	}

	/* If everything went ok (i.e. 1 msg transmitted), return #bytes
	transmitted, else error code. */
	return (ret == 0) ? count : ret;
}

/*----------------------------------------------------------------------------*/
static int ltr578_master_send(struct i2c_client *client, u16 addr, u8 *buf,
				  int count)
{
	int ret = 0, retry = 0;
	int trc = atomic_read(&ltr578_obj->trace);
	int max_try = atomic_read(&ltr578_obj->i2c_retry);

	while (retry++ < max_try) {
		ret = ltr578_i2c_write_block(client, addr, buf, count);
		if (ret == 0)
			break;
		udelay(100);
	}

	if (unlikely(trc)) {
		if ((retry != 1) && (trc & 0x8000)) {
			APS_LOG("(send) %d/%d\n", retry - 1, max_try);
		}
	}
	/* If everything went ok (i.e. 1 msg transmitted), return #bytes
	transmitted, else error code. */
	return (ret == 0) ? count : ret;
}

/*----------------------------------------------------------------------------*/
static int ltr578_set_gain(int init_als_gain)
{
	int res= 0;
	u8 buf = 0;

	struct i2c_client *client = ltr578_obj->client;
	als_gainrange = init_als_gain; /* Set global variable */

	APS_LOG("ALS sensor gainrange %d!\n", init_als_gain);

	switch (als_gainrange) {
	case ALS_RANGE_1:
		buf = MODE_ALS_Range1;
		res = ltr578_master_send(client, LTR578_ALS_GAIN, &buf,
					 1);
		break;

	case ALS_RANGE_3:
		buf = MODE_ALS_Range3;
		res = ltr578_master_send(client, LTR578_ALS_GAIN, &buf,
					 1);
		break;

	case ALS_RANGE_6:
		buf = MODE_ALS_Range6;
		res = ltr578_master_send(client, LTR578_ALS_GAIN, &buf,
					 1);
		break;

	case ALS_RANGE_9:
		buf = MODE_ALS_Range9;
		res = ltr578_master_send(client, LTR578_ALS_GAIN, &buf,
					 1);
		break;

	case ALS_RANGE_18:
		buf = MODE_ALS_Range18;
		res = ltr578_master_send(client, LTR578_ALS_GAIN, &buf,
					 1);
		break;

	default:
		buf = MODE_ALS_Range3;
		res = ltr578_master_send(client, LTR578_ALS_GAIN, &buf,
					 1);
		break;
	}
	if (res < 0) {
		APS_ERR("set gain fail: %d\n", res);
	}

	return res;
}

/********************************************************************/
/*
 * ################
 * ## ALS CONFIG ##
 * ################
 */

static int ltr578_als_enable(struct i2c_client *client, int enable)
{
	int err = 0;
	u8 regdata = 0;

	if (enable != 0 && als_enabled == 1) {
		APS_LOG("ALS: Already enabled \n");
		return 0;
	}

	if (enable == 0 && als_enabled == 0) {
		APS_LOG("ALS: Already disabled \n");
		return 0;
	}

	err = ltr578_master_recv(client, LTR578_MAIN_CTRL, &regdata, 0x01);
	if (err < 0) {
		APS_ERR("i2c error: %d\n", err);
	}

	regdata &= 0xEF; /* Clear reset bit */

	if (enable != 0) {
		APS_LOG("ALS(1): enable als only \n");
		regdata |= 0x02;
	} else {
		APS_LOG("ALS(1): disable als only \n");
		regdata &= 0xFD;
	}

	err = ltr578_master_send(client, LTR578_MAIN_CTRL, (char *)&regdata, 1);
	if (err < 0) {
		APS_ERR("ALS: enable als err: %d en: %d \n", err, enable);
		return err;
	}

	msleep(WAKEUP_DELAY);

	if (enable != 0)
		als_enabled = 1;
	else
		als_enabled = 0;

	return 0;
}

static int ltr578_als_read(struct i2c_client *client, u16 *data, bool is_calibration)
{
	int alsval = 0, clearval = 0;
	int luxdata_int, cal_factor, ratio, res = 0;
	u8 buf[3] = {0};

	res = ltr578_master_recv(client, LTR578_ALS_DATA_0, buf, 0x03);
	if (res < 0) {
		luxdata_int = 0;
		APS_ERR("i2c error: %d\n", res);
		goto out;
	}

	alsval = (buf[2] * 256 * 256) + (buf[1] * 256) + buf[0];
	res = ltr578_master_recv(client, LTR578_CLEAR_DATA_0, buf, 0x03);
	if (res < 0) {
		luxdata_int = 0;
		APS_ERR("i2c error: %d\n", res);
		goto out;
	}

	clearval = (buf[2] * 256 * 256) + (buf[1] * 256) + buf[0];
	if (alsval == 0) {
		luxdata_int = 0;
		goto out;
	}

	ratio = clearval * 100 / (alsval + 1);

	if (ratio <= 89) {
		/* CFL3000, 4000, 6500, LED6500 */
		cal_factor = 337;
	} else if (ratio <= 126) {
		/* D6500, LED4500, LED6500 */
		cal_factor = 374;
	} else if (ratio <= 300) {
		/* LED2700 */
		cal_factor = 374;
	} else {
		/* H2700 */
		cal_factor = 460;
	}
	if (is_calibration == true) {
		luxdata_int = alsval * cal_factor * ENLARGE_TEN_TIMES / 100 / als_gainrange / int_fac;
	} else {
		luxdata_int = alsval * cal_factor / 100 / als_gainrange / int_fac;
	}

	if (atomic_read(&ltr578_obj->trace) & ALS_TRC_RAWDATA) {
		APS_LOG("%s: alsval = %d\n", __func__, alsval);
		APS_LOG("%s: clearval = %d\n", __func__, clearval);
		APS_LOG("%s: ratio = %d\n", __func__, ratio);
		APS_LOG("%s: cal_factor = %d\n", __func__, cal_factor);
		APS_LOG("%s: als_gainrange = %d\n", __func__, als_gainrange);
		APS_LOG("%s: int_fac = %d\n", __func__, int_fac);
		APS_LOG("%s: luxdata_int = %d\n", __func__, luxdata_int);
	}
out:
	*data = luxdata_int;
	final_lux_val = luxdata_int;
	return res;
}
/*-------------------------------attribute file for debugging----------------------------------*/

/******************************************************************************
 * Sysfs attributes
*******************************************************************************/

inline uint32_t ltr_alscode2lux(uint32_t alscode)
{
	int32_t lux, als_cal_high_reading, als_cal_low_reading = 0;
	int32_t def_high, def_low = 0;

	if (alscode <= 0) {
		return 0;
	}

	if (alscode > 56000) {
		alscode = 56000;
	}

	lux = (int)alscode * ENLARGE_TEN_TIMES;
	als_cal_high_reading = (int)ltr578_obj->als_cal_high_reading;
	als_cal_low_reading = (int)ltr578_obj->als_cal_low_reading;
	def_high = (int)DEF_HIGH_LUX_THRESHOLD * ENLARGE_TEN_TIMES;
	def_low = (int)DEF_LOW_LUX_THRESHOLD * ENLARGE_TEN_TIMES;

	if (atomic_read(&ltr578_obj->trace) & ALS_TRC_RAWDATA) {
		APS_LOG("%s: lux = %d\n", __func__, lux);
		APS_LOG("%s: als_cal_high_reading = %d\n", __func__, als_cal_high_reading);
		APS_LOG("%s: als_cal_low_reading = %d\n", __func__, als_cal_low_reading);
	}

	if (ltr578_obj->als_cal_high_reading > 0 && ltr578_obj->als_cal_low_reading > 0) {
		lux = (def_high - def_low) * (lux - als_cal_low_reading) /
		(als_cal_high_reading - als_cal_low_reading) + (int)def_low;
	} else {
		return alscode;
	}

	lux = (lux + 5) / ENLARGE_TEN_TIMES;
	return (uint32_t)lux;
}

/*----------------------------------------------------------------------------*/
static inline int32_t ltr578_als_get_data_avg(int sSampleNo, bool is_calibration)
{
	int32_t DataCount = 0;
	int32_t sAveAlsData = 0;

	u16 als_reading = 0;
	int result = 0;

	struct ltr578_priv *obj = NULL;

	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return -1;
	}
	obj = ltr578_obj;

	result = ltr578_als_read(obj->client, &als_reading, is_calibration);
	APS_LOG("[%s]: Ignore first als value:%d\n", __func__, als_reading);

	while (DataCount < sSampleNo) {
		msleep(100);
		result = ltr578_als_read(obj->client, &als_reading, is_calibration);
		if (atomic_read(&ltr578_obj->trace) & ALS_TRC_INFO) {
			APS_LOG("%s: als_reading = %d\n", __func__, als_reading);
		}
		sAveAlsData += als_reading;
		DataCount++;
	}
	sAveAlsData = ((sAveAlsData * 10) + 5 * sSampleNo) / 10 / sSampleNo;
	if (atomic_read(&ltr578_obj->trace) & ALS_TRC_INFO) {
			APS_LOG("%s: sAveAlsData = %d\n", __func__, sAveAlsData);
	}
	return sAveAlsData;
}

/*----------------------------------------------------------------------------*/
static bool als_store_cali_in_file(const char *filename,
						 unsigned int value, unsigned int value_low)
{
	struct file *cali_file;
	mm_segment_t fs;
	char w_buf[LTR_DATA_BUF_NUM * sizeof(unsigned int) * 2 + 1] = { 0 };
	char r_buf[LTR_DATA_BUF_NUM * sizeof(unsigned int) * 2 + 1] = { 0 };
	int i;
	char *dest = w_buf;

	APS_LOG("%s enter", __func__);
	cali_file = filp_open(filename, O_CREAT | O_RDWR, 0777);

	if (IS_ERR(cali_file)) {
		APS_ERR("open error! exit!\n");
		return false;
	} else {
		fs = get_fs();
		set_fs(KERNEL_DS);

		for (i = 0; i < LTR_DATA_BUF_NUM; i++) {
			sprintf(dest, "%02X", value & 0x000000FF);
			dest += 2;
			sprintf(dest, "%02X", (value >> 8) & 0x000000FF);
			dest += 2;
			sprintf(dest, "%02X", value_low & 0x000000FF);
			dest += 2;
			sprintf(dest, "%02X", (value_low >> 8) & 0x000000FF);
		}

		APS_LOG("w_buf: %s \n", w_buf);
		kernel_write(cali_file, (void *)w_buf,
				LTR_DATA_BUF_NUM * sizeof(unsigned int) * 2 + 1, &cali_file->f_pos);
		cali_file->f_pos = 0x00;
		kernel_read(cali_file, (void *)r_buf,
				LTR_DATA_BUF_NUM * sizeof(unsigned int) * 2 + 1, &cali_file->f_pos);

		for (i = 0; i < LTR_DATA_BUF_NUM * sizeof(unsigned int) * 2 + 1; i++) {
			if (r_buf[i] != w_buf[i]) {
				set_fs(fs);
				filp_close(cali_file, NULL);
				APS_ERR("read back error! exit!\n");
				return false;
			}
		}

		set_fs(fs);
	}

	filp_close(cali_file, NULL);
	APS_LOG("pass\n");
	return true;
}

static ssize_t ltr578_show_config(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;

	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return 0;
	}

	res = scnprintf(buf, PAGE_SIZE, "(%d %d)\n",
			   atomic_read(&ltr578_obj->i2c_retry),
			   atomic_read(&ltr578_obj->als_debounce));
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t ltr578_store_config(struct device_driver *ddri, const char *buf,
				   size_t count)
{
	int retry, als_deb = 0;
	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return 0;
	}

	if (2 == sscanf(buf, "%d %d", &retry, &als_deb)) {
		atomic_set(&ltr578_obj->i2c_retry, retry);
		atomic_set(&ltr578_obj->als_debounce, als_deb);
	} else {
		APS_ERR("invalid content: '%s', length = %zu\n", buf, count);
	}
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t ltr578_show_trace(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;
	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return 0;
	}

	res = scnprintf(buf, PAGE_SIZE, "0x%04X\n",
			   atomic_read(&ltr578_obj->trace));
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t ltr578_store_trace(struct device_driver *ddri, const char *buf,
				  size_t count)
{
	int trace = 0;
	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&ltr578_obj->trace, trace);
	} else {
		APS_ERR("invalid content: '%s', length = %zu\n", buf, count);
	}
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t ltr578_show_als(struct device_driver *ddri, char *buf)
{
	int res = 0;

	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return 0;
	}
	res = ltr578_als_read(ltr578_obj->client, &ltr578_obj->als, false);
	return scnprintf(buf, PAGE_SIZE, "0x%04X(%d)\n", ltr578_obj->als, res);
}

/*----------------------------------------------------------------------------*/
static ssize_t ltr578_show_reg(struct device_driver *ddri, char *buf)
{
	int i, len = 0;
	int reg[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
			  0x09, 0x0d, 0x0e, 0x0f, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
			  0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26 };
	int ret = 0;
	u8 buffer = 0;

	for (i = 0; i < 27; i++) {
		ret = ltr578_master_recv(ltr578_obj->client, reg[i], &buffer,
					 0x01);
		if (ret < 0) {
			APS_ERR("i2c error: %d\n", ret);
		}
		len += scnprintf(buf + len, PAGE_SIZE - len,
				"reg:0x%04X value: 0x%04X\n", reg[i], buffer);
	}
	return len;
}

#ifdef LTR578_DEBUG
static int ltr578_dump_reg(void)
{
	int i = 0;
	int reg[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
			  0x09, 0x0d, 0x0e, 0x0f, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
			  0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26 };
	int ret;
	u8 buffer;

	for (i = 0; i < 27; i++) {
		ret = ltr578_master_recv(ltr578_obj->client, reg[i], &buffer,
					 0x01);
		if (ret < 0) {
			APS_ERR("i2c error: %d\n", ret);
		}

		APS_LOG("reg:0x%04X value: 0x%04X\n", reg[i], buffer);
	}
	return 0;
}
#endif

/*----------------------------------------------------------------------------*/
static ssize_t ltr578_show_cali_Light_High(struct device_driver *ddri, char *buf)
{
	int32_t als_reading = 0;
	bool result = false;

	APS_LOG("%s:[#53][LTR]Start Cali light...\n", __func__);

	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return 0;
	}

	ltr578_obj->lux_threshold = DEF_HIGH_LUX_THRESHOLD;
	msleep(150);
	als_reading = ltr578_als_get_data_avg(10, true);

	if ((als_reading > 0) && (als_reading <= max_uint16)) {
		ltr578_obj->als_cal_high_reading = als_reading;

		result = als_store_cali_in_file(ALS_CAL_FILE,
						ltr578_obj->als_cal_high_reading, ltr578_obj->als_cal_low_reading);
	} else {
		APS_ERR("[#53][LTR]cali light fail!!!als_value= %d\n", als_reading);
		result = false;
	}

	APS_LOG("Threshold:%d, als_cal:%d result:%s\n", ltr578_obj->lux_threshold, als_reading, result ? "PASSED" : "FAIL");
	return scnprintf(buf, PAGE_SIZE,
				"%s:Threshold = %d, als_value = %d\n",
				result ? "PASSED" : "FAIL", ltr578_obj->lux_threshold, als_reading);
}

/*----------------------------------------------------------------------------*/
static ssize_t ltr578_show_cali_Light_Low(struct device_driver *ddri, char *buf)
{
	int32_t als_reading = 0;
	bool result = false;

	APS_LOG("%s:[#53][LTR]Start Cali light...\n", __func__);

	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return 0;
	}

	ltr578_obj->lux_threshold = DEF_LOW_LUX_THRESHOLD;
	msleep(150);

	als_reading = ltr578_als_get_data_avg(20, true);

	if ((als_reading > 0) && (als_reading <= max_uint16)) {
		ltr578_obj->als_cal_low_reading = als_reading;
		/* low value can't larger than half of high */
		if (ltr578_obj->als_cal_low_reading > ltr578_obj->als_cal_high_reading / 2) {
			result = false;
			APS_ERR("als data is not suitable! Please check the light source or light sensor!\n");
			goto out;
		}
		result = als_store_cali_in_file(ALS_CAL_FILE,
						ltr578_obj->als_cal_high_reading, ltr578_obj->als_cal_low_reading);

	} else {
		APS_ERR("[#53][LTR]cali light fail!!!als_value= %d\n", als_reading);
		result = false;
	}

out:
	APS_LOG("Threshold:%d, als_cal:%d result:%s\n", ltr578_obj->lux_threshold, als_reading, result ? "PASSED" : "FAIL");
	return scnprintf(buf, PAGE_SIZE,
				"%s:Threshold = %d, als_value = %d\n",
				result ? "PASSED" : "FAIL", ltr578_obj->lux_threshold, als_reading);
}

static ssize_t ltr578_show_alscal_high_value(struct device_driver *ddri,
						 char *buf)
{
	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return 0;
	}

	return scnprintf(buf, PAGE_SIZE, "%d.%d\n", ltr578_obj->als_cal_high_reading / 10,
		ltr578_obj->als_cal_high_reading % 10);
}

static ssize_t ltr578_show_alscal_low_value(struct device_driver *ddri,
						 char *buf)
{
	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return 0;
	}

	return scnprintf(buf, PAGE_SIZE, "%d.%d\n", ltr578_obj->als_cal_low_reading / 10,
		ltr578_obj->als_cal_low_reading % 10);
}

static ssize_t ltr578_show_als_check(struct device_driver *ddri,
						 char *buf)
{
	int res;

	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return 0;
	}
	res = ltr578_als_read(ltr578_obj->client, &ltr578_obj->als, false);
	return scnprintf(buf, PAGE_SIZE, "%d\n", ltr578_obj->als);
}

static ssize_t ltr578_show_lux(struct device_driver *ddri, char *buf)
{
	int32_t als_reading = 0;

	als_reading = ltr578_als_get_data_avg(10, false);

	als_reading = ltr_alscode2lux(als_reading);

	return scnprintf(buf, PAGE_SIZE, "%d lux\n", als_reading);
}

static ssize_t ltr578_show_golden_lux(struct device_driver *ddri, char *buf)
{
	int32_t als_reading = 0;

	als_reading = ltr578_als_get_data_avg(10, true);

	als_reading = ltr_alscode2lux(als_reading);

	return scnprintf(buf, PAGE_SIZE, "%d.%d lux\n", als_reading / 10,
	als_reading % 10);
}

static ssize_t ltr578_show_enable(struct device_driver *ddri, char *buf)
{
	int32_t enabled = 0;
	int32_t ret = 0;
	u8 regdata = 0;

	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return 0;
	}

	if (test_bit(CMC_BIT_ALS, &ltr578_obj->enable))
		enabled = 1;
	else
		enabled = 0;

	ret = ltr578_master_recv(ltr578_obj->client, LTR578_MAIN_CTRL, &regdata, 0x01);
	if (ret < 0) {
		APS_ERR("ltr578 read reg error\n");
		return 0;
	}

	if (regdata & 0x02) {
		APS_LOG("ALS Enabled \n");
		ret = 1;
	} else {
		APS_LOG("ALS Disabled \n");
		ret = 0;
	}

	if (enabled != ret)
		APS_ERR("driver_enable=0x%x, sensor_enable=%x\n", enabled, ret);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t ltr578_store_enable(struct device_driver *ddri, const char *buf, size_t count)
{
	int err = 0;
	int32_t enabled = -1;

	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return 0;
	}

	if (sscanf(buf, "0x%x", &enabled) == 1) {
		if ((enabled == 1) || (enabled == 0)) {
			err = ltr578_als_enable(ltr578_obj->client, enabled);
			if (err) {
				APS_ERR("ltr578_als_enable failed!!\n");
				return 0;
			}
		} else {
			APS_ERR("invalid parameter, correct parameters: enable:1, disable:0\n");
			return 0;
		}
	} else {
		APS_ERR("invalid content: '%s', length = %zu\n", buf, count);
		return 0;
	}

	mutex_lock(&ltr578_mutex);
	if (enabled) {
		set_bit(CMC_BIT_ALS, &ltr578_obj->enable);
	} else {
		clear_bit(CMC_BIT_ALS, &ltr578_obj->enable);
	}
	mutex_unlock(&ltr578_mutex);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t ltr578_show_send(struct device_driver *ddri, char *buf)
{
	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t ltr578_store_send(struct device_driver *ddri, const char *buf,
				 size_t count)
{
	int addr, cmd = 0;
	u8 dat = 0;

	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return 0;
	} else if (2 != sscanf(buf, "%x %x", &addr, &cmd)) {
		APS_ERR("invalid format: '%s'\n", buf);
		return 0;
	}

	dat = (u8)cmd;

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t ltr578_show_recv(struct device_driver *ddri, char *buf)
{
	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t ltr578_store_recv(struct device_driver *ddri, const char *buf,
				 size_t count)
{
	int addr = 0;

	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return 0;
	} else if (1 != sscanf(buf, "%x", &addr)) {
		APS_ERR("invalid format: '%s'\n", buf);
		return 0;
	}

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t ltr578_show_status(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;

	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return 0;
	}

	if (ltr578_obj->hw) {
		len += scnprintf(buf + len, PAGE_SIZE - len,
				"CUST: %d, (%d %d)\n", ltr578_obj->hw->i2c_num,
				ltr578_obj->hw->power_id,
				ltr578_obj->hw->power_vol);
	} else {
		len += scnprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");
	}

	len += scnprintf(buf + len, PAGE_SIZE - len, "MISC: %d\n",
			atomic_read(&ltr578_obj->als_suspend));

	return len;
}

/*----------------------------------------------------------------------------*/
static int read_int_from_buf(struct ltr578_priv *obj, const char *buf,
				 size_t count, u32 data[], int len)
{
	int idx = 0;
	char *cur = (char *)buf, *end = (char *)(buf + count);

	while (idx < len) {
		while ((cur < end) && IS_SPACE(*cur)) {
			cur++;
		}

		if (1 != sscanf(cur, "%d", &data[idx])) {
			break;
		}

		idx++;
		while ((cur < end) && !IS_SPACE(*cur)) {
			cur++;
		}
	}
	return idx;
}

/*----------------------------------------------------------------------------*/
static ssize_t ltr578_show_alslv(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx = 0;
	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return 0;
	}

	for (idx = 0; idx < ltr578_obj->als_level_num; idx++) {
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d ",
				ltr578_obj->hw->als_level[idx]);
	}
	len += scnprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t ltr578_store_alslv(struct device_driver *ddri, const char *buf,
				  size_t count)
{
	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return 0;
	} else if (!strcmp(buf, "def")) {
		memcpy(ltr578_obj->als_level, ltr578_obj->hw->als_level,
			   sizeof(ltr578_obj->als_level));
	} else if (ltr578_obj->als_level_num !=
		   read_int_from_buf(ltr578_obj, buf, count,
					 ltr578_obj->hw->als_level,
					 ltr578_obj->als_level_num)) {
		APS_ERR("invalid format: '%s'\n", buf);
	}
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t ltr578_show_alsval(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx = 0;
	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return 0;
	}

	for (idx = 0; idx < ltr578_obj->als_value_num; idx++) {
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d ",
				ltr578_obj->hw->als_value[idx]);
	}
	len += scnprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t ltr578_store_alsval(struct device_driver *ddri, const char *buf,
				   size_t count)
{
	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return 0;
	} else if (!strcmp(buf, "def")) {
		memcpy(ltr578_obj->als_value, ltr578_obj->hw->als_value,
			   sizeof(ltr578_obj->als_value));
	} else if (ltr578_obj->als_value_num !=
		   read_int_from_buf(ltr578_obj, buf, count,
					 ltr578_obj->hw->als_value,
					 ltr578_obj->als_value_num)) {
		APS_ERR("invalid format: '%s'\n", buf);
	}
	return count;
}

/*---------------------------------------------------------------------------------------*/
static DRIVER_ATTR(als, S_IWUSR | S_IRUGO, ltr578_show_als, NULL);
static DRIVER_ATTR(config, S_IWUSR | S_IRUGO, ltr578_show_config,
		   ltr578_store_config);
static DRIVER_ATTR(alslv, S_IWUSR | S_IRUGO, ltr578_show_alslv,
		   ltr578_store_alslv);
static DRIVER_ATTR(alsval, S_IWUSR | S_IRUGO, ltr578_show_alsval,
		   ltr578_store_alsval);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, ltr578_show_trace,
		   ltr578_store_trace);
static DRIVER_ATTR(status, S_IWUSR | S_IRUGO, ltr578_show_status, NULL);
static DRIVER_ATTR(send, S_IWUSR | S_IRUGO, ltr578_show_send,
		   ltr578_store_send);
static DRIVER_ATTR(recv, S_IWUSR | S_IRUGO, ltr578_show_recv,
		   ltr578_store_recv);
static DRIVER_ATTR(reg, S_IWUSR | S_IRUGO, ltr578_show_reg, NULL);
static DRIVER_ATTR(cali_Light_High, S_IWUSR | S_IRUGO, ltr578_show_cali_Light_High, NULL);
static DRIVER_ATTR(cali_Light_Low, S_IWUSR | S_IRUGO, ltr578_show_cali_Light_Low, NULL);
static DRIVER_ATTR(alscal_high_value, S_IWUSR | S_IRUGO, ltr578_show_alscal_high_value, NULL);
static DRIVER_ATTR(alscal_low_value, S_IWUSR | S_IRUGO, ltr578_show_alscal_low_value, NULL);
static DRIVER_ATTR(als_check, S_IWUSR | S_IRUGO, ltr578_show_als_check, NULL);
static DRIVER_ATTR(enable, S_IWUSR | S_IRUGO, ltr578_show_enable, ltr578_store_enable);
static DRIVER_ATTR(lux, S_IWUSR | S_IRUGO, ltr578_show_lux, NULL);
static DRIVER_ATTR(golden_lux, S_IWUSR | S_IRUGO, ltr578_show_golden_lux, NULL);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *ltr578_attr_list[] = {
	&driver_attr_als,
	&driver_attr_trace, /*trace log*/
	&driver_attr_config,
	&driver_attr_alslv,
	&driver_attr_alsval,
	&driver_attr_status,
	&driver_attr_send,
	&driver_attr_recv,
	&driver_attr_reg,
	&driver_attr_cali_Light_High,
	&driver_attr_cali_Light_Low,
	&driver_attr_alscal_high_value,
	&driver_attr_alscal_low_value,
	&driver_attr_als_check,
	&driver_attr_enable,
	&driver_attr_lux,
	&driver_attr_golden_lux,
};

/*----------------------------------------------------------------------------*/
static int ltr578_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(ltr578_attr_list) / sizeof(ltr578_attr_list[0]));

	if (driver == NULL) {
		return -EINVAL;
	}

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, ltr578_attr_list[idx]);
		if (err) {
			APS_ERR("driver_create_file (%s) = %d\n",
				ltr578_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int ltr578_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(ltr578_attr_list) / sizeof(ltr578_attr_list[0]));

	if (!driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		driver_remove_file(driver, ltr578_attr_list[idx]);
	}

	return err;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------interrupt functions--------------------------------*/

/*----------------------------------------------------------------------------*/
#if IS_ENABLED(CONFIG_IDME)
static void get_als_cali(void)
{
	als_cal = idme_get_alscal_value();
	APS_LOG("idme to read:als_cal:%d\n", als_cal);
}
#endif

/*----------------------------------------------------------------------------*/
static void get_ltr_dts_func(struct device_node *node)
{
	if (als_cal != 0) {
		ltr578_obj->als_cal_high_reading = (als_cal & 0x0000FFFF);
		ltr578_obj->als_cal_low_reading = (als_cal & 0xFFFF0000) >> 16;
	} else {
		ltr578_obj->als_cal_high_reading = DEF_HIGH_LUX_THRESHOLD;
		ltr578_obj->als_cal_low_reading = DEF_LOW_LUX_THRESHOLD;
	}
	APS_LOG("als_cal_high_reading:%d, als_cal_low_reading:%d\n",
		ltr578_obj->als_cal_high_reading, ltr578_obj->als_cal_low_reading);
}

/*--------------------------------------------------------------------------------*/
static int ltr578_init_client(void)
{
	int res = 0;
	u8 buf = 0;

	struct i2c_client *client = ltr578_obj->client;

	msleep(PON_DELAY);

	/* ===============
	* ** IMPORTANT **
	* ===============
	* Other settings like timing and threshold to be set here, if required.
	* Not set and kept as device default for now.
	*/
#if IS_ENABLED(CONFIG_IDME)
	get_als_cali();
#endif
	get_ltr_dts_func(client->dev.of_node);
	/* Enable ALS to Full Range at startup */
	ltr578_set_gain(ALS_RANGE_18);

	buf = ALS_RESO_MEAS; /* 18 bit & 100ms measurement rate */
	int_fac = ALS_INT_FACTOR; /* resolution 100ms ---  ALS_INT_FACTOR 1,
	resolution 200ms ---  ALS_INT_FACTOR 2, resolution 50ms ---  ALS_INT_FACTOR 0.5 */
	res = ltr578_master_send(client, LTR578_ALS_MEAS_RATE, (char *)&buf, 1);
	APS_LOG("ALS sensor resolution & measurement rate: %d!\n",
		ALS_RESO_MEAS);
#ifdef SENSOR_ALS_ENABLED
	res = ltr578_als_enable(client, 1);
	if (res < 0) {
		APS_ERR("enable als fail: %d\n", res);
		goto EXIT_ERR;
	}
#endif

	return 0;

EXIT_ERR:
	APS_ERR("init dev: %d\n", res);
	return 1;
}

/*--------------------------------------------------------------------------------*/
/* if use  this typ of enable , P-sensor should report inputEvent(x, y, z ,stats, div) to HAL */
static int als_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

/* if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL */
static int als_enable_nodata(int en)
{
	int res = 0;
	APS_LOG("ltr578_obj als enable value = %d\n", en);

	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return -1;
	}

	res = ltr578_als_enable(ltr578_obj->client, en);
	if (res) {
		APS_ERR("als_enable_nodata is failed!!\n");
		return -1;
	}

	mutex_lock(&ltr578_mutex);
	if (en)
		set_bit(CMC_BIT_ALS, &ltr578_obj->enable);
	else
		clear_bit(CMC_BIT_ALS, &ltr578_obj->enable);
	mutex_unlock(&ltr578_mutex);

	return 0;
}

static int als_set_delay(u64 ns)
{
	/* Do nothing */
	return 0;
}

static int als_batch(int flag, int64_t samplingPeriodNs,
			 int64_t maxBatchReportLatencyNs)
{
	return als_set_delay(samplingPeriodNs);
}

static int als_flush(void)
{
	return als_flush_report();
}

static int als_get_data(int *value, int *status)
{
	int err, res = 0;
	int cali_lux = 0;

	if (!ltr578_obj) {
		APS_ERR("ltr578_obj is null!!\n");
		return -1;
	}

	res = ltr578_als_read(ltr578_obj->client, &ltr578_obj->als, false);
	if (res < 0) {
		err = -1;
	} else {
		cali_lux = ltr_alscode2lux(ltr578_obj->als);
		*value = cali_lux;
		err = 0;
		if (*value < 0) {
			err = -1;
		}
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

	return err;
}

/*-----------------------------------i2c operations----------------------------------*/
static int ltr578_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct ltr578_priv *obj = NULL;
	struct als_control_path als_ctl = { 0 };
	struct als_data_path als_data = { 0 };
	int err = 0;

	APS_FUN();
	/* get customization and power on */
	err = get_alsps_dts_func(client->dev.of_node, hw);
	if (err < 0) {
		APS_ERR("get customization info from dts failed\n");
		return -EFAULT;
	}
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}
	ltr578_obj = obj;

	obj->hw = hw;

	obj->client = client;
	i2c_set_clientdata(client, obj);

	/*-----------------------------value need to be confirmed-----------------------------------------*/
	atomic_set(&obj->als_debounce, 300);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	atomic_set(&obj->als_suspend, 0);
	atomic_set(&obj->als_thd_val_high, obj->hw->als_threshold_high);
	atomic_set(&obj->als_thd_val_low, obj->hw->als_threshold_low);

	obj->irq_node = client->dev.of_node;

	obj->enable = 0;
	obj->pending_intr = 0;
	obj->als_level_num =
		sizeof(obj->hw->als_level) / sizeof(obj->hw->als_level[0]);
	obj->als_value_num =
		sizeof(obj->hw->als_value) / sizeof(obj->hw->als_value[0]);
	obj->als_modulus = (400 * 100) / (16 * 150);
	/* (1/Gain)*(400/Tine), this value is fix after init ATIME and CONTROL register value */
	/* (400)/16*2.72 here is amplify *100 */
	/*-----------------------------value need to be confirmed-----------------------------------------*/

	BUG_ON(sizeof(obj->als_level) != sizeof(obj->hw->als_level));
	memcpy(obj->als_level, obj->hw->als_level, sizeof(obj->als_level));
	BUG_ON(sizeof(obj->als_value) != sizeof(obj->hw->als_value));
	memcpy(obj->als_value, obj->hw->als_value, sizeof(obj->als_value));
	atomic_set(&obj->i2c_retry, 3);

#ifdef SENSOR_ALS_ENABLED
	set_bit(CMC_BIT_ALS, &obj->enable);
#else
	clear_bit(CMC_BIT_ALS, &obj->enable);
#endif

	APS_LOG("ltr578_init_client() start...!\n");
	ltr578_i2c_client = client;
	err = ltr578_init_client();
	if (err) {
		goto exit_init_failed;
	}
	APS_LOG("ltr578_init_client() OK!\n");

	/*------------------------ltr578 attribute file for debug--------------------------------------*/
	err = ltr578_create_attr(
		&(ltr578_init_info.platform_diver_addr->driver));
	if (err) {
		APS_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	/*------------------------ltr578 attribute file for debug--------------------------------------*/

	als_ctl.open_report_data = als_open_report_data;
	als_ctl.enable_nodata = als_enable_nodata;
	als_ctl.set_delay = als_set_delay;
	als_ctl.batch = als_batch;
	als_ctl.flush = als_flush;
	als_ctl.is_report_input_direct = false;
	als_ctl.is_support_batch = false;

	err = als_register_control_path(&als_ctl);
	if (err) {
		APS_ERR("register fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	als_data.get_data = als_get_data;
	als_data.vender_div = 100;
	err = als_register_data_path(&als_data);
	if (err) {
		APS_ERR("register fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}
	ltr578_init_flag = 0;
	APS_LOG("%s: OK\n", __func__);
	return 0;

exit_create_attr_failed:
exit_sensor_obj_attach_fail:
exit_init_failed:
	kfree(obj);
exit:
	ltr578_i2c_client = NULL;
	APS_ERR("%s: err = %d\n", __func__, err);
	ltr578_init_flag = -1;
	return err;
}

static int ltr578_i2c_remove(struct i2c_client *client)
{
	int err = 0;

	err = ltr578_delete_attr(&(ltr578_i2c_driver.driver));
	if (err) {
		APS_ERR("ltr578_delete_attr fail: %d\n", err);
	}

	ltr578_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}

static int ltr578_i2c_detect(struct i2c_client *client,
				 struct i2c_board_info *info)
{
	strcpy(info->type, LTR578_DEV_NAME);
	return 0;
}

static int ltr578_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ltr578_priv *obj = i2c_get_clientdata(client);
	int err = 0;
	APS_FUN();

	if (!obj) {
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}

	alsps_driver_pause_polling(1);
	atomic_set(&obj->als_suspend, 1);
	err = ltr578_als_enable(obj->client, 0);
	if (err < 0) {
		alsps_driver_pause_polling(0);
		atomic_set(&obj->als_suspend, 0);
		APS_ERR("disable als: %d\n", err);
		return err;
	}

	return 0;
}

static int ltr578_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ltr578_priv *obj = i2c_get_clientdata(client);
	int err = 0;
	APS_FUN();

	if (!obj) {
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}

	atomic_set(&obj->als_suspend, 0);
	if (alsps_driver_query_polling_state(ID_LIGHT) == 1) {
		err = ltr578_als_enable(obj->client, 1);
		if (err < 0) {
			atomic_set(&obj->als_suspend, 1);
			APS_ERR("enable als fail: %d\n", err);
		}
	}
	alsps_driver_pause_polling(0);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int ltr578_remove(void)
{
	APS_FUN();

	i2c_del_driver(&ltr578_i2c_driver);
	ltr578_init_flag = -1;

	return 0;
}

/*----------------------------------------------------------------------------*/
static int ltr578_local_init(void)
{
	APS_FUN();

	if (i2c_add_driver(&ltr578_i2c_driver)) {
		APS_ERR("add driver error\n");
		return -1;
	}

	if (-1 == ltr578_init_flag) {
		return -1;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init ltr578_init(void)
{
	alsps_driver_add(&ltr578_init_info);
	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit ltr578_exit(void)
{
	APS_FUN();
}
/*----------------------------------------------------------------------------*/
module_init(ltr578_init);
module_exit(ltr578_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("Liteon");
MODULE_DESCRIPTION("LTR-578ALSPS Driver");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
