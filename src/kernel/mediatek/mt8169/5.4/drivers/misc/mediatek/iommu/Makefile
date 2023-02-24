#
# Copyright (C) 2019 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/iommu

obj-$(CONFIG_MTK_IOMMU_MISC_DBG) += iommu_debug.o

ifneq ($(filter y m, $(CONFIG_OPTEE)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/trustzone
ccflags-y += -I$(srctree)/include/trustzone/
endif

obj-$(CONFIG_MTK_IOMMU_MISC_SEC) += mtk_iommu_sec.o
obj-$(CONFIG_MTK_IOMMU_MISC) += mtk_dma_buf_helper.o
