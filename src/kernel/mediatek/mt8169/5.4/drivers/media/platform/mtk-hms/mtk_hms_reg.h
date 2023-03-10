/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Ice Huang <ice.huang@mediatek.com>
 */

#ifndef __MTK_HMS_REG_H__
#define __MTK_HMS_REG_H__

#define MDP_RDMA2_REG_BASE   (0x0)//(0x1B004000)
#define MDP_RDMA1_DISP_RDMA_INTSTA             (MDP_RDMA2_REG_BASE + 0x004)
#define MDP_RDMA1_DISP_RDMA_GLOBAL_CON   (MDP_RDMA2_REG_BASE + 0x010)
#define MDP_RDMA1_DISP_RDMA_SIZE_CON_0    (MDP_RDMA2_REG_BASE + 0x014)
#define MDP_RDMA1_DISP_RDMA_SIZE_CON_1    (MDP_RDMA2_REG_BASE + 0x018)
#define MDP_RDMA1_DISP_RDMA_MEM_CON        (MDP_RDMA2_REG_BASE + 0x024)  // 0x024
#define MDP_RDMA1_DISP_RDMA_MEM_START_ADDR  (MDP_RDMA2_REG_BASE + 0xF00)
#define MDP_RDMA1_DISP_RDMA_MEM_SRC_PITCH   (MDP_RDMA2_REG_BASE + 0x02C)
#define MDP_RDMA1_DISP_RDMA_FIFO_CON   (MDP_RDMA2_REG_BASE + 0x040)
#define MDP_RDMA1_DISP_RDMA_MEM_GMC_SETTING_0   (MDP_RDMA2_REG_BASE + 0x030)
#define MDP_RDMA1_DISP_RDMA_MEM_GMC_SETTING_1   (MDP_RDMA2_REG_BASE + 0x034)
#define MDP_RDMA1_DISP_RDMA_MEM_GMC_SETTING_2   (MDP_RDMA2_REG_BASE + 0x03C)
#define MDP_RDMA1_DISP_RDMA_STALL_CG_CON   (MDP_RDMA2_REG_BASE + 0x0B4)
#define MDP_RDMA1_DISP_RDMA_SRAM_SEL       (MDP_RDMA2_REG_BASE + 0x0B0)
#define MDP_RDMA1_DISP_RDMA_STATUS           (MDP_RDMA2_REG_BASE + 0x004)

#define MDP_HMS_REG_BASE                  (0x0)//(0x1B00D000)
#define MDP_HMS_EN                             (MDP_HMS_REG_BASE + 0x0)
#define MDP_HMS_INTEN	                       (MDP_HMS_REG_BASE + 0x8)
#define MDP_HMS_INTSTA                      (MDP_HMS_REG_BASE + 0x0C)
#define MDP_HMS_STATUS                     (MDP_HMS_REG_BASE + 0x10)
#define MDP_HMS_CFG_SIZE                  (MDP_HMS_REG_BASE + 0x020)
#define MDP_HMS_CFG_BLOCK               (MDP_HMS_REG_BASE + 0x024)
#define MDP_HMS_CFG_MASK                 (MDP_HMS_REG_BASE + 0x02C)
#define MDP_HMS_CFG_GRAY_COEF       (MDP_HMS_REG_BASE + 0x028)

#define MDP_WDMA1_REG_BASE     (0x0)//(0x1B006000)
#define MDP_WDMA0_WDMA_INTSTA                  (MDP_WDMA1_REG_BASE + 0x004)
#define MDP_WDMA0_WDMA_CFG                  (MDP_WDMA1_REG_BASE + 0x014)
#define MDP_WDMA0_WDMA_SRC_SIZE             (MDP_WDMA1_REG_BASE + 0x018)
#define MDP_WDMA0_WDMA_CLIP_SIZE            (MDP_WDMA1_REG_BASE + 0x01C)
#define MDP_WDMA0_WDMA_CLIP_COORD           (MDP_WDMA1_REG_BASE + 0x020)
#define MDP_WDMA0_WDMA_DST_W_IN_BYTE        (MDP_WDMA1_REG_BASE + 0x028)
#define MDP_WDMA0_WDMA_DST_ADDR0            (MDP_WDMA1_REG_BASE + 0xF00)
#define MDP_WDMA0_WDMA_DST_ADDR1            (MDP_WDMA1_REG_BASE + 0xF04)
#define MDP_WDMA0_WDMA_DST_ADDR2            (MDP_WDMA1_REG_BASE + 0xF08)
#define MDP_WDMA0_WDMA_INTEN                (MDP_WDMA1_REG_BASE + 0x0)
#define MDP_WDMA0_WDMA_EN                   (MDP_WDMA1_REG_BASE + 0x008)
#define MDP_WDMA0_WDMA_STATUS             (MDP_WDMA1_REG_BASE + 0x004)

#define MDP_MUTEX_REG_BASE       (0x0)//(0x1B001000)
#define MDP_MUTEX0_DISP_MUTEXN_INTSTA     (MDP_MUTEX_REG_BASE + 0x004)
#define MDP_MUTEX0_DISP_MUTEXN_CTL         (MDP_MUTEX_REG_BASE + 0x02C)
#define MDP_MUTEX0_DISP_MUTEXN_MOD0     (MDP_MUTEX_REG_BASE + 0x030)
#define MDP_MUTEX0_DISP_MUTEXN_MOD1     (MDP_MUTEX_REG_BASE + 0x034)
#define MDP_MUTEX0_DISP_MUTEXN_EN          (MDP_MUTEX_REG_BASE + 0x020)
#define MDP_MUTEX0_DISP_MUTEX5_EN		(MDP_MUTEX_REG_BASE + 0x0C0)


#define HMS_COLOR_FMT_UYVY		(0x40)
#define HMS_COLOR_FMT_VYUY		(0x140)
#define HMS_COLOR_FMT_YUYV		(0x50)
#define HMS_COLOR_FMT_YVYU		(0x150)
#define HMS_COLOR_FMT_RGB565		(0x0)
#define HMS_COLOR_FMT_BGR24		(0x110)
#define HMS_COLOR_FMT_RGB24		(0x10)

#define HMS_COLOR_MATRIX_YUV		(0x620000)

#endif /* __MTK_HMS_REG_H__ */
