# acenna_kmod Makefile

PWD       := $(shell pwd)

ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m   := acenna_kmod.o

else

KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules

endif

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) clean

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules_install

help:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) help
