/*
 * Copyright (C) 2021-2022 Lab126, Inc.  All rights reserved.
 * Author: Akwasi Boateng <boatenga@lab126.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */
#include <linux/thermal_framework.h>
#include <linux/slab.h>
#include <linux/list_sort.h>
#include <linux/pm_qos.h>
#include <linux/cpufreq.h>
#include <linux/energy_model.h>
#include <leds-mtk-pwm.h>

#define WPC_NAME "Wireless"
#define BATTERY_NAME "battery"
#define CHARGER_NAME "mtk-master-charger"
#define COMPATIBLE_NAME "amazon,virtual_sensor-thermal"
#define OFFLINE_TEMP 25000	/* 25.000 degree Celsius */
#define IS_NULL(p)                    (((void*)(p)) == (void*)0)
#define IS_NOT_NULL(p)                (((void*)(p)) != (void*)0)

static struct power_supply *bat_psy;
static struct power_supply *wpc_psy;
static char* ntc_name[]={"charge", "pmic","mux_sd","mux_9221","ap_ntc","usb_ntc"};
static struct vs_zone_dts_data vs_thermal_data;

/**
 * struct vs_cpufreq_cdev - data for cooler budget with cpufreq
 * @max_level: maximum cooling level. One less than total number of valid
 *	cpufreq frequencies.
 * @em: Reference on the Energy Model of the device
 * @policy: cpufreq policy.
 * @qos_req: PM QoS contraint to apply
 */
struct vs_cpufreq_cdev {
	unsigned int max_level;
	struct em_perf_domain *em;
	struct cpufreq_policy *policy;
	struct freq_qos_request qos_req;
};

#ifdef CONFIG_CPU_MULTIPLE_CLUSTERS
static int cpu_polices[] = {0, 6};
enum core_cluster {
	CPU_CLUSTER_0 = 0,
	CPU_CLUSTER_1,
	CPU_CLUSTER_NUM
};
static struct vs_cpufreq_cdev *cpufreq_cdev[CPU_CLUSTER_NUM];
#else
static struct vs_cpufreq_cdev *cpufreq_cdev;
#endif

enum thermal_bank_name {
        THERMAL_BANK0 = 0,
        THERMAL_BANK1,
        THERMAL_BANK2,
        THERMAL_BANK3,
        THERMAL_BANK4,
        THERMAL_BANK5,
        THERMAL_BANK_NUM
};

#ifdef CONFIG_CPU_MULTIPLE_CLUSTERS
static int vs_init_cpufreq_limit_power(int group, int cpu)
{
	struct cpufreq_policy *policy = NULL;
	struct em_perf_domain *em = NULL;
	unsigned int cpu_table_count = 0;
	int ret = -1;

	policy = cpufreq_cpu_get(cpu);
	if (!policy) {
		pr_err("%s: get cpufreq policy failed\n", __func__);
		return ret;
	}

	em = em_cpu_get(policy->cpu);
	if (!em) {
		pr_err("%s: get cpu energy model failed\n", __func__);
		return ret;
	}

	cpu_table_count = cpufreq_table_count_valid_entries(policy);
	if (!cpu_table_count) {
		pr_err("%s: CPUFreq table not found or has no valid entries\n",
			 __func__);
		return ret;
	}

	cpufreq_cdev[group] = kzalloc(sizeof(struct vs_cpufreq_cdev), GFP_KERNEL);
	if (!cpufreq_cdev[group]) {
		pr_err("%s: vs_cpufreq_cdev allocate memory failed\n", __func__);
		return -ENOMEM;
	}

	cpufreq_cdev[group]->policy = policy;
	cpufreq_cdev[group]->em = em;
	cpufreq_cdev[group]->max_level = cpu_table_count - 1;

	ret = freq_qos_add_request(&policy->constraints,
				   &cpufreq_cdev[group]->qos_req, FREQ_QOS_MAX,
				   FREQ_QOS_MAX_DEFAULT_VALUE);

	if (ret < 0) {
		pr_err("%s: Fail to add freq constraint (%d)\n",
				__func__, ret);
		kfree(cpufreq_cdev[group]);
		cpufreq_cdev[group] = NULL;
		return ret;
	}

	pr_info("%s: cpufreq policy(%d) init success!\n", __func__, cpu);

	return ret;
}

static unsigned int vs_cpu_power_to_freq(struct em_perf_domain *em,
					 unsigned int max_level,
					 unsigned int power)
{
	int i;

	for (i = max_level; i > 0; i--) {
		if (power >= em->table[i].power)
			break;
	}

	return em->table[i].frequency;
}

static int get_cpu_cluster_max_power(int cluster)
{
	unsigned int num_cpus = 0;
	int index = 0;

	index = (cpufreq_cdev[cluster])->max_level;
	num_cpus = cpumask_weight(cpufreq_cdev[cluster]->policy->related_cpus);

	return cpufreq_cdev[cluster]->em->table[index].power * num_cpus;
}

static int get_cpu_cluster_min_power(int cluster)
{
        unsigned int num_cpus = 0;

        num_cpus = cpumask_weight(cpufreq_cdev[cluster]->policy->related_cpus);
        return cpufreq_cdev[cluster]->em->table[0].power * num_cpus;
}

static int set_cpufreq_thermal_power(unsigned int budget)
{
	unsigned int target_cpufreq, num_cpus = 0, little_cpu_max_power = 0;
	int little_cpu_min_power = 0, large_cpu_min_power = 0, remain_budget = 0;
	int i, ret = 0;
	unsigned int cpu_budget[CPU_CLUSTER_NUM];

	if (!cpufreq_cdev[0])
		for (i = 0; i < CPU_CLUSTER_NUM; i++)
			vs_init_cpufreq_limit_power(i, cpu_polices[i]);

	if (!budget)
		budget = get_cpu_cluster_max_power(CPU_CLUSTER_0) +
			get_cpu_cluster_max_power(CPU_CLUSTER_1);

	little_cpu_max_power = get_cpu_cluster_max_power(CPU_CLUSTER_0);
	little_cpu_min_power = get_cpu_cluster_min_power(CPU_CLUSTER_0);
	large_cpu_min_power = get_cpu_cluster_min_power(CPU_CLUSTER_1);

	cpu_budget[CPU_CLUSTER_0] = little_cpu_min_power;
	cpu_budget[CPU_CLUSTER_1] = large_cpu_min_power;
	remain_budget = budget - (little_cpu_min_power + large_cpu_min_power);
	if (remain_budget > 0) {
		if (remain_budget <= (little_cpu_max_power - cpu_budget[CPU_CLUSTER_0])) {
			cpu_budget[CPU_CLUSTER_0] += remain_budget;
		} else {
			cpu_budget[CPU_CLUSTER_0] = little_cpu_max_power;
			cpu_budget[CPU_CLUSTER_1] = budget - little_cpu_max_power;
		}
	} else {
		pr_notice("VS temp is to high,power is less than Sum of big and little cores!");
	}
	pr_info("budget:%u buget[0]:%u buget[1]:%u\n", budget,
		cpu_budget[CPU_CLUSTER_0], cpu_budget[CPU_CLUSTER_1]);

	for (i = 0; i < CPU_CLUSTER_NUM; i++) {
		if (cpufreq_cdev[i] && cpufreq_cdev[i]->em) {
			num_cpus = cpumask_weight(cpufreq_cdev[i]->policy->related_cpus);
			target_cpufreq = vs_cpu_power_to_freq(cpufreq_cdev[i]->em,
							      cpufreq_cdev[i]->max_level,
							      cpu_budget[i]/num_cpus);
			freq_qos_update_request(&cpufreq_cdev[i]->qos_req,
						target_cpufreq);
		} else {
			pr_err("%s: Cpufreq failed to update budget (%d) %d\n",
				__func__, i, cpu_budget[i]);
			return -1;
		}
	}

	return ret;
}
#else
static int vs_init_cpufreq_limit_power(void)
{
	struct cpufreq_policy *policy = NULL;
	struct em_perf_domain *em = NULL;
	unsigned int cpu_table_count = 0;
	int ret = -1;

	policy = cpufreq_cpu_get(0);
	if (!policy) {
		pr_err("%s: get cpufreq policy failed\n", __func__);
		return ret;
	}

	em = em_cpu_get(policy->cpu);
	if (!em) {
		pr_err("%s: get cpu energy model failed\n", __func__);
		return ret;
	}

	cpu_table_count = cpufreq_table_count_valid_entries(policy);
	if (!cpu_table_count) {
		pr_err("%s: CPUFreq table not found or has no valid entries\n",
			 __func__);
		return ret;
	}

	cpufreq_cdev = kzalloc(sizeof(*cpufreq_cdev), GFP_KERNEL);
	if (!cpufreq_cdev) {
		pr_err("%s: vs_cpufreq_cdev allocate memory failed\n", __func__);
		return -ENOMEM;
	}

	cpufreq_cdev->policy = policy;
	cpufreq_cdev->em = em;
	cpufreq_cdev->max_level = cpu_table_count - 1;

	ret = freq_qos_add_request(&policy->constraints,
				   &cpufreq_cdev->qos_req, FREQ_QOS_MAX,
				   FREQ_QOS_MAX_DEFAULT_VALUE);

	if (ret < 0) {
		pr_err("%s: Fail to add freq constraint (%d)\n",
				__func__, ret);
		return ret;
	}

	return ret;
}

static unsigned int vs_cpu_power_to_freq(struct em_perf_domain *em,
					 unsigned int max_level,
					 unsigned int power)
{
	int i;

	if (!power)
		return em->table[max_level].frequency;

	for (i = max_level; i > 0; i--) {
		if (power >= em->table[i].power)
			break;
	}

	return em->table[i].frequency;
}

static int set_cpufreq_thermal_power(unsigned int budget)
{
	unsigned int target_cpufreq, num_cpus = 0;
	int ret = 0;

	if (!cpufreq_cdev)
		ret = vs_init_cpufreq_limit_power();

	if (cpufreq_cdev && cpufreq_cdev->em) {
		num_cpus = cpumask_weight(cpufreq_cdev->policy->related_cpus);
		target_cpufreq = vs_cpu_power_to_freq(cpufreq_cdev->em,
						      cpufreq_cdev->max_level,
						      budget/num_cpus);
		freq_qos_update_request(&cpufreq_cdev->qos_req,
					target_cpufreq);
	} else {
		pr_err("%s: Cpufreq failed to update budget (%d)\n",
		       __func__, budget);
		ret = -1;
	}

	return ret;
}
#endif

static int vs_wpc_read_temp(void)
{
	union power_supply_propval val;
	int ret;

	if (!wpc_psy) {
		wpc_psy = power_supply_get_by_name(WPC_NAME);
		if (!wpc_psy) {
			pr_err("%s: get power supply %s failed\n",
			__func__, WPC_NAME);
			return OFFLINE_TEMP;
		}
	}

	ret = power_supply_get_property(wpc_psy, POWER_SUPPLY_PROP_TEMP, &val);
	if (ret) {
		pr_debug("E_WF: %s device %s is not ready\n",
				__func__, WPC_NAME);
		return OFFLINE_TEMP;
	}

	/* Convert tenths of degree Celsius to milli degree Celsius. */
	return val.intval * 1000;
}

int vs_get_wpc_online(void)
{
	union power_supply_propval val;
	int ret;

	if (!wpc_psy) {
		wpc_psy = power_supply_get_by_name(WPC_NAME);
		if (!wpc_psy) {
			pr_err("%s: get power supply %s failed\n",
			__func__, WPC_NAME);
			/* return 0, wpc is offline */
			return 0;
		}
	}
	ret = power_supply_get_property(wpc_psy, POWER_SUPPLY_PROP_ONLINE, &val);
	if (ret) {
		pr_debug("E_WF: %s device %s is not ready\n",
				__func__, WPC_NAME);
		/* return 0, wpc is offline */
		return 0;
	}
	return val.intval;
}
EXPORT_SYMBOL_GPL(vs_get_wpc_online);

static int get_battery_temp(void)
{
	union power_supply_propval val;
	int ret;

	if (!bat_psy) {
		bat_psy = power_supply_get_by_name(BATTERY_NAME);
		if (!bat_psy) {
			pr_err("%s: get power supply %s failed\n",
			__func__, BATTERY_NAME);
			return OFFLINE_TEMP;
		}
	}

	ret = power_supply_get_property(bat_psy, POWER_SUPPLY_PROP_TEMP, &val);
	if (ret) {
		pr_debug("E_WF: %s device %s is not ready\n",
				__func__, BATTERY_NAME);
		return OFFLINE_TEMP;
	}

	pr_debug("%s: thermal_zone: battery temperature is %d\n",
                        __func__, val.intval);
	/* Convert tenths of degree Celsius to milli degree Celsius. */
	return val.intval * 100;
}

static int set_charging_level_limit(int level_limit,
		enum power_supply_property property)
{
	int ret = 0;
	union power_supply_propval propval;

	if (!bat_psy) {
		bat_psy = power_supply_get_by_name(BATTERY_NAME);
		if (!bat_psy) {
			pr_err("%s: get power supply %s failed\n",
			__func__, BATTERY_NAME);
			return -1;
		}
	}

	propval.intval = level_limit;
	ret = power_supply_set_property(bat_psy, property, &propval);
	if (ret < 0)
		pr_err("%s: VS set psy charging level_limit=%d failed, ret = %d\n",
			__func__, level_limit, ret);
	return ret;
}

static int get_charger_temp(void)
{
	union power_supply_propval val;
	int ret;
	static struct power_supply *chg_psy;

	if (!chg_psy) {
		chg_psy = power_supply_get_by_name(CHARGER_NAME);
		if (!chg_psy || IS_ERR(chg_psy)) {
			pr_err("%s: get power supply %s failed\n",
			__func__, CHARGER_NAME);
			return OFFLINE_TEMP;
		}
	}

	ret = power_supply_get_property(chg_psy, POWER_SUPPLY_PROP_TEMP, &val);
	if (ret) {
		pr_debug("E_WF: %s device %s is not ready\n",
				__func__, CHARGER_NAME);
		return OFFLINE_TEMP;
	}

	pr_debug("%s: thermal_zone: charger temperature is %d\n",
                        __func__, val.intval);
	/* Convert tenths of degree Celsius to milli degree Celsius. */
	return val.intval * 100;
}

#ifdef CONFIG_THERMAL_FOD
static int fod_set_charging_level_limit(int level_limit,
		enum power_supply_property property)
{
	int ret = 0;
	union power_supply_propval propval;

	if (!wpc_psy) {
		wpc_psy = power_supply_get_by_name(WPC_NAME);
		if (!wpc_psy) {
			pr_err("%s: get power supply %s failed\n",
				__func__, WPC_NAME);
			return -1;
		}
	}

	propval.intval = level_limit;
	ret = power_supply_set_property(wpc_psy, property, &propval);
	if (ret < 0)
		pr_err("%s: VS set psy charging level_limit=%d failed, ret = %d\n",
			__func__, level_limit, ret);
	return ret;
}
#endif

/* Get the current temperature of the thermal sensor. */
int vs_thermal_sensor_get_temp(enum vs_thermal_sensor_id id, int index)
{
	switch (id) {
	case VS_THERMAL_SENSOR_PMIC:
		return get_pmic_thermal_temp(index);
	case VS_THERMAL_SENSOR_BATTERY:
		return get_battery_temp();
	case VS_THERMAL_SENSOR_THERMISTOR:
		return get_gadc_thermal_temp(ntc_name[index]);
	case VS_THERMAL_SENSOR_WIRELESS_CHG:
		return vs_wpc_read_temp();
	case VS_THERMAL_SENSOR_CHARGER:
		return get_charger_temp();
	default:
		pr_err("E_WF: %s id %d doesn't exist thermal sensor\n",
			__func__, id);
		return OFFLINE_TEMP;
	}
}
EXPORT_SYMBOL_GPL(vs_thermal_sensor_get_temp);

/* Set a level limit via the thermal cooler. */
int vs_set_cooling_level(struct thermal_cooling_device *cdev,
	enum vs_thermal_cooler_id id, int level_limit)
{
	switch (id) {
	case VS_THERMAL_COOLER_BUDGET:
		return set_cpufreq_thermal_power(level_limit);
	case VS_THERMAL_COOLER_BCCT:
		return set_charging_level_limit(
			(level_limit == -1) ? -1 : level_limit * 1000,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT);
	case VS_THERMAL_COOLER_BACKLIGHT:
		return set_max_brightness(level_limit, true);
	case VS_THERMAL_COOLER_WIRELESS_CHG:
		pr_warn("%s cooling_device: %s, level_limit:%d(mW)\n",
			__func__, cdev->type, level_limit);
		return set_charging_level_limit(level_limit,
				POWER_SUPPLY_PROP_THERMAL_INPUT_POWER_LIMIT);
#ifdef CONFIG_THERMAL_FOD
	case VS_THERMAL_COOLER_FOD_WIRELESS_CHG:
		pr_warn("%s cooling_device: %s, level_limit:%d(mA)\n",
			__func__, cdev->type, level_limit);
		return fod_set_charging_level_limit(level_limit,
				POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT);
#endif
	default:
		pr_err("E_WF: %s doesn't exist thermal cooler\n",
			__func__);
		return -1;
	}
}
EXPORT_SYMBOL_GPL(vs_set_cooling_level);

#ifdef CONFIG_THERMAL_DEBOUNCE
/* thermal debounce design interface */
int thermal_zone_debounce(int *pre_temp, int *curr_temp,
		int delta_temp, int *counter, char *thermal_zone_device_type)
{
	int ret = 0;
	/* invalid change */
	if ((*counter != 0)
			&& (abs(*pre_temp - *curr_temp) > delta_temp)) {
		pr_err("%s: [%s] curr_temp: %d, pre_temp: %d,"
				" temp diff(%d) too large, drop this data\n",
				__func__, thermal_zone_device_type,
				*curr_temp, *pre_temp, (*curr_temp - *pre_temp));
		ret = -1;
	}
	if (*counter == 0)
		(*counter)++;
	/* update previous temp */
	*pre_temp = *curr_temp;
	return ret;
}
#endif

int init_vs_thermal_platform_data(void)
{
	int i, ret = 0;
	struct device_node *node;
	vs_thermal_data.vs_zone_dts_nums = 0;


	for_each_compatible_node(node, NULL, COMPATIBLE_NAME) {
		if (!of_device_is_available(node)
			|| !of_device_is_available(of_get_parent(node)))
			continue;
		vs_thermal_data.vs_zone_dts_nums++;
	}
	pr_info("%s vs_thermal_data.vs_zone_dts_nums (%d)\n", __func__, vs_thermal_data.vs_zone_dts_nums);

	vs_thermal_data.vs_pdata = kcalloc(vs_thermal_data.vs_zone_dts_nums,
			sizeof(struct vs_thermal_platform_data), GFP_KERNEL);
	if (!vs_thermal_data.vs_pdata) {
		pr_err("%s: vs_thermal_data->vs_pdata failed to allocate memory!\n",
			__func__);
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < vs_thermal_data.vs_zone_dts_nums; i++) {
		INIT_LIST_HEAD(&vs_thermal_data.vs_pdata[i].ts_list);
		mutex_init(&vs_thermal_data.vs_pdata[i].therm_lock);
	}

out:
	return ret;
}

struct vs_zone_dts_data *get_vs_thermal_data(void)
{
	return &vs_thermal_data;
}
EXPORT_SYMBOL(get_vs_thermal_data);

int thermal_dev_register(struct thermal_dev *tdev)
{
	struct vs_thermal_platform_data *pdata = NULL;

	if (unlikely(IS_ERR_OR_NULL(tdev))) {
		pr_err("%s: NULL sensor thermal device\n", __func__);
		return -ENODEV;
	}
	if (!tdev->dev_ops->get_temp) {
		pr_err("%s: Error getting get_temp()\n", __func__);
		return -EINVAL;
	}
	pr_info("%s %s select_device: %d thermal_sensor_id:%d aux_channel_num %d\n",
		__func__, tdev->name, tdev->tdp->select_device,
		tdev->tdp->thermal_sensor_id, tdev->tdp->aux_channel_num);

	if (tdev->tdp->select_device < vs_thermal_data.vs_zone_dts_nums) {
		pdata = &vs_thermal_data.vs_pdata[tdev->tdp->select_device];
	} else {
		pdata = &vs_thermal_data.vs_pdata[0];
		pr_err("%s select_device invalid!\n", tdev->name);
	}
	mutex_lock(&pdata->therm_lock);
	list_add_tail(&tdev->node, &pdata->ts_list);
	mutex_unlock(&pdata->therm_lock);
	return 0;
}

static int level_cmp(void *priv, struct list_head *a, struct list_head *b)
{
	struct cooler_sort_list *cooler_a =
	    container_of(a, struct cooler_sort_list, list);
	struct cooler_sort_list *cooler_b =
	    container_of(b, struct cooler_sort_list, list);
	int level;

	if (!cooler_a || !cooler_b || !(cooler_a->pdata)
		|| !(cooler_b->pdata)) {
		pr_err("%s cooler_a:%p, cooler_b:%p,"
			" cooler_a->pdata or cooler_b->pdata is NULL!\n",
			__func__, cooler_a, cooler_b);
		return -EINVAL;
	}

	level = cooler_a->pdata->level - cooler_b->pdata->level;

	return level;
}

int thermal_level_compare(struct vs_cooler_platform_data *cooler_data,
			  struct cooler_sort_list *head, bool positive_seq)
{
	struct cooler_sort_list *level_list;

	list_sort(NULL, &head->list, level_cmp);

	if (positive_seq)
		level_list =
		    list_entry(head->list.next, struct cooler_sort_list, list);
	else
		level_list =
		    list_entry(head->list.prev, struct cooler_sort_list, list);

	if (!level_list || !(level_list->pdata)) {
		pr_err("%s level_list:%p or level_list->pdata is NULL!\n", __func__, level_list);
		return -EINVAL;
	}
	return level_list->pdata->level;
}
EXPORT_SYMBOL_GPL(thermal_level_compare);

void thermal_parse_node_int(const struct device_node *np,
			    const char *node_name, int *cust_val)
{
	u32 val = 0;

	if (of_property_read_u32(np, node_name, &val) == 0) {
		(*cust_val) = (int)val;
		pr_debug("Get %s %d\n", node_name, *cust_val);
	} else
		pr_notice("Get %s failed\n", node_name);
}
EXPORT_SYMBOL(thermal_parse_node_int);

struct thermal_dev_params *thermal_sensor_dt_to_params(struct device *dev,
						       struct thermal_dev_params
						       *params,
						       struct
						       thermal_dev_node_names
						       *name_params)
{
	struct device_node *np = dev->of_node;
	int offset_invert = 0;
	int weight_invert = 0;

	if (!params || !name_params) {
		dev_err(dev, "the params or name_params is NULL\n");
		return NULL;
	}

	thermal_parse_node_int(np, name_params->offset_name, &params->offset);
	thermal_parse_node_int(np, name_params->alpha_name, &params->alpha);
	thermal_parse_node_int(np, name_params->weight_name, &params->weight);
	thermal_parse_node_int(np, name_params->select_device_name,
			       &params->select_device);
	thermal_parse_node_int(np, name_params->thermal_sensor_id,
				&params->thermal_sensor_id);

	if (*name_params->offset_invert_name)
		thermal_parse_node_int(np, name_params->offset_invert_name,
				       &offset_invert);

	if (offset_invert)
		params->offset = 0 - params->offset;

	if (*name_params->weight_invert_name)
		thermal_parse_node_int(np, name_params->weight_invert_name,
				       &weight_invert);

	if (weight_invert)
		params->weight = 0 - params->weight;

	if (*name_params->aux_channel_num_name)
		thermal_parse_node_int(np, name_params->aux_channel_num_name,
				       &params->aux_channel_num);

	return params;
}

void virtual_sensor_thermal_parse_node_string_index(const struct device_node *np,
							   const char *node_name,
							   int index,
							   char *cust_string)
{
	const char *string;

	if (of_property_read_string_index(np, node_name, index, &string) == 0) {
		strncpy(cust_string, string, strlen(string));
		pr_debug("Get %s %s\n", node_name, cust_string);
	} else
		pr_notice("Get %s failed\n", node_name);
}
EXPORT_SYMBOL(virtual_sensor_thermal_parse_node_string_index);

static void cooler_parse_node_int_array(const struct device_node *np,
			const char *node_name, int *tripsdata)
{
	u32 array[THERMAL_MAX_TRIPS] = {0};
	int i = 0;

	if (of_property_read_u32_array(np, node_name, array, ARRAY_SIZE(array)) == 0) {
		for (i = 0; i < ARRAY_SIZE(array); i++) {
			tripsdata[i] = array[i];
			pr_debug("Get %s %d\n", node_name, tripsdata[i]);
		}
	} else
		pr_notice("Get %s failed\n", node_name);
}

static void print_cooler_dts(const struct platform_device *pdev,
	const struct vs_cooler_platform_data *pcdata)
{
	int i = 0;
	int offset = 0;
	char levels[THERMAL_NAME_LENGTH * THERMAL_MAX_TRIPS];

	pr_info("Print info: %s cooler dts.\n", dev_name(&pdev->dev));
	pr_info("type %s\n", pcdata->type);
	pr_info("level %d\n", pcdata->level);
	pr_info("thermal_cooler_id %d\n", pcdata->thermal_cooler_id);
	while (i < THERMAL_MAX_TRIPS) {
		offset += scnprintf(levels + offset,
			(THERMAL_NAME_LENGTH * THERMAL_MAX_TRIPS) - offset,
			"%d ", pcdata->levels[i]);
		i++;
	}
	pr_info("levels %s\n", levels);
}

void cooler_init_cust_data_from_dt(struct platform_device *dev,
				struct vs_cooler_platform_data *pcdata)
{
	struct device_node *np = dev->dev.of_node;

	virtual_sensor_thermal_parse_node_string_index(np, "type",
						0, (char *)&pcdata->type);
	thermal_parse_node_int(np, "state", (int *)&pcdata->state);
	thermal_parse_node_int(np, "max_state",
					(int *)&pcdata->max_state);
	thermal_parse_node_int(np, "level", &pcdata->level);
	thermal_parse_node_int(np, "thermal_cooler_id",
					&pcdata->thermal_cooler_id);
	cooler_parse_node_int_array(np, "levels", pcdata->levels);
	print_cooler_dts(dev, pcdata);
}
EXPORT_SYMBOL_GPL(cooler_init_cust_data_from_dt);
