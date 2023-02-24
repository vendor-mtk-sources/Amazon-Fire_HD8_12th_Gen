// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt)    "[SEC M4U] " fmt

#include <linux/memblock.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/arm-smccc.h>
#include <linux/sizes.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/pm_runtime.h>

#include "mtk_iommu_sec.h"

#if IS_ENABLED(CONFIG_OPTEE)
#include "tz_cross/trustzone.h"
#include "kree/system.h"
#include "tz_cross/ta_m4u.h"
#endif

#define for_each_m4u(data, head)  list_for_each_entry(data, head, list)

struct mtk_iommu_sec_data {
	int			id;
	int			irq;
	struct device		*dev;
	struct device		*base_dev;
	struct list_head	list;
};

static LIST_HEAD(sec_m4ulist);	/* List all the sec M4U HWs */

static KREE_SESSION_HANDLE m4u_session;
/* init the sec_mem_size to 1G to avoid build error. */
static unsigned int sec_iova_size = SZ_1G;
static int sec_m4u_inited;

static int mtk_iommu_sec_session_init(void)
{
	int ret;

	if (m4u_session)
		return 0;

	ret = KREE_CreateSession(TZ_TA_M4U_UUID, &m4u_session);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_err("%s error %d\n", __func__, ret);
		m4u_session = (KREE_SESSION_HANDLE)0;
		return ret;
	}

	pr_debug("%s session:0x%x\n", __func__, (unsigned int)m4u_session);
	return 0;
}

static inline void mtk_iommu_sec_session_close(void)
{
	KREE_CloseSession(m4u_session);
	m4u_session = (KREE_SESSION_HANDLE)0;
}

static int mtk_iommu_sec_hw_init(void)
{
	union MTEEC_PARAM param[4];
	int is_4gb = 0; /* !!(max_pfn > (BIT_ULL(32) >> PAGE_SHIFT)) */
	uint32_t paramTypes;
	int ret;

	if (sec_m4u_inited)
		return 0;

	if (mtk_iommu_sec_session_init())
		return -1;

	param[0].value.a = !!IS_ENABLED(CONFIG_ARM64);
	param[0].value.b = is_4gb;
	param[1].value.a = 1;
	paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT);
	ret = KREE_TeeServiceCall(m4u_session, M4U_TZCMD_SEC_INIT,
				  paramTypes, param);
	mtk_iommu_sec_session_close();
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("%s error %d\n", __func__, ret);
		return ret;
	}

	sec_iova_size = param[1].value.a + SZ_1M;
	pr_info("%s done (sec_iova_size 0x%x)\n", __func__, sec_iova_size);
	sec_m4u_inited = 1;

	return 0;
}

static irqreturn_t mtk_iommu_sec_isr(int irq, void *dev_id)
{
	struct mtk_iommu_sec_data *data = dev_id;
	struct arm_smccc_res res;
	int cmd = IOMMU_ATF_SET_MMU_COMMAND(data->id, 4,
					    IOMMU_ATF_DUMP_SECURE_REG);
	unsigned long tf_sta, tf_pa, tf_id;
	int fault_larb = -1, fault_port = -1;

	arm_smccc_smc(MTK_SIP_KERNEL_IOMMU_CONTROL, cmd, 0, 0, 0, 0, 0, 0, &res);
	tf_sta = res.a1;
	tf_pa = res.a2;
	tf_id = res.a3;

	mtk_iommu_get_real_larb_portid(data->base_dev, tf_id,
				       &fault_larb, &fault_port);

	dev_err_ratelimited(data->dev, "fault sec_iova=0x%lx pa=0x%lx larb=%u port=%u(0x%lx)\n",
			    tf_sta, tf_pa, fault_larb, fault_port, tf_id);

	return IRQ_HANDLED;
}

static int mtk_iommu_sec_init(void)
{
	struct mtk_iommu_sec_data *data;
	struct device_node *dev_node = NULL, *iommu_node;
	struct platform_device *pdev, *iommu_pdev;
	struct device *dev;
	struct device_link *link;
	int ret = -ENODEV;

	/* register all sec iommu device IRQ */
	do {
		dev_node = of_find_compatible_node(dev_node, NULL,
						   "mediatek,secure_m4u");
		if (!dev_node)
			break;

		pdev = of_find_device_by_node(dev_node);
		of_node_put(dev_node);
		if (!pdev)
			return -ENODEV;
		dev = &pdev->dev;

		data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
		if (!data)
			return -ENOMEM;

		data->dev = dev;

		ret = of_property_read_u32(dev->of_node, "cell-index",
					   &data->id);
		if (ret) {
			dev_err(dev, "get cell-index fail\n");
			return -ENODEV;
		}

		iommu_node = of_parse_phandle(dev->of_node, "main-bank", 0);
		if (!iommu_node) {
			dev_err(dev, "get main-bank fail\n");
			return -EINVAL;
		}
		iommu_pdev = of_find_device_by_node(iommu_node);
		of_node_put(iommu_node);
		if (!iommu_pdev) {
			dev_err(dev, "get iommu device fail\n");
			return -EINVAL;
		}

		link = device_link_add(dev, &iommu_pdev->dev,
				       DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME);
		if (!link)
			dev_err(dev, "unable link %s\n",
				dev_name(&iommu_pdev->dev));

		data->irq = platform_get_irq(pdev, 0);
		if (data->irq < 0)
			return data->irq;
		if (devm_request_irq(data->dev, data->irq, mtk_iommu_sec_isr, 0,
				     dev_name(data->dev), (void *)data)) {
			dev_err(dev, "request secure m4u-%d IRQ fail\n",
				data->id);
			return -ENODEV;
		}

		data->base_dev = &iommu_pdev->dev;

		list_add_tail(&data->list, &sec_m4ulist);

		pm_runtime_enable(dev);
		dev_info(dev, "%s done\n", __func__);

	} while (dev_node);

	if (ret) {
		pr_info("%s fail %d\n", __func__, ret);
		return ret;
	}

	for_each_m4u(data, &sec_m4ulist)
		pm_runtime_get_sync(data->dev);

	/* tz m4u init */
	mtk_iommu_sec_hw_init();

	/* trigger sec iova reserve flow */
	ret = of_dma_configure(dev, dev->of_node, true);

	for_each_m4u(data, &sec_m4ulist)
		pm_runtime_put_sync(data->dev);

	return ret;
}

void mtk_iommu_sec_get_iova_size(unsigned int *iova_size)
{
	if (iova_size)
		*iova_size = sec_iova_size;
}
EXPORT_SYMBOL_GPL(mtk_iommu_sec_get_iova_size);

late_initcall_sync(mtk_iommu_sec_init);

MODULE_LICENSE("GPL v2");
