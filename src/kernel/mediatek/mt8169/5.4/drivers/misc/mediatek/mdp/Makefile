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

# driver code for MDP

MTK_PLATFORM := "8169"

ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/ \
	     -I$(srctree)/drivers/misc/mediatek/smi/$(MTK_PLATFORM)/ \
	     -I$(srctree)/drivers/misc/mediatek/mmdvfs/ \
	     -I$(srctree)/drivers/misc/mediatek/mtk-interconnect/ \
	     -I$(srctree)/drivers/misc/mediatek/mmp/ \
	     -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include \
	     -I$(srctree)/drivers/iommu/ \
	     -I$(srctree)/drivers/staging/android/ion \
	     -I$(srctree)/drivers/staging/android/ion/mtk

ifneq ($(filter y m, $(CONFIG_MTK_MDP)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox/
endif

ifeq ($(or $(strip $(CONFIG_MTK_SVP_ON_MTEE_SUPPORT)), $(strip $(CONFIG_MTK_CAM_GENIEZONE_SUPPORT))),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/trustzone/m4u
ccflags-y += -I$(srctree)/drivers/iommu
endif

# mdp platform code
MDP_PLATFORM_DIR=$(srctree)/drivers/misc/mediatek/mdp/$(MTK_PLATFORM)

platformdir_exists=$(shell if [ -d ${MDP_PLATFORM_DIR} ]; then echo "y"; else echo "n"; fi;)
ifeq ($(platformdir_exists), y)
ccflags-y += -I$(MDP_PLATFORM_DIR)/
else
ccflags-y += -DMDP_COMMON_ENG_SUPPORT
endif

ifeq ($(CONFIG_MTK_ENG_BUILD),y)
ccflags-y += -DCMDQ_TEST_PROC
ccflags-y += -DCMDQ_PROFILE_LOCK
ccflags-y += -DCMDQ_DEBUG_LOOP_IRQ
ccflags-y += -DMDP_COMMAND_SIMULATE
endif

# secure path
# use CONFIG_MTK_MDP support
# secure path
ifneq ($(filter y m, $(CONFIG_OPTEE)),)
ifeq ($(or $(strip $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)), $(strip $(CONFIG_MTK_CAM_SECURITY_SUPPORT))),y)
ccflags-y += -DCMDQ_SECURE_PATH_SUPPORT
endif	# end of CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT or CONFIG_MTK_CAM_SECURITY_SUPPORT
endif	# end of CONFIG_OPTEE

ifeq ($(or $(strip $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)), $(strip $(CONFIG_MTK_CAM_SECURITY_SUPPORT))),y)
ifeq ($(strip $(CONFIG_MTK_ENABLE_GENIEZONE)),y)
ifeq ($(strip $(CONFIG_MTK_CAM_GENIEZONE_SUPPORT)),y)
ccflags-y += -DCMDQ_SECURE_MTEE_SUPPORT
endif
endif
endif

# platform dependent definition
ifneq ($(strip $(CONFIG_MTK_GMO_RAM_OPTIMIZE)),y)
ccflags-y += -DCMDQ_LARGE_MAX_FIRSTERROR_BUFFER
endif

# driver module
obj-$(CONFIG_MTK_MDP) += mtk-mdp-drv.o
mtk-mdp-drv-y := cmdq_record.o \
	cmdq_virtual.o \
	cmdq_device.o \
	mdp_driver.o \
	mdp_common.o \
	mdp_event_common.o \
	cmdq_subsys_common.o \
	cmdq_helper_ext.o \
	mdp_ioctl_ex.o \
	mdp_m4u.o \
	mdp_fence.o

#obj-$(CONFIG_MTK_MDP) += cmdq_prof.o
#obj-$(CONFIG_MTK_MDP) += cmdq_test.o

ifneq ($(filter y m, $(CONFIG_MMPROFILE)),)
mtk-mdp-drv-y += cmdq_mmp.o
endif

ifeq ($(platformdir_exists), y)
mtk-mdp-drv-y += $(subst ",,$(MTK_PLATFORM))/mdp.o
endif

# mdp_sw_sync
#obj-$(CONFIG_MTK_MDP) += sync/

