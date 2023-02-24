# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/misc/mediatek/iommu

obj-$(CONFIG_ION_SYSTEM_HEAP) += ion_sys_heap.o
ion_sys_heap-y := ion_system_heap.o ion_page_pool.o

obj-$(CONFIG_ION_CMA_HEAP) += ion_cma_heap.o
# ccflags-y += -DMTK_ION_MM_HEAP_DBG
obj-$(CONFIG_MTK_ION_MM_HEAP) += mtk_ion_mm_heap.o

ifneq ($(filter y m, $(CONFIG_OPTEE)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/trustzone
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/include/trustzone/
endif

# ccflags-y += -DMTK_ION_SEC_HEAP_DBG
obj-$(CONFIG_MTK_ION_SEC_HEAP) += mtk_ion_secure_heap.o
