# SPDX-License-Identifier: GPL-2.0
#
# virtual sensor thermal specific Makefile
#
#THERMAL_CHIP_DRIVER_DIR := $(srctree)/drivers/misc/mediatek/thermal/$(MTK_PLATFORM)
#ccflags-y  += -I$(THERMAL_CHIP_DRIVER_DIR)/inc
#ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
#ccflags-y  += -I$(srctree)/drivers/misc/mediatek/gpu/hal/
#ccflags-y  += -I$(srctree)/drivers/misc/mediatek/pmic/include/
#ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/common/inc/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
#ccflags-y  += -I$(srctree)/drivers/misc/mediatek/pmic/mt6370/inc/
ccflags-y  += -I$(srctree)/drivers/thermal/
ccflags-y += -I$(srctree)/drivers/power/supply
ccflags-y += -I$(srctree)/drivers/misc/mediatek/leds


# platform thermal drivers
obj-$(CONFIG_VIRTUAL_SENSOR_THERMAL) += virtual_sensor_tablet_thermal.o

#thermal sensors
obj-$(CONFIG_VIRTUAL_SENSOR_THERMAL) += virtual_sensor_thermal_sensors.o

# platform interface
virtual_sensor_thermal_sensors-objs += virtual_sensor_thermal_sensor.o virtual_sensor_interface.o

# governors
obj-$(CONFIG_VIRTUAL_SENSOR_THERMAL) += virtual_sensor_gov.o

# cooling devices
obj-$(CONFIG_VIRTUAL_SENSOR_THERMAL) += virtual_sensor_cooler_bcct.o \
					virtual_sensor_cooler_backlight.o \
					virtual_sensor_cooler_budget.o \
					virtual_sensor_cooler_wpc_bcct.o
