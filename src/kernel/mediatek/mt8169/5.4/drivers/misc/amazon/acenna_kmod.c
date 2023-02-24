// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015-2018 Ichiro Kawazome
 */

#include <asm/byteorder.h>
#include <asm/page.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/pagemap.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/sysctl.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/pm_runtime.h>
#include <linux/version.h>
#include <asm/atomic.h>
#include <linux/delay.h>
#include <linux/irqdomain.h>
#include <linux/poll.h>
#include <linux/ktime.h>

#include "acenna_kmod.h"

#if defined(CONFIG_NNA_MT8169)
#include "mt8169_acenna.h"
#elif defined CONFIG_AMLOGIC_DRIVER
#include "abc123_acenna.h"
#else
#include "plat_acenna.h"
#endif

/**
 * DOC: NNA Constants
 */

MODULE_DESCRIPTION("Amazon ACE NN Kernel Driver");
MODULE_AUTHOR("Amazon Lab126");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRIVER_VERSION);

#define NNA_DEBUG 1

#if defined(CONFIG_ARM) || defined(CONFIG_ARM64)
#define USE_VMA_FAULT 1
#else
#define USE_VMA_FAULT 0
#endif

#if ((LINUX_VERSION_CODE >= 0x031300) && (defined(CONFIG_ARM) || defined(CONFIG_ARM64)))
#define USE_DMA_COHERENT 1
#else
#define USE_DMA_COHERENT 0
#endif

#if (LINUX_VERSION_CODE >= 0x030B00)
#define USE_DEV_GROUPS 1
#else
#define USE_DEV_GROUPS 0
#endif

#if ((LINUX_VERSION_CODE >= 0x040100) && defined(CONFIG_OF))
#define USE_OF_RESERVED_MEM 1
#else
#define USE_OF_RESERVED_MEM 0
#endif

#if ((LINUX_VERSION_CODE >= 0x040100) && defined(CONFIG_OF))
#define USE_OF_DMA_CONFIG 1
#else
#define USE_OF_DMA_CONFIG 0
#endif

#if (NNA_DEBUG == 1)
#define NNA_DEBUG_CHECK(this, debug) (this->debug)
#else
#define NNA_DEBUG_CHECK(this, debug) (0)
#endif

#if (USE_OF_RESERVED_MEM == 1)
#include <linux/of_reserved_mem.h>
#endif

/**
 * DOC: NNA Static Variables
 *
 * * nna_sys_class     - NNA system class
 * * init_enable       - NNA install/uninstall infomation enable
 * * dma_mask_bit      - NNA dma mask bit
 */

/**
 * nna_sys_class - NNA system class
 */
static struct class* nna_sys_class = NULL;

/**
 * info_enable module parameter
 */
static int info_enable = 1;
module_param(info_enable, int, S_IRUGO);
MODULE_PARM_DESC(info_enable, "NNA install/uninstall infomation enable");

/**
 * dma_mask_bit module parameter
 */
static int dma_mask_bit = 32;
module_param(dma_mask_bit, int, S_IRUGO);
MODULE_PARM_DESC(dma_mask_bit, "NNA dma mask bit(default=32)");

struct stAllocatedMemArea
{
	dma_addr_t    phy_addr;
	void*         kvirt_addr;
	int           size;
	int           clientId;
	unsigned long vm_start;
	unsigned long vm_end;
	int           sync_mode;
};

/**
 * DOC: NNA Device Data Structure
 *
 * This section defines the structure of NNA device.
 *
 */

/**
 * struct nna_device_data - NNA device data structure.
 */

struct nna_device_data
{
	struct device* sys_dev;
	struct device* dma_dev;
	struct cdev    cdev;
	dev_t          device_number;
	struct mutex   sysfs_lock;
	struct mutex   dev_lock;
	bool           is_open;
	int            sync_mode;
	int            buffers_allocated;
	int            mem_allocated;
#if (USE_OF_RESERVED_MEM == 1)
	bool of_reserved_mem;
#endif
#if ((NNA_DEBUG == 1) && (USE_VMA_FAULT == 1))
	bool debug_vma;
#endif
	struct stAllocatedMemArea allocatedMemAreas[NNA_MAX_CLIENTS][NNA_MAX_ALLOCATED_MEMAREAS_PER_CLIENT];
	int                       halt_irq;
	wait_queue_head_t         nna_wait;
	atomic_t                  nna_halt;
	struct resource           nna_res;
	void* __iomem  nna_reg_base;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
	struct timeval irq_ts;
#else
	struct timespec64 irq_ts;
#endif
	struct clk*    clocks[CLK_NUM];
#ifdef CONFIG_AMLOGIC_DRIVER
	struct abc123_acenna_pd pd[abc123_PD_NUM];
#endif
	u64            clk_freq;
};

/**
 * Open Firmware Device Identifier Matching Table
 */
static struct of_device_id nna_of_match[] = {
	{
		.compatible = "amazon,nna-1.0",
	},
	{/* end of table */}};
MODULE_DEVICE_TABLE(of, nna_of_match);

irqreturn_t nna_halt_interrupt(int irq, void* dev_id);

/**
 * sync_mode(synchronous mode) value
 */
#define SYNC_MODE_INVALID (0x00)
#define SYNC_MODE_NONCACHED (0x01)
#define SYNC_MODE_WRITECOMBINE (0x02)
#define SYNC_MODE_DMACOHERENT (0x03)
#define SYNC_MODE_MASK (0x03)
#define SYNC_MODE_MIN (0x01)
#define SYNC_MODE_MAX (0x03)
#define SYNC_ALWAYS (0x04)

/**
 * DOC: NNA System Class Device File Description
 *
 * This section define the device file created in system class when NNA is
 * loaded into the kernel.
 *
 * The device file created in system class is as follows.
 *
 * * /sys/class/acenna/<device-name>/driver_version
 * * /sys/class/acenna/<device-name>/sync_mode
 * * /sys/class/acenna/<device-name>/buffers_allocated
 * * /sys/class/acenna/<device-name>/mem_allocated
 * *
 */

#define DEF_ATTR_SHOW(__attr_name, __format, __value)                                               \
static ssize_t nna_show_##__attr_name(struct device* dev, struct device_attribute* attr, char* buf) \
{                                                                                                   \
	ssize_t status;                                                                             \
	struct nna_device_data* this = dev_get_drvdata(dev);                                        \
	if (mutex_lock_interruptible(&this->sysfs_lock) != 0)                                       \
		return -ERESTARTSYS;                                                                \
	status = sprintf(buf, __format, (__value));                                                 \
	mutex_unlock(&this->sysfs_lock);                                                            \
	return status;                                                                              \
}

static inline int NO_ACTION(struct nna_device_data* this)
{
	return 0;
}

#define DEF_ATTR_SET(__attr_name, __min, __max, __pre_action, __post_action)                                          \
static ssize_t nna_set_##__attr_name(struct device* dev, struct device_attribute* attr, const char* buf, size_t size) \
{                                                                                                                     \
	ssize_t status;                                                                                               \
	u64     value;                                                                                                \
	struct nna_device_data* this = dev_get_drvdata(dev);                                                          \
	if (0 != mutex_lock_interruptible(&this->sysfs_lock)) { return -ERESTARTSYS; }                                \
	if (0 != (status = kstrtoull(buf, 0, &value))) { goto failed; }                                               \
	if ((value < __min) || (__max < value))                                                                       \
	{                                                                                                             \
		status = -EINVAL;                                                                                     \
		goto failed;                                                                                          \
	}                                                                                                             \
	if (0 != (status = __pre_action(this))) { goto failed; }                                                      \
	this->__attr_name = value;                                                                                    \
	if (0 != (status = __post_action(this))) { goto failed; }                                                     \
	status = size;                                                                                                \
failed:                                                                                                               \
	mutex_unlock(&this->sysfs_lock);                                                                              \
	return status;                                                                                                \
}

static inline int nna_set_clock_frequency(struct nna_device_data* this)
{
	int retval = 0;

	retval = acenna_clk_set_rate(this->clocks, &this->clk_freq);
	if (retval) dev_err(this->sys_dev, "acenna_clk_set_rate failed %d\n", retval);

	return retval;
}

static ssize_t nna_get_clock_frequency(struct device* dev, struct device_attribute* attr, char* buf)
{
	ssize_t status;
	struct nna_device_data* this = dev_get_drvdata(dev);

	if (mutex_lock_interruptible(&this->sysfs_lock) != 0)
		return -ERESTARTSYS;
	status = acenna_clk_get_rate(this->clocks, &this->clk_freq);
	if (status == 0) status = sprintf(buf, "%llu\n", this->clk_freq);
	mutex_unlock(&this->sysfs_lock);

	return status;
}

DEF_ATTR_SHOW(driver_version, "%s\n", DRIVER_VERSION);
DEF_ATTR_SHOW(sync_mode, "%d\n", this->sync_mode);
DEF_ATTR_SET(sync_mode, 0, 7, NO_ACTION, NO_ACTION);
DEF_ATTR_SHOW(buffers_allocated, "%d\n", this->buffers_allocated);
DEF_ATTR_SHOW(mem_allocated, "%d\n", this->mem_allocated);
DEF_ATTR_SET(clk_freq, 1000000, 800000000, NO_ACTION, nna_set_clock_frequency);

static struct device_attribute nna_device_attrs[] = {
	__ATTR(driver_version, 0444, nna_show_driver_version, NULL),
	__ATTR(sync_mode, 0664, nna_show_sync_mode, nna_set_sync_mode),
	__ATTR(buffers_allocated, 0444, nna_show_buffers_allocated, NULL),
	__ATTR(mem_allocated, 0444, nna_show_mem_allocated, NULL),
	__ATTR(clk_freq, 0664, nna_get_clock_frequency, nna_set_clk_freq),
	__ATTR_NULL,
};

#if (USE_DEV_GROUPS == 1)

#define nna_device_attrs_size (sizeof(nna_device_attrs) / sizeof(nna_device_attrs[0]))

static struct attribute* nna_attrs[nna_device_attrs_size] = {
	NULL};
static struct attribute_group nna_attr_group = {
	.attrs = nna_attrs};
static const struct attribute_group* nna_attr_groups[] = {
	&nna_attr_group,
	NULL};

static inline void nna_sys_class_set_attributes(void)
{
	int i;
	for (i = 0; i < nna_device_attrs_size - 1; i++)
	{
		nna_attrs[i] = &(nna_device_attrs[i].attr);
	}
	nna_attrs[i] = NULL;
	nna_sys_class->dev_groups = nna_attr_groups;
}
#else

static inline void nna_sys_class_set_attributes(void)
{
	nna_sys_class->dev_attrs = nna_device_attrs;
}

#endif

static int nna_resume(struct device* dev)
{
#if defined(CONFIG_NNA_MT8169)
	struct nna_device_data* this = dev_get_drvdata(dev);

	acenna_clk_prepare_enable(this->clocks);
	clear_imem(this->nna_reg_base);
#endif
	dev_info(dev, "nna_resume\n");

	return 0;
}

static int nna_suspend(struct device* dev)
{
#if defined(CONFIG_NNA_MT8169)
	struct nna_device_data* this = dev_get_drvdata(dev);

	acenna_clk_disable_unprepare(this->clocks);
#endif
	dev_info(dev, "nna_suspend\n");

	return 0;
}

static int nna_runtime_resume(struct device *dev)
{
	int retval = 0;
#if defined(CONFIG_NNA_MT8169)
	struct nna_device_data* this = dev_get_drvdata(dev);

	acenna_clk_prepare_enable(this->clocks);
	clear_imem(this->nna_reg_base);
#endif
	dev_info(dev, "nna_runtime_resume\n");

	return retval;
}

static int nna_runtime_suspend(struct device* dev)
{
	int retval = 0;
#if defined(CONFIG_NNA_MT8169)
	struct nna_device_data* this = dev_get_drvdata(dev);

	acenna_clk_disable_unprepare(this->clocks);
#endif
	dev_info(dev, "nna_runtime_suspend\n");

	return retval;
}

static int nna_runtime_idle(struct device* dev)
{
	dev_info(dev, "nna_runtime_idle\n");
	return 0;
}

#if (USE_VMA_FAULT == 1)
/**
 * DOC: NNA Device VM Area Operations
 *
 * This section defines the operation of vm when mmap-ed the NNA device file.
 *
 * * nna_device_vma_open()  - NNA device vm area open operation.
 * * nna_device_vma_close() - NNA device vm area close operation.
 * * nna_device_vma_fault() - NNA device vm area fault operation.
 * * nna_device_vm_ops      - NNA device vm operation table.
 */

/**
 * nna_device_vma_open() - NNA device vm area open operation.
 * @vma:        Pointer to the vm area structure.
 * Return:  None
 */
static void nna_device_vma_open(struct vm_area_struct* vma)
{
	struct nna_device_data* this = vma->vm_private_data;
	if (NNA_DEBUG_CHECK(this, debug_vma))
		dev_info(this->dma_dev, "vma_open(virt_addr=0x%lx, offset=0x%lx)\n", vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

/**
 * nna_device_vma_close() - NNA device vm area close operation.
 * @vma:        Pointer to the vm area structure.
 * Return:  None
 */
static void nna_device_vma_close(struct vm_area_struct* vma)
{
	struct nna_device_data* this = vma->vm_private_data;
	if (NNA_DEBUG_CHECK(this, debug_vma))
		dev_info(this->dma_dev, "vma_close()\n");
}

/**
 * _nna_device_vma_fault() - NNA device vm area fault operation.
 * VM_FAULT_RETURN_TYPE - Type of nna_device_vma_fault() return value.
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0))
typedef vm_fault_t VM_FAULT_RETURN_TYPE;
#else
typedef int        VM_FAULT_RETURN_TYPE;
#endif

/**
 * _nna_device_vma_fault() - NNA device vm area fault operation.
 * @vma:        Pointer to the vm area structure.
 * @vfm:        Pointer to the vm fault structure.
 * Return:      VM_FAULT_RETURN_TYPE (Success(=0) or error status(!=0)).
 */

static inline VM_FAULT_RETURN_TYPE _nna_device_vma_fault(struct vm_area_struct* vma, struct vm_fault* vmf)
{
	struct nna_device_data* this = vma->vm_private_data;
	struct page*  page_ptr = NULL;
	unsigned long offset = vmf->pgoff << PAGE_SHIFT;
	unsigned long phys_addr = 0;
	unsigned long page_frame_num = 0;
	unsigned long request_size = 1 << PAGE_SHIFT;
	unsigned long available_size = 0;
	int           clientid, memid;
	int           found = 0;
	for (clientid = 0; clientid < NNA_MAX_CLIENTS; clientid++)
		for (memid = 0; memid < NNA_MAX_ALLOCATED_MEMAREAS_PER_CLIENT; memid++)
		{
			if (this->allocatedMemAreas[clientid][memid].vm_start == vma->vm_start && this->allocatedMemAreas[clientid][memid].size != 0)
			{
				phys_addr = this->allocatedMemAreas[clientid][memid].phy_addr + offset;
				available_size = this->allocatedMemAreas[clientid][memid].size - offset;
				//printk(KERN_ALERT "Vma fault called and satisfied for vm_start: %llx offset: %llx phy_addr: %llx\n",vma->vm_start, offset, phys_addr);
				found = 1;
				break;
			}
		}

	if (found == 0)
	{
		printk(KERN_ALERT "Alert vma fault called but mapping not found! vm_start: %lx offset: %lx \n", vma->vm_start, offset);
		return VM_FAULT_SIGBUS;
	}

	page_frame_num = phys_addr >> PAGE_SHIFT;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
	if (NNA_DEBUG_CHECK(this, debug_vma))
		dev_info(this->dma_dev,
				 "vma_fault(virt_addr=0x%lx, phys_addr=%pad)\n",
				 vmf->address,
				 &phys_addr);
#else
	if (NNA_DEBUG_CHECK(this, debug_vma))
		dev_info(this->dma_dev,
				 "vma_fault(virt_addr=0x%lx, phys_addr=%pad)\n",
				 (long unsigned int)vmf->virtual_address,
				 &phys_addr);
#endif

	if (request_size > available_size)
		return VM_FAULT_SIGBUS;
	if (!pfn_valid(page_frame_num))
		return VM_FAULT_SIGBUS;
	page_ptr = pfn_to_page(page_frame_num);
	get_page(page_ptr);
	//printk(KERN_ALERT "page_frame_num: %llx page_to_phys: %llx",page_frame_num, page_to_phys(page_ptr));
	vmf->page = page_ptr;
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
/**
 * nna_device_vma_fault() - NNA device vm area fault operation.
 * @vfm:        Pointer to the vm fault structure.
 * Return:      VM_FAULT_RETURN_TYPE (Success(=0) or error status(!=0)).
 */
static VM_FAULT_RETURN_TYPE nna_device_vma_fault(struct vm_fault* vmf)
{
	return _nna_device_vma_fault(vmf->vma, vmf);
}
#else
/**
 * nna_device_vma_fault() - NNA device vm area fault operation.
 * @vma:        Pointer to the vm area structure.
 * @vfm:        Pointer to the vm fault structure.
 * Return:      Success(=0) or error status(<0).
 */
static VM_FAULT_RETURN_TYPE nna_device_vma_fault(struct vm_area_struct* vma, struct vm_fault* vmf)
{
	return _nna_device_vma_fault(vma, vmf);
}
#endif

/**
 * NNA device VM operation table.
 */
static const struct vm_operations_struct nna_device_vm_ops = {
	.open = nna_device_vma_open,
	.close = nna_device_vma_close,
	.fault = nna_device_vma_fault,
};

#endif /* #if (USE_VMA_FAULT == 1) */

void buffers_cleanup(struct nna_device_data* this)
{
	int clientid, memid;
	for (clientid = 0; clientid < NNA_MAX_CLIENTS; clientid++)
		for (memid = 0; memid < NNA_MAX_ALLOCATED_MEMAREAS_PER_CLIENT; memid++)
		{
			if (this->allocatedMemAreas[clientid][memid].kvirt_addr != 0)
			{
				dma_free_coherent(this->dma_dev, this->allocatedMemAreas[clientid][memid].size, this->allocatedMemAreas[clientid][memid].kvirt_addr, this->allocatedMemAreas[clientid][memid].phy_addr);
				this->mem_allocated -= this->allocatedMemAreas[clientid][memid].size;
				this->allocatedMemAreas[clientid][memid].kvirt_addr = 0;
				this->allocatedMemAreas[clientid][memid].phy_addr = 0;
				this->allocatedMemAreas[clientid][memid].size = 0;
				this->buffers_allocated--;
			}
		}
}

/**
 * DOC: NNA Device File Operations
 *
 * This section defines the operation of the NNA device file.
 *
 * * nna_device_file_open()    - NNA device file open operation.
 * * nna_device_file_release() - NNA device file release operation.
 * * nna_device_file_mmap()    - NNA device file memory map operation.
 * * nna_device_file_read()    - NNA device file read operation.
 * * nna_device_file_write()   - NNA device file write operation.
 * * nna_device_file_llseek()  - NNA device file llseek operation.
 * * nna_device_file_ops       - NNA device file operation table.
 */

/**
 * nna_device_file_open() - NNA device file open operation.
 * @inode:  Pointer to the inode structure of this device.
 * @file:   Pointer to the file structure.
 * Return:      Success(=0) or error status(<0).
 */
static int nna_device_file_open(struct inode* inode, struct file* file)
{
	struct nna_device_data* this;
	int status = 0;

	this = container_of(inode->i_cdev, struct nna_device_data, cdev);
	dev_dbg(this->sys_dev, "Opening device.\n");
	if (mutex_trylock(&this->dev_lock) == 0)
	{
		dev_err(this->sys_dev, "Cannot open, device already locked.\n");
		return -ERESTARTSYS;
	}
	dev_dbg(this->sys_dev, "Device open!\n");
	file->private_data = this;
	this->is_open = 1;

	return status;
}

/**
 * nna_device_file_release() - NNA device file release operation.
 * @inode:  Pointer to the inode structure of this device.
 * @file:   Pointer to the file structure.
 * Return:      Success(=0) or error status(<0).
 */
static int nna_device_file_release(struct inode* inode, struct file* file)
{
	struct nna_device_data* this = file->private_data;
	dev_dbg(this->sys_dev, "Closing device.\n");
	buffers_cleanup(this);
	ioread32(this->nna_reg_base + 0x44);
	atomic_set(&this->nna_halt, 0);
	this->is_open = 0;
	mutex_unlock(&this->dev_lock);
	return 0;
}

/**
 * _PGPROT_NONCACHED    : vm_page_prot value when ((sync_mode & SYNC_MODE_MASK) == SYNC_MODE_NONCACHED   )
 * _PGPROT_WRITECOMBINE : vm_page_prot value when ((sync_mode & SYNC_MODE_MASK) == SYNC_MODE_WRITECOMBINE)
 * _PGPROT_DMACOHERENT  : vm_page_prot value when ((sync_mode & SYNC_MODE_MASK) == SYNC_MODE_DMACOHERENT )
 */
#if defined(CONFIG_ARM)
#define _PGPROT_NONCACHED(vm_page_prot) pgprot_noncached(vm_page_prot)
#define _PGPROT_WRITECOMBINE(vm_page_prot) pgprot_writecombine(vm_page_prot)
#define _PGPROT_DMACOHERENT(vm_page_prot) pgprot_dmacoherent(vm_page_prot)
#elif defined(CONFIG_ARM64)
#define _PGPROT_NONCACHED(vm_page_prot) pgprot_writecombine(vm_page_prot)
#define _PGPROT_WRITECOMBINE(vm_page_prot) pgprot_writecombine(vm_page_prot)
#define _PGPROT_DMACOHERENT(vm_page_prot) pgprot_writecombine(vm_page_prot)
#else
#define _PGPROT_NONCACHED(vm_page_prot) pgprot_noncached(vm_page_prot)
#define _PGPROT_WRITECOMBINE(vm_page_prot) pgprot_writecombine(vm_page_prot)
#define _PGPROT_DMACOHERENT(vm_page_prot) pgprot_writecombine(vm_page_prot)
#endif

static long nna_device_file_ioctl(struct file* filp, unsigned int cmd, unsigned long arg)
{
	struct nna_device_data* this = filp->private_data;

	stMemAreaRequest  memAreaRequest;
	stMemAreaRequest* memAreaRequestP = &memAreaRequest;

	regAccessRequest_t regReq;
	void __iomem* regAddr;
	stMemAreaSync memAreaSync;

	int clientid = 0;
	int memid = 0;
	int size = 0;
	int offset = 0;
	int data_direction = 0;
	int alloc_size = 0;
	int ret = 0;

	timeval_exch irq_ts = {.tv_sec = 0, .tv_usec = 0};

	if ((cmd != NNA_IOCTL_CLEAR_POLL) && (!arg))
		return -EFAULT;

	if ((cmd == NNA_IOCTL_ALLOC) || (cmd == NNA_IOCTL_DEALLOC))
	{
		if (copy_from_user(memAreaRequestP, (stMemAreaRequest*)arg, sizeof(stMemAreaRequest))) return -EFAULT;
		clientid = memAreaRequestP->clientid;
		memid = memAreaRequestP->memid;
		size = memAreaRequestP->size;
		dev_dbg(this->sys_dev, "ACENNA: ioctl cmd: %u clientid: %d memid: %d size:%d\n", cmd, clientid, memid, size);
	}

	if ((cmd == NNA_IOCTL_SYNC_FOR_DEVICE) || (cmd == NNA_IOCTL_SYNC_FOR_CPU))
	{
		ret = copy_from_user(&memAreaSync, (stMemAreaSync*)arg, sizeof(stMemAreaSync));
		if (ret)
			return -EFAULT;
		clientid = memAreaSync.clientid;
		memid = memAreaSync.memid;
		offset = memAreaSync.offset;
		data_direction = memAreaSync.data_direction;
		size = memAreaSync.size;
		if (!this->allocatedMemAreas[clientid][memid].phy_addr || !this->allocatedMemAreas[clientid][memid].kvirt_addr || !this->allocatedMemAreas[clientid][memid].size)
		{
			dev_err(this->sys_dev, "Cannot sync null buffer for clientid %d and memid %d\n", clientid, memid);
			return -ENXIO;
		}
		if (offset + size > this->allocatedMemAreas[clientid][memid].size)
		{
			dev_err(this->sys_dev, "Cannot sync. offset %d and size %d out of bound for buffer size %d\n", offset, size, this->allocatedMemAreas[clientid][memid].size);
			return -EINVAL;
		}
		if ((this->allocatedMemAreas[clientid][memid].sync_mode == SYNC_MODE_NONCACHED) || (filp->f_flags & O_SYNC) || (memAreaSync.size == 0))
		{
			return 0;
		}
	}

	if ((cmd == NNA_IOCTL_REG_READ) || (cmd == NNA_IOCTL_REG_WRITE))
	{
		ret = copy_from_user(&regReq, (regAccessRequest_t*)arg, sizeof(regAccessRequest_t));
		if (ret)
			return -EFAULT;
		if ((regReq.offset < 0) || (regReq.offset > (resource_size(&this->nna_res) / sizeof(u32))))
			return -EINVAL;
	}

	switch (cmd)
	{

	case NNA_IOCTL_ALLOC:
		//arg is a void * to a stMemAreaRequest
		//return val is the phy addr of the allocated area (mmap will give the user space virt addr)
		clientid = memAreaRequestP->clientid;
		memid = memAreaRequestP->memid;
		size = memAreaRequestP->size;

		if (!(clientid >= 0 && clientid < NNA_MAX_CLIENTS && memid >= 0 && memid < NNA_MAX_ALLOCATED_MEMAREAS_PER_CLIENT && size > 0 && size < NNA_MAX_MEMAREA_SIZE))
		{

			printk(KERN_ERR "ACENNA: ioctl arguments out of range\n");
			return -EFAULT;
		}

		if (this->dma_dev->dma_mask == NULL)
		{
			this->dma_dev->dma_mask = &this->dma_dev->coherent_dma_mask;
		}

		if (*this->dma_dev->dma_mask == 0)
		{
			if (dma_set_mask(this->dma_dev, DMA_BIT_MASK(dma_mask_bit)) == 0)
			{
				dma_set_coherent_mask(this->dma_dev, DMA_BIT_MASK(dma_mask_bit));
			}
			else
			{
				printk(KERN_WARNING "ACENNA: dma_set_mask(DMA_BIT_MASK(%d)) failed\n", dma_mask_bit);
				dma_set_mask(this->dma_dev, DMA_BIT_MASK(32));
				dma_set_coherent_mask(this->dma_dev, DMA_BIT_MASK(32));
			}
		}

		alloc_size = ((size + ((1 << PAGE_SHIFT) - 1)) >> PAGE_SHIFT) << PAGE_SHIFT;
		if (this->allocatedMemAreas[clientid][memid].kvirt_addr)
		{
			printk(KERN_ERR "ACENNA: trying to reallocate the same area for same client clientid: %d memid: %d size: %d \n", clientid, memid, size);
			return -EFAULT;
		}
		this->allocatedMemAreas[clientid][memid].kvirt_addr = dma_alloc_coherent(this->dma_dev, alloc_size, &(this->allocatedMemAreas[clientid][memid].phy_addr), GFP_KERNEL);

		if (IS_ERR_OR_NULL(this->allocatedMemAreas[clientid][memid].kvirt_addr))
		{
			int retval = PTR_ERR(this->allocatedMemAreas[clientid][memid].kvirt_addr);
			printk(KERN_ERR "ACENNA: dma_alloc_coherent() failed. return(%d)\n", retval);
			this->allocatedMemAreas[clientid][memid].kvirt_addr = 0;
			this->allocatedMemAreas[clientid][memid].phy_addr = 0;
			this->allocatedMemAreas[clientid][memid].size = 0;
			return (retval == 0) ? -ENOMEM : retval;
		}
		else
		{
			this->allocatedMemAreas[clientid][memid].size = alloc_size;
			this->allocatedMemAreas[clientid][memid].sync_mode = memAreaRequestP->sync_mode;
			this->mem_allocated += alloc_size;
			this->buffers_allocated++;
			if (info_enable)
				dev_dbg(this->sys_dev, "ACENNA: dma_alloc_coherent() success clientid: %d memid: %d alloc_size: %d phy_addr: %pa\n", clientid, memid, alloc_size, &this->allocatedMemAreas[clientid][memid].phy_addr);
			memAreaRequestP->phy_addr = this->allocatedMemAreas[clientid][memid].phy_addr;
			if (copy_to_user((void*)arg, memAreaRequestP, sizeof(stMemAreaRequest))) return -EFAULT;
			return 0;
		}

		break;

	case NNA_IOCTL_DEALLOC:
		//arg is a void * to a stMemAreaRequest
		clientid = memAreaRequestP->clientid;
		memid = memAreaRequestP->memid;
		if (!(clientid >= 0 && clientid < NNA_MAX_CLIENTS && memid >= 0 && memid < NNA_MAX_ALLOCATED_MEMAREAS_PER_CLIENT))
		{
			printk(KERN_ALERT "ACENNA: ioctl dealloc arguments out of range\n");
			return -EFAULT;
		}
		if (this->allocatedMemAreas[clientid][memid].kvirt_addr && this->allocatedMemAreas[clientid][memid].size > 0)
		{
			dev_dbg(this->sys_dev, "ACENNA: ioctl dealloc clientid: %d memid: %d size:%d\n", clientid, memid, this->allocatedMemAreas[clientid][memid].size);
			dma_free_coherent(this->dma_dev, this->allocatedMemAreas[clientid][memid].size, this->allocatedMemAreas[clientid][memid].kvirt_addr, this->allocatedMemAreas[clientid][memid].phy_addr);
			this->mem_allocated -= this->allocatedMemAreas[clientid][memid].size;
			this->allocatedMemAreas[clientid][memid].kvirt_addr = 0;
			this->allocatedMemAreas[clientid][memid].phy_addr = 0;
			this->allocatedMemAreas[clientid][memid].size = 0;
			this->buffers_allocated--;
			return 0;
		}
		else
		{
			printk(KERN_ALERT "ACENNA: ioctl dealloc trying to dealloc unallocated cma memory clientid:%d memid: %d\n", clientid, memid);
			return -EFAULT;
		}

		break;

	case NNA_IOCTL_TRIGGER:
		printk(KERN_WARNING "ACENNA: NNA_IOCTL_TRIGGER Deprecated. Please update\n");
		break;

	case NNA_IOCTL_HW_INT_TIME:

		/** copy over timestamp to a fixed bit-structure */
		irq_ts.tv_sec = this->irq_ts.tv_sec;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
		irq_ts.tv_usec = this->irq_ts.tv_usec;
#else
		irq_ts.tv_usec = this->irq_ts.tv_nsec / 1000;
#endif

		/** copy that structure from kernel to userspace */
		ret = copy_to_user((void*)arg, &irq_ts, sizeof(struct timeval_exch));

		if (ret)
		{
			return -EFAULT;
		}
		break;

	case NNA_IOCTL_REG_READ:
		regAddr = (void __iomem*)(((uint32_t*)(this->nna_reg_base)) + regReq.offset);
		regReq.value = ioread32(regAddr);
		ret = copy_to_user((void*)arg, &regReq, sizeof(regAccessRequest_t));
		if (ret)
			return -EFAULT;
		dev_dbg(this->sys_dev, "ACENNA: R Addr=0x%p Offset=0x%08X Value=0x%08X\n",
				regAddr, regReq.offset, regReq.value);
		break;

	case NNA_IOCTL_REG_WRITE:
		regAddr = (void __iomem*)(((uint32_t*)(this->nna_reg_base)) + regReq.offset);
		iowrite32(regReq.value, regAddr);
		dev_dbg(this->sys_dev, "ACENNA: W Addr=0x%p Offset=0x%08X Value=0x%08X\n",
				regAddr, regReq.offset, regReq.value);
		break;

	case NNA_IOCTL_CLEAR_POLL:
		dev_dbg(this->sys_dev, "ACENNA: Clear Poll Event\n");
		atomic_set(&this->nna_halt, 0);
		break;

	case NNA_IOCTL_SYNC_FOR_DEVICE:
		dma_sync_single_for_device(this->dma_dev, this->allocatedMemAreas[clientid][memid].phy_addr, this->allocatedMemAreas[clientid][memid].size, data_direction);
		break;
	case NNA_IOCTL_SYNC_FOR_CPU:
		dma_sync_single_for_cpu(this->dma_dev, this->allocatedMemAreas[clientid][memid].phy_addr, this->allocatedMemAreas[clientid][memid].size, data_direction);
		break;
	default:
		printk(KERN_ALERT "ACENNA: invalid ioctl cmd: %u\n", cmd);
		return -ENOTTY;
	}

	return 0;
}

/**
 * nna_device_file_mmap() - NNA device file memory map operation.
 * @file:   Pointer to the file structure.
 * @vma:        Pointer to the vm area structure.
 * Return:      Success(=0) or error status(<0).
 */
static int nna_device_file_mmap(struct file* file, struct vm_area_struct* vma)
{
	//offset parameter  of mmap carries clientid and memid in concataneted format
	//offset = userid * 1000 + memid;
	int ret;
	struct nna_device_data* this = file->private_data;
	int clientid = vma->vm_pgoff / 1000;
	int memid = vma->vm_pgoff % 1000;
	dev_dbg(this->sys_dev, "ACENNA: mmap call for clientid: %d memid:%d\n", clientid, memid);
	if (!(clientid >= 0 && clientid < NNA_MAX_CLIENTS && memid >= 0 && memid < NNA_MAX_ALLOCATED_MEMAREAS_PER_CLIENT))
	{
		printk(KERN_ALERT "ACENNA: ioctl arguments out of range\n");
		return -EFAULT;
	}
	if (!(this->allocatedMemAreas[clientid][memid].kvirt_addr && this->allocatedMemAreas[clientid][memid].size))
	{
		printk(KERN_ALERT "ACENNA: mmap asked for invalid area clientid: %d memid:%d\n", clientid, memid);
		return -EFAULT;
	}
	//TODO Add check here for validity of the area

	if ((file->f_flags & O_SYNC) | (this->sync_mode & SYNC_ALWAYS) && (this->allocatedMemAreas[clientid][memid].sync_mode == 0))
	{
		switch (this->sync_mode & SYNC_MODE_MASK)
		{
		case SYNC_MODE_NONCACHED:
			vma->vm_flags |= VM_IO;
			vma->vm_page_prot = _PGPROT_NONCACHED(vma->vm_page_prot);
			break;
		case SYNC_MODE_WRITECOMBINE:
			vma->vm_flags |= VM_IO;
			vma->vm_page_prot = _PGPROT_WRITECOMBINE(vma->vm_page_prot);
			break;
		case SYNC_MODE_DMACOHERENT:
			vma->vm_flags |= VM_IO;
			vma->vm_page_prot = _PGPROT_DMACOHERENT(vma->vm_page_prot);
			break;
		default:
			break;
		}
	}
	else
	{
		switch (this->allocatedMemAreas[clientid][memid].sync_mode & SYNC_MODE_MASK)
		{
		case SYNC_MODE_NONCACHED:
			vma->vm_flags |= VM_IO;
			vma->vm_page_prot = _PGPROT_NONCACHED(vma->vm_page_prot);
			break;
		case SYNC_MODE_WRITECOMBINE:
			vma->vm_flags |= VM_IO;
			vma->vm_page_prot = _PGPROT_WRITECOMBINE(vma->vm_page_prot);
			break;
		case SYNC_MODE_DMACOHERENT:
			vma->vm_flags |= VM_IO;
			vma->vm_page_prot = _PGPROT_DMACOHERENT(vma->vm_page_prot);
			break;
		default:
			break;
		}
	}
	vma->vm_private_data = this;
	vma->vm_pgoff = 0;
	this->allocatedMemAreas[clientid][memid].vm_start = vma->vm_start;
	this->allocatedMemAreas[clientid][memid].vm_end = vma->vm_end;
	dev_dbg(this->sys_dev, "ACENNA: mmap success vm_start:%lx vm_end:%lx for clientid: %d memid:%d\n", vma->vm_start, vma->vm_end, clientid, memid);
#if (USE_VMA_FAULT == 1)
	{
		//AD Touched here:
		unsigned long page_frame_num = this->allocatedMemAreas[clientid][memid].phy_addr >> PAGE_SHIFT;
		if (pfn_valid(page_frame_num))
		{
			vma->vm_ops = &nna_device_vm_ops;
			nna_device_vma_open(vma);
			return 0;
		}
	}
#endif
	ret = dma_mmap_coherent(this->dma_dev, vma, this->allocatedMemAreas[clientid][memid].kvirt_addr, this->allocatedMemAreas[clientid][memid].phy_addr, this->allocatedMemAreas[clientid][memid].size);
	if (0 == ret)
	{
		this->allocatedMemAreas[clientid][memid].vm_start = vma->vm_start;
		this->allocatedMemAreas[clientid][memid].vm_end = vma->vm_end;
		dev_dbg(this->sys_dev, "ACENNA: mmap success vm_start:%lx vm_end:%lx for clientid: %d memid:%d\n", vma->vm_start, vma->vm_end, clientid, memid);
	}
	else
	{
		this->allocatedMemAreas[clientid][memid].vm_start = 0;
		this->allocatedMemAreas[clientid][memid].vm_end = 0;
		printk(KERN_ALERT "ACENNA: mmap error: %d for clientid: %d memid:%d\n", ret, clientid, memid);
	}
	return ret;
}

unsigned int nna_device_file_poll(struct file* file_ptr, struct poll_table_struct* wait)
{
	unsigned int mask = 0;
	struct nna_device_data* this = file_ptr->private_data;
	if (this->halt_irq < 0)
	{
		pr_info("No IRQ Registered.\n");
		return 0;
	}

	poll_wait(file_ptr, &this->nna_wait, wait);
	//  Finished with context. Executed HALT
	if (atomic_read(&this->nna_halt) > 0)
	{
		mask |= (POLLIN);
	}
	return mask;
}

/**
 * NNA device file operation table.
 */
static const struct file_operations nna_device_file_ops = {
	.owner = THIS_MODULE,
	.open = nna_device_file_open,
	.release = nna_device_file_release,
	.mmap = nna_device_file_mmap,
	.poll = nna_device_file_poll,
	.unlocked_ioctl = nna_device_file_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = nna_device_file_ioctl,
#endif
};

/**
 * DOC: NNA Device Data Operations
 *
 * This section defines the operation of NNA device data.
 *
 * * nna_device_ida         - NNA Device Minor Number allocator variable.
 * * nna_device_number      - NNA Device Major Number.
 * * nna_device_create()    - Create NNA device data.
 * * nna_device_setup()     - Setup the NNA device data.
 * * nna_device_info()      - Print infomation the NNA device data.
 * * nna_device_destroy()   - Destroy the NNA device data.
 */
static DEFINE_IDA(nna_device_ida);
static dev_t nna_device_number = 0;

/**
 * nna_device_create() -  Create NNA device data.
 * @name:       device name   or NULL.
 * @parent:     parent device or NULL.
 * @minor:      minor_number  or -1.
 * Return:      Pointer to the NNA device data or NULL.
 */
static struct nna_device_data* nna_device_create(const char* name, struct device* parent, int minor)
{
	struct nna_device_data* this = NULL;
	unsigned int       done = 0;
	const unsigned int DONE_ALLOC_MINOR = (1 << 0);
	const unsigned int DONE_CHRDEV_ADD = (1 << 1);
	const unsigned int DONE_DEVICE_CREATE = (1 << 3);
	/*
	 * allocate device minor number
	 */
	{
		if ((0 <= minor) && (minor < DEVICE_MAX_NUM))
		{
			if (ida_simple_get(&nna_device_ida, minor, minor + 1, GFP_KERNEL) < 0)
			{
				printk(KERN_ERR "couldn't allocate minor number(=%d).\n", minor);
				goto failed;
			}
		}
		else if (minor == -1)
		{
			if ((minor = ida_simple_get(&nna_device_ida, 0, DEVICE_MAX_NUM, GFP_KERNEL)) < 0)
			{
				printk(KERN_ERR "couldn't allocate new minor number. return=%d.\n", minor);
				goto failed;
			}
		}
		else
		{
			printk(KERN_ERR "invalid minor number(=%d), valid range is 0 to %d\n", minor, DEVICE_MAX_NUM - 1);
			goto failed;
		}
		done |= DONE_ALLOC_MINOR;
	}
	/*
	 * create (nna_device_data*) this.
	 */
	{
		this = kzalloc(sizeof(*this), GFP_KERNEL);
		if (IS_ERR_OR_NULL(this))
		{
			int retval = PTR_ERR(this);
			this = NULL;
			printk(KERN_ERR "kzalloc() failed. return=%d\n", retval);
			goto failed;
		}
	}
	/*
	 * set device_number
	 */
	{
		this->device_number = MKDEV(MAJOR(nna_device_number), minor);
	}
	/*
	 * register /sys/class/acenna/<name>
	 */
	{
		if (name == NULL)
		{
			this->sys_dev = device_create(nna_sys_class,
										  parent,
										  this->device_number,
										  (void*)this,
										  DEVICE_NAME_FORMAT, MINOR(this->device_number));
		}
		else
		{
			this->sys_dev = device_create(nna_sys_class,
										  parent,
										  this->device_number,
										  (void*)this,
										  "%s", name);
		}
		if (IS_ERR_OR_NULL(this->sys_dev))
		{
			int retval = PTR_ERR(this->sys_dev);
			this->sys_dev = NULL;
			printk(KERN_ERR "device_create() failed. return=%d\n", retval);
			goto failed;
		}
		done |= DONE_DEVICE_CREATE;
	}
	/*
	 * add chrdev.
	 */
	{
		int retval;
		cdev_init(&this->cdev, &nna_device_file_ops);
		this->cdev.owner = THIS_MODULE;
		if ((retval = cdev_add(&this->cdev, this->device_number, 1)) != 0)
		{
			printk(KERN_ERR "cdev_add() failed. return=%d\n", retval);
			goto failed;
		}
		done |= DONE_CHRDEV_ADD;
	}
	/*
	 * set dma_dev
	 */
	if (parent != NULL)
		this->dma_dev = parent;
	else
		this->dma_dev = this->sys_dev;
	/*
	 * initialize other variables.
	 */
	{
		this->sync_mode = SYNC_MODE_NONCACHED;
	}
#if (USE_OF_RESERVED_MEM == 1)
	{
		this->of_reserved_mem = 0;
	}
#endif
#if ((NNA_DEBUG == 1) && (USE_VMA_FAULT == 1))
	{
		this->debug_vma = 0;
	}
#endif
	mutex_init(&this->sysfs_lock);
	mutex_init(&this->dev_lock);

	return this;

failed:
	if (done & DONE_CHRDEV_ADD) { cdev_del(&this->cdev); }
	if (done & DONE_DEVICE_CREATE) { device_destroy(nna_sys_class, this->device_number); }
	if (done & DONE_ALLOC_MINOR) { ida_simple_remove(&nna_device_ida, minor); }
	if (this != NULL) { kfree(this); }
	return NULL;
}

/**
 * nna_device_destroy() -  Destroy the NNA device data.
 * @this:       Pointer to the NNA device data.
 * Return:      Success(=0) or error status(<0).
 *
 * Unregister the device after releasing the resources.
 */
static int nna_device_destroy(struct nna_device_data* this)
{
	if (!this)
		return -ENODEV;

	buffers_cleanup(this);
	cdev_del(&this->cdev);
	device_destroy(nna_sys_class, this->device_number);
	ida_simple_remove(&nna_device_ida, MINOR(this->device_number));
#ifdef CONFIG_AMLOGIC_DRIVER
	abc123_pd_remove(this->pd);
#endif
	kfree(this);
	return 0;
}

/**
 * DOC: NNA Platform Driver
 *
 * This section defines the NNA platform driver.
 *
 * * nna_platform_driver_probe()   - Probe call for the device.
 * * nna_platform_driver_remove()  - Remove call for the device.
 * * nna_of_match                  - Open Firmware Device Identifier Matching Table.
 * * nna_platform_driver           - Platform Driver Structure.
 */

/**
 * nna_platform_driver_cleanup()   - Clean Up NNA platform driver
 * @pdev:   handle to the platform device structure.
 * @devdata     Pointer to the NNA device data structure.
 * Return:      Success(=0) or error status(<0).
 */
static int nna_platform_driver_cleanup(struct platform_device* pdev, struct nna_device_data* devdata)
{
	int retval = 0;

	if (devdata != NULL)
	{
#if (USE_OF_RESERVED_MEM == 1)
		bool of_reserved_mem = devdata->of_reserved_mem;
#endif
#if 0
		acenna_clk_disable_unprepare(devdata->clocks);
#endif

		if (pm_runtime_enabled(&pdev->dev)) {
			retval = pm_runtime_put_sync(&pdev->dev);
			if (retval)
				dev_info(&pdev->dev, "pm_runtime_put_sync failed %d", retval);
			pm_runtime_disable(&pdev->dev);
		}

		if (devdata->halt_irq > 0)
		{
			free_irq(devdata->halt_irq, devdata);
			devdata->halt_irq = 0;
		}

		if (devdata->nna_reg_base)
			iounmap(devdata->nna_reg_base);

		release_mem_region(devdata->nna_res.start, resource_size(&devdata->nna_res));

		retval = nna_device_destroy(devdata);
		dev_set_drvdata(&pdev->dev, NULL);
#if (USE_OF_RESERVED_MEM == 1)
		if (of_reserved_mem)
		{
			of_reserved_mem_device_release(&pdev->dev);
		}
#endif
	}
	else
	{
		retval = -ENODEV;
	}
	return retval;
}

/**
 * nna_platform_driver_probe() -  Probe call for the device.
 * @pdev:   handle to the platform device structure.
 * Return:      Success(=0) or error status(<0).
 *
 * It does all the memory allocation and registration for the device.
 */
static int nna_platform_driver_probe(struct platform_device* pdev)
{
	int                        retval = 0;
	int                        of_status = 0;
	unsigned int               of_u32_value = 0;
	int                        minor_number = -1;
	struct nna_device_data*    device_data = NULL;
	const char*                device_name = NULL;
	const struct of_device_id* match;

	dev_dbg(&pdev->dev, "NNA Driver probe start.\n");

	match = of_match_device(nna_of_match, &pdev->dev);
	if (!match)
	{
		dev_err(&pdev->dev, "Device does not match nna_of_match\n");
		return -EINVAL;
	}

	of_status = of_property_read_u32(pdev->dev.of_node, "minor-number", &of_u32_value);
	minor_number = (of_status == 0) ? of_u32_value : -1;

	device_name = of_get_property(pdev->dev.of_node, "device-name", NULL);
	if (IS_ERR_OR_NULL(device_name))
	{
		device_name = NULL;
	}

	/*
	 * nna_device_create()
	 */
	device_data = nna_device_create(device_name, &pdev->dev, minor_number);
	if (IS_ERR_OR_NULL(device_data))
	{
		retval = PTR_ERR(device_data);
		dev_err(&pdev->dev, "Device create failed %d\n", retval);
		device_data = NULL;
		retval = (retval == 0) ? -EINVAL : retval;
		goto failed;
	}
	dev_set_drvdata(&pdev->dev, device_data);

	retval = of_address_to_resource(pdev->dev.of_node, 0, &device_data->nna_res);
	if (retval)
	{
		dev_err(&pdev->dev, "Parsing register info failed %d\n", retval);
		goto failed;
	}

	dev_info(&pdev->dev, "Reg Map Info Start %pa Size %pa\n", &device_data->nna_res.start,
			 &(resource_size_t){resource_size(&device_data->nna_res)});

	if (!request_mem_region(device_data->nna_res.start,
							resource_size(&device_data->nna_res),
							"nna_register_memory"))
	{
		dev_err(&pdev->dev, "Register Mem request failed\n");
		goto failed;
	}

	device_data->nna_reg_base = of_iomap(pdev->dev.of_node, 0);
	if (!device_data->nna_reg_base)
	{
		dev_err(&pdev->dev, "IO map failed\n");
		goto failed;
	}
	dev_info(&pdev->dev, "Register mapped to 0x%p\n", device_data->nna_reg_base);

#ifdef CONFIG_AMLOGIC_DRIVER
	/* init power domains*/
	if (of_property_read_bool(pdev->dev.of_node, "power-domains")) {
		retval = abc123_pd_init(&pdev->dev, device_data->pd);
		if (retval) {
			abc123_pd_remove(device_data->pd);
			dev_err(&pdev->dev, "Power domain init failed %d\n", retval);
			goto failed;
		}
	}
	/* set power domains on*/
	abc123_pd_on(device_data->pd);
#endif

	device_data->halt_irq = platform_get_irq(pdev, 0);
	if (device_data->halt_irq < 0)
	{
		dev_err(&pdev->dev, "Cannot get NNA Halt IRQ\n");
		goto failed;
	}

	dev_info(&pdev->dev, "NNA Halt Irq found %d\n", device_data->halt_irq);
	of_status = nna_irq_wrapper_request(device_data->halt_irq, nna_halt_interrupt, IRQF_SHARED | IRQF_TRIGGER_HIGH, device_name, device_data, MINOR(device_data->device_number));
	if (of_status)
	{
		pr_err("Cannot request halt irq %d (%d)\n", device_data->halt_irq, of_status);
	}
	/*
	 * of_reserved_mem_device_init()
	 */
#if (USE_OF_RESERVED_MEM == 1)
	if (pdev->dev.of_node != NULL)
	{
		retval = of_reserved_mem_device_init(&pdev->dev);
		if (retval == 0)
		{
			device_data->of_reserved_mem = 1;
		}
		else if (retval != -ENODEV)
		{
			dev_err(&pdev->dev, "of_reserved_mem_device_init failed. return=%d\n", retval);
			goto failed;
		}
	}
#endif
#if (USE_OF_DMA_CONFIG == 1)
	/*
	 * of_dma_configure()
	 * - set pdev->dev->dma_mask
	 * - set pdev->dev->coherent_dma_mask
	 * - call of_dma_is_coherent()
	 * - call arch_setup_dma_ops()
	 */
#if (USE_OF_RESERVED_MEM == 1)
	/* If "memory-region" property is spsecified, of_dma_configure() will not be executed.
	 * Because in that case, it is already executed in of_reserved_mem_device_init().
	 */
	if (device_data->of_reserved_mem == 0)
#endif
	{
		retval = 0;
#if (LINUX_VERSION_CODE >= 0x041200)
		retval = of_dma_configure(&pdev->dev, pdev->dev.of_node, true);
#elif (LINUX_VERSION_CODE >= 0x040C00)
		retval = of_dma_configure(&pdev->dev, pdev->dev.of_node);
#else
		of_dma_configure(&pdev->dev, pdev->dev.of_node);
#endif
		if (retval != 0)
		{
			dev_err(&pdev->dev, "of_dma_configure failed. return=%d\n", retval);
			goto failed;
		}
	}
#endif
	/*
	 * sync-mode property
	 */
	if (of_property_read_u32(pdev->dev.of_node, "sync-mode", &of_u32_value) == 0)
	{
		if ((of_u32_value < SYNC_MODE_MIN) || (of_u32_value > SYNC_MODE_MAX))
		{
			dev_err(&pdev->dev, "invalid sync-mode property value=%d\n", of_u32_value);
			goto failed;
		}
		device_data->sync_mode &= ~SYNC_MODE_MASK;
		device_data->sync_mode |= (int)of_u32_value;
	}

	init_waitqueue_head(&device_data->nna_wait);
	atomic_set(&device_data->nna_halt, 0);

	if (of_property_read_bool(pdev->dev.of_node, "clocks"))
	{
		retval = acenna_clk_init(&pdev->dev, device_data->clocks);
		if (retval == 0) retval = acenna_clk_get_rate(device_data->clocks, &device_data->clk_freq);
//		if (retval == 0) acenna_clk_prepare_enable(device_data->clocks);
	}

	pm_runtime_enable(&pdev->dev);
	retval = pm_runtime_get_sync(&pdev->dev);
	if (retval)
		dev_info(&pdev->dev, "pm_runtime_get_sync failed %d", retval);

	if (info_enable)
	{
		dev_info(device_data->sys_dev, "driver installed.\n");
		dev_info(device_data->sys_dev, "driver version = %s\n", DRIVER_VERSION);
		dev_info(device_data->sys_dev, "major number = %d\n", MAJOR(device_data->device_number));
		dev_info(device_data->sys_dev, "minor number = %d\n", MINOR(device_data->device_number));
	}
	return 0;

failed:
	nna_platform_driver_cleanup(pdev, device_data);

	return retval;
}

/**
 * nna_platform_driver_remove() -  Remove call for the device.
 * @pdev:   Handle to the platform device structure.
 * Return:      Success(=0) or error status(<0).
 *
 * Unregister the device after releasing the resources.
 */
static int nna_platform_driver_remove(struct platform_device* pdev)
{
	struct nna_device_data* this = dev_get_drvdata(&pdev->dev);
	int retval = 0;

	dev_dbg(&pdev->dev, "driver remove start.\n");

	retval = nna_platform_driver_cleanup(pdev, this);

	if (info_enable)
	{
		dev_info(&pdev->dev, "driver removed.\n");
	}
	return retval;
}

static struct dev_pm_ops nna_pm_ops = {
	.suspend = nna_suspend,
	.resume = nna_resume,
	.runtime_suspend = nna_runtime_suspend,
	.runtime_resume = nna_runtime_resume,
	.runtime_idle = nna_runtime_idle,
};

/**
 * Platform Driver Structure
 */
static struct platform_driver nna_platform_driver = {
	.probe = nna_platform_driver_probe,
	.remove = nna_platform_driver_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = DRIVER_NAME,
		.pm = &nna_pm_ops,
		.of_match_table = nna_of_match,
	},
};

/**
 * DOC: NNA Module Operations
 *
 * * nna_module_cleanup()
 * * nna_module_init()
 * * nna_module_exit()
 */

static bool nna_platform_driver_registered = 0;

/**
 * nna_module_cleanup()
 */
static void nna_module_cleanup(void)
{
	if (nna_platform_driver_registered) { platform_driver_unregister(&nna_platform_driver); }
	if (nna_sys_class != NULL) { class_destroy(nna_sys_class); }
	if (nna_device_number != 0) { unregister_chrdev_region(nna_device_number, DEVICE_MAX_NUM); }
	ida_destroy(&nna_device_ida);
}

irqreturn_t nna_halt_interrupt(int irq, void* dev_id)
{
	struct nna_device_data* this = dev_id;
	ioread32(this->nna_reg_base + 0x44);
	atomic_set(&this->nna_halt, 1);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
	do_gettimeofday(&this->irq_ts);
#else
	ktime_get_real_ts64(&this->irq_ts);
#endif

	wake_up_interruptible(&this->nna_wait);
	return IRQ_HANDLED;
}

/**
 * nna_module_init()
 */
static int __init nna_module_init(void)
{
	int retval = 0;

	ida_init(&nna_device_ida);

	retval = alloc_chrdev_region(&nna_device_number, 0, DEVICE_MAX_NUM, DRIVER_NAME);
	if (retval != 0)
	{
		printk(KERN_ERR "%s: couldn't allocate device major number. return=%d\n", DRIVER_NAME, retval);
		nna_device_number = 0;
		goto failed;
	}

	nna_sys_class = class_create(THIS_MODULE, DRIVER_NAME);
	if (IS_ERR_OR_NULL(nna_sys_class))
	{
		retval = PTR_ERR(nna_sys_class);
		nna_sys_class = NULL;
		printk(KERN_ERR "%s: couldn't create sys class. return=%d\n", DRIVER_NAME, retval);
		retval = (retval == 0) ? -ENOMEM : retval;
		goto failed;
	}

	nna_sys_class_set_attributes();

	retval = nna_irq_wrapper_init();
	if (retval)
	{
		printk(KERN_ERR "nna_irq_wrapper_init failed with error %d\n", retval);
		goto failed;
	}

	retval = platform_driver_register(&nna_platform_driver);
	if (retval)
	{
		printk(KERN_ERR "%s: couldn't register platform driver. return=%d\n", DRIVER_NAME, retval);
		nna_platform_driver_registered = 0;
		goto failed;
	}
	else
	{
		nna_platform_driver_registered = 1;
	}

	return 0;

failed:
	nna_module_cleanup();
	return retval;
}

/**
 * nna_module_exit()
 */
static void __exit nna_module_exit(void)
{
	nna_module_cleanup();
	nna_irq_wrapper_cleanup();
}

module_init(nna_module_init);
module_exit(nna_module_exit);
