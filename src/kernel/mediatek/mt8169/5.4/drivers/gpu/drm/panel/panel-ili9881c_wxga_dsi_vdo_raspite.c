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

#define LCM_ILI9881C_STARRY_INX    9
#define LCM_ILI9881C_KD_BOE        5
#define REGFLAG_DELAY              0xFC

static unsigned char vendor_id = 0xFF;
static void get_lcm_id(void);
#if IS_ENABLED(CONFIG_IDME)
extern unsigned int idme_get_lcmid_info(void);
#endif

#define FRAME_WIDTH                (800)
#define FRAME_HEIGHT               (1280)
/* STARRY INX panel porch */
#define STARRY_INX_HFP (80)    // params->dsi.horizontal_frontporch
#define STARRY_INX_HSA (40)    // params->dsi.horizontal_sync_active
#define STARRY_INX_HBP (80)    // params->dsi.horizontal_backporch
#define STARRY_INX_VFP (24)    // params->dsi.vertical_frontporch
#define STARRY_INX_VSA (8)     // params->dsi.vertical_sync_active
#define STARRY_INX_VBP (24)    // params->dsi.vertical_backporch

/* KD BOE panel porch */
#define KD_BOE_HFP (90)    // params->dsi.horizontal_frontporch
#define KD_BOE_HSA (20)    // params->dsi.horizontal_sync_active
#define KD_BOE_HBP (90)    // params->dsi.horizontal_backporch
#define KD_BOE_VFP (30)    // params->dsi.vertical_frontporch
#define KD_BOE_VSA (4)     // params->dsi.vertical_sync_active
#define KD_BOE_VBP (18)    // params->dsi.vertical_backporch

static const struct drm_display_mode starry_inx_mode = {
	.clock = (FRAME_WIDTH + STARRY_INX_HFP + STARRY_INX_HSA + STARRY_INX_HBP) *
		(FRAME_HEIGHT + STARRY_INX_VFP + STARRY_INX_VSA + STARRY_INX_VBP) *
		60 / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + STARRY_INX_HFP,
	.hsync_end = FRAME_WIDTH + STARRY_INX_HFP + STARRY_INX_HSA,
	.htotal = FRAME_WIDTH + STARRY_INX_HFP + STARRY_INX_HSA + STARRY_INX_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + STARRY_INX_VFP,
	.vsync_end = FRAME_HEIGHT + STARRY_INX_VFP + STARRY_INX_VSA,
	.vtotal = FRAME_HEIGHT + STARRY_INX_VFP + STARRY_INX_VSA + STARRY_INX_VBP,
	.vrefresh = 60,
};

static const struct drm_display_mode kd_boe_mode = {
	.clock = (FRAME_WIDTH + KD_BOE_HFP + KD_BOE_HSA + KD_BOE_HBP) *
		(FRAME_HEIGHT + KD_BOE_VFP + KD_BOE_VSA + KD_BOE_VBP) *
		60 / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + KD_BOE_HFP,
	.hsync_end = FRAME_WIDTH + KD_BOE_HFP + KD_BOE_HSA,
	.htotal = FRAME_WIDTH + KD_BOE_HFP + KD_BOE_HSA + KD_BOE_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + KD_BOE_VFP,
	.vsync_end = FRAME_HEIGHT + KD_BOE_VFP + KD_BOE_VSA,
	.vtotal = FRAME_HEIGHT + KD_BOE_VFP + KD_BOE_VSA + KD_BOE_VBP,
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
	if (lcm_id == 0)
		pr_err("idme_get_lcm_id failed.\n");
#else
	pr_notice("%s, idme is not ready.\n", __func__);
#endif

	vendor_id = (unsigned char)lcm_id;
	pr_notice("[ili9881c] %s, vendor id = 0x%x\n", __func__, vendor_id);
}

static struct LCM_setting_table lcm_init_ili_starry_inx[] = {
	{0xFF, 3, {0x98, 0x81, 0x03}},
	{0x01, 1, {0x00}},
	{0x02, 1, {0x00}},
	{0x03, 1, {0x53}},
	{0x04, 1, {0x53}},
	{0x05, 1, {0x13}},
	{0x06, 1, {0x04}},
	{0x07, 1, {0x02}},
	{0x08, 1, {0x02}},
	{0x09, 1, {0x00}},
	{0x0a, 1, {0x00}},
	{0x0b, 1, {0x00}},
	{0x0c, 1, {0x00}},
	{0x0d, 1, {0x00}},
	{0x0e, 1, {0x00}},
	{0x0f, 1, {0x00}},
	{0x10, 1, {0x00}},
	{0x11, 1, {0x00}},
	{0x12, 1, {0x00}},
	{0x13, 1, {0x00}},
	{0x14, 1, {0x00}},
	{0x15, 1, {0x00}},
	{0x16, 1, {0x00}},
	{0x17, 1, {0x00}},
	{0x18, 1, {0x00}},
	{0x19, 1, {0x00}},
	{0x1a, 1, {0x00}},
	{0x1b, 1, {0x00}},
	{0x1c, 1, {0x00}},
	{0x1d, 1, {0x00}},
	{0x1e, 1, {0xc0}},
	{0x1f, 1, {0x80}},
	{0x20, 1, {0x02}},
	{0x21, 1, {0x09}},
	{0x22, 1, {0x00}},
	{0x23, 1, {0x00}},
	{0x24, 1, {0x00}},
	{0x25, 1, {0x00}},
	{0x26, 1, {0x00}},
	{0x27, 1, {0x00}},
	{0x28, 1, {0x55}},
	{0x29, 1, {0x03}},
	{0x2a, 1, {0x00}},
	{0x2b, 1, {0x00}},
	{0x2c, 1, {0x00}},
	{0x2d, 1, {0x00}},
	{0x2e, 1, {0x00}},
	{0x2f, 1, {0x00}},
	{0x30, 1, {0x00}},
	{0x31, 1, {0x00}},
	{0x32, 1, {0x00}},
	{0x33, 1, {0x00}},
	{0x34, 1, {0x00}},
	{0x35, 1, {0x00}},
	{0x36, 1, {0x00}},
	{0x37, 1, {0x00}},
	{0x38, 1, {0x3C}},
	{0x39, 1, {0x00}},
	{0x3a, 1, {0x00}},
	{0x3b, 1, {0x00}},
	{0x3c, 1, {0x00}},
	{0x3d, 1, {0x00}},
	{0x3e, 1, {0x00}},
	{0x3f, 1, {0x00}},
	{0x40, 1, {0x00}},
	{0x41, 1, {0x00}},
	{0x42, 1, {0x00}},
	{0x43, 1, {0x00}},
	{0x44, 1, {0x00}},
	{0x50, 1, {0x01}},
	{0x51, 1, {0x23}},
	{0x52, 1, {0x45}},
	{0x53, 1, {0x67}},
	{0x54, 1, {0x89}},
	{0x55, 1, {0xab}},
	{0x56, 1, {0x01}},
	{0x57, 1, {0x23}},
	{0x58, 1, {0x45}},
	{0x59, 1, {0x67}},
	{0x5a, 1, {0x89}},
	{0x5b, 1, {0xab}},
	{0x5c, 1, {0xcd}},
	{0x5d, 1, {0xef}},
	{0x5e, 1, {0x01}},
	{0x5f, 1, {0x08}},
	{0x60, 1, {0x02}},
	{0x61, 1, {0x02}},
	{0x62, 1, {0x0A}},
	{0x63, 1, {0x15}},
	{0x64, 1, {0x14}},
	{0x65, 1, {0x02}},
	{0x66, 1, {0x11}},
	{0x67, 1, {0x10}},
	{0x68, 1, {0x02}},
	{0x69, 1, {0x0F}},
	{0x6a, 1, {0x0E}},
	{0x6b, 1, {0x02}},
	{0x6c, 1, {0x0D}},
	{0x6d, 1, {0x0C}},
	{0x6e, 1, {0x06}},
	{0x6f, 1, {0x02}},
	{0x70, 1, {0x02}},
	{0x71, 1, {0x02}},
	{0x72, 1, {0x02}},
	{0x73, 1, {0x02}},
	{0x74, 1, {0x02}},
	{0x75, 1, {0x06}},
	{0x76, 1, {0x02}},
	{0x77, 1, {0x02}},
	{0x78, 1, {0x0A}},
	{0x79, 1, {0x15}},
	{0x7a, 1, {0x14}},
	{0x7b, 1, {0x02}},
	{0x7c, 1, {0x10}},
	{0x7d, 1, {0x11}},
	{0x7e, 1, {0x02}},
	{0x7f, 1, {0x0C}},
	{0x80, 1, {0x0D}},
	{0x81, 1, {0x02}},
	{0x82, 1, {0x0E}},
	{0x83, 1, {0x0F}},
	{0x84, 1, {0x08}},
	{0x85, 1, {0x02}},
	{0x86, 1, {0x02}},
	{0x87, 1, {0x02}},
	{0x88, 1, {0x02}},
	{0x89, 1, {0x02}},
	{0x8A, 1, {0x02}},
	{0xFF, 3, {0x98, 0x81, 0x04}},
	{0x92, 1, {0x0F}},
	{0x6C, 1, {0x15}},
	{0x6E, 1, {0x30}},
	{0x6F, 1, {0x37}},
	{0x8D, 1, {0x1F}},
	{0x87, 1, {0xBA}},
	{0x26, 1, {0x76}},
	{0xB2, 1, {0xD1}},
	{0xB5, 1, {0x07}},
	{0x35, 1, {0x17}},
	{0x33, 1, {0x14}},
	{0x31, 1, {0x75}},
	{0x3A, 1, {0x85}},
	{0x3B, 1, {0x98}},
	{0x38, 1, {0x01}},
	{0x39, 1, {0x00}},
	{0x7A, 1, {0x10}},
	{0xFF, 3, {0x98, 0x81, 0x01}},
	{0x22, 1, {0x0A}},
	{0x31, 1, {0x00}},
	{0x50, 1, {0xCF}},
	{0x51, 1, {0xCA}},
	{0x60, 1, {0x28}},
	{0x2E, 1, {0xC8}},
	{0x34, 1, {0x01}},
	{0xA0, 1, {0x00}},
	{0xA1, 1, {0x11}},
	{0xA2, 1, {0x1E}},
	{0xA3, 1, {0x14}},
	{0xA4, 1, {0x16}},
	{0xA5, 1, {0x2A}},
	{0xA6, 1, {0x1E}},
	{0xA7, 1, {0x20}},
	{0xA8, 1, {0x7A}},
	{0xA9, 1, {0x1B}},
	{0xAA, 1, {0x27}},
	{0xAB, 1, {0x6B}},
	{0xAC, 1, {0x1B}},
	{0xAD, 1, {0x1B}},
	{0xAE, 1, {0x50}},
	{0xAF, 1, {0x25}},
	{0xB0, 1, {0x2A}},
	{0xB1, 1, {0x51}},
	{0xB2, 1, {0x62}},
	{0xB3, 1, {0x3F}},
	{0xC0, 1, {0x00}},
	{0xC1, 1, {0x11}},
	{0xC2, 1, {0x1E}},
	{0xC3, 1, {0x14}},
	{0xC4, 1, {0x16}},
	{0xC5, 1, {0x2A}},
	{0xC6, 1, {0x1E}},
	{0xC7, 1, {0x20}},
	{0xC8, 1, {0x7A}},
	{0xC9, 1, {0x1B}},
	{0xCA, 1, {0x27}},
	{0xCB, 1, {0x6B}},
	{0xCC, 1, {0x1B}},
	{0xCD, 1, {0x1B}},
	{0xCE, 1, {0x50}},
	{0xCF, 1, {0x25}},
	{0xD0, 1, {0x2A}},
	{0xD1, 1, {0x51}},
	{0xD2, 1, {0x62}},
	{0xD3, 1, {0x3F}},
	{0xFF, 3, {0x98, 0x81, 0x00}},
	{0x11, 0, {}},
	{REGFLAG_DELAY, 120, {}},
	{0x29, 0, {}},
	{REGFLAG_DELAY, 20, {}},
	{0x35, 1, {0x00}},
};

static struct LCM_setting_table lcm_init_ili_kd_boe[] = {
	{0xFF, 3, {0x98, 0x81, 0x03}},
	{0x01, 1, {0x00}},
	{0x02, 1, {0x00}},
	{0x03, 1, {0x57}},
	{0x04, 1, {0xD3}},
	{0x05, 1, {0x00}},
	{0x06, 1, {0x11}},
	{0x07, 1, {0x08}},
	{0x08, 1, {0x00}},
	{0x09, 1, {0x00}},
	{0x0a, 1, {0x3F}},
	{0x0b, 1, {0x00}},
	{0x0c, 1, {0x00}},
	{0x0d, 1, {0x00}},
	{0x0e, 1, {0x00}},
	{0x0f, 1, {0x3F}},
	{0x10, 1, {0x3F}},
	{0x11, 1, {0x00}},
	{0x12, 1, {0x00}},
	{0x13, 1, {0x00}},
	{0x14, 1, {0x00}},
	{0x15, 1, {0x00}},
	{0x16, 1, {0x00}},
	{0x17, 1, {0x00}},
	{0x18, 1, {0x00}},
	{0x19, 1, {0x00}},
	{0x1a, 1, {0x00}},
	{0x1b, 1, {0x00}},
	{0x1c, 1, {0x00}},
	{0x1d, 1, {0x00}},
	{0x1e, 1, {0x40}},
	{0x1f, 1, {0x80}},
	{0x20, 1, {0x06}},
	{0x21, 1, {0x01}},
	{0x22, 1, {0x00}},
	{0x23, 1, {0x00}},
	{0x24, 1, {0x00}},
	{0x25, 1, {0x00}},
	{0x26, 1, {0x00}},
	{0x27, 1, {0x00}},
	{0x28, 1, {0x33}},
	{0x29, 1, {0x33}},
	{0x2a, 1, {0x00}},
	{0x2b, 1, {0x00}},
	{0x2c, 1, {0x00}},
	{0x2d, 1, {0x00}},
	{0x2e, 1, {0x00}},
	{0x2f, 1, {0x00}},
	{0x30, 1, {0x00}},
	{0x31, 1, {0x00}},
	{0x32, 1, {0x00}},
	{0x33, 1, {0x00}},
	{0x34, 1, {0x00}},
	{0x35, 1, {0x00}},
	{0x36, 1, {0x00}},
	{0x37, 1, {0x00}},
	{0x38, 1, {0x78}},
	{0x39, 1, {0x00}},
	{0x3a, 1, {0x00}},
	{0x3b, 1, {0x00}},
	{0x3c, 1, {0x00}},
	{0x3d, 1, {0x00}},
	{0x3e, 1, {0x00}},
	{0x3f, 1, {0x00}},
	{0x40, 1, {0x00}},
	{0x41, 1, {0x00}},
	{0x42, 1, {0x00}},
	{0x43, 1, {0x00}},
	{0x44, 1, {0x00}},
	{0x50, 1, {0x00}},
	{0x51, 1, {0x23}},
	{0x52, 1, {0x45}},
	{0x53, 1, {0x67}},
	{0x54, 1, {0x89}},
	{0x55, 1, {0xab}},
	{0x56, 1, {0x01}},
	{0x57, 1, {0x23}},
	{0x58, 1, {0x45}},
	{0x59, 1, {0x67}},
	{0x5a, 1, {0x89}},
	{0x5b, 1, {0xab}},
	{0x5c, 1, {0xcd}},
	{0x5d, 1, {0xef}},
	{0x5e, 1, {0x00}},
	{0x5f, 1, {0x0D}},
	{0x60, 1, {0x0D}},
	{0x61, 1, {0x0C}},
	{0x62, 1, {0x0C}},
	{0x63, 1, {0x0F}},
	{0x64, 1, {0x0F}},
	{0x65, 1, {0x0E}},
	{0x66, 1, {0x0E}},
	{0x67, 1, {0x08}},
	{0x68, 1, {0x02}},
	{0x69, 1, {0x02}},
	{0x6a, 1, {0x02}},
	{0x6b, 1, {0x02}},
	{0x6c, 1, {0x02}},
	{0x6d, 1, {0x02}},
	{0x6e, 1, {0x02}},
	{0x6f, 1, {0x02}},
	{0x70, 1, {0x14}},
	{0x71, 1, {0x15}},
	{0x72, 1, {0x06}},
	{0x73, 1, {0x02}},
	{0x74, 1, {0x02}},
	{0x75, 1, {0x0D}},
	{0x76, 1, {0x0D}},
	{0x77, 1, {0x0C}},
	{0x78, 1, {0x0C}},
	{0x79, 1, {0x0F}},
	{0x7a, 1, {0x0F}},
	{0x7b, 1, {0x0E}},
	{0x7c, 1, {0x0E}},
	{0x7d, 1, {0x08}},
	{0x7e, 1, {0x02}},
	{0x7f, 1, {0x02}},
	{0x80, 1, {0x02}},
	{0x81, 1, {0x02}},
	{0x82, 1, {0x02}},
	{0x83, 1, {0x02}},
	{0x84, 1, {0x02}},
	{0x85, 1, {0x02}},
	{0x86, 1, {0x14}},
	{0x87, 1, {0x15}},
	{0x88, 1, {0x06}},
	{0x89, 1, {0x02}},
	{0x8A, 1, {0x02}},
	{0xFF, 3, {0x98, 0x81, 0x04}},
	{0x92, 1, {0x0F}},
	{0x6E, 1, {0x3A}},
	{0x6F, 1, {0x57}},
	{0x35, 1, {0x1F}},
	{0x3A, 1, {0x24}},
	{0x8D, 1, {0x1F}},
	{0x87, 1, {0xBA}},
	{0xB2, 1, {0xD1}},
	{0x88, 1, {0x0B}},
	{0x38, 1, {0x01}},
	{0x39, 1, {0x00}},
	{0xB5, 1, {0x07}},
	{0x31, 1, {0x75}},
	{0x3B, 1, {0x98}},
	{0xFF, 3, {0x98, 0x81, 0x01}},
	{0x22, 1, {0x0A}},
	{0x31, 1, {0x09}},
	{0x35, 1, {0x07}},
	{0x50, 1, {0x85}},
	{0x51, 1, {0x80}},
	{0x60, 1, {0x10}},
	{0x62, 1, {0x00}},
	{0x42, 1, {0x55}},
	{0xA0, 1, {0x0C}},
	{0xA1, 1, {0x16}},
	{0xA2, 1, {0x23}},
	{0xA3, 1, {0x12}},
	{0xA4, 1, {0x14}},
	{0xA5, 1, {0x27}},
	{0xA6, 1, {0x1C}},
	{0xA7, 1, {0x1D}},
	{0xA8, 1, {0x7E}},
	{0xA9, 1, {0x1D}},
	{0xAA, 1, {0x29}},
	{0xAB, 1, {0x6C}},
	{0xAC, 1, {0x19}},
	{0xAD, 1, {0x16}},
	{0xAE, 1, {0x49}},
	{0xAF, 1, {0x20}},
	{0xB0, 1, {0x27}},
	{0xB1, 1, {0x50}},
	{0xB2, 1, {0x64}},
	{0xB3, 1, {0x3F}},
	{0xC0, 1, {0x00}},
	{0xC1, 1, {0x16}},
	{0xC2, 1, {0x23}},
	{0xC3, 1, {0x12}},
	{0xC4, 1, {0x14}},
	{0xC5, 1, {0x27}},
	{0xC6, 1, {0x1C}},
	{0xC7, 1, {0x1D}},
	{0xC8, 1, {0x7F}},
	{0xC9, 1, {0x1D}},
	{0xCA, 1, {0x29}},
	{0xCB, 1, {0x6C}},
	{0xCC, 1, {0x19}},
	{0xCD, 1, {0x16}},
	{0xCE, 1, {0x49}},
	{0xCF, 1, {0x20}},
	{0xD0, 1, {0x27}},
	{0xD1, 1, {0x50}},
	{0xD2, 1, {0x64}},
	{0xD3, 1, {0x3F}},
	{0xFF, 3, {0x98, 0x81, 0x02}},
	{0x06, 1, {0x40}},
	{0x07, 1, {0x09}},
	{0xFF, 3, {0x98, 0x81, 0x00}},
	{0x51, 2, {0x0F, 0xFF}},
	{0x53, 1, {0x2C}},
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

static void raspite_ili_starry_inx_panel_init(struct p2in1 *ctx)
{
	pr_notice("[lcm][ili9881c]%s enter.\n", __func__);

	lcm_init_push_table(ctx, lcm_init_ili_starry_inx,
		sizeof(lcm_init_ili_starry_inx) / sizeof(struct LCM_setting_table));
}

static void raspite_ili_kd_boe_panel_init(struct p2in1 *ctx)
{
	pr_notice("[lcm][ili9881c]%s enter.\n", __func__);

	lcm_init_push_table(ctx, lcm_init_ili_kd_boe,
		sizeof(lcm_init_ili_kd_boe) / sizeof(struct LCM_setting_table));
}

static int p2in1_disable(struct drm_panel *panel)
{
	struct p2in1 *ctx = panel_to_p2in1(panel);

	pr_notice("[lcm][ili9881c]%s enter\n", __func__);

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
	pr_notice("[lcm][ili9881c]%s enter\n", __func__);

	if (!ctx->prepared)
		return 0;

#ifdef CONFIG_LK_FASTLOGO
	if (ctx->lk_fastlogo)
		ctx->lk_fastlogo = false;
#endif
	/* swich to page 0 */
	p2in1_dcs_write_seq_static(ctx, 0xFF, 0x98, 0x81, 0x00);
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

	pr_notice("[lcm][ili9881c]%s enter\n", __func__);

	if (!ctx->prepared_power)
		return 0;

#ifdef CONFIG_LK_FASTLOGO
	if (ctx->lk_fastlogo)
		pr_notice("[lcm][ili9881c]%s +++\n", __func__);
#endif
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

	pr_notice("[lcm][ili9881c]%s enter.\n", __func__);

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
	usleep_range(12000, 12100);

	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(2000, 2100);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(2000, 2100);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(12000, 12100);

	ret = ctx->error;
	if (ret < 0)
		p2in1_unprepare_power(panel);

	ctx->prepared_power = true;

	pr_notice("[lcm][ili9881c]%s exit.\n", __func__);
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
	pr_notice("[lcm][ili9881c]%s enter.\n", __func__);

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

	switch (vendor_id) {
	case LCM_ILI9881C_STARRY_INX:
		raspite_ili_starry_inx_panel_init(ctx);
		break;
	case LCM_ILI9881C_KD_BOE:
		raspite_ili_kd_boe_panel_init(ctx);
		break;
	default:
		pr_notice("Not match correct ID, loading default init code...\n");
		raspite_ili_starry_inx_panel_init(ctx);
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

	pr_notice("[lcm][ili9881c]%s enter.\n", __func__);

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
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
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
	case LCM_ILI9881C_STARRY_INX:
		mode = drm_mode_duplicate(panel->drm, &starry_inx_mode);
		if (!mode) {
			dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
				starry_inx_mode.hdisplay, starry_inx_mode.vdisplay,
				starry_inx_mode.vrefresh);
			return -ENOMEM;
		}
		break;
	case LCM_ILI9881C_KD_BOE:
		mode = drm_mode_duplicate(panel->drm, &kd_boe_mode);
		if (!mode) {
			dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
				kd_boe_mode.hdisplay, kd_boe_mode.vdisplay,
				kd_boe_mode.vrefresh);
			return -ENOMEM;
		}
		break;
	default:
		mode = drm_mode_duplicate(panel->drm, &starry_inx_mode);
		if (!mode) {
			dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
				starry_inx_mode.hdisplay, starry_inx_mode.vdisplay,
				starry_inx_mode.vrefresh);
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

static int ili9881c_lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct p2in1 *ctx;
	int ret;

	pr_notice("[lcm][%s] enter.\n", __func__);

	if (vendor_id == 0xFF)
		get_lcm_id();

	switch (vendor_id) {
	case LCM_ILI9881C_STARRY_INX:
	case LCM_ILI9881C_KD_BOE:
		pr_notice("[Kernel/LCM] Ili9881c panel, vendor_id = %d loading.\n", vendor_id);
		break;
	default:
		pr_notice("[Kernel/LCM] It's not Ili9881c panel, exit.\n");
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

static int ili9881c_lcm_remove(struct mipi_dsi_device *dsi)
{
	struct p2in1 *ctx = mipi_dsi_get_drvdata(dsi);

	pr_notice("[lcm][ili9881c]%s .\n", __func__);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id p2in1_of_match[] = {
	{ .compatible = "panel,ili9881c_panel_ic" },
	{ }
};

MODULE_DEVICE_TABLE(of, p2in1_of_match);

static struct mipi_dsi_driver ili9881c_driver = {
	.probe = ili9881c_lcm_probe,
	.remove = ili9881c_lcm_remove,
	.driver = {
		.name = "panel-ili9881c",
		.owner = THIS_MODULE,
		.of_match_table = p2in1_of_match,
	},
};

module_mipi_dsi_driver(ili9881c_driver);

MODULE_DESCRIPTION("ILI9881C VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");
