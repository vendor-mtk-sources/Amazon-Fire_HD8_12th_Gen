# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_OPTEE) += optee.o
optee-objs += core.o
optee-objs += call.o
optee-objs += rpc.o
optee-objs += supp.o
optee-objs += shm_pool.o
ifneq ($(filter y m, $(CONFIG_OPTEE_REE_CONSOLE)),)
optee-objs += optee_kreeconsole.o
endif
optee-objs += device.o
ifneq ($(filter y m, $(CONFIG_OPTEE_REE_CLK_CTRL)),)
optee-objs += optee_clk_ctrl.o
optee-objs += $(optee-objs-y)
endif
