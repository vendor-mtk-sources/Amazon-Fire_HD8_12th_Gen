/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __MT_IOMMU_SEC_H__
#define __MT_IOMMU_SEC_H__

#define IOMMU_BANK_NUM				5

/* m4u atf debug parameter */

#define IOMMU_ATF_CMD_SHIFT			8
#define IOMMU_ATF_CMD_MASK			F_MSK(16, 8)
#define IOMMU_ATF_GET_COMMAND(cmd)		\
	(((cmd) & IOMMU_ATF_CMD_MASK) >> IOMMU_ATF_CMD_SHIFT)

#define IOMMU_ATF_LARB_ID_MASK			F_MSK(7, 0)
#define IOMMU_ATF_GET_LARB_ID(cmd)		((cmd) & IOMMU_ATF_LARB_ID_MASK)
#define IOMMU_ATF_SET_LARB_COMMAND(id, cmd)	((id) | ((cmd) << IOMMU_ATF_CMD_SHIFT))

#define IOMMU_ATF_MMU_BANK_ID_MASK		F_MSK(7, 4)
#define IOMMU_ATF_MMU_ID_MASK			F_MSK(3, 0)
#define IOMMU_ATF_GET_MMU_BANK_ID(cmd)		\
	(((cmd) & IOMMU_ATF_MMU_BANK_ID_MASK) >> 4)
#define IOMMU_ATF_GET_MMU_ID(cmd)		\
	((cmd) & IOMMU_ATF_MMU_ID_MASK)
#define IOMMU_ATF_SET_MMU_COMMAND(mmu, bk, cmd)	\
	((mmu) | ((bk) << 4) | ((cmd) << IOMMU_ATF_CMD_SHIFT))

#define IOMMU_ATF_IFR_MST_ID_MASK		F_MSK(7, 0)
#define IOMMU_ATF_GET_IFR_MST_ID(cmd)		((cmd) & IOMMU_ATF_IFR_MST_ID_MASK)
#define IOMMU_ATF_SET_IFR_MST_COMMAND(id, cmd)	((id) | ((cmd) << IOMMU_ATF_CMD_SHIFT))

enum IOMMU_ATF_CMD {
	IOMMU_ATF_DUMP_SECURE_REG,	/* for sec m4u irq dump */
	IOMMU_ATF_SECURITY_BACKUP,	/* for sec m4u reg backup */
	IOMMU_ATF_SECURITY_RESTORE,	/* for sec m4u reg restore */
	IOMMU_ATF_SET_SMI_SEC_LARB,	/* for sec smi reg configure */
	IOMMU_ATF_SMI_LARB_BACKUP,	/* for smi larb reg backup */
	IOMMU_ATF_SMI_LARB_RESTORE,	/* for smi larb reg restore */
	IOMMU_ATF_ENABLE_INFRA_MMU,	/* for infra module enable iommu */
	IOMMU_ATF_CMD_COUNT
};

void mtk_iommu_sec_get_iova_size(unsigned int *iova_size);
int mtk_iommu_get_real_larb_portid(struct device *dev, u32 int_id,
				   int *larb_id, int *port_id);

#endif
