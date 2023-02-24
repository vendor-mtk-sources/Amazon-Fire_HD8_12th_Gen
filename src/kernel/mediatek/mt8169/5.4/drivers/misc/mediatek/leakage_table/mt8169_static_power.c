// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/proc_fs.h>
#include <linux/debugfs.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/nvmem-consumer.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>

#define NR_HRID	4

struct spower_leakage_info {
	const char *name;
	unsigned int devinfo_idx;
	unsigned long devinfo_offset;
	unsigned int value;
	unsigned int v_of_fuse;
	int t_of_fuse;
	unsigned int instance;
};

struct spower_leakage_info *spower_lkg_info;
static char static_power_buf[128];
static char static_power_buf_precise[128];
static char efuse_hrid_buf[128];

static const int devinfo_table[] = {
	3539,   492,    1038,   106,    231,    17,     46,     2179,
	4,      481,    1014,   103,    225,    17,     45,     2129,
	3,      516,    1087,   111,    242,    19,     49,     2282,
	4,      504,    1063,   108,    236,    18,     47,     2230,
	4,      448,    946,    96,     210,    15,     41,     1986,
	2,      438,    924,    93,     205,    14,     40,     1941,
	2,      470,    991,    101,    220,    16,     43,     2080,
	3,      459,    968,    98,     215,    16,     42,     2033,
	3,      594,    1250,   129,    279,    23,     57,     2621,
	6,      580,    1221,   126,    273,    22,     56,     2561,
	6,      622,    1309,   136,    293,    24,     60,     2745,
	7,      608,    1279,   132,    286,    23,     59,     2683,
	6,      541,    1139,   117,    254,    20,     51,     2390,
	5,      528,    1113,   114,    248,    19,     50,     2335,
	4,      566,    1193,   123,    266,    21,     54,     2503,
	5,      553,    1166,   120,    260,    21,     53,     2446,
	5,      338,    715,    70,     157,    9,      29,     1505,
	3153,   330,    699,    69,     153,    9,      28,     1470,
	3081,   354,    750,    74,     165,    10,     31,     1576,
	3302,   346,    732,    72,     161,    10,     30,     1540,
	3227,   307,    652,    63,     142,    8,      26,     1371,
	2875,   300,    637,    62,     139,    7,      25,     1340,
	2809,   322,    683,    67,     149,    8,      27,     1436,
	3011,   315,    667,    65,     146,    8,      26,     1404,
	2942,   408,    862,    86,     191,    13,     37,     1811,
	1,      398,    842,    84,     186,    12,     36,     1769,
	1,      428,    903,    91,     200,    14,     39,     1896,
	2,      418,    882,    89,     195,    13,     38,     1853,
	2,      371,    785,    78,     173,    11,     33,     1651,
	3458,   363,    767,    76,     169,    10,     32,     1613,
	3379,   389,    823,    82,     182,    12,     35,     1729,
	1,      380,    804,    80,     177,    11,     34,     1689,
};

static int static_power_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "%s", efuse_hrid_buf);
	seq_printf(s, "%s", static_power_buf);
	return 0;
}

static int static_power_open(struct inode *inode, struct file *file)
{
	return single_open(file, static_power_show, NULL);
}

static const struct file_operations static_power_operations = {
	.open = static_power_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int static_power_precise_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "%s", efuse_hrid_buf);
	seq_printf(s, "%s", static_power_buf_precise);
	return 0;
}

static int static_power_precise_open(struct inode *inode, struct file *file)
{
	return single_open(file, static_power_precise_show, NULL);
}

static const struct file_operations static_power_precise_operations = {
	.open = static_power_precise_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#define PROC_FOPS_RO(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _proc_open,		\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

static int spower_lkg_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s", static_power_buf);
	return 0;
}
PROC_FOPS_RO(spower_lkg);

int spower_procfs_init(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(spower_lkg),
	};

	dir = proc_mkdir("leakage", NULL);

	if (!dir) {
		pr_notice("fail to create /proc/leakage @ %s()\n",
								__func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create
		    (entries[i].name, 0660, dir, entries[i].fops))
			pr_notice("%s(), create /proc/leakage/%s failed\n",
				__func__, entries[i].name);
	}
	return 0;
}

static const struct of_device_id mtk_static_power_of_match[] = {
	{ .compatible = "mediatek,mt8169-static-power", },
	{},
};

static int mt_spower_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int devinfo = 0;
	unsigned int temp_lkg;
	struct nvmem_device *nvmem_dev;
	int i;
	unsigned int v_of_fuse;
	int t_of_fuse;
	unsigned int idx = 0;
	char *p_hrid = efuse_hrid_buf;
	char *p_buf = static_power_buf;
	char *p_buf_precise = static_power_buf_precise;
	int n_domain = 0;
	struct device_node *node = pdev->dev.of_node;
	const char *domain = NULL;
	u32 value[6] = {0};
	int ret;
	unsigned long offset;
	int shift;

	nvmem_dev = nvmem_device_get(dev, "mtk_efuse");
	ret = IS_ERR(nvmem_dev);
	if (ret) {
		ret = PTR_ERR(nvmem_dev);
		return ret;
	}

	n_domain = of_property_count_strings(node, "domain");
	if (n_domain <= 0) {
		dev_err(&pdev->dev, "failed to find leakage domain\n");
		ret = -EINVAL;
		goto out_free_nvmem;
	}

	ret = of_property_read_u32_array(node, "hrid", value, NR_HRID);
	for (i = 0; i < NR_HRID; ++i) {
		nvmem_device_read(nvmem_dev, value[i], sizeof(__u32), &devinfo);
		p_hrid += sprintf(p_hrid, "e-fuse HRID[%d]: 0x%8x\n", i, devinfo);
	}

	spower_lkg_info = kmalloc_array(n_domain, sizeof(struct spower_leakage_info), GFP_KERNEL);

	for (i = 0; i < n_domain; i++) {
		ret = of_property_read_string_index(node, "domain", i, &domain);
		ret = of_property_read_u32_array(node, domain, value, ARRAY_SIZE(value));
		spower_lkg_info[i].name = domain;
		spower_lkg_info[i].devinfo_offset = value[1];
		spower_lkg_info[i].v_of_fuse = value[2];
		spower_lkg_info[i].t_of_fuse = value[3];
		spower_lkg_info[i].instance = value[4];

		nvmem_device_read(nvmem_dev, value[0], sizeof(__u32), &devinfo);
		offset = spower_lkg_info[i].devinfo_offset;
		shift = find_first_bit(&offset, 32);
		if (shift > 31) {
			dev_err(&pdev->dev, "failed to get valid shift: %d\n", shift);
			ret = -EINVAL;
			goto out_free_leakage;
		}

		temp_lkg = (devinfo & spower_lkg_info[i].devinfo_offset) >> shift;
		if (temp_lkg != 0) {
			temp_lkg = devinfo_table[temp_lkg];
			spower_lkg_info[i].value = temp_lkg * spower_lkg_info[i].v_of_fuse;
		} else
			spower_lkg_info[i].value = 0;
	}
	nvmem_device_put(nvmem_dev);

	for (idx = 0; idx < n_domain; idx++)  {
		v_of_fuse = spower_lkg_info[idx].v_of_fuse;
		t_of_fuse = spower_lkg_info[idx].t_of_fuse;
		p_buf += sprintf(p_buf, "%d/",
			(spower_lkg_info[idx].value / 1000 /
			spower_lkg_info[idx].instance));
		p_buf_precise += sprintf(p_buf_precise, "%d.%d/",
			DIV_ROUND_CLOSEST(spower_lkg_info[idx].value,
				spower_lkg_info[idx].instance * 100) / 10,
			DIV_ROUND_CLOSEST(spower_lkg_info[idx].value,
					spower_lkg_info[idx].instance *
					100) % 10
		);
	}

	p_buf += sprintf(p_buf, "\n");
	p_buf_precise += sprintf(p_buf_precise, "\n");

	debugfs_create_file("static_power", S_IFREG | 0400, NULL, NULL,
			    &static_power_operations);

	debugfs_create_file("static_power_precise", S_IFREG | 0400, NULL, NULL,
			    &static_power_precise_operations);

	spower_procfs_init();

	return 0;

out_free_leakage:
	kfree(spower_lkg_info);

out_free_nvmem:
	nvmem_device_put(nvmem_dev);

	return ret;
}

static struct platform_driver mtk_static_power_driver = {
	.probe    = mt_spower_init,
	.driver  = {
		.name   = "mt8169-static-power",
		.of_match_table = mtk_static_power_of_match,
	},
};

int mt_spower_get_efuse_lkg(int dev)
{
	return spower_lkg_info[dev].value / 1000;
}
/*EXPORT_SYMBOL(mt_spower_get_efuse_lkg);*/

module_platform_driver(mtk_static_power_driver);

MODULE_DESCRIPTION("MediaTek Leakage Driver");
MODULE_LICENSE("GPL");
