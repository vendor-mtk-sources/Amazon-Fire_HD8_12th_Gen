// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Ice Huang <ice.huang@mediatek.com>
 */

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <linux/dma-mapping.h>
#include "mtk_hms_core.h"
#include "mtk_hms_m2m.h"
#include "mtk_hms_regs.h"

static const struct mtk_hms_fmt mtk_hms_formats[] = {
	{
		.pixelformat	= V4L2_PIX_FMT_UYVY,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
//		.num_comp	= 2,
		.flags		= MTK_HMS_FMT_FLAG_OUTPUT,
	}, {

		.pixelformat	= V4L2_PIX_FMT_VYUY,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
//		.num_comp	= 2,
		.flags		= MTK_HMS_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
//		.num_comp	= 2,
		.flags		= MTK_HMS_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YVYU,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
//		.num_comp	= 2,
		.flags		= MTK_HMS_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGB565,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
//		.num_comp	= 2,
		.flags		= MTK_HMS_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_BGR24,
		.depth		= { 24 },
		.row_depth	= { 24 },
		.num_planes	= 1,
//		.num_comp	= 2,
		.flags		= MTK_HMS_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGB24,
		.depth		= { 24 },
		.row_depth	= { 24 },
		.num_planes	= 1,
//		.num_comp	= 2,
		.flags		= MTK_HMS_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_GREY,
		.depth		= { 8 },
		.row_depth	= { 8 },
		.num_planes	= 1,
//		.num_comp	= 2,
		.flags		= MTK_HMS_FMT_FLAG_CAPTURE,
	}
};

static const struct mtk_hms_fmt *mtk_hms_find_fmt(u32 pixelformat, u32 type)
{
	u32 i, flag;

	flag = V4L2_TYPE_IS_OUTPUT(type) ? MTK_HMS_FMT_FLAG_OUTPUT :
					   MTK_HMS_FMT_FLAG_CAPTURE;

	for (i = 0; i < ARRAY_SIZE(mtk_hms_formats); ++i) {
		if (!(mtk_hms_formats[i].flags & flag))
			continue;
		if (mtk_hms_formats[i].pixelformat == pixelformat)
			return &mtk_hms_formats[i];
	}
	return NULL;
}

static const struct mtk_hms_fmt *mtk_hms_find_fmt_by_index(u32 index, u32 type)
{
	u32 i, flag, num = 0;

	flag = V4L2_TYPE_IS_OUTPUT(type) ? MTK_HMS_FMT_FLAG_OUTPUT :
					   MTK_HMS_FMT_FLAG_CAPTURE;

	for (i = 0; i < ARRAY_SIZE(mtk_hms_formats); ++i) {
		if (!(mtk_hms_formats[i].flags & flag))
			continue;
		if (index == num)
			return &mtk_hms_formats[i];
		num++;
	}
	return NULL;
}

static const struct mtk_hms_fmt *mtk_hms_try_fmt(struct mtk_hms_ctx *ctx,
							struct v4l2_format *f)
{
	struct v4l2_pix_format *pix_fmt = &f->fmt.pix;
	const struct mtk_hms_fmt *fmt;
	u32 aligned_w, aligned_h;
	int i = 0;

	fmt = mtk_hms_find_fmt(pix_fmt->pixelformat, f->type);
	if (!fmt)
		fmt = mtk_hms_find_fmt_by_index(0, f->type);
	if (!fmt) {
		dev_dbg(&ctx->hms_dev->pdev->dev,
			"pixelformat format 0x%X invalid\n",
			pix_fmt->pixelformat);
		return NULL;
	}

	pr_info("field %d fmt 0x%X clr %d %d %d %d  sizeof %d fmt %c%c%c%c\n",
	pix_fmt->field,
	pix_fmt->pixelformat,
	pix_fmt->colorspace,
	pix_fmt->xfer_func,
	pix_fmt->ycbcr_enc,
	pix_fmt->quantization,
	sizeof(struct v4l2_pix_format),
	fmt->pixelformat & 0xff,
	(fmt->pixelformat >> 8) & 0xff,
	(fmt->pixelformat >> 16) & 0xff,
	(fmt->pixelformat >> 24) & 0xff);

	pix_fmt->field = V4L2_FIELD_NONE;
	pix_fmt->pixelformat = fmt->pixelformat;
	if (!V4L2_TYPE_IS_OUTPUT(f->type)) {
		pix_fmt->colorspace = ctx->colorspace;
		pix_fmt->xfer_func = ctx->xfer_func;
		pix_fmt->ycbcr_enc = ctx->ycbcr_enc;
		pix_fmt->quantization = ctx->quant;
	}

	aligned_w = pix_fmt->width;
	aligned_h = pix_fmt->height;

	//for (i = 0; i < pix_mp->num_planes; ++i)
	{
		int bpl = (aligned_w * fmt->row_depth[i]) / 8;
		int sizeimage = (aligned_w * aligned_h *
			fmt->depth[i]) / 8;

		if (pix_fmt->bytesperline < bpl)
			pix_fmt->bytesperline = bpl;
		if (pix_fmt->sizeimage < sizeimage)
			pix_fmt->sizeimage = sizeimage;
		pr_info("[%d] p%d, bpl:%u (%u), sizeimage:%u (%u)\n",
			ctx->id, i,
			pix_fmt->bytesperline, (u32)bpl,
			pix_fmt->sizeimage, (u32)sizeimage);
	}

	return fmt;
}

static struct mtk_hms_frame *mtk_hms_ctx_get_frame(struct mtk_hms_ctx *ctx,
					    enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &ctx->s_frame;
	return &ctx->d_frame;
}

static inline struct mtk_hms_ctx *fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_hms_ctx, fh);
}

static inline struct mtk_hms_ctx *ctrl_to_ctx(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct mtk_hms_ctx, ctrl_handler);
}

void mtk_hms_clear_int_status(struct mtk_hms_ctx *ctx)
{
	mtk_hms_hw_clear_int_status(ctx);
}

void mtk_hms_ctx_state_lock_set(struct mtk_hms_ctx *ctx, u32 state)
{
	mutex_lock(&ctx->slock);
	ctx->state |= state;
	pr_info("state:%d\n", state);
	mutex_unlock(&ctx->slock);
}

static void mtk_hms_ctx_state_lock_clear(struct mtk_hms_ctx *ctx, u32 state)
{
	mutex_lock(&ctx->slock);
	ctx->state &= ~state;
	pr_info("state:%d\n", state);
	mutex_unlock(&ctx->slock);
}

static bool mtk_hms_ctx_state_is_set(struct mtk_hms_ctx *ctx, u32 mask)
{
	bool ret;

	mutex_lock(&ctx->slock);
	ret = (ctx->state & mask) == mask;
	mutex_unlock(&ctx->slock);
	return ret;
}

static void mtk_hms_ctx_lock(struct vb2_queue *vq)
{
	struct mtk_hms_ctx *ctx = vb2_get_drv_priv(vq);

	mutex_lock(&ctx->hms_dev->lock);
}

static void mtk_hms_ctx_unlock(struct vb2_queue *vq)
{
	struct mtk_hms_ctx *ctx = vb2_get_drv_priv(vq);

	mutex_unlock(&ctx->hms_dev->lock);
}

static void mtk_hms_set_frame_size(struct mtk_hms_frame *frame, int width,
				   int height)
{
	frame->width = width;
	frame->height = height;
}

static int mtk_hms_m2m_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct mtk_hms_ctx *ctx = q->drv_priv;
	int ret;

	ret = pm_runtime_get_sync(&ctx->hms_dev->pdev->dev);
	if (ret < 0)
		pr_info("[%d] pm_runtime_get_sync failed:%d\n",
			    ctx->id, ret);
	return 0;
}

static void *mtk_hms_m2m_buf_remove(struct mtk_hms_ctx *ctx,
				    enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
	else
		return v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
}

static void mtk_hms_m2m_stop_streaming(struct vb2_queue *q)
{
	struct mtk_hms_ctx *ctx = q->drv_priv;
	struct vb2_buffer *vb;

	vb = mtk_hms_m2m_buf_remove(ctx, q->type);
	while (vb != NULL) {
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(vb), VB2_BUF_STATE_ERROR);
		vb = mtk_hms_m2m_buf_remove(ctx, q->type);
	}

	pm_runtime_put(&ctx->hms_dev->pdev->dev);
}

static void mtk_hms_m2m_job_abort(void *priv)
{
}

/* The color format (num_planes) must be already configured. */
static void mtk_hms_prepare_addr(struct mtk_hms_ctx *ctx,
				 struct vb2_buffer *vb,
				 struct mtk_hms_frame *frame)
{
	u32 pix_size, planes = 1, i;

	pix_size = frame->width * frame->height;
	//planes = 1;//min_t(u32, frame->fmt->num_planes, ARRAY_SIZE(frame->addr));
	for (i = 0; i < planes; i++) {
		{
			frame->addr[i] = vb2_dma_contig_plane_dma_addr(vb, i);
			if (vb->vb2_queue->memory == (u32)VB2_MEMORY_USERPTR)
				frame->addr[i] += 0;//vb->planes[i].data_offset;
		}
	}
	pr_info("[%d] planes:%d, size%u, pitch:%u addr:%p,\n",
		ctx->id, planes, pix_size, frame->pitch[0],  (void *)frame->addr[0]);
}

static void mtk_hms_m2m_get_bufs(struct mtk_hms_ctx *ctx)
{
	struct mtk_hms_frame *s_frame, *d_frame;
	struct vb2_buffer *src_vb, *dst_vb;
	struct vb2_v4l2_buffer *src_vbuf, *dst_vbuf;

	s_frame = &ctx->s_frame;
	d_frame = &ctx->d_frame;

	src_vbuf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	if (!src_vbuf) {
		pr_info("src_vbuf is null");
		return;
	}
	src_vb = &src_vbuf->vb2_buf;
	if (!src_vb) {
		pr_info("src_vb is null");
		return;
	}
	mtk_hms_prepare_addr(ctx, src_vb, s_frame);

	dst_vbuf = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	if (!dst_vbuf) {
		pr_info("dst_vbuf is null");
		return;
	}
	dst_vb = &dst_vbuf->vb2_buf;
	if (!dst_vb) {
		pr_info("dst_vb is null");
		return;
	}
	mtk_hms_prepare_addr(ctx, dst_vb, d_frame);

	src_vbuf = to_vb2_v4l2_buffer(src_vb);
	dst_vbuf = to_vb2_v4l2_buffer(dst_vb);
	dst_vbuf->vb2_buf.timestamp = src_vbuf->vb2_buf.timestamp;
}

void mtk_hms_process_done(void *priv, int vb_state)
{
	struct mtk_hms_dev *hms = priv;
	struct mtk_hms_ctx *ctx;
	//struct vb2_buffer *src_vb, *dst_vb;
	struct vb2_v4l2_buffer *src_vbuf = NULL, *dst_vbuf = NULL;

	ctx = v4l2_m2m_get_curr_priv(hms->m2m_dev);
	if (!ctx)
		return;

	src_vbuf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
	//src_vbuf = to_vb2_v4l2_buffer(src_vb);
	dst_vbuf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
	//dst_vbuf = to_vb2_v4l2_buffer(dst_vb);

	if (!src_vbuf) {
		pr_info("src_vbuf is null");
		return;
	}
	if (!dst_vbuf) {
		pr_info("dst_vbuf is null");
		return;
	}
	dst_vbuf->vb2_buf.timestamp = src_vbuf->vb2_buf.timestamp;
	dst_vbuf->timecode = src_vbuf->timecode;
	dst_vbuf->flags &= ~V4L2_BUF_FLAG_TSTAMP_SRC_MASK;
	dst_vbuf->flags |= src_vbuf->flags & V4L2_BUF_FLAG_TSTAMP_SRC_MASK;

	v4l2_m2m_buf_done(src_vbuf, vb_state);
	v4l2_m2m_buf_done(dst_vbuf, vb_state);
	v4l2_m2m_job_finish(ctx->hms_dev->m2m_dev, ctx->m2m_ctx);
}

static void mtk_hms_m2m_worker(struct work_struct *work)
{
	struct mtk_hms_ctx *ctx =
				container_of(work, struct mtk_hms_ctx, work);
	//struct mtk_hms_dev *hms = ctx->hms_dev;
	//enum vb2_buffer_state buf_state = VB2_BUF_STATE_ERROR;

	if (mtk_hms_ctx_state_is_set(ctx, MTK_HMS_CTX_ERROR))
		return;

	mtk_hms_m2m_get_bufs(ctx);

	mtk_hms_hw_set_input_addr(ctx);
	mtk_hms_hw_set_output_addr(ctx);

	mtk_hms_hw_set_in_size(ctx);
	mtk_hms_hw_set_in_image_format(ctx);

	mtk_hms_hw_set_out_size(ctx);
	mtk_hms_hw_set_out_image_format(ctx);

	mtk_hms_hw_set_saturation_mask(ctx);
	mtk_hms_hw_set_under_exposed_mask(ctx);
	mtk_hms_hw_set_grayscale_rgb_coef(ctx);
	mtk_hms_hw_set_block_size(ctx);

	mtk_hms_hw_start(ctx);

	//buf_state = VB2_BUF_STATE_DONE;

	//mtk_hms_process_done(hms, buf_state);
}

static void mtk_hms_m2m_device_run(void *priv)
{
	struct mtk_hms_ctx *ctx = priv;

	queue_work(ctx->hms_dev->job_wq, &ctx->work);
}

static int mtk_hms_m2m_queue_setup(struct vb2_queue *vq,
			unsigned int *num_buffers, unsigned int *num_planes,
			unsigned int sizes[], struct device *alloc_devs[])
{
	struct mtk_hms_ctx *ctx = vb2_get_drv_priv(vq);
	struct mtk_hms_frame *frame;
	int i, planes = 1;

	frame = mtk_hms_ctx_get_frame(ctx, vq->type);
	*num_planes = planes;
	for (i = 0; i < planes; i++)
		sizes[i] = frame->payload[i];
	pr_info("[%d] type:%d, planes:%d, buffers:%d, size:%u,%u\n",
		    ctx->id, vq->type, *num_planes, *num_buffers,
		    sizes[0], sizes[1]);
	return 0;
}

static int mtk_hms_m2m_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_hms_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_hms_frame *frame;
	int i, num_planes = 1;

	frame = mtk_hms_ctx_get_frame(ctx, vb->vb2_queue->type);

	if (!V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		for (i = 0; i < num_planes; i++)
			vb2_set_plane_payload(vb, i, frame->payload[i]);
	}

	return 0;
}

static void mtk_hms_m2m_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_hms_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);

	v4l2_m2m_buf_queue(ctx->m2m_ctx, to_vb2_v4l2_buffer(vb));
}

static const struct vb2_ops mtk_hms_m2m_qops = {
	.queue_setup	 = mtk_hms_m2m_queue_setup,
	.buf_prepare	 = mtk_hms_m2m_buf_prepare,
	.buf_queue	 = mtk_hms_m2m_buf_queue,
	.wait_prepare	 = mtk_hms_ctx_unlock,
	.wait_finish	 = mtk_hms_ctx_lock,
	.stop_streaming = mtk_hms_m2m_stop_streaming,
	.start_streaming = mtk_hms_m2m_start_streaming,
};

static int mtk_hms_m2m_querycap(struct file *file, void *fh,
				struct v4l2_capability *cap)
{
	cap->device_caps = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_M2M;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int mtk_hms_enum_fmt(struct v4l2_fmtdesc *f, u32 type)
{
	const struct mtk_hms_fmt *fmt;

	fmt = mtk_hms_find_fmt_by_index(f->index, type);
	if (!fmt)
		return -EINVAL;

	f->pixelformat = fmt->pixelformat;

	return 0;
}

static int mtk_hms_m2m_enum_fmt_vid_cap(struct file *file, void *priv,
				       struct v4l2_fmtdesc *f)
{
	return mtk_hms_enum_fmt(f, V4L2_BUF_TYPE_VIDEO_CAPTURE);
}

static int mtk_hms_m2m_enum_fmt_vid_out(struct file *file, void *priv,
				       struct v4l2_fmtdesc *f)
{
	return mtk_hms_enum_fmt(f, V4L2_BUF_TYPE_VIDEO_OUTPUT);
}

static int mtk_hms_m2m_g_fmt(struct file *file, void *fh,
				    struct v4l2_format *f)
{
	struct mtk_hms_ctx *ctx = fh_to_ctx(fh);
	struct mtk_hms_frame *frame;
	struct v4l2_pix_format *pix_fmt;
	int i = 0;

	pr_info("[%d] type:%d\n", ctx->id, f->type);

	frame = mtk_hms_ctx_get_frame(ctx, f->type);
	pix_fmt = &f->fmt.pix;

	pix_fmt->width = frame->width;
	pix_fmt->height = frame->height;
	pix_fmt->field = V4L2_FIELD_NONE;
	pix_fmt->pixelformat = frame->fmt->pixelformat;
	pix_fmt->colorspace = ctx->colorspace;
	pix_fmt->xfer_func = ctx->xfer_func;
	pix_fmt->ycbcr_enc = ctx->ycbcr_enc;
	pix_fmt->quantization = ctx->quant;
	pr_info("[%d] wxh:%dx%d\n", ctx->id,
		    pix_fmt->width, pix_fmt->height);

//	for (i = 0; i < pix_mp->num_planes; ++i)
	{
		pix_fmt->bytesperline = (frame->width *
			frame->fmt->row_depth[i]) / 8;
		pix_fmt->sizeimage = (frame->width *
			frame->height * frame->fmt->depth[i]) / 8;

		pr_info("[%d] p%d, bpl:%d, sizeimage:%d\n", ctx->id, i,
			    pix_fmt->bytesperline,
			    pix_fmt->sizeimage);
	}

	return 0;
}

static int mtk_hms_m2m_try_fmt(struct file *file, void *fh,
				      struct v4l2_format *f)
{
	struct mtk_hms_ctx *ctx = fh_to_ctx(fh);

	if (!mtk_hms_try_fmt(ctx, f))
		return -EINVAL;
	return 0;
}

static int mtk_hms_m2m_s_fmt(struct file *file, void *fh,
				    struct v4l2_format *f)
{
	struct mtk_hms_ctx *ctx = fh_to_ctx(fh);
	struct vb2_queue *vq;
	struct mtk_hms_frame *frame;
	struct v4l2_pix_format *pix_fmt;
	const struct mtk_hms_fmt *fmt;
	int i = 0;

	pr_info("[%d] type:%d\n", ctx->id, f->type);

	frame = mtk_hms_ctx_get_frame(ctx, f->type);
	fmt = mtk_hms_try_fmt(ctx, f);
	if (!fmt)
		return -EINVAL;

	frame->fmt = fmt;
	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);
	if (!vq) {
		pr_info("vq is null");
		return -EINVAL;
	}
	if (vb2_is_streaming(vq)) {
		dev_info(&ctx->hms_dev->pdev->dev, "queue %d busy", f->type);
		return -EBUSY;
	}

	pix_fmt = &f->fmt.pix;
//	for (i = 0; i < frame->fmt->num_planes; i++)
	{
		frame->payload[i] = pix_fmt->sizeimage;
		frame->pitch[i] = pix_fmt->bytesperline;
		pr_info("plane %d sizeimage %ld bytesperline %d\n",
			i, frame->payload[i], frame->pitch[i]);
	}

	mtk_hms_set_frame_size(frame, pix_fmt->width, pix_fmt->height);
	if (V4L2_TYPE_IS_OUTPUT(f->type)) {
		ctx->colorspace = pix_fmt->colorspace;
		ctx->xfer_func = pix_fmt->xfer_func;
		ctx->ycbcr_enc = pix_fmt->ycbcr_enc;
		ctx->quant = pix_fmt->quantization;
	}

	if (V4L2_TYPE_IS_OUTPUT(f->type))
		mtk_hms_ctx_state_lock_set(ctx, MTK_HMS_SRC_FMT);
	else
		mtk_hms_ctx_state_lock_set(ctx, MTK_HMS_DST_FMT);

	pr_info("[%d] type:%d, frame:%dx%d\n", ctx->id, f->type,
		    frame->width, frame->height);

	return 0;
}

static int mtk_hms_m2m_reqbufs(struct file *file, void *fh,
			       struct v4l2_requestbuffers *reqbufs)
{
	struct mtk_hms_ctx *ctx = fh_to_ctx(fh);

	pr_info("count %d type %d memory %d\n",
			reqbufs->count,
			reqbufs->type,
			reqbufs->memory);

	return v4l2_m2m_reqbufs(file, ctx->m2m_ctx, reqbufs);
}

static int mtk_hms_m2m_streamon(struct file *file, void *fh,
				enum v4l2_buf_type type)
{
	struct mtk_hms_ctx *ctx = fh_to_ctx(fh);

	/* The source and target color format need to be set */
	if (V4L2_TYPE_IS_OUTPUT(type)) {
		if (!mtk_hms_ctx_state_is_set(ctx, MTK_HMS_SRC_FMT))
			return -EINVAL;
	} else if (!mtk_hms_ctx_state_is_set(ctx, MTK_HMS_DST_FMT)) {
		return -EINVAL;
	}

	return v4l2_m2m_streamon(file, ctx->m2m_ctx, type);
}

static int mtk_hms_m2m_streamoff(struct file *file, void *fh,
				 enum v4l2_buf_type type)
{
	struct mtk_hms_ctx *ctx = fh_to_ctx(fh);

	pr_info("type %d output %d\n", type, V4L2_TYPE_IS_OUTPUT(type));

	if (V4L2_TYPE_IS_OUTPUT(type))
		mtk_hms_ctx_state_lock_clear(ctx, MTK_HMS_SRC_FMT);
	else
		mtk_hms_ctx_state_lock_clear(ctx, MTK_HMS_DST_FMT);

	return v4l2_m2m_streamoff(file, ctx->m2m_ctx, type);
}

static const struct v4l2_ioctl_ops mtk_hms_m2m_ioctl_ops = {
	.vidioc_querycap		= mtk_hms_m2m_querycap,
	.vidioc_enum_fmt_vid_cap	= mtk_hms_m2m_enum_fmt_vid_cap,
	.vidioc_enum_fmt_vid_out	= mtk_hms_m2m_enum_fmt_vid_out,
	.vidioc_g_fmt_vid_cap_mplane	= mtk_hms_m2m_g_fmt,
	.vidioc_g_fmt_vid_cap	= mtk_hms_m2m_g_fmt,
	.vidioc_g_fmt_vid_out_mplane	= mtk_hms_m2m_g_fmt,
	.vidioc_g_fmt_vid_out	= mtk_hms_m2m_g_fmt,
	.vidioc_try_fmt_vid_cap_mplane	= mtk_hms_m2m_try_fmt,
	.vidioc_try_fmt_vid_cap	= mtk_hms_m2m_try_fmt,
	.vidioc_try_fmt_vid_out_mplane	= mtk_hms_m2m_try_fmt,
	.vidioc_try_fmt_vid_out	= mtk_hms_m2m_try_fmt,
	.vidioc_s_fmt_vid_cap_mplane	= mtk_hms_m2m_s_fmt,
	.vidioc_s_fmt_vid_cap	= mtk_hms_m2m_s_fmt,
	.vidioc_s_fmt_vid_out_mplane	= mtk_hms_m2m_s_fmt,
	.vidioc_s_fmt_vid_out	= mtk_hms_m2m_s_fmt,
	.vidioc_reqbufs			= mtk_hms_m2m_reqbufs,
	.vidioc_create_bufs		= v4l2_m2m_ioctl_create_bufs,
	.vidioc_expbuf			= v4l2_m2m_ioctl_expbuf,
	.vidioc_subscribe_event		= v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,
	.vidioc_querybuf		= v4l2_m2m_ioctl_querybuf,
	.vidioc_qbuf			= v4l2_m2m_ioctl_qbuf,
	.vidioc_dqbuf			= v4l2_m2m_ioctl_dqbuf,
	.vidioc_streamon		= mtk_hms_m2m_streamon,
	.vidioc_streamoff		= mtk_hms_m2m_streamoff,
};

static int mtk_hms_m2m_queue_init(void *priv, struct vb2_queue *src_vq,
				  struct vb2_queue *dst_vq)
{
	struct mtk_hms_ctx *ctx = priv;
	int ret;

	memset(src_vq, 0, sizeof(*src_vq));
	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	src_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	src_vq->drv_priv = ctx;
	src_vq->ops = &mtk_hms_m2m_qops;
	src_vq->mem_ops = &vb2_dma_contig_memops;
	src_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->dev = &ctx->hms_dev->pdev->dev;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	memset(dst_vq, 0, sizeof(*dst_vq));
	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	dst_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	dst_vq->drv_priv = ctx;
	dst_vq->ops = &mtk_hms_m2m_qops;
	dst_vq->mem_ops = &vb2_dma_contig_memops;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->dev = &ctx->hms_dev->pdev->dev;

	return vb2_queue_init(dst_vq);
}

static int mtk_hms_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_hms_ctx *ctx = ctrl_to_ctx(ctrl);

	if (ctrl->flags & V4L2_CTRL_FLAG_INACTIVE)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_SATURATION:
		ctx->saturation_mask = (u32)ctrl->val;
		break;
	case V4L2_CID_EXPOSURE:
		ctx->under_exposed_mask = (u32)ctrl->val;
		break;
	case V4L2_CID_BG_COLOR:
		ctx->grayscale_rgb_coef = (u32)ctrl->val;
		break;
	case V4L2_CID_GAIN:
		ctx->block_size = (u32)ctrl->val;
		break;
	}
	return 0;
}

static int mtk_hms_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_hms_ctx *ctx = ctrl_to_ctx(ctrl);

	if (ctrl->flags & V4L2_CTRL_FLAG_INACTIVE)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_SATURATION:
		ctrl->val = ctx->saturation_mask;
		break;
	case V4L2_CID_EXPOSURE:
		ctrl->val = ctx->under_exposed_mask;
		break;
	case V4L2_CID_BG_COLOR:
		ctrl->val = ctx->grayscale_rgb_coef;
		break;
	case V4L2_CID_GAIN:
		ctrl->val = ctx->block_size;
		break;
	}
	return 0;
}

static const struct v4l2_ctrl_ops mtk_hms_ctrl_ops = {
	.s_ctrl = mtk_hms_s_ctrl,
	.g_volatile_ctrl = mtk_hms_g_ctrl,
};

static int mtk_hms_ctrls_create(struct mtk_hms_ctx *ctx)
{
	int err = 0;

	v4l2_ctrl_handler_init(&ctx->ctrl_handler, MTK_HMS_MAX_CTRL_NUM);

	ctx->ctrls.ctrl_saturation_mask = v4l2_ctrl_new_std(&ctx->ctrl_handler,
						    &mtk_hms_ctrl_ops,
						    V4L2_CID_SATURATION,
						    0, 255, 1, 240);
	ctx->ctrls.ctrl_under_exposed_mask = v4l2_ctrl_new_std(&ctx->ctrl_handler,
						    &mtk_hms_ctrl_ops,
						    V4L2_CID_EXPOSURE,
						    0, 255, 1, 10);
	ctx->ctrls.ctrl_grayscale_rgb_coef = v4l2_ctrl_new_std(&ctx->ctrl_handler,
						    &mtk_hms_ctrl_ops,
						    V4L2_CID_BG_COLOR,
						    0, 0xffffff, 1, 0x4A854);
	ctx->ctrls.ctrl_block_size = v4l2_ctrl_new_std(&ctx->ctrl_handler,
						    &mtk_hms_ctrl_ops,
						    V4L2_CID_GAIN,
						    1, 8, 1, 4);

	ctx->ctrls_rdy = ctx->ctrl_handler.error == 0;

	if (ctx->ctrl_handler.error) {
		err = ctx->ctrl_handler.error;
		v4l2_ctrl_handler_free(&ctx->ctrl_handler);
		return err;
	}
	return 0;
}

static void mtk_hms_set_default_params(struct mtk_hms_ctx *ctx)
{
	struct mtk_hms_frame *frame;

	frame = mtk_hms_ctx_get_frame(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	frame->fmt = mtk_hms_find_fmt_by_index(0,
					V4L2_BUF_TYPE_VIDEO_OUTPUT);
	frame->width = 320;
	frame->height = 240;
	frame->payload[0] = frame->width * frame->height;

	frame = mtk_hms_ctx_get_frame(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	frame->fmt = mtk_hms_find_fmt_by_index(0,
					V4L2_BUF_TYPE_VIDEO_CAPTURE);
	frame->width = 160;
	frame->height = 60;
	frame->payload[0] = frame->width * frame->height;
}

static int mtk_hms_m2m_open(struct file *file)
{
	struct mtk_hms_dev *hms = video_drvdata(file);
	struct video_device *vfd = video_devdata(file);
	struct mtk_hms_ctx *ctx = NULL;
	int ret;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	if (mutex_lock_interruptible(&hms->lock)) {
		ret = -ERESTARTSYS;
		goto err_lock;
	}

	mutex_init(&ctx->slock);
	ctx->id = hms->id_counter++;
	v4l2_fh_init(&ctx->fh, vfd);
	file->private_data = &ctx->fh;
	ret = mtk_hms_ctrls_create(ctx);
	if (ret)
		goto error_ctrls;

	/* Use separate control handler per file handle */
	ctx->fh.ctrl_handler = &ctx->ctrl_handler;
	v4l2_fh_add(&ctx->fh);
	INIT_LIST_HEAD(&ctx->list);

	ctx->hms_dev = hms;
	mtk_hms_set_default_params(ctx);

	INIT_WORK(&ctx->work, mtk_hms_m2m_worker);
	ctx->m2m_ctx = v4l2_m2m_ctx_init(hms->m2m_dev, ctx,
					 mtk_hms_m2m_queue_init);
	if (IS_ERR(ctx->m2m_ctx)) {
		ret = PTR_ERR(ctx->m2m_ctx);
		goto error_m2m_ctx;
	}
	ctx->fh.m2m_ctx = ctx->m2m_ctx;

	list_add(&ctx->list, &hms->ctx_list);
	mutex_unlock(&hms->lock);

	pr_info("%s [%d]\n", dev_name(&hms->pdev->dev), ctx->id);

	return 0;

error_m2m_ctx:
	v4l2_ctrl_handler_free(&ctx->ctrl_handler);
error_ctrls:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	mutex_unlock(&hms->lock);
err_lock:
	kfree(ctx);

	return ret;
}

static int mtk_hms_m2m_release(struct file *file)
{
	struct mtk_hms_ctx *ctx = fh_to_ctx(file->private_data);
	struct mtk_hms_dev *hms = ctx->hms_dev;

	flush_workqueue(hms->job_wq);
	mutex_lock(&hms->lock);
	v4l2_m2m_ctx_release(ctx->m2m_ctx);
	v4l2_ctrl_handler_free(&ctx->ctrl_handler);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	hms->ctx_num--;
	list_del_init(&ctx->list);

	pr_info("%s [%d] [%p]\n", dev_name(&hms->pdev->dev), ctx->id);

	mutex_unlock(&hms->lock);
	kfree(ctx);

	return 0;
}

static const struct v4l2_file_operations mtk_hms_m2m_fops = {
	.owner		= THIS_MODULE,
	.open		= mtk_hms_m2m_open,
	.release	= mtk_hms_m2m_release,
	.poll		= v4l2_m2m_fop_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= v4l2_m2m_fop_mmap,
};

static const struct v4l2_m2m_ops mtk_hms_m2m_ops = {
	.device_run	= mtk_hms_m2m_device_run,
	.job_abort		= mtk_hms_m2m_job_abort,
};

int mtk_hms_register_m2m_device(struct mtk_hms_dev *hms)
{
	//struct device *dev = &hms->pdev->dev;
	int ret = 0;

	hms->vdev = video_device_alloc();
	if (!hms->vdev) {
		ret = -ENOMEM;
		goto err_video_alloc;
	}
	hms->vdev->device_caps = V4L2_CAP_VIDEO_M2M | V4L2_CAP_STREAMING;
	hms->vdev->fops = &mtk_hms_m2m_fops;
	hms->vdev->ioctl_ops = &mtk_hms_m2m_ioctl_ops;
	hms->vdev->release = video_device_release;
	hms->vdev->lock = &hms->lock;
	hms->vdev->vfl_dir = VFL_DIR_M2M;
	hms->vdev->v4l2_dev = &hms->v4l2_dev;
	if (snprintf(hms->vdev->name, sizeof(hms->vdev->name), "%s:m2m",
		 MTK_HMS_MODULE_NAME) < 0)
		pr_info("snprintf error");
	video_set_drvdata(hms->vdev, hms);

	hms->m2m_dev = v4l2_m2m_init(&mtk_hms_m2m_ops);
	if (IS_ERR(hms->m2m_dev)) {
		ret = PTR_ERR(hms->m2m_dev);
		goto err_m2m_init;
	}

	ret = video_register_device(hms->vdev, VFL_TYPE_GRABBER, 2);
	if (ret)
		goto err_vdev_register;

	v4l2_info(&hms->v4l2_dev, "driver registered as /dev/video%d",
		  hms->vdev->num);
	return 0;

err_vdev_register:
	v4l2_m2m_release(hms->m2m_dev);
err_m2m_init:
	video_device_release(hms->vdev);
err_video_alloc:

	return ret;
}

void mtk_hms_unregister_m2m_device(struct mtk_hms_dev *hms)
{
	video_unregister_device(hms->vdev);
	v4l2_m2m_release(hms->m2m_dev);
}

