// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015 Linaro Ltd.
 * Author: Pi-Cheng Chen <pi-cheng.chen@linaro.org>
 */

#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/thermal.h>

/*
 * The struct mtk_cpu_dvfs_info holds necessary information for doing CPU DVFS
 * on each CPU power/clock domain of Mediatek SoCs. Each CPU cluster in
 * Mediatek SoCs has two voltage inputs, Vproc and Vsram. In some cases the two
 * voltage inputs need to be controlled under a hardware limitation:
 * 100mV < Vsram - Vproc < 200mV
 *
 * When scaling the clock frequency of a CPU clock domain, the clock source
 * needs to be switched to another stable PLL clock temporarily until
 * the original PLL becomes stable at target frequency.
 */
struct mtk_cpu_dvfs_info {
	struct cpumask cpus;
	struct device *cpu_dev;
	struct regulator *proc_reg;
	struct regulator *sram_reg;
	struct clk *cpu_clk;
	struct clk *inter_clk;
	struct list_head list_head;
	int intermediate_voltage;
	bool need_voltage_tracking;
	bool is_cci_devfreq_bounded;
	bool is_cci_dvfs_support;
	int old_vproc;
	struct mutex lock; /* avoid notify and policy race condition */
	struct notifier_block opp_nb;
	int opp_cpu;
	unsigned long opp_freq;
	int min_volt_shift;
	int max_volt_shift;
	int proc_max_volt;
	int sram_min_volt;
	int sram_max_volt;
};

static LIST_HEAD(dvfs_info_list);

static struct mtk_cpu_dvfs_info *mtk_cpu_dvfs_info_lookup(int cpu)
{
	struct mtk_cpu_dvfs_info *info;

	list_for_each_entry(info, &dvfs_info_list, list_head) {
		if (cpumask_test_cpu(cpu, &info->cpus))
			return info;
	}

	return NULL;
}

static unsigned int mtk_cpufreq_get(unsigned int cpu)
{
	struct mtk_cpu_dvfs_info *info;
	info = mtk_cpu_dvfs_info_lookup(cpu);
	if (!info)
		return 0;
	return info->opp_freq / 1000;
}

static int mtk_cpufreq_voltage_tracking(struct mtk_cpu_dvfs_info *info,
					int new_vproc)
{
	struct regulator *proc_reg = info->proc_reg;
	struct regulator *sram_reg = info->sram_reg;
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
	new_vsram = min(new_vproc + info->min_volt_shift, info->sram_max_volt);
	new_vsram = max(new_vsram, info->sram_min_volt);

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

			vsram = min(new_vsram, old_vproc + info->max_volt_shift);
			vsram = max(vsram, info->sram_min_volt);
			ret = regulator_set_voltage(sram_reg, vsram,
						    info->sram_max_volt);
			if (ret)
				return ret;

			if (vsram == info->sram_max_volt ||
			    new_vsram == info->sram_min_volt)
				vproc = new_vproc;
			else
				vproc = vsram - info->min_volt_shift;

			ret = regulator_set_voltage(proc_reg, vproc,
						    info->proc_max_volt);
			if (ret) {
				regulator_set_voltage(sram_reg, old_vsram,
						      info->sram_max_volt);
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

			vproc = max(new_vproc, old_vsram - info->max_volt_shift);
			ret = regulator_set_voltage(proc_reg, vproc,
						    info->proc_max_volt);
			if (ret)
				return ret;

			if (vproc == new_vproc)
				vsram = new_vsram;
			else
				vsram = max(new_vsram, vproc + info->min_volt_shift);

			ret = regulator_set_voltage(sram_reg, vsram,
						    info->sram_max_volt);
			if (ret) {
				regulator_set_voltage(proc_reg, old_vproc,
						      info->proc_max_volt);
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
			pr_err("cpufreq%d: new(%d, %d), cur(%d, %d), %u\n",
			       info->opp_cpu, new_vproc, new_vsram, vproc,
			       vsram, count);
			BUG_ON(1);
		}
	} while (vproc != new_vproc || vsram != new_vsram);

	return 0;
}

static int mtk_cpufreq_set_voltage(struct mtk_cpu_dvfs_info *info, int vproc)
{
	int ret;

	if (info->need_voltage_tracking)
		ret = mtk_cpufreq_voltage_tracking(info, vproc);
	else
		ret = regulator_set_voltage(info->proc_reg, vproc,
					    info->proc_max_volt);
	if (!ret)
		info->old_vproc = vproc;

	return ret;
}

static bool is_cci_devfreq_ready(struct mtk_cpu_dvfs_info *info, const char *node_name)
{
	struct platform_device *pdev_sup;
	struct device_node *np;
	struct device_link *sup_link;

	if (info->is_cci_devfreq_bounded)
		return true;

	if (!node_name) {
		pr_err("cpu%d: node name cannot be null\n", info->opp_cpu);
		return false;
	}

	np = of_find_node_by_name(NULL, node_name);
	if (!np) {
		pr_err("cpu%d: cannot find %s node\n", info->opp_cpu, node_name);
		return false;
	}

	pdev_sup = of_find_device_by_node(np);
	if (!pdev_sup) {
		of_node_put(np);
		pr_err("cpu%d: cannot find pdev by %s\n", info->opp_cpu, node_name);
		return false;
	}

	of_node_put(np);

	sup_link = device_link_add(info->cpu_dev, &pdev_sup->dev,
				   DL_FLAG_AUTOREMOVE_CONSUMER);
	if (!sup_link) {
		pr_err("cpu%d: sup_link is NULL\n", info->opp_cpu);
		return false;
	}

	if (sup_link->supplier->links.status != DL_DEV_DRIVER_BOUND)
		return false;

	info->is_cci_devfreq_bounded = true;

	return true;
}

static int mtk_cpufreq_set_target(struct cpufreq_policy *policy,
				  unsigned int index)
{
	struct cpufreq_frequency_table *freq_table = policy->freq_table;
	struct clk *cpu_clk = policy->clk;
	struct clk *armpll = clk_get_parent(cpu_clk);
	struct mtk_cpu_dvfs_info *info = policy->driver_data;
	struct device *cpu_dev = info->cpu_dev;
	struct dev_pm_opp *opp;
	long freq_hz, old_freq_hz;
	int vproc, old_vproc, inter_vproc, target_vproc, ret;

	/*
	 * To prevent this scenario:
	 * 1. cpufreq sets armpll/vproc with (500Mhz/600mV)
	 * 2. cci devfreq not ready and its ccipll needs more than 600mV.
	 * So, this will cause high freq low voltage issue on cci side
	 * (system random hung).
	 */
	if (info->is_cci_dvfs_support && !is_cci_devfreq_ready(info, "cci"))
		return 0;

	inter_vproc = info->intermediate_voltage;

	old_freq_hz = clk_get_rate(cpu_clk);
	old_vproc = info->old_vproc;
	if (old_vproc == 0)
		old_vproc = regulator_get_voltage(info->proc_reg);
	if (old_vproc < 0) {
		pr_err("%s: invalid Vproc value: %d\n", __func__, old_vproc);
		return old_vproc;
	}

	freq_hz = freq_table[index].frequency * 1000;

	opp = dev_pm_opp_find_freq_ceil(cpu_dev, &freq_hz);
	if (IS_ERR(opp)) {
		pr_err("cpu%d: failed to find OPP for %ld\n",
		       policy->cpu, freq_hz);
		return PTR_ERR(opp);
	}

	vproc = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);

	mutex_lock(&info->lock);

	/*
	 * If the new voltage or the intermediate voltage is higher than the
	 * current voltage, scale up voltage first.
	 */
	target_vproc = (inter_vproc > vproc) ? inter_vproc : vproc;
	if (old_vproc <= target_vproc) {
		ret = mtk_cpufreq_set_voltage(info, target_vproc);
		if (ret) {
			pr_err("cpu%d: failed to scale up voltage!\n",
			       policy->cpu);
			mtk_cpufreq_set_voltage(info, old_vproc);
			mutex_unlock(&info->lock);
			return ret;
		}
	}

	/* Reparent the CPU clock to intermediate clock. */
	ret = clk_set_parent(cpu_clk, info->inter_clk);
	if (ret) {
		pr_err("cpu%d: failed to re-parent cpu clock!\n",
		       policy->cpu);
		mtk_cpufreq_set_voltage(info, old_vproc);
		WARN_ON(1);
		mutex_unlock(&info->lock);
		return ret;
	}

	/* Set the original PLL to target rate. */
	ret = clk_set_rate(armpll, freq_hz);
	if (ret) {
		pr_err("cpu%d: failed to scale cpu clock rate!\n",
		       policy->cpu);
		clk_set_parent(cpu_clk, armpll);
		mtk_cpufreq_set_voltage(info, old_vproc);
		mutex_unlock(&info->lock);
		return ret;
	}

	/* Set parent of CPU clock back to the original PLL. */
	ret = clk_set_parent(cpu_clk, armpll);
	if (ret) {
		pr_err("cpu%d: failed to re-parent cpu clock!\n",
		       policy->cpu);
		mtk_cpufreq_set_voltage(info, inter_vproc);
		WARN_ON(1);
		mutex_unlock(&info->lock);
		return ret;
	}

	/*
	 * If the new voltage is lower than the intermediate voltage or the
	 * original voltage, scale down to the new voltage.
	 */
	if (vproc < inter_vproc || vproc < old_vproc) {
		ret = mtk_cpufreq_set_voltage(info, vproc);
		if (ret) {
			pr_err("cpu%d: failed to scale down voltage!\n",
			       policy->cpu);
			WARN_ON(1);
			mutex_unlock(&info->lock);
			return ret;
		}
	}

	info->opp_freq = freq_hz;
	mutex_unlock(&info->lock);

	return 0;
}

#define DYNAMIC_POWER "dynamic-power-coefficient"

static int mtk_cpufreq_opp_notifier(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct dev_pm_opp *opp = data;
	struct dev_pm_opp *new_opp;
	struct mtk_cpu_dvfs_info *info;
	unsigned long freq, volt;
	struct cpufreq_policy *policy;
	int ret = 0;

	info = container_of(nb, struct mtk_cpu_dvfs_info, opp_nb);

	if (event == OPP_EVENT_ADJUST_VOLTAGE) {
		freq = dev_pm_opp_get_freq(opp);

		mutex_lock(&info->lock);
		if (info->opp_freq == freq) {
			volt = dev_pm_opp_get_voltage(opp);
			ret = mtk_cpufreq_set_voltage(info, volt);
			if (ret)
				dev_err(info->cpu_dev, "failed to scale voltage: %d\n",
					ret);
		}
		mutex_unlock(&info->lock);
	} else if (event == OPP_EVENT_DISABLE) {
		freq = dev_pm_opp_get_freq(opp);
		/* case of current opp item is disabled */
		if (info->opp_freq == freq) {
			freq = 1;
			new_opp = dev_pm_opp_find_freq_ceil(info->cpu_dev,
							    &freq);
			if (!IS_ERR(new_opp)) {
				dev_pm_opp_put(new_opp);
				policy = cpufreq_cpu_get(info->opp_cpu);
				if (policy) {
					cpufreq_driver_target(policy,
							      freq / 1000,
							      CPUFREQ_RELATION_L);
					cpufreq_cpu_put(policy);
				}
			} else {
				pr_err("%s: all opp items are disabled\n",
				       __func__);
			}
		}
	}

	return notifier_from_errno(ret);
}

static int mtk_cpufreq_set_highest_voltage(struct mtk_cpu_dvfs_info *info)
{
	struct dev_pm_opp *opp;
	unsigned long rate = U32_MAX, opp_volt;
	int cur_vproc, ret;

	cur_vproc = regulator_get_voltage(info->proc_reg);
	if (cur_vproc < 0) {
		pr_err("cpu%d cannot get vproc, ret = %d\n", info->opp_cpu, cur_vproc);
		return cur_vproc;
	}

	opp = dev_pm_opp_find_freq_floor(info->cpu_dev, &rate);
	if (IS_ERR(opp)) {
		pr_err("cpu%d failed to get opp, ret = %ld\n",
		       info->opp_cpu, PTR_ERR(opp));
		return PTR_ERR(opp);
	}

	opp_volt = dev_pm_opp_get_voltage(opp);
	ret = mtk_cpufreq_set_voltage(info, opp_volt);
	if (ret) {
		pr_err("cpu%d failed to opp_volt = %lu in proc_reg\n",
		       info->opp_cpu, opp_volt);
		dev_pm_opp_put(opp);
		return ret;
	}

	dev_pm_opp_put(opp);

	return ret;
}

static bool is_mt8169_cpu_dvfs_init(void)
{
	struct device_node *np;

	np = of_find_node_by_path("/");
	if (!np)
		return false;

	if (of_device_is_compatible(np, "mediatek,mt8169")) {
		of_node_put(np);
		return true;
	}

	of_node_put(np);

	return false;
}

static int mtk_cpu_dvfs_info_init(struct mtk_cpu_dvfs_info *info, int cpu)
{
	struct device *cpu_dev;
	struct regulator *proc_reg = ERR_PTR(-ENODEV);
	struct regulator *sram_reg = ERR_PTR(-ENODEV);
	struct clk *cpu_clk = ERR_PTR(-ENODEV);
	struct clk *inter_clk = ERR_PTR(-ENODEV);
	struct dev_pm_opp *opp;
	unsigned long rate;
	int ret;

	info->is_cci_dvfs_support = is_mt8169_cpu_dvfs_init();

	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev) {
		pr_err("failed to get cpu%d device\n", cpu);
		return -ENODEV;
	}

	cpu_clk = clk_get(cpu_dev, "cpu");
	if (IS_ERR(cpu_clk)) {
		if (PTR_ERR(cpu_clk) == -EPROBE_DEFER)
			pr_warn("cpu clk for cpu%d not ready, retry.\n", cpu);
		else
			pr_err("failed to get cpu clk for cpu%d\n", cpu);

		ret = PTR_ERR(cpu_clk);
		return ret;
	}

	inter_clk = clk_get(cpu_dev, "intermediate");
	if (IS_ERR(inter_clk)) {
		if (PTR_ERR(inter_clk) == -EPROBE_DEFER)
			pr_warn("intermediate clk for cpu%d not ready, retry.\n",
				cpu);
		else
			pr_err("failed to get intermediate clk for cpu%d\n",
			       cpu);

		ret = PTR_ERR(inter_clk);
		goto out_free_resources;
	}

	proc_reg = regulator_get_optional(cpu_dev, "proc");
	if (IS_ERR(proc_reg)) {
		if (PTR_ERR(proc_reg) == -EPROBE_DEFER)
			pr_warn("proc regulator for cpu%d not ready, retry.\n",
				cpu);
		else
			pr_err("failed to get proc regulator for cpu%d, %ld\n",
			       cpu, PTR_ERR(proc_reg));

		ret = PTR_ERR(proc_reg);
		goto out_free_resources;
	}

	ret = regulator_enable(proc_reg);
	if (ret) {
		pr_warn("enable vproc for cpu%d fail\n", cpu);
		goto out_free_resources;
	}

	/* Both presence and absence of sram regulator are valid cases. */
	sram_reg = regulator_get_optional(cpu_dev, "sram");
	if (!IS_ERR(sram_reg)) {
		ret = regulator_enable(sram_reg);
		if (ret) {
			pr_warn("enable vsram for cpu%d fail\n", cpu);
			goto out_disable_regulator_vproc;
		}
	}

	/* Get OPP-sharing information from "operating-points-v2" bindings */
	ret = dev_pm_opp_of_get_sharing_cpus(cpu_dev, &info->cpus);
	if (ret) {
		pr_err("failed to get OPP-sharing information for cpu%d\n",
		       cpu);
		goto out_disable_regulator_vsram;
	}

	ret = dev_pm_opp_of_cpumask_add_table(&info->cpus);
	if (ret) {
		pr_warn("no OPP table for cpu%d\n", cpu);
		goto out_disable_regulator_vsram;
	}

	ret = clk_prepare_enable(cpu_clk);
	if (ret)
		goto out_free_opp_table;

	ret = clk_prepare_enable(inter_clk);
	if (ret)
		goto out_disable_mux_clock;

	/* Search a safe voltage for intermediate frequency. */
	rate = clk_get_rate(inter_clk);
	opp = dev_pm_opp_find_freq_ceil(cpu_dev, &rate);
	if (IS_ERR(opp)) {
		pr_err("failed to get intermediate opp for cpu%d\n", cpu);
		ret = PTR_ERR(opp);
		goto out_disable_inter_clock;
	}

	info->intermediate_voltage = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);

	info->opp_cpu = cpu;
	info->opp_nb.notifier_call = mtk_cpufreq_opp_notifier;
	ret = dev_pm_opp_register_notifier(cpu_dev, &info->opp_nb);
	if (ret) {
		pr_warn("cannot register opp notification\n");
		goto out_disable_inter_clock;
	}

	mutex_init(&info->lock);
	info->cpu_dev = cpu_dev;
	info->proc_reg = proc_reg;
	info->sram_reg = IS_ERR(sram_reg) ? NULL : sram_reg;
	info->cpu_clk = cpu_clk;
	info->inter_clk = inter_clk;
	info->opp_freq = clk_get_rate(cpu_clk);

	if (info->is_cci_dvfs_support) {
		info->min_volt_shift = 100000;
		info->max_volt_shift = 250000;
		info->proc_max_volt = 1118750;
		info->sram_min_volt = 850000;
		info->sram_max_volt = 1118750;
	} else { /* for old projects */
		info->min_volt_shift = 100000;
		info->max_volt_shift = 200000;
		info->proc_max_volt = 1150000;
		info->sram_min_volt = 0;
		info->sram_max_volt = 1150000;
	}

	/*
	 * If SRAM regulator is present, software "voltage tracking" is needed
	 * for this CPU power domain.
	 */
	info->need_voltage_tracking = !IS_ERR(sram_reg);

	/*
	 * To prevent this scenario:
	 * 1. cci devfreq sets ccipll/vproc with (500MHz/600mV)
	 * 2. cpufreq didn't request the vproc before so we'll encounter the
	 * high freq low voltage issue on cpufreq side (system random hung).
	 */
	ret = mtk_cpufreq_set_highest_voltage(info);
	if (ret) {
		pr_err("cpu%d fails to set opp highest voltage, ret = %d\n",
		       cpu, ret);
		goto out_unregister_pm_opp_notifier;
	}

	return 0;

out_unregister_pm_opp_notifier:
	dev_pm_opp_unregister_notifier(cpu_dev, &info->opp_nb);

out_disable_inter_clock:
	clk_disable_unprepare(inter_clk);

out_disable_mux_clock:
	clk_disable_unprepare(cpu_clk);

out_free_opp_table:
	dev_pm_opp_of_cpumask_remove_table(&info->cpus);

out_disable_regulator_vsram:
	regulator_disable(info->sram_reg);

out_disable_regulator_vproc:
	regulator_disable(info->proc_reg);

out_free_resources:
	if (!IS_ERR(proc_reg))
		regulator_put(proc_reg);
	if (!IS_ERR(sram_reg))
		regulator_put(sram_reg);
	if (!IS_ERR(cpu_clk))
		clk_put(cpu_clk);
	if (!IS_ERR(inter_clk))
		clk_put(inter_clk);

	return ret;
}

static void mtk_cpu_dvfs_info_release(struct mtk_cpu_dvfs_info *info)
{
	if (!IS_ERR(info->proc_reg)) {
		regulator_disable(info->proc_reg);
		regulator_put(info->proc_reg);
	}
	if (!IS_ERR(info->sram_reg)) {
		regulator_disable(info->sram_reg);
		regulator_put(info->sram_reg);
	}
	if (!IS_ERR(info->cpu_clk)) {
		clk_disable_unprepare(info->cpu_clk);
		clk_put(info->cpu_clk);
	}
	if (!IS_ERR(info->inter_clk)) {
		clk_disable_unprepare(info->inter_clk);
		clk_put(info->inter_clk);
	}

	dev_pm_opp_of_cpumask_remove_table(&info->cpus);
}

static int mtk_cpufreq_init(struct cpufreq_policy *policy)
{
	struct mtk_cpu_dvfs_info *info;
	struct cpufreq_frequency_table *freq_table;
	int ret;

	info = mtk_cpu_dvfs_info_lookup(policy->cpu);
	if (!info) {
		pr_err("dvfs info for cpu%d is not initialized.\n",
		       policy->cpu);
		return -EINVAL;
	}

	ret = dev_pm_opp_init_cpufreq_table(info->cpu_dev, &freq_table);
	if (ret) {
		pr_err("failed to init cpufreq table for cpu%d: %d\n",
		       policy->cpu, ret);
		return ret;
	}

	cpumask_copy(policy->cpus, &info->cpus);
	policy->freq_table = freq_table;
	policy->driver_data = info;
	policy->clk = info->cpu_clk;

	dev_pm_opp_of_register_em(policy->cpus);

	return 0;
}

static int mtk_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct mtk_cpu_dvfs_info *info = policy->driver_data;

	dev_pm_opp_free_cpufreq_table(info->cpu_dev, &policy->freq_table);

	return 0;
}

static struct cpufreq_driver mtk_cpufreq_driver = {
	.flags = CPUFREQ_STICKY | CPUFREQ_NEED_INITIAL_FREQ_CHECK |
		 CPUFREQ_HAVE_GOVERNOR_PER_POLICY |
		 CPUFREQ_IS_COOLING_DEV,
	.verify = cpufreq_generic_frequency_table_verify,
	.get = mtk_cpufreq_get,
	.target_index = mtk_cpufreq_set_target,
	.init = mtk_cpufreq_init,
	.exit = mtk_cpufreq_exit,
	.name = "mtk-cpufreq",
	.attr = cpufreq_generic_attr,
};

static int mtk_cpufreq_probe(struct platform_device *pdev)
{
	struct mtk_cpu_dvfs_info *info, *tmp;
	int cpu, ret;

	for_each_possible_cpu(cpu) {
		info = mtk_cpu_dvfs_info_lookup(cpu);
		if (info)
			continue;

		info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
		if (!info) {
			ret = -ENOMEM;
			goto release_dvfs_info_list;
		}

		ret = mtk_cpu_dvfs_info_init(info, cpu);
		if (ret) {
			dev_err(&pdev->dev,
				"failed to initialize dvfs info for cpu%d\n",
				cpu);
			goto release_dvfs_info_list;
		}

		list_add(&info->list_head, &dvfs_info_list);
	}

	ret = cpufreq_register_driver(&mtk_cpufreq_driver);
	if (ret) {
		dev_err(&pdev->dev, "failed to register mtk cpufreq driver\n");
		goto release_dvfs_info_list;
	}

	return 0;

release_dvfs_info_list:
	list_for_each_entry_safe(info, tmp, &dvfs_info_list, list_head) {
		mtk_cpu_dvfs_info_release(info);
		list_del(&info->list_head);
	}

	return ret;
}

static struct platform_driver mtk_cpufreq_platdrv = {
	.driver = {
		.name	= "mtk-cpufreq",
	},
	.probe		= mtk_cpufreq_probe,
};

/* List of machines supported by this driver */
static const struct of_device_id mtk_cpufreq_machines[] __initconst = {
	{ .compatible = "mediatek,mt2701", },
	{ .compatible = "mediatek,mt2712", },
	{ .compatible = "mediatek,mt7622", },
	{ .compatible = "mediatek,mt7623", },
	{ .compatible = "mediatek,mt8169", },
	{ .compatible = "mediatek,mt817x", },
	{ .compatible = "mediatek,mt8173", },
	{ .compatible = "mediatek,mt8176", },
	{ .compatible = "mediatek,mt8183", },
	{ .compatible = "mediatek,mt8516", },
	{ }
};
MODULE_DEVICE_TABLE(of, mtk_cpufreq_machines);

static int __init mtk_cpufreq_driver_init(void)
{
	struct device_node *np;
	const struct of_device_id *match;
	struct platform_device *pdev;
	int err;

	np = of_find_node_by_path("/");
	if (!np)
		return -ENODEV;

	match = of_match_node(mtk_cpufreq_machines, np);
	of_node_put(np);
	if (!match) {
		pr_debug("Machine is not compatible with mtk-cpufreq\n");
		return -ENODEV;
	}

	err = platform_driver_register(&mtk_cpufreq_platdrv);
	if (err)
		return err;

	/*
	 * Since there's no place to hold device registration code and no
	 * device tree based way to match cpufreq driver yet, both the driver
	 * and the device registration codes are put here to handle defer
	 * probing.
	 */
	pdev = platform_device_register_simple("mtk-cpufreq", -1, NULL, 0);
	if (IS_ERR(pdev)) {
		pr_err("failed to register mtk-cpufreq platform device\n");
		return PTR_ERR(pdev);
	}

	return 0;
}
device_initcall(mtk_cpufreq_driver_init);

MODULE_DESCRIPTION("MediaTek CPUFreq driver");
MODULE_AUTHOR("Pi-Cheng Chen <pi-cheng.chen@linaro.org>");
MODULE_LICENSE("GPL v2");
