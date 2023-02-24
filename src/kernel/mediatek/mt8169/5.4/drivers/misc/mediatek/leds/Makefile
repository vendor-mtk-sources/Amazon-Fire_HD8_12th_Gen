# SPDX-License-Identifier: GPL-2.0

ifdef CONFIG_LEDS_BL_MTK_DISP
obj-$(CONFIG_LEDS_BL_MTK_DISP)		+= leds-mtk-disp.o
else ifdef CONFIG_LEDS_BL_MTK_PWM
obj-$(CONFIG_LEDS_BL_MTK_PWM)		+= leds-mtk-pwm.o
ccflags-y += -I$(srctree)/drivers/misc/mediatek/boot
else
obj-$(CONFIG_MTK_LEDS) += mtk_leds_drv.o
obj-$(CONFIG_MTK_LEDS) := mtk_leds.o
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/include
endif
