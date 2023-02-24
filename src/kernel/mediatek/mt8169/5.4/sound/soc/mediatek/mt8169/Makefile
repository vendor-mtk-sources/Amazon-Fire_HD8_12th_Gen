# SPDX-License-Identifier: GPL-2.0

# platform driver
snd-soc-mt8169-afe-objs := \
	mt8169-afe-pcm.o \
	mt8169-afe-clk.o \
	mt8169-afe-gpio.o \
	mt8169-dai-adda.o \
	mt8169-afe-control.o \
	mt8169-dai-i2s.o \
	mt8169-dai-hw-gain.o \
	mt8169-dai-pcm.o \
	mt8169-dai-src.o \
	mt8169-dai-hostless.o \
	mt8169-dai-tdm.o \
	mt8169-misc-control.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hifidsp_audio_ipi/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/

ifeq ($(CONFIG_MTK_QOS_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/devfreq/
endif

snd-soc-mt8169-adsp-pcm-objs := \
    mt8169-adsp-pcm.o mt8169-adsp-utils.o

obj-$(CONFIG_SND_SOC_MT8169) += snd-soc-mt8169-afe.o
obj-$(CONFIG_MTK_HIFIXDSP_SUPPORT) += snd-soc-mt8169-adsp-pcm.o

# machine driver
obj-$(CONFIG_SND_SOC_MT8169_MT6366) += mt8169-mt6366.o
obj-$(CONFIG_SND_SOC_MT8169_MT6357) += mt8169-mt6366.o
obj-$(CONFIG_SND_SOC_MT8169_abc123) += abc123.o
obj-$(CONFIG_SND_SOC_MT8169_RASPITE) += mt8169-raspite.o

# MENU Governor Predict(low power)
ifeq ($(CONFIG_MTK_ACAO_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/
endif

# spm
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
