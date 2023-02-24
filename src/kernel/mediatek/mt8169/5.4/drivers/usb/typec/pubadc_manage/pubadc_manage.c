/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/iio/consumer.h>
#include <linux/iio/types.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/iio/machine.h>
#include <linux/iio/driver.h>
#include "pubadc_manage.h"

struct public_adc {
	bool init_state;
	int gpio_pubadc_id0;
	int gpio_pubadc_id1;
	struct mutex pubadc_io_mutex;
	struct pinctrl *pubadc_pinctrl;
	struct pinctrl_state *pubadc_bat_id;
	struct pinctrl_state *pubadc_liquid;
	struct iio_channel *adc_ch;
};

static struct public_adc public_adc = {
	.init_state = false,
};

bool get_pubadc_init_state(void)
{
	return public_adc.init_state;
}
EXPORT_SYMBOL_GPL(get_pubadc_init_state);

int pubadc_iio_read_channel(struct iio_channel *adc_ch, int *val, enum PUBADC_TYPE type)
{
	int ret;
	static enum PUBADC_TYPE last_pubadc_type = PUBADC_BAT_ID;

	if (public_adc.init_state) {
		mutex_lock(&public_adc.pubadc_io_mutex);
		if (last_pubadc_type != type) {
			if (type == PUBADC_LIQUID)
				pinctrl_select_state(public_adc.pubadc_pinctrl,
					public_adc.pubadc_liquid);
			else
				pinctrl_select_state(public_adc.pubadc_pinctrl,
					public_adc.pubadc_bat_id);
			msleep(50);
			last_pubadc_type = type;
		}
		ret = iio_read_channel_processed(public_adc.adc_ch, val);
		mutex_unlock(&public_adc.pubadc_io_mutex);
	} else {
		ret = iio_read_channel_processed(adc_ch, val);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(pubadc_iio_read_channel);

static unsigned char pubadc_get_id(struct platform_device *pdev)
{
	int ret = 0;
	int liquid_id0 = 0;
	int liquid_id1 = 0;
	struct device_node *np = pdev->dev.of_node;

	public_adc.gpio_pubadc_id0 = of_get_named_gpio(np, "pubadc-id", 0);
	if (!gpio_is_valid(public_adc.gpio_pubadc_id0)) {
		pr_err("%s: no valid gpio-id0\n", __func__);
		return 0;
	}

	ret = gpio_request(public_adc.gpio_pubadc_id0, "pubadc_id0");
	if (ret) {
		pr_err("[%s] request pubadc gpio-id0 failed ret = %d\n",
			 __func__, ret);
		return 0;
	}
	gpio_direction_input(public_adc.gpio_pubadc_id0);
	liquid_id0 = gpio_get_value(public_adc.gpio_pubadc_id0);
	gpio_free(public_adc.gpio_pubadc_id0);

	public_adc.gpio_pubadc_id1 = of_get_named_gpio(np, "pubadc-id", 1);
	if (!gpio_is_valid(public_adc.gpio_pubadc_id1)) {
		pr_err("%s: no valid gpio-id1\n", __func__);
		return 0;
	}

	ret = gpio_request(public_adc.gpio_pubadc_id1, "pubadc_id1");
	if (ret) {
		pr_err("[%s] request pubadc gpio-id1 failed ret = %d\n",
			 __func__, ret);
		return 0;
	}
	gpio_direction_input(public_adc.gpio_pubadc_id1);
	liquid_id1 = gpio_get_value(public_adc.gpio_pubadc_id1);
	gpio_free(public_adc.gpio_pubadc_id1);

	return ((unsigned char)((liquid_id1<<1)|liquid_id0));
}

static bool pubadc_check_id(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;
	unsigned char pubadc_dts_id;
	unsigned char pubadc_io_id;

	ret = of_property_read_u8(np, "pubadc_dts_id", &pubadc_dts_id);
	if (ret < 0) {
		pr_err("[%s] can not find id by dts\n", __func__);
		return false;
	}
	pubadc_io_id = pubadc_get_id(pdev);
	if (pubadc_dts_id == pubadc_io_id)
		return true;

	pr_err("[%s] id not match, pubadc_dts_id = %d\
		 pubadc_io_id = %d\n", __func__, pubadc_dts_id, pubadc_io_id);

	return false;
}

static int pubadc_pinctrl_init(struct platform_device *pdev)
{
	public_adc.pubadc_pinctrl = devm_pinctrl_get(&pdev->dev);

	if (IS_ERR(public_adc.pubadc_pinctrl)) {
		pr_err("%s: can't find pinctrl\n", __func__);
		goto out;
	}

	public_adc.pubadc_bat_id = pinctrl_lookup_state(
		 public_adc.pubadc_pinctrl, "pubadc_bat_id");
	if (IS_ERR(public_adc.pubadc_bat_id)) {
		pr_err("%s: can't find pubadc_batid\n", __func__);
		goto out;
	}

	public_adc.pubadc_liquid = pinctrl_lookup_state(
		 public_adc.pubadc_pinctrl, "pubadc_liquid");
	if (IS_ERR(public_adc.pubadc_liquid)) {
		pr_err("%s: can't find pubadc_liquid\n", __func__);
		goto out;
	}

	pinctrl_select_state(public_adc.pubadc_pinctrl,
		 public_adc.pubadc_bat_id);

	return 0;
out:
	return -1;
}

static int pubadc_get_auxadc_channel(struct platform_device *dev)
{
	int ret;

	public_adc.adc_ch = iio_channel_get(&dev->dev, "pubadc_adc_channel");
	if (IS_ERR(public_adc.adc_ch)) {
		ret = PTR_ERR(public_adc.adc_ch);
		pr_err("%s: IIO channel not found: %d\n", __func__, ret);
		return -1;
	}

	return 0;
}

static int pubadc_probe(struct platform_device *pdev)
{
	int ret = 0;

	mutex_init(&public_adc.pubadc_io_mutex);

	if (!pubadc_check_id(pdev))
		goto err_id;

	ret = pubadc_pinctrl_init(pdev);
	if (ret)
		goto err_pinctrl;

	ret = pubadc_get_auxadc_channel(pdev);
	if (ret)
		goto err_getadc;

	public_adc.init_state = true;
	pr_info("[%s] probe sucessfull\n", __func__);

	return 0;

err_getadc:
err_pinctrl:
err_id:
	mutex_destroy(&public_adc.pubadc_io_mutex);
	public_adc.init_state = false;
	pr_info("[%s] probe failed\n", __func__);
	return -ENODEV;
}

static int pubadc_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct platform_device_id match_id[] = {
	{"amzn,pubadc", 0},
	{},
};

#ifdef CONFIG_OF
static const struct of_device_id pubadc_dt_match[] = {
	{.compatible = "amzn,pubadc",},
	{},
};
MODULE_DEVICE_TABLE(of, pubadc_dt_match);
#endif

struct platform_driver pubadc_driver = {
	.driver = {
		.name = "amzn_pubadc",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(pubadc_dt_match),
	},
	.probe = pubadc_probe,
	.remove = pubadc_remove,
	.id_table = match_id,
};

static int __init pubadc_init(void)
{
	return platform_driver_register(&pubadc_driver);
}
module_init(pubadc_init);

static void __exit pubadc_exit(void)
{
	platform_driver_unregister(&pubadc_driver);
}
module_exit(pubadc_exit);

MODULE_AUTHOR("Junbo Pan");
MODULE_DESCRIPTION("public_adc_management_driver");
MODULE_LICENSE("GPL v2");
