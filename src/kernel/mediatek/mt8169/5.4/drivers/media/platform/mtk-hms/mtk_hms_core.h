/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Ice Huang <ice.huang@mediatek.com>
 */

#ifndef __MTK_HMS_CORE_H__
#define __MTK_HMS_CORE_H__

#include <linux/videodev2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#include "mtk_hms_comp.h"
#ifdef CONFIG_VB2_MEDIATEK_DMA_CONTIG
#include "mtk_dma_contig.h"
#endif

#define MTK_HMS_MAX_NUM_PLANE	1
#define MTK_HMS_MODULE_NAME		"mtk-hms"
#define MTK_HMS_MAX_CTRL_NUM		4
#define MTK_HMS_FMT_FLAG_OUTPUT		BIT(0)
#define MTK_HMS_FMT_FLAG_CAPTURE		BIT(1)
#define MTK_HMS_SRC_FMT			BIT(1)
#define MTK_HMS_DST_FMT			BIT(2)
#define MTK_HMS_CTX_ERROR		BIT(5)
#define MTK_HMS_EVENT_NR 13

/**
 * struct mtk_hms_fmt - the driver's internal color format data
 * @pixelformat: the fourcc code for this format, 0 if not applicable
 * @num_planes: number of physically non-contiguous data planes
 * @depth: per plane driver's private 'number of bits per pixel'
 * @row_depth: per plane driver's private 'number of bits per pixel per row'
 */
struct mtk_hms_fmt {
	u32	pixelformat;
	u16	num_planes;
	u8	depth[MTK_HMS_MAX_NUM_PLANE];
	u8	row_depth[MTK_HMS_MAX_NUM_PLANE];
	u32	flags;
};

/* struct mtk_hms_ctrls - the image processor control set
 * @ctrl_saturation_mask: value of saturation mask
 * @ctrl_under_exposed_mask:  value of under exposed mask
 * @ctrl_grayscale_rgb_coef: values of rgb coefficient for grayscale
 * @ctrl_block_size: value of block size, 1,2,4,8
 */
struct mtk_hms_ctrls {
	struct v4l2_ctrl *ctrl_saturation_mask;
	struct v4l2_ctrl *ctrl_under_exposed_mask;
	struct v4l2_ctrl *ctrl_grayscale_rgb_coef;
	struct v4l2_ctrl *ctrl_block_size;
};

/**
 * struct mtk_hms_frame - source/target frame properties
 * @width:	SRC : SRCIMG_WIDTH, DST : OUTPUTDMA_WHOLE_IMG_WIDTH
 * @height:	SRC : SRCIMG_HEIGHT, DST : OUTPUTDMA_WHOLE_IMG_HEIGHT
 * @payload:	image size in bytes (w x h x bpp)
 * @pitch:	bytes per line of image in memory
 * @addr:	image frame buffer physical addresses
 * @fmt:	color format pointer
 */
struct mtk_hms_frame {
	u32				width;
	u32				height;
	unsigned long			payload[MTK_HMS_MAX_NUM_PLANE];//sizeimage
	unsigned int			pitch[MTK_HMS_MAX_NUM_PLANE];//bytesperline
	dma_addr_t			addr[MTK_HMS_MAX_NUM_PLANE];
	const struct mtk_hms_fmt	*fmt;//must
};

/**
 * struct mtk_hms_dev - abstraction for image processor entity
 * @lock:	the mutex protecting this data structure
 * @pdev:	pointer to the image processor platform device
 * @id:		image processor device index
 * @irq:		interrupt request number
 * @comp:	HMS function components
 * @m2m_dev:	v4l2 memory-to-memory device data
 * @ctx_list:	list of struct mtk_hms_ctx
 * @vdev:	video device for image processor driver
 * @v4l2_dev:	V4L2 device to register video devices for.
 * @job_wq:	processor work queue
 * @ctx_num:	counter of active MTK HMS context
 * @id_counter:	An integer id given to the next opened context
 * @wdt_wq:	work queue for watchdog
 * @wdt_work:	worker for watchdog
 */
struct mtk_hms_dev {
	struct mutex			lock;
	spinlock_t		hw_lock;
	struct platform_device		*pdev;
	u16				id;
	u32				irq;
	struct mtk_hms_comp		*comp[MTK_HMS_COMP_ID_MAX];
	struct v4l2_m2m_dev		*m2m_dev;
	struct list_head		ctx_list;
	struct video_device		*vdev;
	struct v4l2_device		v4l2_dev;
	struct workqueue_struct		*job_wq;
	int				ctx_num;
	unsigned long			id_counter;
	struct workqueue_struct		*wdt_wq;
	struct work_struct		wdt_work;
};

/**
 * mtk_hms_ctx - the device context data
 * @list:		link to ctx_list of mtk_hms_dev
 * @s_frame:		source frame properties
 * @d_frame:		destination frame properties
 * @flags:		additional flags for image conversion
 * @state:		flags to keep track of user configuration
			Protected by slock
 * @id:			index of the context that this structure describes
 * @saturation_mask:  value of saturation mask
 * @under_exposed_mask:  value of unser exposed mask
 * @grayscale_rgb_coef:  value of coeffients of RGB for gray scale stt
 * @block_size:   value of block size
 * @hms_dev:		the image processor device this context applies to
 * @m2m_ctx:		memory-to-memory device context
 * @fh:			v4l2 file handle
 * @ctrl_handler:	v4l2 controls handler
 * @ctrls		image processor control set
 * @ctrls_rdy:		true if the control handler is initialized
 * @colorspace:		enum v4l2_colorspace; supplemental to pixelformat
 * @ycbcr_enc:		enum v4l2_ycbcr_encoding, Y'CbCr encoding
 * @xfer_func:		enum v4l2_xfer_func, colorspace transfer function
 * @quant:		enum v4l2_quantization, colorspace quantization
 * @slock:		the mutex protecting mtp_hms_ctx.state
 * @work:		worker for image processing
 */
struct mtk_hms_ctx {
	struct list_head		list;
	struct mtk_hms_frame		s_frame;
	struct mtk_hms_frame		d_frame;
	u32				flags;
	u32				state;
	int				id;
	u32				saturation_mask;
	u32				under_exposed_mask;
	u32				grayscale_rgb_coef;
	u32				block_size;
	struct mtk_hms_dev		*hms_dev;
	struct v4l2_m2m_ctx		*m2m_ctx;
	struct v4l2_fh			fh;
	struct v4l2_ctrl_handler	ctrl_handler;
	struct mtk_hms_ctrls		ctrls;
	bool				ctrls_rdy;
	enum v4l2_colorspace		colorspace;
	enum v4l2_ycbcr_encoding	ycbcr_enc;
	enum v4l2_xfer_func		xfer_func;
	enum v4l2_quantization		quant;
	struct mutex			slock;
	struct work_struct		work;
};

#endif /* __MTK_HMS_CORE_H__ */
