// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/backlight.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#if IS_ENABLED(CONFIG_AMAZON_METRICS_LOG)
#include <linux/metricslog.h>
#endif

#define LCM_ST7705_KD_HSD    10
#define LCM_ST7705_TG_BOE    6
#define REGFLAG_DELAY        0xFC

static unsigned char vendor_id = 0xFF;
static void get_lcm_id(void);
#if IS_ENABLED(CONFIG_IDME)
extern unsigned int idme_get_lcmid_info(void);
#endif

#define FRAME_WIDTH           (800)
#define FRAME_HEIGHT          (1280)
/* KD HSD panel porch */
#define KD_HSD_HFP (80)    // params->dsi.horizontal_frontporch
#define KD_HSD_HSA (16)    // params->dsi.horizontal_sync_active
#define KD_HSD_HBP (80)    // params->dsi.horizontal_backporch
#define KD_HSD_VFP (40)    // params->dsi.vertical_frontporch
#define KD_HSD_VSA (4)     // params->dsi.vertical_sync_active
#define KD_HSD_VBP (40)    // params->dsi.vertical_backporch

/* KD HSD panel porch */
#define TG_BOE_HFP (80)    // params->dsi.horizontal_frontporch
#define TG_BOE_HSA (51)    // params->dsi.horizontal_sync_active
#define TG_BOE_HBP (80)    // params->dsi.horizontal_backporch
#define TG_BOE_VFP (16)    // params->dsi.vertical_frontporch
#define TG_BOE_VSA (6)     // params->dsi.vertical_sync_active
#define TG_BOE_VBP (16)    // params->dsi.vertical_backporch

static const struct drm_display_mode kd_hsd_mode = {
	.clock = (FRAME_WIDTH + KD_HSD_HFP + KD_HSD_HSA + KD_HSD_HBP) *
		(FRAME_HEIGHT + KD_HSD_VFP + KD_HSD_VSA + KD_HSD_VBP) *
		60 / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + KD_HSD_HFP,
	.hsync_end = FRAME_WIDTH + KD_HSD_HFP + KD_HSD_HSA,
	.htotal = FRAME_WIDTH + KD_HSD_HFP + KD_HSD_HSA + KD_HSD_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + KD_HSD_VFP,
	.vsync_end = FRAME_HEIGHT + KD_HSD_VFP + KD_HSD_VSA,
	.vtotal = FRAME_HEIGHT + KD_HSD_VFP + KD_HSD_VSA + KD_HSD_VBP,
	.vrefresh = 60,
};

static const struct drm_display_mode tg_boe_mode = {
	.clock = (FRAME_WIDTH + TG_BOE_HFP + TG_BOE_HSA + TG_BOE_HBP) *
		(FRAME_HEIGHT + TG_BOE_VFP + TG_BOE_VSA + TG_BOE_VBP) *
		60 / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + TG_BOE_HFP,
	.hsync_end = FRAME_WIDTH + TG_BOE_HFP + TG_BOE_HSA,
	.htotal = FRAME_WIDTH + TG_BOE_HFP + TG_BOE_HSA + TG_BOE_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + TG_BOE_VFP,
	.vsync_end = FRAME_HEIGHT + TG_BOE_VFP + TG_BOE_VSA,
	.vtotal = FRAME_HEIGHT + TG_BOE_VFP + TG_BOE_VSA + TG_BOE_VBP,
	.vrefresh = 60,
};

struct p2in1 {
	struct device *dev;
	struct drm_panel panel;
	struct gpio_desc *reset_gpio;
	struct regulator *vsim1;
	struct regulator *biasp;
	struct regulator *biasn;
	bool lk_fastlogo;

	bool prepared_power;
	bool prepared;
	bool enabled;
	int error;
};

struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    u8 para_list[64];
};

#define p2in1_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	p2in1_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define p2in1_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	p2in1_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct p2in1 *panel_to_p2in1(struct drm_panel *panel)
{
	return container_of(panel, struct p2in1, panel);
}

static void p2in1_dcs_write(struct p2in1 *ctx,
	const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);

	if (ret < 0) {
		dev_err(ctx->dev,
			"error 0x%x writing dcs seq: 0x%x data(0x%p)\n",
			(unsigned int)ret, (unsigned int)len, data);
		ctx->error = ret;
	}
}

static void get_lcm_id(void)
{
	unsigned int lcm_id = 0xff;

#if IS_ENABLED(CONFIG_IDME)
	lcm_id = idme_get_lcmid_info();
	if (lcm_id == 0) {
		pr_err("idme_get_lcm_id failed.\n");
	}
#else
	pr_notice("%s, idme is not ready.\n", __func__);
#endif

	vendor_id = (unsigned char)lcm_id;
	pr_notice("[st7705] %s, vendor id = 0x%x\n", __func__, vendor_id);
}

static struct LCM_setting_table lcm_init_st7705_kd_hsd[] = {
	{0xB9, 3, {0xF1, 0x12, 0x84}},
	{0xB1, 10, {
		0x21, 0x53, 0x23, 0x24, 0x24, 0x44,
		0x77, 0x01, 0xDB, 0x0C}},
	{0xB2, 2, {0x40, 0x08}},
	{0xB3, 8, {
		0x00, 0x00, 0x00, 0x00, 0x28, 0x28,
		0x28, 0x28}},
	{0xB4, 1, {0x80}},
	{0xB5, 2, {0x16, 0x16}},
	{0xB6, 2, {0x3F, 0x3F}},
	{0xB8, 2, {0x77, 0x03}},
	{0xBA, 27, {
		0x33, 0x81, 0x05, 0xF9, 0x0E, 0x0E,
		0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x44, 0x25, 0x00, 0x90,
		0x0A, 0x00, 0x00, 0x02, 0x4F, 0x01,
		0x00, 0x00, 0x37}},
	{0xBC, 1, {0x46}},
	{0xBF, 3, {0x00, 0x11, 0x82}},
	{0xC0, 9, {
		0x73, 0x73, 0x50, 0x50, 0x80, 0x00,
		0x38, 0x70, 0x00}},
	{0xCC, 1, {0x0B}},
	{0xE0, 34, {
		0x00, 0x0A, 0x0C, 0x29, 0x31, 0x3F,
		0x3B, 0x37, 0x05, 0x0A, 0x0C, 0x10,
		0x12, 0x10, 0x12, 0x12, 0x17, 0x04,
		0x0A, 0x0C, 0x29, 0x31, 0x3B, 0x3B,
		0x37, 0x05, 0x0A, 0x0C, 0x10, 0x12,
		0x10, 0x12, 0x12, 0x17}},
	{0xE3, 11, {
		0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
		0x00, 0x00, 0x00, 0xC0, 0x20}},
	{0xE9, 63, {
		0x02, 0x00, 0x08, 0x05, 0x0C, 0x80,
		0x36, 0x12, 0x31, 0x23, 0x47, 0x0A,
		0x7E, 0x36, 0x47, 0x00, 0x00, 0x81,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x81,
		0x00, 0x00, 0x00, 0x00, 0xF8, 0xAB,
		0x02, 0x46, 0x08, 0x88, 0x88, 0x84,
		0x88, 0x88, 0x88, 0xF8, 0xAB, 0x13,
		0x57, 0x18, 0x88, 0x88, 0x85, 0x88,
		0x88, 0x88, 0x00, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00}},
	{0xEA, 63, {
		0x96, 0x12, 0x01, 0x01, 0x02, 0x3C,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x8F, 0xAB, 0x75, 0x31, 0x58, 0x88,
		0x88, 0x81, 0x88, 0x88, 0x88, 0x8F,
		0xAB, 0x64, 0x20, 0x48, 0x88, 0x88,
		0x80, 0x88, 0x88, 0x88, 0x23, 0x00,
		0x00, 0x00, 0x01, 0x48, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x40, 0x80, 0x36, 0x00, 0x00, 0x00,
		0x00, 0x01, 0x0E}},
	{0x53, 1, {0x2C}},
	{0xC7, 4, {0xA0, 0x00, 0x00, 0x04}},
	{0xC8, 4, {0x10, 0x06, 0x06, 0x08}},
	{0x51, 1, {0xFF}},
	{0x11, 0, {}},
	{REGFLAG_DELAY, 120, {}},
	{0x29, 0, {}},
	{REGFLAG_DELAY, 20, {}},
};

static struct LCM_setting_table lcm_init_st7705_tg_boe[] = {
	{0xB9, 3, {0xF1, 0x12, 0x84}},
	{0xB1, 10, {
		0x43, 0x74, 0x23, 0x2E, 0x2E, 0x33,
		0x77, 0x01, 0xDB, 0x0C}},
	{0xB2, 2, {0x40, 0x08}},
	{0xB3, 8, {
		0x00, 0x00, 0x00, 0x00, 0x28, 0x28,
		0x28, 0x28}},
	{0xB4, 1, {0x20}},
	{0xB5, 2, {0x09, 0x09}},
	{0xB6, 2, {0x86, 0x86}},
	{0xB8, 2, {0x77, 0x03}},
	{0xBA, 27, {
		0x33, 0x81, 0x05, 0xF9, 0x0E, 0x0E,
		0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x44, 0x25, 0x00, 0x90,
		0x0A, 0x00, 0x00, 0x02, 0x4F, 0x01,
		0x00, 0x00, 0x37}},
	{0xBC, 1, {0x46}},
	{0xBF, 3, {0x00, 0x11, 0x82}},
	{0xC0, 9, {
		0x73, 0x73, 0x50, 0x50, 0x80, 0x00,
		0x08, 0x70, 0x00}},
	{0xCC, 1, {0x0B}},
	{0xE3, 10, {
		0x03, 0x03, 0x00, 0x00, 0x03, 0x03,
		0x00, 0x00, 0x00, 0xC0}},
	{0xE9, 63, {
		0x02, 0x00, 0x0B, 0x05, 0x1A, 0x80,
		0x81, 0x12, 0x31, 0x23, 0x5F, 0x0E,
		0x80, 0x28, 0x47, 0x08, 0x08, 0x01,
		0x00, 0x00, 0x00, 0x00, 0x08, 0x01,
		0x00, 0x00, 0x00, 0x00, 0x33, 0x11,
		0x77, 0x55, 0x18, 0x88, 0x88, 0x88,
		0x8A, 0xB4, 0x88, 0x22, 0x00, 0x66,
		0x44, 0x08, 0x88, 0x88, 0x88, 0x8A,
		0xB4, 0x88, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00}},
	{0xEA, 63, {
		0x96, 0x0C, 0x02, 0x02, 0x02, 0xB4,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x44, 0x66, 0x00, 0x22, 0x08, 0x88,
		0x88, 0x88, 0x8A, 0xB4, 0x88, 0x55,
		0x77, 0x11, 0x33, 0x18, 0x88, 0x88,
		0x88, 0x8A, 0xB4, 0x88, 0x23, 0x10,
		0x00, 0x00, 0x01, 0x45, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x50, 0x80, 0x81, 0x00, 0x00, 0x00,
		0x00, 0x01, 0x16}},
	{0xE0, 34, {
		0x01, 0x12, 0x16, 0x2E, 0x38, 0x3F,
		0x44, 0x38, 0x06, 0x0A, 0x0C, 0x11,
		0x13, 0x11, 0x12, 0x0F, 0x14, 0x01,
		0x12, 0x16, 0x2E, 0x38, 0x3F, 0x44,
		0x38, 0x06, 0x0A, 0x0C, 0x11, 0x13,
		0x11, 0x12, 0x0F, 0x14}},
	{0x11, 0, {}},
	{REGFLAG_DELAY, 120, {}},
	{0x29, 0, {}},
	{REGFLAG_DELAY, 20, {}},
};

static void lcm_init_push_table(struct p2in1 *ctx, struct LCM_setting_table *table,
			unsigned int count)
{
	unsigned int i;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	for (i = 0; i < count; i++) {
		unsigned int cmd;
		unsigned int cmd_length;

		cmd = table[i].cmd;
		cmd_length = table[i].count;

		if (unlikely(cmd == REGFLAG_DELAY)) {
			msleep(cmd_length);
		} else {
			ret = mipi_dsi_dcs_write(dsi, cmd, table[i].para_list, cmd_length);
			if (ret < 0)
				pr_err("mipi_dsi_dcs_write failed!"
					"ret = %d, i = %d, cmd = 0x%x.\n", ret, i, cmd);
		}
	}
}

static void raspite_st7705_kd_hsd_panel_init(struct p2in1 *ctx)
{
	/* GIP_1 */
	pr_notice("[lcm][st7705]%s enter.\n", __func__);

	lcm_init_push_table(ctx, lcm_init_st7705_kd_hsd,
		sizeof(lcm_init_st7705_kd_hsd) / sizeof(struct LCM_setting_table));
}

static void raspite_st7705_tg_boe_panel_init(struct p2in1 *ctx)
{
	pr_notice("[lcm][st7705]%s enter.\n", __func__);

	lcm_init_push_table(ctx, lcm_init_st7705_tg_boe,
		sizeof(lcm_init_st7705_tg_boe) / sizeof(struct LCM_setting_table));
}

static int p2in1_disable(struct drm_panel *panel)
{
	struct p2in1 *ctx = panel_to_p2in1(panel);

	pr_notice("[lcm][st7705]%s enter\n", __func__);

	if (!ctx->enabled)
		return 0;

	ctx->enabled = false;

	return 0;
}

static int p2in1_unprepare(struct drm_panel *panel)
{
	struct p2in1 *ctx = panel_to_p2in1(panel);
#if IS_ENABLED(CONFIG_AMAZON_METRICS_LOG)
	char buf[128];
	snprintf(buf, sizeof(buf), "%s:lcd:suspend=1;CT;1:NR", __func__);
	log_to_metrics(ANDROID_LOG_INFO, "LCDEvent", buf);
#endif
	pr_notice("[lcm][st7705]%s enter\n", __func__);

	if (!ctx->prepared)
		return 0;

#ifdef CONFIG_LK_FASTLOGO
	if (ctx->lk_fastlogo)
		ctx->lk_fastlogo = false;
#endif

	p2in1_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(20);
	p2in1_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(120);

	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

static int p2in1_unprepare_power(struct drm_panel *panel)
{
	struct p2in1 *ctx = panel_to_p2in1(panel);

	pr_notice("[lcm][st7705]%s enter\n", __func__);

	if (!ctx->prepared_power)
		return 0;

#ifdef CONFIG_LK_FASTLOGO
	if (ctx->lk_fastlogo)
		pr_notice("[lcm][st7705]%s +++\n", __func__);
#endif

	usleep_range(15000, 15100);
	gpiod_set_value(ctx->reset_gpio, 0);

	usleep_range(2000, 2100);
	if (ctx->biasn)
		regulator_disable(ctx->biasn);
	usleep_range(5000, 5100);

	if (ctx->biasp)
		regulator_disable(ctx->biasp);

	usleep_range(10000, 11000);
	if (ctx->vsim1)
		regulator_disable(ctx->vsim1);

	ctx->prepared_power = false;

	return 0;
}

static int p2in1_prepare_power(struct drm_panel *panel)
{
	struct p2in1 *ctx = panel_to_p2in1(panel);
	int ret;

	pr_notice("[lcm][st7705]%s enter.\n", __func__);

	if (ctx->prepared_power)
		return 0;

#ifdef CONFIG_LK_FASTLOGO
	if (ctx->lk_fastlogo) {
		ctx->prepared_power = true;
		return 0;
	}
#endif

	if (ctx->vsim1) {
		ret = regulator_enable(ctx->vsim1);
		if (ret < 0) {
			DRM_ERROR("[lcm]fails to enable ctx->vsim1");
			return ret;
		}
	}
	usleep_range(2000, 2100);

	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(2000, 2100);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(2000, 2100);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(12000, 12100);

	if (ctx->biasp) {
		ret = regulator_enable(ctx->biasp);
		if (ret < 0) {
			DRM_ERROR("[lcm]fails to enable ctx->biasp");
			return ret;
		}
	}

	usleep_range(5000, 5100);
	if (ctx->biasn) {
		ret = regulator_enable(ctx->biasn);
		if (ret < 0) {
			DRM_ERROR("[lcm]fails to enable ctx->biasn");
			return ret;
		}
	}

	ret = ctx->error;
	if (ret < 0)
		p2in1_unprepare_power(panel);

	ctx->prepared_power = true;

	pr_notice("[lcm][st7705]%s exit.\n", __func__);
	return ret;
}

static int p2in1_prepare(struct drm_panel *panel)
{
	struct p2in1 *ctx = panel_to_p2in1(panel);
	int ret;
#if IS_ENABLED(CONFIG_AMAZON_METRICS_LOG)
	char buf[128];
	snprintf(buf, sizeof(buf), "%s:lcd:resume=1;CT;1:NR", __func__);
	log_to_metrics(ANDROID_LOG_INFO, "LCDEvent", buf);
#endif
	pr_notice("[lcm][st7705]%s enter.\n", __func__);

	if (ctx->prepared)
		return 0;

#ifdef CONFIG_LK_FASTLOGO
	if (ctx->lk_fastlogo) {
		ctx->prepared = true;
		return 0;
	}
#endif

	if (vendor_id == 0xFF)
		get_lcm_id();

	usleep_range(5000, 5100);
	switch (vendor_id) {
	case LCM_ST7705_KD_HSD:
		raspite_st7705_kd_hsd_panel_init(ctx);
		break;
	case LCM_ST7705_TG_BOE:
		raspite_st7705_tg_boe_panel_init(ctx);
		break;
	default:
		pr_notice("Not match correct ID, loading default init code...\n");
		raspite_st7705_kd_hsd_panel_init(ctx);
		break;
	}

	ret = ctx->error;

	if (ret < 0)
		p2in1_unprepare(panel);

	ctx->prepared = true;

	return ret;
}

static int p2in1_enable(struct drm_panel *panel)
{
	struct p2in1 *ctx = panel_to_p2in1(panel);

	pr_notice("[lcm][st7705]%s enter.\n", __func__);

	if (ctx->enabled)
		return 0;

#ifdef CONFIG_LK_FASTLOGO
	if (ctx->lk_fastlogo) {
		ctx->enabled = true;
		return 0;
	}
#endif

	ctx->enabled = true;

	return 0;
}

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct p2in1 *ctx = panel_to_p2in1(panel);

	pr_notice("[lcm][%s] enter.\n", __func__);

	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	pr_notice("[lcm][%s] enter.\n", __func__);
	return 1;
}

static struct mtk_panel_params ext_params = {
	.pll_clk = 250,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x09,
		.count = 1,
		.para_list[0] = 0x80,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.lcm_esd_check_table[2] = {
		.cmd = 0xba,
		.count = 1,
		.para_list[0] = 0x33,
	},
};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ata_check = panel_ata_check,
};
#endif

static int p2in1_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;

	pr_notice("[lcm][%s] enter.\n", __func__);

	if (vendor_id == 0xFF)
		get_lcm_id();

	switch (vendor_id) {
	case LCM_ST7705_KD_HSD:
		mode = drm_mode_duplicate(panel->drm, &kd_hsd_mode);
		if (!mode) {
			dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
				kd_hsd_mode.hdisplay, kd_hsd_mode.vdisplay,
				kd_hsd_mode.vrefresh);
			return -ENOMEM;
		}
		break;
	case LCM_ST7705_TG_BOE:
		mode = drm_mode_duplicate(panel->drm, &tg_boe_mode);
		if (!mode) {
			dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
				tg_boe_mode.hdisplay, tg_boe_mode.vdisplay,
				tg_boe_mode.vrefresh);
			return -ENOMEM;
		}
		break;
	default:
		mode = drm_mode_duplicate(panel->drm, &kd_hsd_mode);
		if (!mode) {
			dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
				kd_hsd_mode.hdisplay, kd_hsd_mode.vdisplay,
				kd_hsd_mode.vrefresh);
			return -ENOMEM;
		}
		break;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode);

	panel->connector->display_info.width_mm = 108;
	panel->connector->display_info.height_mm = 172;
	panel->connector->display_info.panel_orientation =
		DRM_MODE_PANEL_ORIENTATION_RIGHT_UP;

	return 1;
}

static const struct drm_panel_funcs p2in1_drm_funcs = {
	.disable = p2in1_disable,
	.unprepare = p2in1_unprepare,
	.unprepare_power = p2in1_unprepare_power,
	.prepare_power = p2in1_prepare_power,
	.prepare = p2in1_prepare,
	.enable = p2in1_enable,
	.get_modes = p2in1_get_modes,
};

static int st7705_lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct p2in1 *ctx;
	int ret;

	pr_notice("[lcm][%s] enter.\n", __func__);

	if (vendor_id == 0xFF)
		get_lcm_id();

	switch (vendor_id) {
	case LCM_ST7705_KD_HSD:
	case LCM_ST7705_TG_BOE:
		pr_notice("[Kernel/LCM] St7705 panel, vendor_id = %d loading.\n", vendor_id);
		break;
	default:
		pr_notice("[Kernel/LCM] It's not St7705 panel, exit.\n");
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(struct p2in1), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ctx->vsim1 = regulator_get(dev, "vsim1");
	if (IS_ERR(ctx->vsim1)) {
		dev_err(dev, "[lcm]cannot get 1.8v regulator %ld\n",
			PTR_ERR(ctx->vsim1));
		return -EPROBE_DEFER;
	}

	ctx->biasp = regulator_get(NULL, "dsv_pos");
	if (IS_ERR(ctx->biasp)) {
		dev_err(dev, "[lcm]cannot get biasp regulator %ld\n",
			PTR_ERR(ctx->biasp));
		return -EPROBE_DEFER;
	}

	ctx->biasn = regulator_get(NULL, "dsv_neg");
	if (IS_ERR(ctx->biasn)) {
		dev_err(dev, "[lcm]cannot get biasn regulator %ld\n",
			PTR_ERR(ctx->biasn));
		return -EPROBE_DEFER;
	}

#ifdef CONFIG_LK_FASTLOGO
	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_ASIS);
#else
	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
#endif
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "[lcm]cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}

#ifdef CONFIG_LK_FASTLOGO
	if (ctx->vsim1) {
		ret = regulator_set_voltage(ctx->vsim1, 1800000, 1800000);
		if (!ret) {
			ret = regulator_enable(ctx->vsim1);
			if (ret < 0) {
				DRM_ERROR("[lcm]fails to enable ctx->vsim1");
				return ret;
			}
		}
	}

	if (ctx->biasp && regulator_is_enabled(ctx->biasp)) {
		ret = regulator_set_voltage(ctx->biasp, 6000000, 6000000);
		if (!ret) {
			ret = regulator_enable(ctx->biasp);
			if (ret < 0) {
				DRM_ERROR("[lcm]fails to enable ctx->biasp");
				return ret;
			}
		}
	}

	if (ctx->biasn && regulator_is_enabled(ctx->biasn)) {
		ret = regulator_set_voltage(ctx->biasn, 6000000, 6000000);
		if (!ret) {
			ret = regulator_enable(ctx->biasn);
			if (ret < 0) {
				DRM_ERROR("[lcm]fails to enable ctx->biasn");
				return ret;
			}
		}
	}
#endif

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &p2in1_drm_funcs;
	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;
	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

#ifdef CONFIG_LK_FASTLOGO
	ctx->enabled = true;
	ctx->prepared = true;
	ctx->lk_fastlogo = true;
	ctx->prepared_power = true;
#endif
	return ret;
}

static int st7705_lcm_remove(struct mipi_dsi_device *dsi)
{
	struct p2in1 *ctx = mipi_dsi_get_drvdata(dsi);

	pr_notice("[lcm][st7705]%s .\n", __func__);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id p2in1_of_match[] = {
	{ .compatible = "panel,st7705_panel_ic" },
	{ }
};

MODULE_DEVICE_TABLE(of, p2in1_of_match);

static struct mipi_dsi_driver st7705_driver = {
	.probe = st7705_lcm_probe,
	.remove = st7705_lcm_remove,
	.driver = {
		.name = "panel-st7705",
		.owner = THIS_MODULE,
		.of_match_table = p2in1_of_match,
	},
};

module_mipi_dsi_driver(st7705_driver);

MODULE_DESCRIPTION("ST7705 VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");
