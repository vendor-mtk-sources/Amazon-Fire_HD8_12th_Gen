/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Ice Huang <ice.huang@mediatek.com>
 */

#ifndef __MTK_HMS_REGS_H__
#define __MTK_HMS_REGS_H__

void mtk_hms_hw_set_input_addr(struct mtk_hms_ctx *ctx);
void mtk_hms_hw_set_output_addr(struct mtk_hms_ctx *ctx);
void mtk_hms_hw_set_in_size(struct mtk_hms_ctx *ctx);
void mtk_hms_hw_set_in_image_format(struct mtk_hms_ctx *ctx);
void mtk_hms_hw_set_out_size(struct mtk_hms_ctx *ctx);
void mtk_hms_hw_set_out_image_format(struct mtk_hms_ctx *ctx);
void mtk_hms_hw_start(struct mtk_hms_ctx *ctx);
void mtk_hms_hw_clear_int_status(struct mtk_hms_ctx *ctx);
void mtk_hms_hw_set_saturation_mask(struct mtk_hms_ctx *ctx);
void mtk_hms_hw_set_under_exposed_mask(struct mtk_hms_ctx *ctx);
void mtk_hms_hw_set_grayscale_rgb_coef(struct mtk_hms_ctx *ctx);
void mtk_hms_hw_set_block_size(struct mtk_hms_ctx *ctx);


#endif /* __MTK_HMS_REGS_H__ */
