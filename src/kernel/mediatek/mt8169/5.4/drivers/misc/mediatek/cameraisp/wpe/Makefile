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

obj-$(CONFIG_MTK_CAMERA_ISP_WPE_SUPPORT) := camerawpe_dummy.o
MTK_WPE_COUNT := 1


ifeq ($(CONFIG_MTK_CAMERA_ISP_PLATFORM),"mt8169")
  obj-$(CONFIG_MTK_CAMERA_ISP_WPE_SUPPORT) += $(subst ",,isp_6s)/
  MTK_WPE_COUNT := 1
endif

subdir-ccflags-y += -DMTK_WPE_COUNT=$(MTK_WPE_COUNT)

