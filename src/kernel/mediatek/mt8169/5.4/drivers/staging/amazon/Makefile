MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -Werror -I$(srctree)/include/linux/mt-plat/
subdir-ccflags-y += -Werror -I$(srctree)/include/linux/


obj-$(CONFIG_AMAZON_SIGN_OF_LIFE) += amzn_sign_of_life.o
amzn_sign_of_life-objs += amzn_sign_of_life_v3.o amzn_sign_of_life_rtc_impl.o
obj-$(CONFIG_AMAZON_INPUT_KEYCOMBO) += amzn_keycombo.o
obj-$(CONFIG_AMAZON_DSP_FRAMEWORK) += dspframework/
