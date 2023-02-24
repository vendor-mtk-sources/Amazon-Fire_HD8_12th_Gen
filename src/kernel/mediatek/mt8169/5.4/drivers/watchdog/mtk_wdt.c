// SPDX-License-Identifier: GPL-2.0+
/*
 * Mediatek Watchdog Driver
 *
 * Copyright (C) 2014 Matthias Brugger
 *
 * Matthias Brugger <matthias.bgg@gmail.com>
 *
 * Based on sunxi_wdt.c
 */

#include <dt-bindings/reset-controller/mt8169-resets.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <asm/system_misc.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/types.h>
#include <linux/watchdog.h>
#if defined(CONFIG_MAGIC_SYSRQ_WD_TEST)
#include <linux/mtk_wdt_func.h>
#endif
#define WDT_MAX_TIMEOUT		31
#define WDT_MIN_TIMEOUT		1
#define WDT_LENGTH_TIMEOUT(n)	((n) << 5)

#define WDT_LENGTH		0x04
#define WDT_LENGTH_KEY		0x8

#define WDT_RST			0x08
#define WDT_RST_RELOAD		0x1971

#define WDT_MODE		0x00
#define WDT_MODE_EN		(1 << 0)
#define WDT_MODE_EXT_POL_LOW	(0 << 1)
#define WDT_MODE_EXT_POL_HIGH	(1 << 1)
#define WDT_MODE_EXRST_EN	(1 << 2)
#define WDT_MODE_IRQ_EN		(1 << 3)
#define WDT_MODE_AUTO_START	(1 << 4)
#define WDT_MODE_IRQ_LVL	(1 << 5)
#define WDT_MODE_DUAL_EN	(1 << 6)
#define WDT_MODE_KEY		0x22000000

#define WDT_SWRST		0x14
#define WDT_SWRST_KEY		0x1209

#define WDT_SWSYSRST		0x18U
#define WDT_SWSYS_RST_KEY	0x88000000

#define WDT_NONRST_REG2       0x24

#define WDT_NONRST2_STAGE_OFS 29 /* 31:29: 3-bits for current stage */
#define RGU_STAGE_MASK        0x7
#define RGU_STAGE_KERNEL      0x3

#define DRV_NAME		"mtk-wdt"
#define DRV_VERSION		"1.0"

static void __iomem *toprgu_base;
static struct watchdog_device *mtk_wdt_dev;
static bool nowayout = WATCHDOG_NOWAYOUT;
static unsigned int timeout;

static int mtk_wdt_start(struct watchdog_device *wdt_dev);
static int mtk_wdt_stop(struct watchdog_device *wdt_dev);

struct mtk_wdt_dev {
	struct watchdog_device wdt_dev;
	void __iomem *wdt_base;
	spinlock_t lock; /* protects WDT_SWSYSRST reg */
	struct reset_controller_dev rcdev;
};

struct mtk_wdt_data {
	int rst_num;
};

static const struct mtk_wdt_data mt8169_data = {
	.rst_num = MT8169_TOPRGU_SW_RST_NUM,
};

static void mtk_wdt_init(struct device_node *np,
			struct watchdog_device *wdt_dev)
{
	struct mtk_wdt_dev *mtk_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base;

	wdt_base = mtk_wdt->wdt_base;

	if (readl(wdt_base + WDT_MODE) & WDT_MODE_EN)
		mtk_wdt_start(wdt_dev);
	else
		mtk_wdt_stop(wdt_dev);
}

static int toprgu_reset_update(struct reset_controller_dev *rcdev,
			       unsigned long id, bool assert)
{
	unsigned int tmp;
	unsigned long flags;
	struct mtk_wdt_dev *data =
		 container_of(rcdev, struct mtk_wdt_dev, rcdev);

	spin_lock_irqsave(&data->lock, flags);

	tmp = readl(data->wdt_base + WDT_SWSYSRST);
	if (assert)
		tmp |= BIT(id);
	else
		tmp &= ~BIT(id);
	tmp |= WDT_SWSYS_RST_KEY;
	writel(tmp, data->wdt_base + WDT_SWSYSRST);

	spin_unlock_irqrestore(&data->lock, flags);

	return 0;
}

static int toprgu_reset_assert(struct reset_controller_dev *rcdev,
			       unsigned long id)
{
	return toprgu_reset_update(rcdev, id, true);
}

static int toprgu_reset_deassert(struct reset_controller_dev *rcdev,
				 unsigned long id)
{
	return toprgu_reset_update(rcdev, id, false);
}

static int toprgu_reset(struct reset_controller_dev *rcdev,
			unsigned long id)
{
	int ret;

	ret = toprgu_reset_assert(rcdev, id);
	if (ret)
		return ret;

	return toprgu_reset_deassert(rcdev, id);
}

static const struct reset_control_ops toprgu_reset_ops = {
	.assert = toprgu_reset_assert,
	.deassert = toprgu_reset_deassert,
	.reset = toprgu_reset,
};

static int toprgu_register_reset_controller(struct platform_device *pdev,
					    int rst_num)
{
	int ret;
	struct mtk_wdt_dev *mtk_wdt = platform_get_drvdata(pdev);

	spin_lock_init(&mtk_wdt->lock);

	mtk_wdt->rcdev.owner = THIS_MODULE;
	mtk_wdt->rcdev.nr_resets = rst_num;
	mtk_wdt->rcdev.ops = &toprgu_reset_ops;
	mtk_wdt->rcdev.of_node = pdev->dev.of_node;
	ret = devm_reset_controller_register(&pdev->dev, &mtk_wdt->rcdev);
	if (ret != 0)
		dev_err(&pdev->dev,
			"couldn't register wdt reset controller: %d\n", ret);
	return ret;
}

static int mtk_wdt_restart(struct watchdog_device *wdt_dev,
			   unsigned long action, void *data)
{
	struct mtk_wdt_dev *mtk_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base;

	wdt_base = mtk_wdt->wdt_base;

	while (1) {
		writel(WDT_SWRST_KEY, wdt_base + WDT_SWRST);
		mdelay(5);
	}

	return 0;
}

static int mtk_wdt_ping(struct watchdog_device *wdt_dev)
{
	struct mtk_wdt_dev *mtk_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base = mtk_wdt->wdt_base;

	iowrite32(WDT_RST_RELOAD, wdt_base + WDT_RST);
	pr_info("[wdtk] kick watchdog\n");

	return 0;
}

static int mtk_wdt_set_timeout(struct watchdog_device *wdt_dev,
				unsigned int timeout)
{
	struct mtk_wdt_dev *mtk_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base = mtk_wdt->wdt_base;
	u32 reg;

	wdt_dev->timeout = timeout;

	/*
	 * One bit is the value of 512 ticks
	 * The clock has 32 KHz
	 */
	reg = WDT_LENGTH_TIMEOUT(timeout << 6) | WDT_LENGTH_KEY;
	iowrite32(reg, wdt_base + WDT_LENGTH);

	mtk_wdt_ping(wdt_dev);

	return 0;
}

static int mtk_wdt_stop(struct watchdog_device *wdt_dev)
{
	struct mtk_wdt_dev *mtk_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base = mtk_wdt->wdt_base;
	u32 reg;

	reg = readl(wdt_base + WDT_MODE);
	reg &= ~WDT_MODE_EN;
	reg |= WDT_MODE_KEY;
	iowrite32(reg, wdt_base + WDT_MODE);

	clear_bit(WDOG_HW_RUNNING, &wdt_dev->status);
	pr_info("watchdog%d, mtk wdt stop, wdd->status=%ld\n",
		 wdt_dev->id, wdt_dev->status);
	return 0;
}

static int mtk_wdt_start(struct watchdog_device *wdt_dev)
{
	u32 reg;
	struct mtk_wdt_dev *mtk_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base = mtk_wdt->wdt_base;
	int ret;

	ret = mtk_wdt_set_timeout(wdt_dev, wdt_dev->timeout);
	if (ret < 0)
		return ret;

	reg = ioread32(wdt_base + WDT_MODE);
	reg |= (WDT_MODE_DUAL_EN | WDT_MODE_IRQ_EN | WDT_MODE_EXRST_EN);
	reg &= ~(WDT_MODE_IRQ_LVL | WDT_MODE_EXT_POL_HIGH);
	reg |= (WDT_MODE_EN | WDT_MODE_KEY);
	iowrite32(reg, wdt_base + WDT_MODE);

	set_bit(WDOG_HW_RUNNING, &wdt_dev->status);
	pr_info("watchdog%d, mtk wdt start, wdd->status=%ld\n",
		 wdt_dev->id, wdt_dev->status);
	return 0;
}

static void mtk_wdt_mark_stage(struct mtk_wdt_dev *mtk_wdt)
{
	void __iomem *wdt_base = NULL;
	u32 reg = 0;

	if (!mtk_wdt)
		return;

	wdt_base = mtk_wdt->wdt_base;
	if (!wdt_base)
		return;

	reg = readl(wdt_base + WDT_NONRST_REG2);
	reg = (reg & ~(RGU_STAGE_MASK << WDT_NONRST2_STAGE_OFS)) |
		(RGU_STAGE_KERNEL << WDT_NONRST2_STAGE_OFS);

	writel(reg, wdt_base + WDT_NONRST_REG2);
}

static int wk_proc_cmd_read(struct seq_file *s, void *v)
{
	unsigned int enabled = 1;

	if (0 == (ioread32(toprgu_base + WDT_MODE) & WDT_MODE_EN))
		enabled = 0;

	seq_printf(s, "enabled timeout\n%-4d %-8d\n",
		enabled, mtk_wdt_dev->timeout);

	return 0;
}

static int wk_proc_cmd_open(struct inode *inode, struct file *file)
{
	return single_open(file, wk_proc_cmd_read, NULL);
}

static ssize_t wk_proc_cmd_write
	(struct file *file, const char *buf, size_t count, loff_t *data)
{
	int ret;
	int enable;
	unsigned int timeout;
	char wk_cmd_buf[256];

	if (count == 0)
		return -1;

	if (count > 255)
		count = 255;

	ret = (int)copy_from_user(wk_cmd_buf, buf, count);
	if (ret < 0)
		return -1;

	wk_cmd_buf[count] = '\0';

	pr_debug("Write %s\n", wk_cmd_buf);

	ret = sscanf(wk_cmd_buf, "%d %d", &enable, &timeout);
	if (ret != 2)
		pr_debug("%s: expect 2 numbers\n", __func__);

	pr_debug("[WDK] enable=%d  timeout=%d\n", enable, timeout);

	if (timeout > 20 && timeout <= WDT_MAX_TIMEOUT) {
		mtk_wdt_dev->timeout = timeout;
		ret = mtk_wdt_set_timeout(mtk_wdt_dev, mtk_wdt_dev->timeout);
		if (ret < 0)
			return ret;
	} else {
		pr_info("[WDK] The timeout(%d) should bigger than 20 and not bigger than %d\n",
				timeout, WDT_MAX_TIMEOUT);

	}

	if (enable == 1) {
		ret = mtk_wdt_start(mtk_wdt_dev);
		if (ret < 0)
			return ret;
		set_bit(WDOG_ACTIVE, &mtk_wdt_dev->status);
		pr_info("[WDK] enable wdt\n");
	} else if (enable == 0) {
		mtk_wdt_stop(mtk_wdt_dev);
		clear_bit(WDOG_ACTIVE, &mtk_wdt_dev->status);
		pr_info("[WDK] disable wdt\n");
	}

	return count;
}

static const struct file_operations wk_proc_cmd_fops = {
	.owner = THIS_MODULE,
	.open = wk_proc_cmd_open,
	.read = seq_read,
	.write = wk_proc_cmd_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static void wk_proc_init(void)
{
	struct proc_dir_entry *de = proc_create
				("wdk", 0660, NULL, &wk_proc_cmd_fops);

	if (de == 0)
		pr_info("[%s]: create /proc/wdk failed\n", __func__);

	pr_debug("[WDK] Initialize proc\n");
}

#if defined(CONFIG_MAGIC_SYSRQ_WD_TEST)
/*
 *   watchdog mode:
 *   debug_en:   debug module reset enable.
 *   irq:        generate interrupt instead of reset
 *   ext_en:     output reset signal to outside
 *   ext_pol:    polarity of external reset signal
 *   wdt_en:     enable watch dog timer
 */
int mtk_wdt_mode_config_for_sysrq(void)
{
        u32 reg;
        struct mtk_wdt_dev *mtk_wdt;
        void __iomem *wdt_base;

        if (unlikely(mtk_wdt_dev == NULL))
                return -1;

        /* get regs base */
        mtk_wdt = watchdog_get_drvdata(mtk_wdt_dev);
        wdt_base = mtk_wdt->wdt_base;

        reg = ioread32(wdt_base + WDT_MODE);

        /* Bit 0 : Whether enable watchdog or not */
        reg |= WDT_MODE_EN;

        /* Bit 1 : Configure extern reset signal polarity. */
        reg &= ~WDT_MODE_EXT_POL_HIGH;

        /* Bit 2 : Whether enable external reset signal */
        reg |= WDT_MODE_EXRST_EN;

        /* Bit 3 : Whether generating interrupt instead of reset signal */
        reg &= ~WDT_MODE_IRQ_EN;

        /* Bit 6 : Whether enable debug module reset */
        reg &= ~WDT_MODE_DUAL_EN;

        reg |= WDT_MODE_KEY | WDT_MODE_AUTO_START;

        iowrite32(reg, wdt_base + WDT_MODE);

        return 0;

}

void deactive_mtk_wdd(void)
{
        if (unlikely(mtk_wdt_dev == NULL))
                return;

        clear_bit(WDOG_ACTIVE, &mtk_wdt_dev->status);
}
#endif

static const struct watchdog_info mtk_wdt_info = {
	.identity	= DRV_NAME,
	.options	= WDIOF_SETTIMEOUT |
			  WDIOF_KEEPALIVEPING |
			  WDIOF_MAGICCLOSE,
};

static const struct watchdog_ops mtk_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= mtk_wdt_start,
	.stop		= mtk_wdt_stop,
	.ping		= mtk_wdt_ping,
	.set_timeout	= mtk_wdt_set_timeout,
	.restart	= mtk_wdt_restart,
};

#if defined(CONFIG_MAGIC_SYSRQ_WD_TEST)
const struct wdt_mode_ops wdt_ops = {
	.mtk_wdt_mode_config_for_sysrq = mtk_wdt_mode_config_for_sysrq,
	.deactive_mtk_wdd = deactive_mtk_wdd,
};
#endif

static int mtk_wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_wdt_dev *mtk_wdt;
	const struct mtk_wdt_data *wdt_data;
	int err;

	mtk_wdt = devm_kzalloc(dev, sizeof(*mtk_wdt), GFP_KERNEL);
	if (!mtk_wdt)
		return -ENOMEM;

	platform_set_drvdata(pdev, mtk_wdt);

	mtk_wdt->wdt_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(mtk_wdt->wdt_base))
		return PTR_ERR(mtk_wdt->wdt_base);

	toprgu_base = mtk_wdt->wdt_base;
	mtk_wdt_dev = &mtk_wdt->wdt_dev;

	mtk_wdt->wdt_dev.info = &mtk_wdt_info;
	mtk_wdt->wdt_dev.ops = &mtk_wdt_ops;
	mtk_wdt->wdt_dev.timeout = WDT_MAX_TIMEOUT;
	mtk_wdt->wdt_dev.max_timeout = WDT_MAX_TIMEOUT;
	mtk_wdt->wdt_dev.min_timeout = WDT_MIN_TIMEOUT;
	mtk_wdt->wdt_dev.parent = dev;

	watchdog_init_timeout(&mtk_wdt->wdt_dev, timeout, dev);
	watchdog_set_nowayout(&mtk_wdt->wdt_dev, nowayout);
	watchdog_set_restart_priority(&mtk_wdt->wdt_dev, 128);

	watchdog_set_drvdata(&mtk_wdt->wdt_dev, mtk_wdt);

	mtk_wdt_init(pdev->dev.of_node, &mtk_wdt->wdt_dev);

	watchdog_stop_on_reboot(&mtk_wdt->wdt_dev);
	mtk_wdt_mark_stage(mtk_wdt);

	err = devm_watchdog_register_device(dev, &mtk_wdt->wdt_dev);
	if (unlikely(err))
		return err;

	dev_info(dev, "Watchdog enabled (timeout=%d sec, nowayout=%d)\n",
		 mtk_wdt->wdt_dev.timeout, nowayout);

	wdt_data = of_device_get_match_data(dev);
	if (wdt_data) {
		err = toprgu_register_reset_controller(pdev,
						       wdt_data->rst_num);
		if (err)
			return err;
	}

	wk_proc_init();
#if defined(CONFIG_MAGIC_SYSRQ_WD_TEST)
	wdt_mode_ops_set(&wdt_ops);
#endif

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mtk_wdt_suspend(struct device *dev)
{
	struct mtk_wdt_dev *mtk_wdt = dev_get_drvdata(dev);

	if (watchdog_active(&mtk_wdt->wdt_dev))
		mtk_wdt_stop(&mtk_wdt->wdt_dev);

	return 0;
}

static int mtk_wdt_resume(struct device *dev)
{
	struct mtk_wdt_dev *mtk_wdt = dev_get_drvdata(dev);

	if (watchdog_active(&mtk_wdt->wdt_dev)) {
		mtk_wdt_start(&mtk_wdt->wdt_dev);
		mtk_wdt_ping(&mtk_wdt->wdt_dev);
	}

	return 0;
}
#endif

static const struct of_device_id mtk_wdt_dt_ids[] = {
	{ .compatible = "mediatek,mt6589-wdt" },
	{ .compatible = "mediatek,mt8169-wdt", .data = &mt8169_data },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mtk_wdt_dt_ids);

static const struct dev_pm_ops mtk_wdt_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_wdt_suspend,
				mtk_wdt_resume)
};

static struct platform_driver mtk_wdt_driver = {
	.probe		= mtk_wdt_probe,
	.driver		= {
		.name		= DRV_NAME,
		.pm		= &mtk_wdt_pm_ops,
		.of_match_table	= mtk_wdt_dt_ids,
	},
};

module_platform_driver(mtk_wdt_driver);
module_param(timeout, uint, 0);
MODULE_PARM_DESC(timeout, "Watchdog heartbeat in seconds");

module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
			__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Matthias Brugger <matthias.bgg@gmail.com>");
MODULE_DESCRIPTION("Mediatek WatchDog Timer Driver");
MODULE_VERSION(DRV_VERSION);
