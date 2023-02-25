// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.

 * Author: Andrew-sh.Cheng <andrew-sh.cheng@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/devfreq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/time.h>

#define MIN_VOLT_SHIFT		(100000)
#define MAX_VOLT_SHIFT		(250000)
#define PROC_MAX_VOLT		(1118750)
#define SRAM_MIN_VOLT		(850000)
#define SRAM_MAX_VOLT		(1118750)

struct cci_devfreq {
	struct device *cci_dev;
	struct devfreq *devfreq;
	struct regulator *proc_reg;
	struct regulator *sram_reg;
	struct clk *cci_clk;
	struct clk *inter_clk;
	int intermediate_voltage;
	int old_vproc;
	unsigned long old_freq;
	/* Avoid race condition for regulators between notify and policy */
	struct mutex reg_lock;
	struct notifier_block opp_nb;
	bool need_voltage_tracking;
};

static int mtk_cci_voltage_tracking(struct cci_devfreq *cci_info, int new_vproc)
{
	struct regulator *proc_reg = cci_info->proc_reg;
	struct regulator *sram_reg = cci_info->sram_reg;
	int old_vproc, old_vsram, new_vsram, vsram, vproc, ret;
	unsigned int count = 0;

	old_vproc = regulator_get_voltage(proc_reg);
	if (old_vproc < 0) {
		pr_err("%s: invalid Vproc value: %d\n", __func__, old_vproc);
		return old_vproc;
	}

	old_vsram = regulator_get_voltage(sram_reg);
	if (old_vsram < 0) {
		pr_err("%s: invalid Vsram value: %d\n", __func__, old_vsram);
		return old_vsram;
	}

	/* Vsram should not exceed the max/min allowed voltage of SoC. */
	new_vsram = min(new_vproc + MIN_VOLT_SHIFT, SRAM_MAX_VOLT);
	new_vsram = max(new_vsram, SRAM_MIN_VOLT);

	do {
		count++;

		if (old_vproc <= new_vproc) {
			/*
			 * When scaling up voltages, Vsram and Vproc scale up
			 * step by step. At each step, set Vsram to
			 * (Vproc + 200mV), then set Vproc to
			 * (Vsram - 100mV). Keep doing it until Vsram and Vproc
			 * hit target voltages.
			 */

			vsram = min(new_vsram, old_vproc + MAX_VOLT_SHIFT);
			vsram = max(vsram, SRAM_MIN_VOLT);
			ret = regulator_set_voltage(sram_reg, vsram,
						    SRAM_MAX_VOLT);
			if (ret)
				return ret;

			if (vsram == SRAM_MAX_VOLT || new_vsram == SRAM_MIN_VOLT)
				vproc = new_vproc;
			else
				vproc = vsram - MIN_VOLT_SHIFT;

			ret = regulator_set_voltage(proc_reg, vproc,
						    PROC_MAX_VOLT);
			if (ret) {
				regulator_set_voltage(sram_reg, old_vsram,
						      SRAM_MAX_VOLT);
				return ret;
			}
		} else if (old_vproc > new_vproc) {
			/*
			 * When scaling down voltages, Vsram and Vproc scale
			 * down step by step. At each step, set
			 * Vproc to (Vsram - 200mV) first,
			 * then set Vsram to (Vproc + 100mV).
			 * Keep doing it until Vsram and Vproc hit target
			 * voltages.
			 */

			vproc = max(new_vproc, old_vsram - MAX_VOLT_SHIFT);
			ret = regulator_set_voltage(proc_reg, vproc,
						    PROC_MAX_VOLT);
			if (ret)
				return ret;

			if (vproc == new_vproc)
				vsram = new_vsram;
			else
				vsram = max(new_vsram, vproc + MIN_VOLT_SHIFT);

			ret = regulator_set_voltage(sram_reg, vsram,
						    SRAM_MAX_VOLT);
			if (ret) {
				regulator_set_voltage(proc_reg, old_vproc,
						      PROC_MAX_VOLT);
				return ret;
			}
		}

		/*
		 * Set the next voltage start. For the heads-up, we cannot set
		 * it from regulator vproc/vsram because CPU/CCI DVFS shares
		 * the same vproc/vsram and regulator framework only set the
		 * highest minimun voltage.
		 * E.g: devices(A/B) share the same buck (Vproc)
		 * device(A) sets Vproc = 600mV
		 * device(B) sets Vproc = 1000mV
		 * Even though A device can set Vproc 600mV via regulator
		 * framework successfully, regulator still shows Vproc with
		 * 1000mV instead of 600mV because the highest minimun voltage
		 * is 1000mV, Therefore, if you set the next voltage start from
		 * regulator, you won't meet the target voltage and cause an
		 * inifinite loop here.
		 */
		old_vproc = vproc;
		old_vsram = vsram;

		if (count >= 20 || vproc > vsram) {
			pr_err("cci: new(%d, %d), cur(%d, %d), %u\n",
			       new_vproc, new_vsram, vproc, vsram, count);
			BUG_ON(1);
		}
	} while (vproc != new_vproc || vsram != new_vsram);

	return 0;
}

static int mtk_cci_set_voltage(struct cci_devfreq *cci_info, int vproc)
{
	int ret;

	if (cci_info->need_voltage_tracking)
		ret = mtk_cci_voltage_tracking(cci_info, vproc);
	else
		ret = regulator_set_voltage(cci_info->proc_reg, vproc,
					    PROC_MAX_VOLT);
	if (!ret)
		cci_info->old_vproc = vproc;

	return ret;
}

static int mtk_cci_devfreq_target(struct device *dev, unsigned long *freq,
				  u32 flags)
{
	struct cci_devfreq *cci_info = dev_get_drvdata(dev);
	struct clk *ccipll = clk_get_parent(cci_info->cci_clk);
	struct dev_pm_opp *opp;
	unsigned long opp_rate;
	int vproc, old_vproc, inter_vproc, target_vproc, ret;

	if (!cci_info)
		return -EINVAL;

	if (cci_info->old_freq == *freq)
		return 0;

	inter_vproc = cci_info->intermediate_voltage;

	opp_rate = *freq;
	opp = devfreq_recommended_opp(dev, &opp_rate, 1);

	mutex_lock(&cci_info->reg_lock);

	vproc = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);

	old_vproc = cci_info->old_vproc;
	if (old_vproc == 0)
		old_vproc = regulator_get_voltage(cci_info->proc_reg);

	target_vproc = (inter_vproc > vproc) ? inter_vproc : vproc;

	/* scale up: set voltage first then freq */
	if (old_vproc <= target_vproc) {
		ret = mtk_cci_set_voltage(cci_info, target_vproc);
		if (ret) {
			pr_err("cci: failed to scale up voltage\n");
			mtk_cci_set_voltage(cci_info, old_vproc);
			goto out_unlock;
		}
	}

	/* Reparent the CCI clock to intermediate clock. */
	ret = clk_set_parent(cci_info->cci_clk, cci_info->inter_clk);
	if (ret) {
		pr_err("cci: failed to re-parent cci clock!\n");
		WARN_ON(1);
		goto out_unlock;
	}

	ret = clk_set_rate(ccipll, *freq);
	if (ret) {
		pr_err("cci: failed to set ccipll rate: %d\n", ret);
		mtk_cci_set_voltage(cci_info, old_vproc);
		goto out_unlock;
	}

	/* Set parent of CPU clock back to the original PLL. */
	ret = clk_set_parent(cci_info->cci_clk, ccipll);
	if (ret) {
		pr_err("cci: failed to re-parent cci clock!\n");
		WARN_ON(1);
		goto out_unlock;
	}

	/*
	 * If the new voltage is lower than the intermediate voltage or the
	 * original voltage, scale down to the new voltage.
	 */
	if (vproc < inter_vproc || vproc < old_vproc) {
		ret = mtk_cci_set_voltage(cci_info, vproc);
		if (ret) {
			pr_err("cci: failed to scale down voltage\n");
			clk_set_rate(cci_info->cci_clk, cci_info->old_freq);
			goto out_unlock;
		}
	}

	cci_info->old_freq = *freq;

	mutex_unlock(&cci_info->reg_lock);
	return 0;

out_unlock:
	mutex_unlock(&cci_info->reg_lock);
	return ret;
}

static int cci_devfreq_opp_notifier(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct dev_pm_opp *opp = data;
	struct cci_devfreq *cci_info = container_of(nb, struct cci_devfreq,
						  opp_nb);
	unsigned long freq, volt;

	if (event == OPP_EVENT_ADJUST_VOLTAGE) {
		freq = dev_pm_opp_get_freq(opp);
		/* current opp item is changed */
		mutex_lock(&cci_info->reg_lock);
		if (freq == cci_info->old_freq) {
			volt = dev_pm_opp_get_voltage(opp);
			mtk_cci_set_voltage(cci_info, volt);
		}
		mutex_unlock(&cci_info->reg_lock);
	}

	return 0;
}

static int devfreq_status(struct device *dev, struct devfreq_dev_status *stat)
{
	return 0;
}

static struct devfreq_dev_profile cci_devfreq_profile = {
	.target = mtk_cci_devfreq_target,
	.get_dev_status = devfreq_status,
};

static int mtk_cci_set_highest_voltage(struct cci_devfreq *cci_info)
{
	struct dev_pm_opp *opp;
	unsigned long rate = U32_MAX, opp_volt;
	int cur_vproc, ret;

	cur_vproc = regulator_get_voltage(cci_info->proc_reg);
	if (cur_vproc < 0) {
		pr_err("%s: cannot get vproc, ret = %d\n", __func__, cur_vproc);
		return cur_vproc;
	}

	opp = dev_pm_opp_find_freq_floor(cci_info->cci_dev, &rate);
	if (IS_ERR(opp)) {
		pr_err("%s: failed to get opp, ret = %ld\n",
		       __func__, PTR_ERR(opp));
		return PTR_ERR(opp);
	}

	opp_volt = dev_pm_opp_get_voltage(opp);
	ret = mtk_cci_set_voltage(cci_info, opp_volt);
	if (ret) {
		pr_err("%s: failed to opp_volt = %ul in proc_reg\n",
		       __func__, opp_volt);
		goto out_dev_pm_opp_put;
	}

out_dev_pm_opp_put:
	dev_pm_opp_put(opp);

	return ret;
}

static int mtk_cci_devfreq_probe(struct platform_device *pdev)
{
	struct device *cci_dev = &pdev->dev;
	struct cci_devfreq *cci_info;
	struct devfreq_passive_data *passive_data;
	struct notifier_block *opp_nb;
	struct dev_pm_opp *opp;
	unsigned long rate;
	int ret;

	cci_info = devm_kzalloc(cci_dev, sizeof(*cci_info), GFP_KERNEL);
	if (!cci_info)
		return -ENOMEM;

	opp_nb = &cci_info->opp_nb;
	cci_info->cci_dev = cci_dev;
	mutex_init(&cci_info->reg_lock);

	cci_info->cci_clk = devm_clk_get(cci_dev, "cci");
	ret = PTR_ERR_OR_ZERO(cci_info->cci_clk);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(cci_dev, "failed to get clock for CCI: %d\n",
				ret);
		return ret;
	}

	cci_info->inter_clk = clk_get(cci_dev, "intermediate");
	ret = PTR_ERR_OR_ZERO(cci_info->inter_clk);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(cci_dev, "failed to get clock for CCI: %d\n",
				ret);
		return ret;
	}

	cci_info->proc_reg = devm_regulator_get_optional(cci_dev, "proc");
	ret = PTR_ERR_OR_ZERO(cci_info->proc_reg);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(cci_dev, "failed to get regulator for CCI: %d\n",
				ret);
		return ret;
	}

	/* Both presence and absence of sram regulator are valid cases. */
	cci_info->sram_reg = regulator_get_optional(cci_dev, "sram");
	cci_info->need_voltage_tracking = !IS_ERR(cci_info->sram_reg);

	ret = dev_pm_opp_of_add_table(cci_dev);
	if (ret) {
		dev_err(cci_dev, "Fail to get OPP table for CCI: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(cci_info->cci_clk);
	if (ret)
		goto out_free_opp_table;

	ret = clk_prepare_enable(cci_info->inter_clk);
	if (ret)
		goto out_free_opp_table;

	platform_set_drvdata(pdev, cci_info);

	/* Search a safe voltage for intermediate frequency. */
	rate = clk_get_rate(cci_info->inter_clk);
	opp = dev_pm_opp_find_freq_ceil(cci_dev, &rate);
	if (IS_ERR(opp)) {
		pr_err("failed to get intermediate opp for cci devfreq\n");
		ret = PTR_ERR(opp);
		goto out_disable_inter_clock;
	}

	cci_info->intermediate_voltage = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);

	passive_data = devm_kzalloc(cci_dev, sizeof(*passive_data), GFP_KERNEL);
	if (!passive_data) {
		ret = -ENOMEM;
		goto out_disable_inter_clock;
	}

	/*
	 * To prevent this scenario:
	 * 1. cpufreq sets armpll/vproc with (500Mhz/600mV)
	 * 2. DEVFREQ_GOV_PASSIVE hasn't been ready to help notify cci devfreq
	 * to set its coresponding ccipll/vproc. This will cause high freq
	 * low voltage issue on cci side (system random hung).
	 */
	ret = mtk_cci_set_highest_voltage(cci_info);
	if (ret) {
		pr_err("cci fails to set opp highest voltage, ret = %d\n", ret);
		goto out_disable_inter_clock;
	}

	passive_data->parent_type = CPUFREQ_PARENT_DEV;
	cci_info->devfreq = devm_devfreq_add_device(cci_dev,
						    &cci_devfreq_profile,
						    DEVFREQ_GOV_PASSIVE,
						    passive_data);
	if (IS_ERR(cci_info->devfreq)) {
		ret = -EPROBE_DEFER;
		dev_err(cci_dev, "cannot create cci devfreq device:%d\n", ret);
		goto out_disable_inter_clock;
	}

	opp_nb->notifier_call = cci_devfreq_opp_notifier;
	ret = dev_pm_opp_register_notifier(cci_dev, opp_nb);
	if (ret) {
		pr_warn("cci devfreq register pm opp notifier fail, ret\n",
			ret);
		goto out_devfreq_remove_device;
	}

	return 0;

out_devfreq_remove_device:
	devm_devfreq_remove_device(cci_dev, cci_info->devfreq);

out_disable_inter_clock:
	clk_disable_unprepare(cci_info->inter_clk);

out_free_opp_table:
	dev_pm_opp_of_remove_table(cci_dev);

	return ret;
}

static int mtk_cci_devfreq_remove(struct platform_device *pdev)
{
	struct device *cci_dev = &pdev->dev;
	struct cci_devfreq *cci_info;
	struct notifier_block *opp_nb;

	cci_info = platform_get_drvdata(pdev);
	opp_nb = &cci_info->opp_nb;

	dev_pm_opp_unregister_notifier(cci_dev, opp_nb);
	dev_pm_opp_of_remove_table(cci_dev);
	regulator_disable(cci_info->proc_reg);
	regulator_disable(cci_info->sram_reg);

	return 0;
}

static const struct of_device_id mediatek_cci_of_match[] = {
	{ .compatible = "mediatek,mt8169-cci" },
	{ .compatible = "mediatek,mt8183-cci" },
	{ },
};
MODULE_DEVICE_TABLE(of, mediatek_cci_of_match);

static struct platform_driver cci_devfreq_driver = {
	.probe	= mtk_cci_devfreq_probe,
	.remove	= mtk_cci_devfreq_remove,
	.driver = {
		.name = "mediatek-cci-devfreq",
		.of_match_table = of_match_ptr(mediatek_cci_of_match),
	},
};

module_platform_driver(cci_devfreq_driver);

MODULE_DESCRIPTION("Mediatek CCI devfreq driver");
MODULE_AUTHOR("Andrew-sh.Cheng <andrew-sh.cheng@mediatek.com>");
MODULE_LICENSE("GPL v2");
