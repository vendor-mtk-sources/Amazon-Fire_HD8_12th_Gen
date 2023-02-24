ifneq ($(filter y m, $(CONFIG_MTK_SENSOR_SUPPORT)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/sensorHub/inc_v1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/accelerometer/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/alsps/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/gyroscope/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/accelgyro/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/magnetometer/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/barometer/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/humidity/inc

obj-$(CONFIG_MTK_SENSOR_SUPPORT) += hwmon.o
hwmon-objs += sensor_event/sensor_event.o
hwmon-objs += sensor_attributes/sensor_attr.o
hwmon-objs += hwmsen/hwmsen_helper.o
hwmon-objs += sensor_performance/sensor_performance.o
hwmon-objs += sensor_dts/sensor_dts.o
obj-$(CONFIG_MTK_SENSOR_SUPPORT) += sensor_list/sensor_list.o

#obj-$(CONFIG_MTK_HWMON) += hwmsen/hwmsen_helper.o
#obj-$(CONFIG_MTK_HWMON) += sensor_dts/sensor_dts.o
#obj-$(CONFIG_MTK_HWMON) += sensor_attributes/sensor_attr.o
#obj-$(CONFIG_MTK_HWMON) += sensor_event/sensor_event.o
#obj-$(CONFIG_MTK_HWMON) += sensor_performance/sensor_performance.o
#obj-$(CONFIG_MTK_HWMON) += sensor_list/sensor_list.o
else
obj-n := hwmsen/
endif
