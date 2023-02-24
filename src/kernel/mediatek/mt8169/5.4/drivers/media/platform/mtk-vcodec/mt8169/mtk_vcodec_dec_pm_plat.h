/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#ifndef _MTK_VCODEC_DEC_PM_PLAT_H_
#define _MTK_VCODEC_DEC_PM_PLAT_H_

#include "mtk_vcodec_drv.h"
#if IS_ENABLED(CONFIG_MTK_MMDVFS)
#define DEC_DVFS	1
#else
#define DEC_DVFS	0
#endif

#if IS_ENABLED(CONFIG_INTERCONNECT_MTK_MMQOS_MT8169)
#define DEC_EMI_BW	1
#else
#define DEC_EMI_BW	0
#endif

void mtk_prepare_vdec_dvfs(struct mtk_vcodec_dev *dev);
void mtk_unprepare_vdec_dvfs(struct mtk_vcodec_dev *dev);
void mtk_prepare_vdec_emi_bw(struct mtk_vcodec_dev *dev);
void mtk_unprepare_vdec_emi_bw(struct mtk_vcodec_dev *dev);

void mtk_vdec_pmqos_prelock(struct mtk_vcodec_ctx *ctx, int hw_id);
void mtk_vdec_pmqos_begin_frame(struct mtk_vcodec_ctx *ctx, int hw_id);
void mtk_vdec_pmqos_end_frame(struct mtk_vcodec_ctx *ctx, int hw_id);

#endif /* _MTK_VCODEC_DEC_PM_PLAT_H_ */
