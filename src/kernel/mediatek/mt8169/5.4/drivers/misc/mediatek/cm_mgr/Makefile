#
# Copyright (C) 2015 MediaTek Inc.
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

ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/cm_mgr/ \
	-I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat \
	-I$(srctree)/drivers/misc/mediatek/sspm \
	-I$(srctree)/drivers/misc/mediatek/tinysys_scmi/ \
	-I$(srctree)/drivers/misc/mediatek/qos \
	-I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/ \
	-I$(srctree)/include \

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_V2),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2
endif

obj-${CONFIG_MTK_CM_MGR} += mtk_cm_mgr.o
mtk_cm_mgr-y = mtk_cm_mgr_common.o

obj-${CONFIG_MTK_CM_IPI} += mtk_cm_ipi.o

obj-${CONFIG_MTK_CM_MGR_MT6873} += mtk_cm_mgr_mt6873.o
obj-${CONFIG_MTK_CM_MGR_MT6853} += mtk_cm_mgr_mt6853.o
obj-${CONFIG_MTK_CM_MGR_MT8169} += mtk_cm_mgr_mt6873.o
