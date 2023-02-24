/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Ice Huang <ice.huang@mediatek.com>
 */

#ifndef __MTK_HMS_M2M_H__
#define __MTK_HMS_M2M_H__

void mtk_hms_clear_int_status(struct mtk_hms_ctx *ctx);
void mtk_hms_ctx_state_lock_set(struct mtk_hms_ctx *ctx, u32 state);
int mtk_hms_register_m2m_device(struct mtk_hms_dev *hms);
void mtk_hms_unregister_m2m_device(struct mtk_hms_dev *hms);
void mtk_hms_process_done(void *priv, int vb_state);

#endif /* __MTK_HMS_M2M_H__ */
