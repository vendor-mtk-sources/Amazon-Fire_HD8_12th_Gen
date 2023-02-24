// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <drm/drmP.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>

static const struct of_device_id ext_mipi_phy_match[] = {
	{ .compatible = "mediatek,ext-mipi-phy" },
	{},
};

struct ext_mipi_phy {
	struct device *dev;
	void __iomem *regs;
	u32 data_rate;

	struct clk_hw pll_hw;
	struct clk *pll;
};

static inline struct ext_mipi_phy *ext_mipi_phy_from_clk_hw(struct clk_hw *hw)
{
	return container_of(hw, struct ext_mipi_phy, pll_hw);
}

static void ext_mipi_phy_write60384(struct ext_mipi_phy *ext_phy, u8 dev_addr,
				    u8 write_addr, u8 write_data)
{
	u8 read_data;

	writel(0x2, ext_phy->regs + 0x14);
	writel(0x1, ext_phy->regs + 0x18);
	writel(dev_addr << 0x1, ext_phy->regs + 0x04);
	writel(write_addr, ext_phy->regs + 0x0);
	writel(write_data, ext_phy->regs + 0x0);
	writel(0x1, ext_phy->regs + 0x24);
	while ((readl(ext_phy->regs + 0xC) & 0x1) != 0x1)
		;
	writel(0xFF, ext_phy->regs + 0xC);

	writel(0x1, ext_phy->regs + 0x14);
	writel(0x1, ext_phy->regs + 0x18);
	writel(dev_addr << 0x1, ext_phy->regs + 0x04);
	writel(write_addr, ext_phy->regs + 0x0);
	writel(0x1, ext_phy->regs + 0x24);
	while ((readl(ext_phy->regs + 0xC) & 0x1) != 0x1)
		;
	writel(0xFF, ext_phy->regs + 0xC);

	writel(0x1, ext_phy->regs + 0x14);
	writel(0x1, ext_phy->regs + 0x18);
	writel((dev_addr << 0x1) + 1, ext_phy->regs + 0x04);
	writel(0x1, ext_phy->regs + 0x24);
	while ((readl(ext_phy->regs + 0xC) & 0x1) != 0x1)
		;
	writel(0xFF, ext_phy->regs + 0xC);

	read_data = readl(ext_phy->regs);

	if (read_data == write_data)
		dev_info(ext_phy->dev, "MIPI wr 0x%02x, rd 0x%02x success\n",
			 read_data, write_data);
	else
		dev_info(ext_phy->dev, "MIPI wr 0x%02x, rd 0x%02x fail\n",
			 read_data, write_data);
}

static int ext_mipi_phy_pll_prepare(struct clk_hw *hw)
{
	struct ext_mipi_phy *ext_phy = ext_mipi_phy_from_clk_hw(hw);
	u8 postdiv, prediv, fbk;

	postdiv = 3;
	prediv = 0;
	fbk = 4;

	if (ext_phy == NULL)
		pr_info("ext_phy is  null %s\n", __func__);
	else
		pr_info("ext_phy is  ok %s\n", __func__);

	ext_mipi_phy_write60384(ext_phy, 0x18, 0x00, 0x10);
	ext_mipi_phy_write60384(ext_phy, 0x20, 0x42, 0x01);
	ext_mipi_phy_write60384(ext_phy, 0x20, 0x43, 0x01);
	ext_mipi_phy_write60384(ext_phy, 0x20, 0x05, 0x01);
	ext_mipi_phy_write60384(ext_phy, 0x20, 0x22, 0x01);

	ext_mipi_phy_write60384(ext_phy, 0x30, 0x44, 0x83);
	ext_mipi_phy_write60384(ext_phy, 0x30, 0x40, 0x82);

	usleep_range(30, 100);

	ext_mipi_phy_write60384(ext_phy, 0x30, 0x00, 0x03);

	ext_mipi_phy_write60384(ext_phy, 0x30, 0x68, 0x03);
	ext_mipi_phy_write60384(ext_phy, 0x30, 0x68, 0x01);

	ext_mipi_phy_write60384(ext_phy, 0x30, 0x50, (postdiv & 0x1) << 7 | prediv << 1);
	ext_mipi_phy_write60384(ext_phy, 0x30, 0x51, (postdiv >> 1) & 0x3);
	ext_mipi_phy_write60384(ext_phy, 0x30, 0x54, 0x01);

	ext_mipi_phy_write60384(ext_phy, 0x30, 0x58, 0);
	ext_mipi_phy_write60384(ext_phy, 0x30, 0x59, 0);
	ext_mipi_phy_write60384(ext_phy, 0x30, 0x5a, 0);
	ext_mipi_phy_write60384(ext_phy, 0x30, 0x5b, fbk << 2);

	usleep_range(20, 100);

	ext_mipi_phy_write60384(ext_phy, 0x30, 0x04, 0x11);
	ext_mipi_phy_write60384(ext_phy, 0x30, 0x08, 0x01);
	ext_mipi_phy_write60384(ext_phy, 0x30, 0x0C, 0x01);
	ext_mipi_phy_write60384(ext_phy, 0x30, 0x10, 0x01);
	ext_mipi_phy_write60384(ext_phy, 0x30, 0x14, 0x01);

	ext_mipi_phy_write60384(ext_phy, 0x30, 0x64, 0x20);

	ext_mipi_phy_write60384(ext_phy, 0x30, 0x50, ((postdiv & 1) << 7) | (prediv << 1) | 1);

	ext_mipi_phy_write60384(ext_phy, 0x30, 0x28, 0x00);
	ext_mipi_phy_write60384(ext_phy, 0x18, 0x27, 0x70);

	return 0;
}

static void ext_mipi_phy_pll_unprepare(struct clk_hw *hw)
{
}

static long ext_mipi_phy_pll_round_rate(struct clk_hw *hw, unsigned long rate,
					unsigned long *prate)
{
	DRM_DEBUG_DRIVER("\n");

	return clamp_val(rate, 50000000, 1250000000);
}

static int ext_mipi_phy_pll_set_rate(struct clk_hw *hw, unsigned long rate,
				     unsigned long parent_rate)
{
	struct ext_mipi_phy *ext_phy = ext_mipi_phy_from_clk_hw(hw);

	DRM_DEBUG_DRIVER("\n");

	ext_phy->data_rate = rate;

	return 0;
}

static unsigned long ext_mipi_phy_pll_recalc_rate(struct clk_hw *hw,
						  unsigned long parent_rate)
{
	struct ext_mipi_phy *ext_phy = ext_mipi_phy_from_clk_hw(hw);

	DRM_DEBUG_DRIVER("\n");

	return ext_phy->data_rate;
}

static const struct clk_ops ext_mipi_phy_pll_ops = {
	.prepare = ext_mipi_phy_pll_prepare,
	.unprepare = ext_mipi_phy_pll_unprepare,
	.round_rate = ext_mipi_phy_pll_round_rate,
	.set_rate = ext_mipi_phy_pll_set_rate,
	.recalc_rate = ext_mipi_phy_pll_recalc_rate,
};

static int ext_mipi_phy_power_on_signal(struct phy *phy)
{
	DRM_DEBUG_DRIVER("%d\n", __LINE__);

	return 0;
}

static int ext_mipi_phy_power_on(struct phy *phy)
{
	struct ext_mipi_phy *ext_phy = phy_get_drvdata(phy);
	int ret;

	DRM_DEBUG_DRIVER("%d\n", __LINE__);

	/* Power up core and enable PLL */
	ret = clk_prepare_enable(ext_phy->pll);

	if (ret < 0) {
		dev_err(ext_phy->dev, "clk prepare fail %d\n", ret);
		return ret;
	}

	DRM_DEBUG_DRIVER("%d\n", __LINE__);

	/* Enable DSI Lane LDO outputs, disable pad tie low */
	ext_mipi_phy_power_on_signal(phy);

	return 0;
}

static void ext_mipi_phy_power_off_signal(struct phy *phy)
{
}

static int ext_mipi_phy_power_off(struct phy *phy)
{
	struct ext_mipi_phy *ext_phy = phy_get_drvdata(phy);

	DRM_DEBUG_DRIVER("\n");

	/* Enable pad tie low, disable DSI Lane LDO outputs */
	ext_mipi_phy_power_off_signal(phy);

	/* Disable PLL and power down core */
	clk_disable_unprepare(ext_phy->pll);

	return 0;
}

static const struct phy_ops ext_mipi_phy_ops = {
	.power_on = ext_mipi_phy_power_on,
	.power_off = ext_mipi_phy_power_off,
	.owner = THIS_MODULE,
};

static int ext_mipi_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ext_mipi_phy *ext_phy;
	struct resource *mem;
	struct clk *ref_clk;
	const char *ref_clk_name;
	struct clk_init_data clk_init = {
		.ops = &ext_mipi_phy_pll_ops,
		.num_parents = 1,
		.parent_names = (const char * const *)&ref_clk_name,
		.flags = CLK_SET_RATE_GATE,
	};
	struct phy *phy;
	struct phy_provider *phy_provider;
	int ret;

	DRM_DEBUG_DRIVER("start\n");

	ext_phy = devm_kzalloc(dev, sizeof(*ext_phy), GFP_KERNEL);
	if (!ext_phy)
		return -ENOMEM;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ext_phy->regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(ext_phy->regs)) {
		ret = PTR_ERR(ext_phy->regs);
		dev_err(dev, "Failed to get memory resource: %d\n", ret);
		return ret;
	}

	ref_clk = devm_clk_get(dev, NULL);
	if (IS_ERR(ref_clk)) {
		ret = PTR_ERR(ref_clk);
		dev_err(dev, "Failed to get reference clock: %d\n", ret);
		return ret;
	}
	ref_clk_name = __clk_get_name(ref_clk);

	ret = of_property_read_string(dev->of_node, "clock-output-names",
				      &clk_init.name);
	if (ret < 0) {
		dev_err(dev, "Failed to read clock-output-names: %d\n", ret);
		return ret;
	}

	ext_phy->pll_hw.init = &clk_init;
	ext_phy->pll = devm_clk_register(dev, &ext_phy->pll_hw);
	if (IS_ERR(ext_phy->pll)) {
		ret = PTR_ERR(ext_phy->pll);
		dev_err(dev, "Failed to register PLL: %d\n", ret);
		return ret;
	}

	phy = devm_phy_create(dev, NULL, &ext_mipi_phy_ops);
	if (IS_ERR(phy)) {
		ret = PTR_ERR(phy);
		dev_err(dev, "Failed to create MIPI D-PHY: %d\n", ret);
		return ret;
	}
	phy_set_drvdata(phy, ext_phy);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider)) {
		ret = PTR_ERR(phy_provider);
		dev_err(dev, "Failed to phy provider register: %d\n", ret);
		return ret;
	}

	ext_phy->dev = dev;

	ret = of_clk_add_provider(dev->of_node, of_clk_src_simple_get,
					   ext_phy->pll);

	DRM_DEBUG_DRIVER("finish ret %d\n", ret);

	return ret;
}

static int ext_mipi_phy_remove(struct platform_device *pdev)
{
	of_clk_del_provider(pdev->dev.of_node);
	return 0;
}

struct platform_driver ext_mipi_phy_driver = {
	.probe = ext_mipi_phy_probe,
	.remove = ext_mipi_phy_remove,
	.driver = {
		.name = "mediatek-ext-mipi-tx",
		.of_match_table = ext_mipi_phy_match,
	},
};

