# SPDX-License-Identifier: GPL-2.0

mtk-hms-objs := mtk_hms_core.o \
		 mtk_hms_comp.o \
		 mtk_hms_m2m.o \
		 mtk_hms_regs.o
obj-$(CONFIG_VIDEO_MEDIATEK_HMS) += mtk-hms.o

ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vcu
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vcodec
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vcodec/$(MTK_VCODEC_PLATFORM)
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-videobuf
ccflags-y += -I$(srctree)/drivers/misc/mediatek/slbc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/iommu/
ccflags-y += -I$(srctree)/arch/arm
ccflags-y += -I$(srctree)/drivers/staging/android/ion
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mtk-interconnect
ifdef CONFIG_VB2_MEDIATEK_DMA_CONTIG
	ccflags-y += -I$(srctree)/drivers/media/platform/mtk-videobuf
endif