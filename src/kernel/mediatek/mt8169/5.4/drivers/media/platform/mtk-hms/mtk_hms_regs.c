// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Ice Huang <ice.huang@mediatek.com>
 */

#include <linux/platform_device.h>
#include "mtk_hms_core.h"
#include "mtk_hms_regs.h"
#include "mtk_hms_type.h"
#include "mtk_hms_reg.h"

void mtk_hms_hw_start(struct mtk_hms_ctx *ctx)
{
	void __iomem *base_addr;

	// config
	base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_HMSENG]->regs;
	writel(0x00000007, base_addr + MDP_HMS_INTEN);

	base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_MUTEX]->regs;
	writel(0x00000000, base_addr + MDP_MUTEX0_DISP_MUTEXN_CTL);
	writel(0x0000fffe, base_addr + MDP_MUTEX0_DISP_MUTEXN_MOD0);
	writel(0x00000000, base_addr + MDP_MUTEX0_DISP_MUTEXN_MOD1);

	// start
	base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_RDMA]->regs;
	writel(0x00000003, base_addr + MDP_RDMA1_DISP_RDMA_GLOBAL_CON);

	base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_HMSENG]->regs;
	writel(0x00000001, base_addr + MDP_HMS_EN);

	base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_WDMA]->regs;
	writel(0xc0000001, base_addr + MDP_WDMA0_WDMA_EN);

	base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_MUTEX]->regs;
	writel(0x00000001, base_addr + MDP_MUTEX0_DISP_MUTEX5_EN);
}

void mtk_hms_hw_clear_int_status(struct mtk_hms_ctx *ctx)
{
	void __iomem *base_addr;
	unsigned int reg_val;

	base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_HMSENG]->regs;
	reg_val = readl(base_addr + MDP_HMS_INTSTA);
	writel(0x0, base_addr + MDP_HMS_INTSTA);

	base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_WDMA]->regs;
	reg_val = readl(base_addr + MDP_WDMA0_WDMA_INTSTA);
	writel(0x0, base_addr + MDP_WDMA0_WDMA_INTSTA);

	base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_RDMA]->regs;
	reg_val = readl(base_addr + MDP_RDMA1_DISP_RDMA_INTSTA);
	writel(0x0, base_addr + MDP_RDMA1_DISP_RDMA_INTSTA);

	base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_MUTEX]->regs;
	reg_val = readl(base_addr + MDP_MUTEX0_DISP_MUTEXN_INTSTA);
	writel(0x0, base_addr + MDP_MUTEX0_DISP_MUTEXN_INTSTA);
}

void mtk_hms_hw_set_input_addr(struct mtk_hms_ctx *ctx)
{
	struct mtk_hms_frame *frame = &ctx->s_frame;
	void __iomem *base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_RDMA]->regs;

	writel((uint32_t)frame->addr[0], base_addr + MDP_RDMA1_DISP_RDMA_MEM_START_ADDR);
}

void mtk_hms_hw_set_output_addr(struct mtk_hms_ctx *ctx)
{
	struct mtk_hms_frame *frame = &ctx->d_frame;
	void __iomem *base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_WDMA]->regs;

	writel((uint32_t)frame->addr[0], base_addr + MDP_WDMA0_WDMA_DST_ADDR0);
}

void mtk_hms_hw_set_in_size(struct mtk_hms_ctx *ctx)
{
	struct mtk_hms_frame *frame = &ctx->s_frame;
	unsigned int reg_val;
	void __iomem *base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_HMSENG]->regs;

	// update HMS
	reg_val = ((frame->width & 0x3ff) << 16) | (frame->height & 0x3ff);
	writel(reg_val, base_addr + MDP_HMS_CFG_SIZE);

	// update RDMA size
	base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_RDMA]->regs;
	reg_val = frame->width & 0x1fff;
	writel(reg_val, base_addr + MDP_RDMA1_DISP_RDMA_SIZE_CON_0);
	reg_val = frame->height & 0x1fff;
	writel(reg_val, base_addr + MDP_RDMA1_DISP_RDMA_SIZE_CON_1);
}

void mtk_hms_hw_set_in_image_format(struct mtk_hms_ctx *ctx)
{
	struct mtk_hms_frame *frame = &ctx->s_frame;
	unsigned int reg_val, reg_color_fmt, reg_color_matrix;
	void __iomem *base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_RDMA]->regs;

	//rdma pitch from fmt
	//reg_val = (frame->fmt->depth[0] / 8) * (frame->width);
	//reg_val = frame->pitch[0]*(frame->height);
	reg_val = frame->pitch[0];
	writel(reg_val, base_addr + MDP_RDMA1_DISP_RDMA_MEM_SRC_PITCH);

	// rdma fmt
	reg_color_fmt = readl(base_addr + MDP_RDMA1_DISP_RDMA_MEM_CON) & 0xFFFFEE0F;
	reg_color_matrix = readl(base_addr + MDP_RDMA1_DISP_RDMA_SIZE_CON_0) & 0xFF09FFFF;
	switch (frame->fmt->pixelformat) {
	case V4L2_PIX_FMT_UYVY:
		reg_color_fmt |= HMS_COLOR_FMT_UYVY;
		reg_color_matrix |= HMS_COLOR_MATRIX_YUV;
		break;
	case V4L2_PIX_FMT_VYUY:
		reg_color_fmt |= HMS_COLOR_FMT_VYUY;
		reg_color_matrix |= HMS_COLOR_MATRIX_YUV;
		break;
	case V4L2_PIX_FMT_YUYV:
		reg_color_fmt |= HMS_COLOR_FMT_YUYV;
		reg_color_matrix |= HMS_COLOR_MATRIX_YUV;
		break;
	case V4L2_PIX_FMT_YVYU:
		reg_color_fmt |= HMS_COLOR_FMT_YVYU;
		reg_color_matrix |= HMS_COLOR_MATRIX_YUV;
		break;
	case V4L2_PIX_FMT_RGB565:
		reg_color_fmt |= HMS_COLOR_FMT_RGB565;
		break;
	case V4L2_PIX_FMT_BGR24:
		reg_color_fmt |= HMS_COLOR_FMT_BGR24;
		break;
	case V4L2_PIX_FMT_RGB24:
		reg_color_fmt |= HMS_COLOR_FMT_RGB24;
		break;
	default:
		break;
	}
	writel(reg_color_fmt, base_addr + MDP_RDMA1_DISP_RDMA_MEM_CON);
	writel(reg_color_matrix, base_addr + MDP_RDMA1_DISP_RDMA_SIZE_CON_0);
	// config
	writel(0x000000ff, base_addr);
	writel(0x88c002cb, base_addr + MDP_RDMA1_DISP_RDMA_FIFO_CON);
	writel(0x42e802cb, base_addr + MDP_RDMA1_DISP_RDMA_MEM_GMC_SETTING_0);
	writel(0x42cb029f, base_addr + MDP_RDMA1_DISP_RDMA_MEM_GMC_SETTING_1);
	writel(0x00000100, base_addr + MDP_RDMA1_DISP_RDMA_MEM_GMC_SETTING_2);
	writel(0x00000000, base_addr + MDP_RDMA1_DISP_RDMA_STALL_CG_CON);
	writel(0x00000000, base_addr + MDP_RDMA1_DISP_RDMA_SRAM_SEL);
	writel(0x0140006A, base_addr + MDP_RDMA1_DISP_RDMA_FIFO_CON);
}

void mtk_hms_hw_set_out_size(struct mtk_hms_ctx *ctx)
{
	struct mtk_hms_frame *frame = &ctx->d_frame;
	unsigned int reg_val, block_size, stt_width, stt_height;
	void __iomem *base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_WDMA]->regs;

	block_size = ctx->block_size & 0xf;
	stt_width = frame->width / block_size * 2;
	stt_height = frame->height / block_size;
	reg_val = stt_width * 4;
	writel(reg_val, base_addr + MDP_WDMA0_WDMA_DST_W_IN_BYTE);
	reg_val = (stt_height << 16) | stt_width;
	writel(reg_val, base_addr + MDP_WDMA0_WDMA_SRC_SIZE);
	writel(reg_val, base_addr + MDP_WDMA0_WDMA_CLIP_SIZE);
}

void mtk_hms_hw_set_out_image_format(struct mtk_hms_ctx *ctx)
{
	void __iomem *base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_WDMA]->regs;

	writel(0x02020020, base_addr + MDP_WDMA0_WDMA_CFG);
	writel(0x00000000, base_addr + MDP_WDMA0_WDMA_CLIP_COORD);
	writel(0x0000000F, base_addr);
}

void mtk_hms_hw_set_saturation_mask(struct mtk_hms_ctx *ctx)
{
	unsigned int reg_val;
	void __iomem *base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_HMSENG]->regs;

	reg_val = readl(base_addr + MDP_HMS_CFG_MASK) & 0xffffff00;
	reg_val |= (ctx->saturation_mask & 0xff);
	writel(reg_val, base_addr + MDP_HMS_CFG_MASK);
}

void mtk_hms_hw_set_under_exposed_mask(struct mtk_hms_ctx *ctx)
{
	unsigned int reg_val;
	void __iomem *base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_HMSENG]->regs;

	reg_val = readl(base_addr + MDP_HMS_CFG_MASK) & 0xffff00ff;
	reg_val |= (ctx->under_exposed_mask & 0xff) << 8;
	writel(reg_val, base_addr + MDP_HMS_CFG_MASK);
}

void mtk_hms_hw_set_grayscale_rgb_coef(struct mtk_hms_ctx *ctx)
{
	unsigned int reg_val;
	void __iomem *base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_HMSENG]->regs;

	reg_val = readl(base_addr + MDP_HMS_CFG_GRAY_COEF) & 0xff000000;
	reg_val |= (ctx->grayscale_rgb_coef & 0xffffff);
	writel(reg_val, base_addr + MDP_HMS_CFG_GRAY_COEF);
}

void mtk_hms_hw_set_block_size(struct mtk_hms_ctx *ctx)
{
	unsigned int reg_val, total_regions, block_size_idx = 2, block_size = 4;
	void __iomem *base_addr = ctx->hms_dev->comp[MTK_HMS_COMP_HMSENG]->regs;

	block_size = ctx->block_size & 0xf;
	switch (block_size) {
	case 1:
		block_size_idx = 0;
		break;
	case 2:
		block_size_idx = 1;
		break;
	case 4:
		block_size_idx = 2;
		break;
	case 8:
		block_size_idx = 3;
		break;
	}
	total_regions = (ctx->s_frame.width / block_size) * (ctx->s_frame.height / block_size);
	reg_val = (total_regions << 8) | block_size_idx;
	writel(reg_val, base_addr + MDP_HMS_CFG_BLOCK);
}

