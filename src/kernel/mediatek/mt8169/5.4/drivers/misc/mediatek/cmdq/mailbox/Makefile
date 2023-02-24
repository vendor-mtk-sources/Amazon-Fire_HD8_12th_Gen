# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2019 MediaTek Inc.
#

CMDQ_PLATFORM := 8169

MDP_PLATFORM := 8169

ccflags-y += -DCMDQ_FPGA_EARLY_PORTING

ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(CMDQ_PLATFORM)/include \
             -I$(srctree)/drivers/misc/mediatek/include \
             -I$(srctree)/drivers/misc/mediatek/smi/ \
             -I$(srctree)/drivers/misc/mediatek/mdp/$(MDP_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/mmp/
ifeq ($(or $(strip $(CONFIG_MTK_SVP_ON_MTEE_SUPPORT)), $(strip $(CONFIG_MTK_CAM_GENIEZONE_SUPPORT))),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/trustzone/m4u
ccflags-y += -I$(srctree)/drivers/iommu
endif


ifeq ($(or $(strip $(CONFIG_MTK_SVP_ON_MTEE_SUPPORT)), $(strip $(CONFIG_MTK_CAM_GENIEZONE_SUPPORT))),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/trustzone/m4u
ccflags-y += -I$(srctree)/drivers/iommu
endif

ifeq ($(or $(strip $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)), $(strip $(CONFIG_MTK_CAM_SECURITY_SUPPORT))),y)

ifeq ($(CONFIG_MICROTRUST_TEE_SUPPORT),y)
include $(srctree)/drivers/misc/mediatek/teei/Makefile.include
endif

ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
endif

ifeq ($(strip $(CONFIG_MTK_ENABLE_GENIEZONE)),y)
ifeq ($(strip $(CONFIG_MTK_CAM_GENIEZONE_SUPPORT)),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public
ccflags-y += -DCMDQ_SECURE_MTEE_SUPPORT
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq_sec_mtee.o
endif
endif

ifeq ($(strip $(CONFIG_MTK_SVP_ON_MTEE_SUPPORT)),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public
ccflags-y += -DCMDQ_SECURE_MTEE_SUPPORT
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq_sec_mtee.o

else ifeq ($(strip $(CONFIG_MTK_TEE_GP_SUPPORT)),y)
#ccflags-y += -DCMDQ_GP_SUPPORT
#obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-sec-gp.o
endif

include $(srctree)/drivers/misc/mediatek/tee_gp/Makefile.include
ccflags-y += -DCMDQ_SECURE_SUPPORT
ccflags-y += -DCMDQ_GP_SUPPORT
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-sec-drv.o
cmdq-sec-drv-y := cmdq-sec-gp.o cmdq-sec-mailbox.o cmdq-sec-helper.o

endif # $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) or $(CONFIG_MTK_CAM_SECURITY_SUPPORT)


obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += mtk-cmdq-drv-ext.o
mtk-cmdq-drv-ext-y := mtk-cmdq-mailbox-ext.o mtk-cmdq-helper-ext.o cmdq-util.o


obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-test.o

ifneq ($(wildcard $(srctree)/$(src)/cmdq-platform-mt8169.c),)
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-platform-mt8169.o
endif

obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-bw-mon.o