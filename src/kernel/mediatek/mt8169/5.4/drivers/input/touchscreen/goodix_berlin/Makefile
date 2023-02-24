obj-$(CONFIG_TOUCHSCREEN_GOODIX_BRL) += goodix_core.o
goodix_core-y := \
				goodix_brl_i2c.o \
				goodix_brl_spi.o \
				goodix_ts_core.o \
				goodix_brl_hw.o \
				goodix_cfg_bin.o \
				goodix_ts_utils.o \
				goodix_brl_fwupdate.o \
				goodix_ts_gesture.o \
				goodix_ts_inspect.o \
				goodix_ts_tools.o

ccflags-y += -DANDROID_GKI_VFS_EXPORT_ONLY=VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver
