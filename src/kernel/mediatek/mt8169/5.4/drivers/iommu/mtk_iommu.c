// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Yong Wu <yong.wu@mediatek.com>
 */
#include <linux/bitfield.h>
#include <linux/bug.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/device.h>
#include <linux/dma-iommu.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/iopoll.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_iommu.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <asm/barrier.h>
#ifndef CONFIG_ARM64
#include <asm/dma-iommu.h>
#endif
#include <soc/mediatek/smi.h>

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
#include <../misc/mediatek/iommu/iommu_debug.h>
#endif

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SEC)
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include "mtk_iommu_sec.h"
#endif

#include "mtk_iommu.h"

#define REG_MMU_PT_BASE_ADDR			0x000
#define MMU_PT_ADDR_MASK			GENMASK(31, 7)

#define REG_MMU_INVALIDATE			0x020
#define F_ALL_INVLD				0x2
#define F_MMU_INV_RANGE				0x1

#define REG_MMU_INVLD_START_A			0x024
#define REG_MMU_INVLD_END_A			0x028

#define REG_MMU_INV_SEL_GEN2			0x02c
#define REG_MMU_INV_SEL_GEN1			0x038
#define F_INVLD_EN0				BIT(0)
#define F_INVLD_EN1				BIT(1)

#define REG_MMU_MISC_CTRL			0x048
#define F_MMU_STANDARD_AXI_MODE_BIT		(BIT(3) | BIT(19))

#define REG_MMU_DCM_DIS				0x050
#define REG_MMU_WR_LEN				0x054
#define F_MMU_WR_THROT_DIS_BIT			(BIT(5) |  BIT(21))

#define REG_MMU_CTRL_REG			0x110
#define F_MMU_TF_PROT_TO_PROGRAM_ADDR		(2 << 4)
#define F_MMU_PREFETCH_RT_REPLACE_MOD		BIT(4)
#define F_MMU_TF_PROT_TO_PROGRAM_ADDR_MT8173	(2 << 5)

#define REG_MMU_IVRP_PADDR			0x114

#define REG_MMU_VLD_PA_RNG			0x118
#define F_MMU_VLD_PA_RNG(EA, SA)		(((EA) << 8) | (SA))

#define REG_MMU_INT_CONTROL0			0x120
#define F_L2_MULIT_HIT_EN			BIT(0)
#define F_TABLE_WALK_FAULT_INT_EN		BIT(1)
#define F_PREETCH_FIFO_OVERFLOW_INT_EN		BIT(2)
#define F_MISS_FIFO_OVERFLOW_INT_EN		BIT(3)
#define F_PREFETCH_FIFO_ERR_INT_EN		BIT(5)
#define F_MISS_FIFO_ERR_INT_EN			BIT(6)
#define F_INT_CLR_BIT				BIT(12)

#define REG_MMU_INT_MAIN_CONTROL		0x124
						/* mmu0 | mmu1 */
#define F_INT_TRANSLATION_FAULT			(BIT(0) | BIT(7))
#define F_INT_MAIN_MULTI_HIT_FAULT		(BIT(1) | BIT(8))
#define F_INT_INVALID_PA_FAULT			(BIT(2) | BIT(9))
#define F_INT_ENTRY_REPLACEMENT_FAULT		(BIT(3) | BIT(10))
#define F_INT_TLB_MISS_FAULT			(BIT(4) | BIT(11))
#define F_INT_MISS_TRANSACTION_FIFO_FAULT	(BIT(5) | BIT(12))
#define F_INT_PRETETCH_TRANSATION_FIFO_FAULT	(BIT(6) | BIT(13))

#define REG_MMU_CPE_DONE			0x12C

#define REG_MMU_FAULT_ST1			0x134
#define F_REG_MMU0_FAULT_MASK			GENMASK(6, 0)
#define F_REG_MMU1_FAULT_MASK			GENMASK(13, 7)

#define REG_MMU0_FAULT_VA			0x13c
#define F_MMU_INVAL_VA_31_12_MASK		GENMASK(31, 12)
#define F_MMU_INVAL_VA_34_32_MASK		GENMASK(11, 9)
#define F_MMU_INVAL_PA_34_32_MASK		GENMASK(8, 6)
#define F_MMU_FAULT_VA_WRITE_BIT		BIT(1)
#define F_MMU_FAULT_VA_LAYER_BIT		BIT(0)

#define REG_MMU0_INVLD_PA			0x140
#define REG_MMU1_FAULT_VA			0x144
#define REG_MMU1_INVLD_PA			0x148
#define REG_MMU0_INT_ID				0x150
#define REG_MMU1_INT_ID				0x154
#define F_MMU_INT_ID_COMM_ID(a)			(((a) >> 9) & 0x7)
#define F_MMU_INT_ID_SUB_COMM_ID(a)		(((a) >> 7) & 0x3)
#define F_MMU_INT_ID_LARB_ID(a)			(((a) >> 7) & 0x7)
#define F_MMU_INT_ID_PORT_ID(a)			(((a) >> 2) & 0x1f)

#define MTK_PROTECT_PA_ALIGN			256

struct mtk_iommu_domain {
	struct io_pgtable_cfg		cfg;
	struct io_pgtable_ops		*iop;

	struct mtk_iommu_data		*data;
	struct iommu_domain		domain;
};

static const struct iommu_ops mtk_iommu_ops;

/*
 * In M4U 4GB mode, the physical address is remapped as below:
 *
 * CPU Physical address:
 * ====================
 *
 * 0      1G       2G     3G       4G     5G
 * |---A---|---B---|---C---|---D---|---E---|
 * +--I/O--+------------Memory-------------+
 *
 * IOMMU output physical address:
 *  =============================
 *
 *                                 4G      5G     6G      7G      8G
 *                                 |---E---|---B---|---C---|---D---|
 *                                 +------------Memory-------------+
 *
 * The Region 'A'(I/O) can NOT be mapped by M4U; For Region 'B'/'C'/'D', the
 * bit32 of the CPU physical address always is needed to set, and for Region
 * 'E', the CPU physical address keep as is.
 * Additionally, The iommu consumers always use the CPU phyiscal address.
 */
#define MTK_IOMMU_4GB_MODE_REMAP_BASE	 0x140000000UL

static LIST_HEAD(m4ulist);	/* List all the M4U HWs */
static LIST_HEAD(iommu_vpulist);	/* List the iommu_vpu HW */
static LIST_HEAD(iommu_infralist);	/* List the iommu_infra HW */

#define for_each_m4u(data, head)  list_for_each_entry(data, head, list)

#define mtk_iommu_is_infra(d)	((d)->plat_data->type == MTK_IOMMU_INFRA)
#define mtk_iommu_is_apu(d)	((d)->plat_data->type == MTK_IOMMU_APU)
#define mtk_iommu_is_mm(d)	((d)->plat_data->type == MTK_IOMMU_MM)

/* Only for domain_alloc. */
static struct mtk_iommu_data *tmp_domain_alloc_data;

struct mtk_iommu_resv_iova_region {
	const char	*device_comp;
	dma_addr_t	iova_base;
	size_t		iova_size;
	enum iommu_resv_type type;
	void (*get_resv_data)(dma_addr_t *base, size_t *size,
			      struct mtk_iommu_data *data);
};

struct mtk_iommu_iova_region {
	dma_addr_t		iova_base;
	size_t			size;
	enum iommu_resv_type	type;
};

#ifdef CONFIG_ARM64
#define IMU_SZ_4G		SZ_4G
#else
#define IMU_SZ_4G		DMA_BIT_MASK(32)	/* 4G - 1 */
#endif

static const struct mtk_iommu_iova_region single_domain[] = {
	{.iova_base = 0, .size = IMU_SZ_4G},
};

static const struct mtk_iommu_iova_region mt6873_multi_dom[] = {
	{ .iova_base = 0x0, .size = IMU_SZ_4G},	      /* disp : 0 ~ 4G */
#ifdef CONFIG_ARM64
	{ .iova_base = SZ_4G, .size = SZ_4G},     /* vdec : 4G ~ 8G */
	{ .iova_base = SZ_4G * 2, .size = SZ_4G}, /* CAM/MDP: 8G ~ 12G */
	{ .iova_base = 0x240000000ULL, .size = 0x4000000}, /* CCU0 */
	{ .iova_base = 0x244000000ULL, .size = 0x4000000}, /* CCU1 */
	{ .iova_base = SZ_4G * 3, .size = SZ_4G}, /* APU DATA */
	{ .iova_base = 0x304000000ULL, .size = 0x4000000}, /* APU VLM */
	{ .iova_base = 0x310000000ULL, .size = 0x10000000}, /* APU VPU */
	{ .iova_base = 0x370000000ULL, .size = 0x12600000}, /* APU REG */
#endif
};

static struct mtk_iommu_domain *to_mtk_domain(struct iommu_domain *dom)
{
	return container_of(dom, struct mtk_iommu_domain, domain);
}

static int mtk_iommu_rpm_get(struct device *dev)
{
	if (pm_runtime_enabled(dev))
		return pm_runtime_get_sync(dev);
	return 0;
}

static void mtk_iommu_rpm_put(struct device *dev)
{
	if (pm_runtime_enabled(dev))
		pm_runtime_put_sync(dev);
}

static int mtk_iommu_clk_enable(struct mtk_iommu_data *data)
{
	int ret;

	ret = clk_prepare_enable(data->clk_rsi);
	if (ret) {
		dev_err(data->dev, "Failed to enable clk_rsi(%d)\n", ret);
		return ret;
	}
	ret = clk_prepare_enable(data->bclk);
	if (ret) {
		dev_err(data->dev, "Failed to enable iommu bclk(%d)\n", ret);
		clk_disable_unprepare(data->clk_rsi);
	}

	return ret;
}

static void mtk_iommu_clk_disable(struct mtk_iommu_data *data)
{
	clk_disable_unprepare(data->bclk);
	clk_disable_unprepare(data->clk_rsi);
}

static void mtk_iommu_tlb_flush_all(void *cookie)
{
	struct mtk_iommu_data *data = cookie;
	struct list_head *head = data->plat_data->hw_list;

	for_each_m4u(data, head) {
		if (data->dev->pm_domain)
			pm_runtime_get_sync(data->dev);

		writel_relaxed(F_INVLD_EN1 | F_INVLD_EN0,
			       data->base + data->plat_data->inv_sel_reg);
		writel_relaxed(F_ALL_INVLD, data->base + REG_MMU_INVALIDATE);
		wmb(); /* Make sure the tlb flush all done */

		if (data->dev->pm_domain)
			pm_runtime_put_sync(data->dev);
	}
}

static void mtk_iommu_tlb_flush_range_sync(unsigned long iova, size_t size,
					   size_t granule, void *cookie)
{
	struct mtk_iommu_data *data = cookie;
	struct list_head *head = data->plat_data->hw_list;
	unsigned long flags, end;
	int ret;
	u32 tmp;

	for_each_m4u(data, head) {
		if (data->dev->pm_domain)
			pm_runtime_get_sync(data->dev);

		spin_lock_irqsave(&data->tlb_lock, flags);
		writel_relaxed(F_INVLD_EN1 | F_INVLD_EN0,
			       data->base + data->plat_data->inv_sel_reg);

		tmp = lower_32_bits(iova) | upper_32_bits(iova);
		writel_relaxed(tmp, data->base + REG_MMU_INVLD_START_A);
		end = iova + size - 1;
		tmp = (end & GENMASK(31, 12)) | upper_32_bits(end);
		writel_relaxed(tmp, data->base + REG_MMU_INVLD_END_A);
		writel_relaxed(F_MMU_INV_RANGE,
			       data->base + REG_MMU_INVALIDATE);

		/* tlb sync */
		ret = readl_poll_timeout_atomic(data->base + REG_MMU_CPE_DONE,
						tmp, tmp != 0, 10, 1000);
		if (ret) {
			dev_warn(data->dev,
				 "Partial TLB flush timed out, falling back to full flush\n");
			mtk_iommu_tlb_flush_all(cookie);
		}
		/* Clear the CPE status */
		writel_relaxed(0, data->base + REG_MMU_CPE_DONE);
		spin_unlock_irqrestore(&data->tlb_lock, flags);

		if (data->dev->pm_domain)
			pm_runtime_put_sync(data->dev);
	}
}

static void mtk_iommu_tlb_flush_page_nosync(struct iommu_iotlb_gather *gather,
					    unsigned long iova, size_t granule,
					    void *cookie)
{
	struct mtk_iommu_data *data = cookie;
	struct iommu_domain *domain = &data->m4u_dom->domain;

	iommu_iotlb_gather_add_page(domain, gather, iova, granule);
}

static const struct iommu_flush_ops mtk_iommu_flush_ops = {
	.tlb_flush_all = mtk_iommu_tlb_flush_all,
	.tlb_flush_walk = mtk_iommu_tlb_flush_range_sync,
	.tlb_flush_leaf = mtk_iommu_tlb_flush_range_sync,
	.tlb_add_page = mtk_iommu_tlb_flush_page_nosync,
};

/* Add for iommu security bank to parse the larb/portid */
int mtk_iommu_get_real_larb_portid(struct device *dev, u32 int_id,
				   int *larb_id, int *port_id)
{
	struct mtk_iommu_data *data;
	unsigned int sub_comm = 0, larb_tmp;

	if (!dev || !larb_id || !port_id)
		return -EINVAL;

	data = dev_get_drvdata(dev);
	if (!data)
		return -ENODEV;

	larb_tmp = F_MMU_INT_ID_LARB_ID(int_id);
	if (data->plat_data->has_sub_comm) {
		larb_tmp = F_MMU_INT_ID_COMM_ID(int_id);
		sub_comm = F_MMU_INT_ID_SUB_COMM_ID(int_id);
	}

	*larb_id = data->plat_data->larbid_remap[larb_tmp][sub_comm];
	*port_id = F_MMU_INT_ID_PORT_ID(int_id);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_iommu_get_real_larb_portid);

static phys_addr_t mtk_iommu_iova_to_phys(struct iommu_domain *domain,
					  dma_addr_t iova);
static irqreturn_t mtk_iommu_isr(int irq, void *dev_id)
{
	struct mtk_iommu_data *data = dev_id;
	struct mtk_iommu_domain *dom = data->m4u_dom;
	int fault_larb = -1, fault_port = -1;
	u32 int_state, regval, va34_32, pa34_32;
	u64 fault_iova, fault_pa;
	bool layer, write;
	phys_addr_t fault_pa_sw;
	unsigned long flags;
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
	int i;
	u64 tf_iova_tmp;
	phys_addr_t fault_pgpa;
	#define TF_IOVA_DUMP_NUM	5
#endif

	/* Read error info from registers */
	int_state = readl_relaxed(data->base + REG_MMU_FAULT_ST1);
	if (int_state & F_REG_MMU0_FAULT_MASK) {
		regval = readl_relaxed(data->base + REG_MMU0_INT_ID);
		fault_iova = readl_relaxed(data->base + REG_MMU0_FAULT_VA);
		fault_pa = readl_relaxed(data->base + REG_MMU0_INVLD_PA);
	} else {
		regval = readl_relaxed(data->base + REG_MMU1_INT_ID);
		fault_iova = readl_relaxed(data->base + REG_MMU1_FAULT_VA);
		fault_pa = readl_relaxed(data->base + REG_MMU1_INVLD_PA);
	}
	layer = fault_iova & F_MMU_FAULT_VA_LAYER_BIT;
	write = fault_iova & F_MMU_FAULT_VA_WRITE_BIT;
	if (data->plat_data->iova_34_en) {
		va34_32 = FIELD_GET(F_MMU_INVAL_VA_34_32_MASK, fault_iova);
		pa34_32 = FIELD_GET(F_MMU_INVAL_PA_34_32_MASK, fault_iova);
		fault_iova = fault_iova & F_MMU_INVAL_VA_31_12_MASK;
		fault_iova |=  (u64)va34_32 << 32;
		fault_pa |= (u64)pa34_32 << 32;
	}

	mtk_iommu_get_real_larb_portid(data->dev, regval,
				       &fault_larb, &fault_port);

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
	for (i = 0, tf_iova_tmp = fault_iova; i < TF_IOVA_DUMP_NUM; i++) {
		if (i > 0)
			tf_iova_tmp -= SZ_4K;
		fault_pgpa = mtk_iommu_iova_to_phys(&data->m4u_dom->domain, tf_iova_tmp);
		pr_info("[iommu_debug] %s error, index:%d, falut_iova:0x%llx, fault_pa(pg):%pa\n",
			data->plat_data->is_apu ? "apu_iommu" : "mm_iommu",
			i, tf_iova_tmp, &fault_pgpa);
		if (!fault_pgpa && i > 0)
			break;
	}
	if (fault_iova) /* skip dump when fault iova = 0 */
		mtk_iova_map_dump(fault_iova);
	report_custom_iommu_fault(fault_iova, fault_pa, regval,
				  data->plat_data->is_apu ? true : false);
#endif
	if (report_iommu_fault(&dom->domain, data->dev, fault_iova,
			       write ? IOMMU_FAULT_WRITE : IOMMU_FAULT_READ)) {
		fault_pa_sw = iommu_iova_to_phys(&dom->domain, fault_iova);
		dev_err_ratelimited(
			data->dev,
			"fault type=0x%x iova=0x%llx pa=0x%llx(0x%pa) larb=%d port=%d(0x%x) layer=%d %s\n",
			int_state, fault_iova, fault_pa, &fault_pa_sw,
			fault_larb, fault_port,
			regval, layer, write ? "write" : "read");
	}

	/* Interrupt clear */
	regval = readl_relaxed(data->base + REG_MMU_INT_CONTROL0);
	regval |= F_INT_CLR_BIT;
	writel_relaxed(regval, data->base + REG_MMU_INT_CONTROL0);

	/* flush TLB */
	spin_lock_irqsave(&data->tlb_lock, flags);
	writel_relaxed(F_INVLD_EN1 | F_INVLD_EN0,
		       data->base + data->plat_data->inv_sel_reg);
	writel_relaxed(F_ALL_INVLD, data->base + REG_MMU_INVALIDATE);
	wmb(); /* Make sure the tlb flush all done */
	spin_unlock_irqrestore(&data->tlb_lock, flags);

	return IRQ_HANDLED;
}

static void mtk_iommu_config(struct mtk_iommu_data *data,
			     struct device *dev, bool enable)
{
	struct mtk_smi_larb_iommu    *larb_mmu;
	unsigned int                 larbid, portid, domid;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	const struct mtk_iommu_iova_region *region;
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SEC)
	struct arm_smccc_res res;
#endif
	int i;

	for (i = 0; i < fwspec->num_ids; ++i) {
		larbid = MTK_M4U_TO_LARB(fwspec->ids[i]);
		portid = MTK_M4U_TO_PORT(fwspec->ids[i]);

		if (mtk_iommu_is_mm(data)) {
			domid = MTK_M4U_TO_DOM(fwspec->ids[i]);

			larb_mmu = &data->larb_imu[larbid];
			region = data->plat_data->iova_region + domid;
			larb_mmu->bank[portid] =
				upper_32_bits(region->iova_base);

			dev_dbg(dev, "%s iommu port: %d\n",
				enable ? "enable" : "disable", portid);

			if (enable)
				larb_mmu->mmu |= MTK_SMI_MMU_EN(portid);
			else
				larb_mmu->mmu &= ~MTK_SMI_MMU_EN(portid);

		} else if (mtk_iommu_is_infra(data) && enable) {
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SEC)
			arm_smccc_smc(MTK_SIP_KERNEL_IOMMU_CONTROL,
				IOMMU_ATF_SET_IFR_MST_COMMAND(portid,
				IOMMU_ATF_ENABLE_INFRA_MMU), 0,
				0, 0, 0, 0, 0, &res);
			dev_dbg(dev, "enable iommu for 0x%x\n", fwspec->ids[i]);
#else
			dev_info(dev, "fake enable iommu for 0x%x\n",
				 fwspec->ids[i]);
#endif
		}
	}
}

static int mtk_iommu_domain_finalise(struct mtk_iommu_domain *dom)
{
	struct mtk_iommu_data *data = dom->data;

	/* Use the exist domain as there is one m4u pgtable here. */
	if (data->m4u_dom) {
		dom->iop = data->m4u_dom->iop;
		dom->cfg = data->m4u_dom->cfg;
		dom->domain.pgsize_bitmap = data->m4u_dom->cfg.pgsize_bitmap;
		return 0;
	}

	dom->cfg = (struct io_pgtable_cfg) {
		.quirks = IO_PGTABLE_QUIRK_ARM_NS |
			IO_PGTABLE_QUIRK_NO_PERMS |
			IO_PGTABLE_QUIRK_ARM_MTK_EXT,
		.pgsize_bitmap = mtk_iommu_ops.pgsize_bitmap,
#ifdef CONFIG_ARM64
		.ias = 34,
		.oas = 35,
#else
		.ias = 32,
		.oas = 32,
#endif
		.tlb = &mtk_iommu_flush_ops,
		.iommu_dev = data->dev,
	};

	dom->iop = alloc_io_pgtable_ops(ARM_V7S, &dom->cfg, data);
	if (!dom->iop) {
		dev_err(data->dev, "Failed to alloc io pgtable\n");
		return -EINVAL;
	}

	/* Update our support page sizes bitmap */
	dom->domain.pgsize_bitmap = dom->cfg.pgsize_bitmap;
	return 0;
}

static struct iommu_domain *mtk_iommu_domain_alloc(unsigned type)
{
	struct mtk_iommu_data *data = tmp_domain_alloc_data;
	const struct mtk_iommu_iova_region *region;
	struct mtk_iommu_domain *dom;

#ifdef CONFIG_ARM64
	if (type != IOMMU_DOMAIN_DMA)
		return NULL;
#else
	/* IOMMU_DOMAIN_UNMANAGED is used by iommu_domain_alloc() in
	 * arm_iommu_create_mapping().
	 */
	if (type != IOMMU_DOMAIN_DMA && type != IOMMU_DOMAIN_UNMANAGED)
		return NULL;
	/* Avoid duplicate domain allocate when device add IOMMU group. */
	if (data->m4u_dom) {
		dom = data->m4u_dom;
		tmp_domain_alloc_data = NULL;
		return &dom->domain;
	}
#endif

	dom = kzalloc(sizeof(*dom), GFP_KERNEL);
	if (!dom)
		return NULL;

	if (iommu_get_dma_cookie(&dom->domain))
		goto  free_dom;

	dom->data = data;
	if (mtk_iommu_domain_finalise(dom))
		goto  put_dma_cookie;

	region = data->plat_data->iova_region + data->cur_domid;
	if (mtk_iommu_is_infra(data) && (region->iova_base < SZ_1G))
		dom->domain.geometry.aperture_start = SZ_1G;
	else
		dom->domain.geometry.aperture_start = region->iova_base;
	dom->domain.geometry.aperture_end = region->iova_base +
						region->size - 1;
	dom->domain.geometry.force_aperture = true;

#ifndef CONFIG_ARM64
	/* Set domain's type as IOMMU_DOMAIN_DMA for direct_mapping in
	 * iommu_group_add_device()
	 */
	dom->domain.type = IOMMU_DOMAIN_DMA;
	data->m4u_dom = dom;
#else
	tmp_domain_alloc_data = NULL; /* Avoid others call this global. */
#endif
	return &dom->domain;

put_dma_cookie:
	iommu_put_dma_cookie(&dom->domain);
free_dom:
	kfree(dom);
	return NULL;
}

static void mtk_iommu_domain_free(struct iommu_domain *domain)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);

	free_io_pgtable_ops(dom->iop);
	iommu_put_dma_cookie(domain);
	kfree(to_mtk_domain(domain));
}

static int mtk_iommu_hw_init(const struct mtk_iommu_data *data);

static int mtk_iommu_attach_device(struct iommu_domain *domain,
				   struct device *dev)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_data *data = dev_iommu_fwspec_get(dev)->iommu_priv;
	int ret;

	if (!data)
		return -ENODEV;

	/* Update the pgtable base address register of the M4U HW */
	if (!data->m4u_dom) {
		ret = mtk_iommu_rpm_get(data->dev);
		if (ret < 0) {
			pr_err("%s pm get failed(%d) at %s attach\n",
			       dev_name(data->dev), ret, dev_name(dev));
			return ret;
		}
		ret = mtk_iommu_clk_enable(data);
		if (ret) {
			mtk_iommu_rpm_put(data->dev);
			return ret;
		}

		ret = mtk_iommu_hw_init(data);
		if (ret) {
			mtk_iommu_clk_disable(data);
			mtk_iommu_rpm_put(data->dev);
			return ret;
		}
		writel(dom->cfg.arm_v7s_cfg.ttbr[0] & MMU_PT_ADDR_MASK,
		       data->base + REG_MMU_PT_BASE_ADDR);
		mtk_iommu_clk_disable(data);
		mtk_iommu_rpm_put(data->dev);
		data->m4u_dom = dom;
	}

#ifndef CONFIG_ARM64
	/* Initialize the max segment size as 4G for device to support
	 * continuous IOVA mapping at ARM32.
	 */
	if (!dev->dma_parms) {
		dev->dma_parms = kzalloc(sizeof(*dev->dma_parms), GFP_KERNEL);
		if (!dev->dma_parms)
			return -ENOMEM;
	}
	dma_set_max_seg_size(dev, DMA_BIT_MASK(32));
#endif

	mtk_iommu_config(data, dev, true);
	return 0;
}

static void mtk_iommu_detach_device(struct iommu_domain *domain,
				    struct device *dev)
{
	struct mtk_iommu_data *data = dev_iommu_fwspec_get(dev)->iommu_priv;

	if (!data)
		return;

#ifndef CONFIG_ARM64
	kfree(dev->dma_parms);
	dev->dma_parms = NULL;
#endif

	mtk_iommu_config(data, dev, false);
}

static int mtk_iommu_map(struct iommu_domain *domain, unsigned long iova,
			 phys_addr_t paddr, size_t size, int prot, gfp_t gfp)
{
	int ret;
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_data *data = dom->data;

	/* The "4GB mode" M4U physically can not use the lower remap of Dram. */
	if (data->enable_4GB)
		paddr |= BIT_ULL(32);

	/* Synchronize with the tlb_lock */
	ret = dom->iop->map(dom->iop, iova, paddr, size, prot);
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
	if (!ret)
		mtk_iova_map(iova, size);
#endif
	return ret;
}

static size_t mtk_iommu_unmap(struct iommu_domain *domain,
			      unsigned long iova, size_t size,
			      struct iommu_iotlb_gather *gather)
{
	size_t ret;
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);

	ret = dom->iop->unmap(dom->iop, iova, size, gather);
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
	if (ret)
		mtk_iova_unmap(iova, size);
#endif
	return ret;
}

static void mtk_iommu_flush_iotlb_all(struct iommu_domain *domain)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);

	mtk_iommu_tlb_flush_all(dom->data);
}

static void mtk_iommu_iotlb_sync(struct iommu_domain *domain,
				 struct iommu_iotlb_gather *gather)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_data *data = dom->data;
	size_t length = gather->end - gather->start;

	if (gather->start == ULONG_MAX)
		return;

	mtk_iommu_tlb_flush_range_sync(gather->start, length, gather->pgsize,
				       data);
}

static void mtk_iommu_sync_map(struct iommu_domain *domain, unsigned long iova,
			       size_t size)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_data *data = dom->data;

	mtk_iommu_tlb_flush_range_sync(iova, size, size, data);
}

static phys_addr_t mtk_iommu_iova_to_phys(struct iommu_domain *domain,
					  dma_addr_t iova)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_data *data = dom->data;
	phys_addr_t pa;

	pa = dom->iop->iova_to_phys(dom->iop, iova);
	if (data->enable_4GB && pa >= MTK_IOMMU_4GB_MODE_REMAP_BASE)
		pa &= ~BIT_ULL(32);

	return pa;
}

static int mtk_iommu_add_device(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct mtk_iommu_data *data;
	struct iommu_group *group;
	struct device_link *link;
	struct device *larbdev;
	unsigned int larbid, i;
	int ret = 0;
#ifndef CONFIG_ARM64
	struct list_head *hw_list;
	struct mtk_iommu_data *m4u_data;
	struct device *curm4u_dev, *m4u_dev;
	struct dma_iommu_mapping *mtk_mapping;
#endif

	if (!fwspec || fwspec->ops != &mtk_iommu_ops)
		return -ENODEV; /* Not a iommu client device */

	data = fwspec->iommu_priv;
	if (!mtk_iommu_is_infra(data)) {
#ifdef CONFIG_ARM64
		dev->coherent_dma_mask = DMA_BIT_MASK(34);
#endif
		if (IS_ERR_OR_NULL(dev->dma_mask))
			dev->dma_mask = &dev->coherent_dma_mask;
		*dev->dma_mask = dev->coherent_dma_mask;
	}
	iommu_device_link(&data->iommu, dev);

#ifndef CONFIG_ARM64
	curm4u_dev = data->dev;
	hw_list = data->plat_data->hw_list;
	m4u_data = list_first_entry_or_null(hw_list, typeof(*data), list);
	m4u_dev = m4u_data->dev;

	tmp_domain_alloc_data = m4u_data;

	mtk_mapping = m4u_dev->archdata.iommu;
	if (!mtk_mapping) {
		/* MTK iommu support 4GB iova address space. */
		mtk_mapping = arm_iommu_create_mapping(&platform_bus_type,
						0, 1ULL << 32);
		if (IS_ERR_OR_NULL(mtk_mapping)) {
			ret = PTR_ERR(mtk_mapping);
			dev_err(dev, "iommu init arm mapping fail %d\n", ret);
			return ret;
		}
		m4u_dev->archdata.iommu = mtk_mapping;
	}
	if (!curm4u_dev->archdata.iommu)
		curm4u_dev->archdata.iommu = mtk_mapping;
#endif

	group = iommu_group_get_for_dev(dev);
	if (IS_ERR(group))
		return PTR_ERR(group);

	if (!mtk_iommu_is_mm(data))
		goto end;
	/* Link the consumer device with the smi-larb device(supplier) */
	for (i = 0; i < fwspec->num_ids; ++i) {
		larbid = MTK_M4U_TO_LARB(fwspec->ids[i]);
		larbdev = data->larb_imu[larbid].dev;
		if (larbdev) {
			link = device_link_add(dev, larbdev,
					DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);
			if (!link)
				dev_err(dev, "Unable to link %s\n",
					dev_name(larbdev));
		}
	}
end:
	iommu_group_put(group);
#ifndef CONFIG_ARM64
	ret = arm_iommu_attach_device(dev, mtk_mapping);
	if (ret)
		dev_err(dev, "arm attach device fail %d\n", ret);
#endif
	return ret;
}

static void mtk_iommu_remove_device(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct mtk_iommu_data *data;
	struct device *larbdev;
	unsigned int larbid, i;

	if (!fwspec || fwspec->ops != &mtk_iommu_ops)
		return;

	data = fwspec->iommu_priv;
	iommu_device_unlink(&data->iommu, dev);

	if (!mtk_iommu_is_mm(data))
		goto end;
	for (i = 0; i < fwspec->num_ids; ++i) {
		larbid = MTK_M4U_TO_LARB(fwspec->ids[i]);
		larbdev = data->larb_imu[larbid].dev;
		if (larbdev)
			device_link_remove(dev, larbdev);
	}
end:
	iommu_group_remove_device(dev);
	iommu_fwspec_free(dev);
}

static struct iommu_group *mtk_iommu_device_group(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct mtk_iommu_data *c_data = fwspec->iommu_priv, *data;
	struct list_head *hw_list = c_data->plat_data->hw_list;
	struct iommu_group *group;
	int domid;

	/* If 2 M4U share a domain, Put the corresponding info in first data. */
	data = list_first_entry_or_null(hw_list, typeof(*data), list);
	if (!data)
		return ERR_PTR(-ENODEV);

	domid = MTK_M4U_TO_DOM(fwspec->ids[0]);
	if (domid >= data->plat_data->iova_region_cnt) {
		dev_err(data->dev, "domain id(%d/%d) is error.\n",
			domid, data->plat_data->iova_region_cnt);
		return ERR_PTR(-EINVAL);
	}

#ifndef CONFIG_ARM64
	group = iommu_group_alloc();
	if (IS_ERR(group))
		dev_err(dev, "Failed to allocate M4U IOMMU group\n");
	/* No need to save data->m4u_group for arm32 because the groups of
	 * iommu devices are independent of each other.
	 */
#else
	group = data->m4u_group[domid];
	if (!group) {
		group = iommu_group_alloc();
		if (IS_ERR(group))
			dev_err(dev, "Failed to allocate M4U IOMMU group\n");
		data->m4u_group[domid] = group;
	} else {
		iommu_group_ref_get(group);
	}
	tmp_domain_alloc_data = data;
#endif
	data->cur_domid = domid;

	return group;
}

static int mtk_iommu_of_xlate(struct device *dev, struct of_phandle_args *args)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct platform_device *m4updev;
	struct mtk_iommu_data *data, *curr;

	if (args->args_count != 1) {
		dev_err(dev, "invalid #iommu-cells(%d) property for IOMMU\n",
			args->args_count);
		return -EINVAL;
	}

	/* Get the m4u device */
	m4updev = of_find_device_by_node(args->np);
	if (WARN_ON(!m4updev))
		return -EINVAL;

	curr = platform_get_drvdata(m4updev);
	if (!fwspec->iommu_priv)
		fwspec->iommu_priv = curr;

	data = fwspec->iommu_priv;
	if (data != curr) {
		dev_err(dev, "try to attach to %s which has attached to %s\n",
			dev_name(curr->dev), dev_name(data->dev));
		return -EINVAL;
	}

	return iommu_fwspec_add_ids(dev, args->args, 1);
}

static void mtk_iommu_get_resv_regions(struct device *dev,
				       struct list_head *head)
{
	struct mtk_iommu_data *data = dev_iommu_fwspec_get(dev)->iommu_priv;
	const struct mtk_iommu_iova_region *resv, *curdom;
	unsigned int i, total_cnt;
	const struct mtk_iommu_resv_iova_region *spec_data;
	dma_addr_t base = 0;
	size_t size = 0;
	struct iommu_resv_region *region;
	int prot = IOMMU_WRITE | IOMMU_READ;

	/* reserve duplicate parts for iova domains */
	curdom = data->plat_data->iova_region + data->cur_domid;
	for (i = 0; i < data->plat_data->iova_region_cnt; i++) {
		resv = data->plat_data->iova_region + i;

		if (resv->iova_base <= curdom->iova_base ||
		    resv->iova_base + resv->size >=
		    curdom->iova_base + curdom->size)
			continue;

		/* Only reserve when the region is in the current domain */
		region = iommu_alloc_resv_region((phys_addr_t)resv->iova_base,
						 resv->size, prot,
						 IOMMU_RESV_RESERVED);
		if (!region)
			return;

		list_add_tail(&region->list, head);
	}

	/* reserve iova range for devices */
	total_cnt = data->plat_data->resv_cnt;
	if (!total_cnt)
		return;
	spec_data = data->plat_data->resv_region;

	for (i = 0; i < total_cnt; i++) {
		if (!of_device_is_compatible(dev->of_node,
		    spec_data[i].device_comp))
			continue;
		size = 0;
		if (spec_data[i].iova_size) {
			base = spec_data[i].iova_base;
			size = spec_data[i].iova_size;
		} else if (spec_data[i].get_resv_data)
			spec_data[i].get_resv_data(&base, &size, data);
		if (!size)
			continue;

		region = iommu_alloc_resv_region((phys_addr_t)base, size, prot,
						 spec_data[i].type);
		if (!region)
			return;

		list_add_tail(&region->list, head);

		/* for debug */
		dev_info(data->dev, "%s iova 0x%x ~ 0x%x\n",
			(spec_data[i].type == IOMMU_RESV_DIRECT) ? "dm" : "rsv",
			(unsigned int)base, (unsigned int)(base + size - 1));
	}
}

#ifndef CONFIG_ARM64

static int extend_iommu_mapping(struct dma_iommu_mapping *mapping)
{
	int next_bitmap;

	if (mapping->nr_bitmaps >= mapping->extensions)
		return -EINVAL;

	next_bitmap = mapping->nr_bitmaps;
	mapping->bitmaps[next_bitmap] = kzalloc(mapping->bitmap_size,
						GFP_ATOMIC);
	if (!mapping->bitmaps[next_bitmap])
		return -ENOMEM;

	mapping->nr_bitmaps++;

	return 0;
}

/* For reserve iova regions */
static inline int __reserve_iova(struct dma_iommu_mapping *mapping,
				 dma_addr_t iova, size_t size)
{
	unsigned long count, start;
	unsigned long flags;
	int i, sbitmap, ebitmap;

	if (!mapping || iova < mapping->base)
		return -EINVAL;

	start = (iova - mapping->base) >> PAGE_SHIFT;
	count = PAGE_ALIGN(size) >> PAGE_SHIFT;

	sbitmap = start / mapping->bits;
	ebitmap = (start + count) / mapping->bits;
	start = start % mapping->bits;

	if (ebitmap > mapping->extensions)
		return -EINVAL;

	spin_lock_irqsave(&mapping->lock, flags);

	for (i = mapping->nr_bitmaps; i <= ebitmap; i++) {
		if (extend_iommu_mapping(mapping)) {
			spin_unlock_irqrestore(&mapping->lock, flags);
			return -ENOMEM;
		}
	}

	for (i = sbitmap; count && i < mapping->nr_bitmaps; i++) {
		int bits = count;

		if (bits + start > mapping->bits)
			bits = mapping->bits - start;

		bitmap_set(mapping->bitmaps[i], start, bits);
		start = 0;
		count -= bits;
	}

	spin_unlock_irqrestore(&mapping->lock, flags);
	return 0;
}

static int arm_dma_reserve(struct dma_iommu_mapping *mapping, dma_addr_t addr,
			   size_t size)
{
	return __reserve_iova(mapping, addr, size);
}

static void mtk_iommu_apply_resv_region(struct device *dev,
					struct iommu_domain *domain,
					struct iommu_resv_region *region)
{
	struct mtk_iommu_data *data = dev->iommu_fwspec->iommu_priv;
	struct dma_iommu_mapping *mtk_mapping = data->dev->archdata.iommu;
	int ret = arm_dma_reserve(mtk_mapping, (dma_addr_t)(region->start),
				  (size_t)region->length);

	if (ret)
		dev_err(dev, "arm dma rsv iova 0x%lx(0x%x) (%d)\n",
			(unsigned long)region->start,
			(unsigned int)region->length, ret);
}
#endif

static void mtk_iommu_put_resv_regions(struct device *dev,
				       struct list_head *head)
{
	struct iommu_resv_region *entry, *next;

	list_for_each_entry_safe(entry, next, head, list)
		kfree(entry);
}

static const struct iommu_ops mtk_iommu_ops = {
	.domain_alloc	= mtk_iommu_domain_alloc,
	.domain_free	= mtk_iommu_domain_free,
	.attach_dev	= mtk_iommu_attach_device,
	.detach_dev	= mtk_iommu_detach_device,
	.map		= mtk_iommu_map,
	.unmap		= mtk_iommu_unmap,
	.flush_iotlb_all = mtk_iommu_flush_iotlb_all,
	.iotlb_sync	= mtk_iommu_iotlb_sync,
	.iotlb_sync_map	= mtk_iommu_sync_map,
	.iova_to_phys	= mtk_iommu_iova_to_phys,
	.add_device	= mtk_iommu_add_device,
	.remove_device	= mtk_iommu_remove_device,
	.device_group	= mtk_iommu_device_group,
	.of_xlate	= mtk_iommu_of_xlate,
	.get_resv_regions = mtk_iommu_get_resv_regions,
	.put_resv_regions = mtk_iommu_put_resv_regions,
#ifndef CONFIG_ARM64
	.apply_resv_region = mtk_iommu_apply_resv_region,
#endif
	.pgsize_bitmap	= SZ_4K | SZ_64K | SZ_1M | SZ_16M,
};

static int mtk_iommu_hw_init(const struct mtk_iommu_data *data)
{
	u32 regval;

	regval = readl_relaxed(data->base + REG_MMU_CTRL_REG);
	if (data->plat_data->m4u_plat == M4U_MT8173)
		regval |= F_MMU_PREFETCH_RT_REPLACE_MOD |
			 F_MMU_TF_PROT_TO_PROGRAM_ADDR_MT8173;
	else
		regval |= F_MMU_TF_PROT_TO_PROGRAM_ADDR;
	writel_relaxed(regval, data->base + REG_MMU_CTRL_REG);

	regval = F_L2_MULIT_HIT_EN |
		F_TABLE_WALK_FAULT_INT_EN |
		F_PREETCH_FIFO_OVERFLOW_INT_EN |
		F_MISS_FIFO_OVERFLOW_INT_EN |
		F_PREFETCH_FIFO_ERR_INT_EN |
		F_MISS_FIFO_ERR_INT_EN;
	writel_relaxed(regval, data->base + REG_MMU_INT_CONTROL0);

	regval = F_INT_TRANSLATION_FAULT |
		F_INT_MAIN_MULTI_HIT_FAULT |
		F_INT_INVALID_PA_FAULT |
		F_INT_ENTRY_REPLACEMENT_FAULT |
		F_INT_TLB_MISS_FAULT |
		F_INT_MISS_TRANSACTION_FIFO_FAULT |
		F_INT_PRETETCH_TRANSATION_FIFO_FAULT;
	writel_relaxed(regval, data->base + REG_MMU_INT_MAIN_CONTROL);

	if (data->plat_data->m4u_plat == M4U_MT8173)
		regval = (data->protect_base >> 1) | (data->enable_4GB << 31);
	else
		regval = lower_32_bits(data->protect_base) |
			 upper_32_bits(data->protect_base);
	writel_relaxed(regval, data->base + REG_MMU_IVRP_PADDR);

	if (data->enable_4GB && data->plat_data->has_vld_pa_rng) {
		/*
		 * If 4GB mode is enabled, the validate PA range is from
		 * 0x1_0000_0000 to 0x1_ffff_ffff. here record bit[32:30].
		 */
		regval = F_MMU_VLD_PA_RNG(7, 4);
		writel_relaxed(regval, data->base + REG_MMU_VLD_PA_RNG);
	}
	writel_relaxed(0, data->base + REG_MMU_DCM_DIS);
	if (data->plat_data->has_wr_len) {
		/* write command throttling mode */
		regval = readl_relaxed(data->base + REG_MMU_WR_LEN);
		regval &= ~F_MMU_WR_THROT_DIS_BIT;
		writel_relaxed(regval, data->base + REG_MMU_WR_LEN);
	}

	if (data->plat_data->has_misc_ctrl) {
		regval = readl_relaxed(data->base + REG_MMU_MISC_CTRL);
		regval &= ~F_MMU_STANDARD_AXI_MODE_BIT;
		writel_relaxed(regval, data->base + REG_MMU_MISC_CTRL);
	} else if (data->plat_data->reset_axi) {
		/* The register is called STANDARD_AXI_MODE in this case */
		writel_relaxed(0, data->base + REG_MMU_MISC_CTRL);
	}

	if (devm_request_irq(data->dev, data->irq, mtk_iommu_isr, 0,
			     dev_name(data->dev), (void *)data)) {
		writel_relaxed(0, data->base + REG_MMU_PT_BASE_ADDR);
		dev_err(data->dev, "Failed @ IRQ-%d Request\n", data->irq);
		return -ENODEV;
	}

	return 0;
}

static const struct component_master_ops mtk_iommu_com_ops = {
	.bind		= mtk_iommu_bind,
	.unbind		= mtk_iommu_unbind,
};

static int mtk_m4u_prepare(struct mtk_iommu_data *data, struct device *dev,
			   struct component_match **match)
{
	struct device_node *larbnode, *sminode;
	struct platform_device *plat_dev;
	struct device_link *link;
	u32 id;
	int i, larb_nr, ret;

	larb_nr = of_count_phandle_with_args(dev->of_node,
					     "mediatek,larbs", NULL);
	if (larb_nr < 0)
		return larb_nr;

	for (i = 0; i < larb_nr; i++) {
		larbnode = of_parse_phandle(dev->of_node, "mediatek,larbs", i);
		if (!larbnode)
			return -EINVAL;

		if (!of_device_is_available(larbnode)) {
			of_node_put(larbnode);
			continue;
		}

		ret = of_property_read_u32(larbnode, "mediatek,larb-id", &id);
		if (ret)/* The id is consecutive if there is no this property */
			id = i;

		plat_dev = of_find_device_by_node(larbnode);
		if (!plat_dev || !plat_dev->dev.driver) {
			of_node_put(larbnode);
			return -EPROBE_DEFER;
		}
		data->larb_imu[id].dev = &plat_dev->dev;

		component_match_add_release(dev, match, release_of,
					    compare_of, larbnode);

		/* Add link for smi-common and m4u once is ok. and the link is
		 * only needed while m4u has power-domain.
		 */
		if (i || !dev->pm_domain)
			continue;

		sminode = of_parse_phandle(larbnode, "mediatek,smi", 0);
		if (!sminode)
			return -EINVAL;

		plat_dev = of_find_device_by_node(sminode);
		of_node_put(sminode);
		if (!plat_dev) {
			of_node_put(larbnode);
			return -EPROBE_DEFER;
		}

		link = device_link_add(&plat_dev->dev, dev,
				       DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME);
		if (!link)
			dev_err(dev, "can't link %s\n", plat_dev->name);

		of_node_put(larbnode);
	}

	return 0;
}

static int mtk_apu_iommu_prepare(struct device *dev)
{
	struct device_node *apunode;
	struct platform_device *apudev;
	struct device_link *link;
	void *apudata;

	apunode = of_parse_phandle(dev->of_node, "mediatek,apu_power", 0);
	if (!apunode) {
		dev_warn(dev, "Can't find apu power node!\n");
		return -EINVAL;
	}
	apudev = of_find_device_by_node(apunode);
	if (!apudev) {
		of_node_put(apunode);
		dev_warn(dev, "Find apudev fail!\n");
		return -EPROBE_DEFER;
	}
	apudata = platform_get_drvdata(apudev);
	if (!apudata) {
		of_node_put(apunode);
		dev_warn(dev, "Find apudata fail!\n");
		return -EPROBE_DEFER;
	}
	link = device_link_add(&apudev->dev, dev,
			       DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME);
	if (!link)
		dev_err(dev, "Unable link %s.\n", apudev->name);

	return 0;
}

static int mtk_iommu_probe(struct platform_device *pdev)
{
	struct mtk_iommu_data   *data;
	struct device           *dev = &pdev->dev;
	struct resource         *res;
	resource_size_t		ioaddr;
	struct component_match  *match = NULL;
	void                    *protect;
	int                     ret;

	pr_info("%s start dev:%s\n", __func__, dev_name(dev));
	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->dev = dev;
	data->plat_data = of_device_get_match_data(dev);

	/* Protect memory. HW will access here while translation fault.*/
	protect = devm_kzalloc(dev, MTK_PROTECT_PA_ALIGN * 2, GFP_KERNEL);
	if (!protect)
		return -ENOMEM;
	data->protect_base = ALIGN(virt_to_phys(protect), MTK_PROTECT_PA_ALIGN);

	/* Whether the current dram is over 4GB */
	data->enable_4GB = !!(totalram_pages() > ((3LL * SZ_1G) >> PAGE_SHIFT));
	if (!data->plat_data->has_4gb_mode)
		data->enable_4GB = false;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->base))
		return PTR_ERR(data->base);
	ioaddr = res->start;

	data->irq = platform_get_irq(pdev, 0);
	if (data->irq < 0)
		return data->irq;

	if (data->plat_data->has_bclk) {
		data->bclk = devm_clk_get(dev, "bclk");
		if (IS_ERR(data->bclk))
			return PTR_ERR(data->bclk);
	}

	data->clk_rsi = devm_clk_get(dev, "rsi");
	if (PTR_ERR(data->clk_rsi) == -ENOENT)
		data->clk_rsi = NULL;
	else if (IS_ERR(data->clk_rsi))
		return PTR_ERR(data->clk_rsi);

	if (mtk_iommu_is_mm(data)) {
		ret = mtk_m4u_prepare(data, dev, &match);
		if (ret)
			return ret;

	} else if (mtk_iommu_is_apu(data)) {
		ret = mtk_apu_iommu_prepare(dev);
		if (ret)
			return ret;
	}

	platform_set_drvdata(pdev, data);
	/* infra iommu was always on, no need enable rpm */
	if (dev->pm_domain)
		pm_runtime_enable(dev);
	ret = iommu_device_sysfs_add(&data->iommu, dev, NULL,
				     "mtk-iommu.%pa", &ioaddr);
	if (ret)
		return ret;

	iommu_device_set_ops(&data->iommu, &mtk_iommu_ops);
	iommu_device_set_fwnode(&data->iommu, &pdev->dev.of_node->fwnode);

	ret = iommu_device_register(&data->iommu);
	if (ret)
		return ret;

	spin_lock_init(&data->tlb_lock);
	list_add_tail(&data->list, data->plat_data->hw_list);

	if (!iommu_present(&platform_bus_type))
		bus_set_iommu(&platform_bus_type, &mtk_iommu_ops);

	if (mtk_iommu_is_mm(data))
		ret = component_master_add_with_match(dev, &mtk_iommu_com_ops,
						      match);
	return ret;
}

static int mtk_iommu_remove(struct platform_device *pdev)
{
	struct mtk_iommu_data *data = platform_get_drvdata(pdev);
#ifndef CONFIG_ARM64
	struct list_head *hw_list = data->plat_data->hw_list;
	struct mtk_iommu_data *m4u_data;
	struct dma_iommu_mapping *mtk_mapping;
#endif

	iommu_device_sysfs_remove(&data->iommu);
	iommu_device_unregister(&data->iommu);

	if (iommu_present(&platform_bus_type))
		bus_set_iommu(&platform_bus_type, NULL);

	device_link_remove(data->smicomm_dev, &pdev->dev);
	mtk_iommu_clk_disable(data);
	devm_free_irq(&pdev->dev, data->irq, data);
	component_master_del(&pdev->dev, &mtk_iommu_com_ops);

#ifndef CONFIG_ARM64
	m4u_data = list_first_entry_or_null(hw_list, typeof(*data), list);
	if (m4u_data == data) {
		mtk_mapping =
			(struct dma_iommu_mapping *)data->dev->archdata.iommu;
		arm_iommu_release_mapping(mtk_mapping);
	}
#endif

	return 0;
}

static int __maybe_unused mtk_iommu_suspend_runtime(struct device *dev)
{
	struct mtk_iommu_data *data = dev_get_drvdata(dev);
	struct mtk_iommu_suspend_reg *reg = &data->reg;
	void __iomem *base = data->base;
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SEC)
	struct arm_smccc_res res;
	int cmd;

	if (mtk_iommu_is_mm(data)) {
		cmd = IOMMU_ATF_SET_MMU_COMMAND(0, IOMMU_BANK_NUM,
		      IOMMU_ATF_SECURITY_BACKUP);
		arm_smccc_smc(MTK_SIP_KERNEL_IOMMU_CONTROL, cmd, 0, 0,
			      0, 0, 0, 0, &res);
	}
#endif

	reg->wr_len = readl_relaxed(base + REG_MMU_WR_LEN);
	reg->misc_ctrl = readl_relaxed(base + REG_MMU_MISC_CTRL);
	reg->dcm_dis = readl_relaxed(base + REG_MMU_DCM_DIS);
	reg->ctrl_reg = readl_relaxed(base + REG_MMU_CTRL_REG);
	reg->int_control0 = readl_relaxed(base + REG_MMU_INT_CONTROL0);
	reg->int_main_control = readl_relaxed(base + REG_MMU_INT_MAIN_CONTROL);
	reg->ivrp_paddr = readl_relaxed(base + REG_MMU_IVRP_PADDR);
	reg->vld_pa_rng = readl_relaxed(base + REG_MMU_VLD_PA_RNG);

	mtk_iommu_clk_disable(data);

	return 0;
}

static int __maybe_unused mtk_iommu_resume_runtime(struct device *dev)
{
	struct mtk_iommu_data *data = dev_get_drvdata(dev);
	struct mtk_iommu_suspend_reg *reg = &data->reg;
	struct mtk_iommu_domain *m4u_dom = data->m4u_dom;
	void __iomem *base = data->base;
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SEC)
	struct arm_smccc_res res;
	int cmd;
#endif
	int ret;

	ret = mtk_iommu_clk_enable(data);
	if (ret)
		return ret;

	/* Avoid first resume to affect the register below default value. */
	if (!m4u_dom)
		return 0;

	writel_relaxed(reg->wr_len, base + REG_MMU_WR_LEN);
	writel_relaxed(reg->misc_ctrl, base + REG_MMU_MISC_CTRL);
	writel_relaxed(reg->dcm_dis, base + REG_MMU_DCM_DIS);
	writel_relaxed(reg->ctrl_reg, base + REG_MMU_CTRL_REG);
	writel_relaxed(reg->int_control0, base + REG_MMU_INT_CONTROL0);
	writel_relaxed(reg->int_main_control, base + REG_MMU_INT_MAIN_CONTROL);
	writel_relaxed(reg->ivrp_paddr, base + REG_MMU_IVRP_PADDR);
	writel_relaxed(reg->vld_pa_rng, base + REG_MMU_VLD_PA_RNG);
	writel(m4u_dom->cfg.arm_v7s_cfg.ttbr[0] & MMU_PT_ADDR_MASK,
	       base + REG_MMU_PT_BASE_ADDR);

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SEC)
	if (mtk_iommu_is_mm(data)) {
		cmd = IOMMU_ATF_SET_MMU_COMMAND(0, IOMMU_BANK_NUM,
		      IOMMU_ATF_SECURITY_RESTORE);
		arm_smccc_smc(MTK_SIP_KERNEL_IOMMU_CONTROL, cmd, 0, 0,
			      0, 0, 0, 0, &res);
	}
#endif

	return 0;
}

static int __maybe_unused mtk_iommu_suspend(struct device *dev)
{
	if (dev->pm_domain)
		return pm_runtime_force_suspend(dev);

	return mtk_iommu_suspend_runtime(dev);
}

static int __maybe_unused mtk_iommu_resume(struct device *dev)
{
	if (dev->pm_domain)
		return pm_runtime_force_resume(dev);

	return mtk_iommu_resume_runtime(dev);
}

static const struct dev_pm_ops mtk_iommu_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_iommu_suspend_runtime,
			   mtk_iommu_resume_runtime, NULL)
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(mtk_iommu_suspend, mtk_iommu_resume)
};

#if IS_BUILTIN(CONFIG_MTK_IOMMU_MISC_SEC)
static void mtk_get_sec_rsv_region(dma_addr_t *base, size_t *size,
				   struct mtk_iommu_data *data)
{
	unsigned int sec_size;

	if (base)
		*base = 0;
	mtk_iommu_sec_get_iova_size(&sec_size);
	if (size)
		*size = (size_t)sec_size;
}
#endif

static const struct mtk_iommu_resv_iova_region mt8169_iommu_rsv_list[] = {
#if IS_BUILTIN(CONFIG_MTK_IOMMU_MISC_SEC)
	{	.type = IOMMU_RESV_RESERVED,
		.iova_base = 0,
		.iova_size = 0,
		.get_resv_data = mtk_get_sec_rsv_region,
		.device_comp = "mediatek,secure_m4u",
	},
#elif IS_MODULE(CONFIG_MTK_IOMMU_MISC_SEC)
	/* TODO: For GKI, the tee will be inserted too late to get its size,
	 * reserve 1G templately.
	 */
	{	.type = IOMMU_RESV_RESERVED,
		.iova_base = 0,
		.iova_size = SZ_1G,
		.device_comp = "mediatek,secure_m4u",
	},
#else /* no-SVP */
	{	.type = IOMMU_RESV_RESERVED,
		.iova_base = 0,
		.iova_size = SZ_1M,
		.device_comp = "mediatek,disp_ovl0",
	},
#endif
};

static const struct mtk_iommu_plat_data mt2712_data = {
	.m4u_plat     = M4U_MT2712,
	.type         = MTK_IOMMU_MM,
	.hw_list      = &m4ulist,
	.has_4gb_mode = true,
	.has_bclk     = true,
	.has_vld_pa_rng   = true,
	.inv_sel_reg = REG_MMU_INV_SEL_GEN1,
	.larbid_remap = {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}},
	.iova_region  = single_domain,
	.iova_region_cnt = ARRAY_SIZE(single_domain),
};

static const struct mtk_iommu_plat_data mt6779_data = {
	.m4u_plat = M4U_MT6779,
	.type         = MTK_IOMMU_MM,
	.hw_list      = &m4ulist,
	.larbid_remap = {{0}, {1}, {2}, {3}, {5}, {7, 8}, {10}, {9}},
	.has_sub_comm = true,
	.has_wr_len = true,
	.has_misc_ctrl = true,
	.inv_sel_reg = REG_MMU_INV_SEL_GEN2,
	.iova_region  = single_domain,
	.iova_region_cnt = ARRAY_SIZE(single_domain),
};

static const struct mtk_iommu_plat_data mt6873_data_mm = {
	.m4u_plat        = M4U_MT6873,
	.type            = MTK_IOMMU_MM,
	.hw_list         = &m4ulist,
	.larbid_remap    = {{0}, {1}, {4, 5}, {7}, {2}, {9, 11, 19, 20},
			    {0, 14, 16}, {0, 13, 18, 17}},
	.has_sub_comm    = true,
	.has_wr_len      = true,
	.has_misc_ctrl   = true,
	.has_bclk        = true,
	.iova_34_en      = true,
	.inv_sel_reg     = REG_MMU_INV_SEL_GEN2,
	.iova_region     = mt6873_multi_dom,
	.iova_region_cnt = ARRAY_SIZE(mt6873_multi_dom),
};

static const struct mtk_iommu_plat_data mt6873_data_apu = {
	.m4u_plat        = M4U_MT6873,
	.type            = MTK_IOMMU_APU,
	.hw_list         = &iommu_vpulist,
	.is_apu          = true,
	.inv_sel_reg     = REG_MMU_INV_SEL_GEN2,
	.larbid_remap    = {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}},
	.iova_region     = mt6873_multi_dom,
	.iova_region_cnt = ARRAY_SIZE(mt6873_multi_dom),
};

static const struct mtk_iommu_plat_data mt8169_data_mm = {
	.m4u_plat        = M4U_MT8169,
	.type            = MTK_IOMMU_MM,
	.hw_list         = &m4ulist,
	.larbid_remap    = {{0}, {1, -1, 8}, {4}, {7}, {2}, {9, 11, 19, 20},
			    {-1, 14, 16}, {-1, 13, -1, 17}},
	.has_sub_comm    = true,
	.has_wr_len      = true,
	.has_misc_ctrl   = true,
	.iova_34_en      = true,
	.has_bclk        = true,
	.inv_sel_reg     = REG_MMU_INV_SEL_GEN2,
	.iova_region     = mt6873_multi_dom,
	.iova_region_cnt = ARRAY_SIZE(mt6873_multi_dom),
	.resv_cnt = ARRAY_SIZE(mt8169_iommu_rsv_list),
	.resv_region = mt8169_iommu_rsv_list,
};

static const struct mtk_iommu_plat_data mt8169_data_infra = {
	.m4u_plat	= M4U_MT8169,
	.type		= MTK_IOMMU_INFRA,
	.hw_list	= &iommu_infralist,
	.inv_sel_reg	= REG_MMU_INV_SEL_GEN2,
	.iova_region	= single_domain,
	.iova_region_cnt = ARRAY_SIZE(single_domain),
};

static const struct mtk_iommu_plat_data mt8173_data = {
	.m4u_plat     = M4U_MT8173,
	.type         = MTK_IOMMU_MM,
	.hw_list      = &m4ulist,
	.has_4gb_mode = true,
	.has_bclk     = true,
	.reset_axi    = true,
	.inv_sel_reg = REG_MMU_INV_SEL_GEN1,
	.larbid_remap = {{0}, {1}, {2}, {3}, {4}, {5}}, /* Linear mapping. */
	.iova_region  = single_domain,
	.iova_region_cnt = ARRAY_SIZE(single_domain),
};

static const struct mtk_iommu_plat_data mt8183_data = {
	.m4u_plat     = M4U_MT8183,
	.type         = MTK_IOMMU_MM,
	.hw_list      = &m4ulist,
	.reset_axi    = true,
	.inv_sel_reg = REG_MMU_INV_SEL_GEN1,
	.larbid_remap = {{0}, {4}, {5}, {6}, {7}, {2}, {3}, {1}},
	.iova_region  = single_domain,
	.iova_region_cnt = ARRAY_SIZE(single_domain),
};

static const struct of_device_id mtk_iommu_of_ids[] = {
	{ .compatible = "mediatek,mt2712-m4u", .data = &mt2712_data},
	{ .compatible = "mediatek,mt6779-m4u", .data = &mt6779_data},
	{ .compatible = "mediatek,mt6873-m4u", .data = &mt6873_data_mm},
	{ .compatible = "mediatek,mt6873-apu-iommu", .data = &mt6873_data_apu},
	{ .compatible = "mediatek,mt8169-iommu-mm", .data = &mt8169_data_mm},
	{ .compatible = "mediatek,mt8169-iommu-infra",
	  .data = &mt8169_data_infra},
	{ .compatible = "mediatek,mt8173-m4u", .data = &mt8173_data},
	{ .compatible = "mediatek,mt8183-m4u", .data = &mt8183_data},
	{}
};

static struct platform_driver mtk_iommu_driver = {
	.probe	= mtk_iommu_probe,
	.remove	= mtk_iommu_remove,
	.driver	= {
		.name = "mtk-iommu",
		.of_match_table = of_match_ptr(mtk_iommu_of_ids),
		.pm = &mtk_iommu_pm_ops,
	}
};
module_platform_driver(mtk_iommu_driver);

MODULE_DESCRIPTION("IOMMU API for MediaTek M4U implementations");
MODULE_AUTHOR("Yong Wu <yong.wu@mediatek.com>");
MODULE_ALIAS("platform:MediaTek-M4U");
MODULE_LICENSE("GPL v2");
