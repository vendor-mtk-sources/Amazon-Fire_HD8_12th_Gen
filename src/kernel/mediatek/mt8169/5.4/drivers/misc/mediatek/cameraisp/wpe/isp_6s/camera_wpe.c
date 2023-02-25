// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author Wei-Yi Wang <Wei-Yi.Wang@mediatek.com>
 */

#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>	/* proc file use */
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/seq_file.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>

#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>


// #include <smi_public.h>
// #include <mt-plat/mtk_chip.h>

#ifdef __WPE_KERNEL_PERFORMANCE_MEASURE__
#include <linux/met_drv.h>
#include <linux/mtk_ftrace.h>
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

/* for MMDVFS */
#define __WPE_EP_NO_MMDVFS__
#ifndef __WPE_EP_NO_MMDVFS__
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include "mtk-interconnect.h"
#endif

#include "inc/camera_wpe.h"

#ifndef MTK_WPE_COUNT
#define MTK_WPE_COUNT 1
#endif

/* CCF */
#include <linux/clk.h>

unsigned int ver;
/*  */
#ifndef MTRUE
#define MTRUE               1
#endif
#ifndef MFALSE
#define MFALSE              0
#endif

#define WPE_DEV_NAME                "camera-wpe"
#define WPE_DEV_NAME_LEN            11
#define WPE_DEV_NUM                 MTK_WPE_COUNT

#define MyTag "[WPE]"

#define LOG_VRB(format,	args...) \
pr_debug(MyTag "[%s] " format, __func__, ##args)

#define WPE_DEBUG
#ifdef WPE_DEBUG
#define LOG_DBG(format, args...) \
pr_info(MyTag "[%s] " format, __func__, ##args)
#else
#define LOG_DBG(format, args...)
#endif

#define LOG_INF(format, args...) \
pr_info(MyTag "[%s] " format, __func__, ##args)
#define LOG_NOTICE(format, args...) \
pr_notice(MyTag "[%s] " format, __func__, ##args)
#define LOG_WRN(format, args...) \
pr_info(MyTag "[%s] " format, __func__, ##args)
#define LOG_ERR(format, args...) \
pr_info(MyTag "[%s] " format, __func__, ##args)
#define LOG_AST(format, args...) \
pr_debug(MyTag "[%s] " format, __func__, ##args)

/***********************************************************************
 *
 ***********************************************************************/
#define WPE_WR32(addr, data)    iowrite32(data, addr)

#define WPESYS_TOP_REG              (0x14020000)
#define WPESYS_RG_004_OFFSET        (0x004)

/***********************************************************************
 *
 ***********************************************************************/
/* dynamic log level */
#define WPE_DBG_DBGLOG              (0x00000001)
#define WPE_DBG_INFLOG              (0x00000002)
#define WPE_DBG_INT                 (0x00000004)
#define WPE_DBG_READ_REG            (0x00000008)
#define WPE_DBG_WRITE_REG           (0x00000010)
#define WPE_DBG_TASKLET             (0x00000020)

/***********************************************************************
 *
 ***********************************************************************/
#define RDMA0_BW_FOR_8195_DPTZ_WORSTCASE 9097
#define RDMA1_BW_FOR_8195_DPTZ_WORSTCASE 6786161
#define WDMA_BW_FOR_8195_DPTZ_WORSTCASE  0
#define WPESYS_CLOCKRATE_273MHZ 273000000
#define WPESYS_CLOCKRATE_364MHZ 364000000
#define WPESYS_CLOCKRATE_436MHZ 436000000
#define WPESYS_CLOCKRATE_624MHZ 624000000

/* /////////////////////////////////////////////////////////////////// */

static DEFINE_MUTEX(gWpeClkMutex);

struct WPE_device {
	struct device *dev;
	struct clk    **clk_list;
	int clk_cnt;
	struct regulator *regu;
	struct icc_path *path_vppwpe_rdma0;
	struct icc_path *path_vppwpe_rdma1;
	struct icc_path *path_vppwpe_wdma;
};

static struct WPE_device *WPE_devs;
static int nr_WPE_devs;

static unsigned int g_u4EnableClockCount[WPE_DEV_NUM];
static unsigned int g_u4WpeCnt[WPE_DEV_NUM];



/***********************************************************************
 *
 ***********************************************************************/
struct WPE_USER_INFO_STRUCT {
	pid_t Pid;
	pid_t Tid;
};



/***********************************************************************
 *
 ***********************************************************************/



struct WPE_INFO_STRUCT {
	spinlock_t SpinLockWPERef;
	unsigned int UserCount;	/* User Count */
};


static struct WPE_INFO_STRUCT WPEInfo[WPE_DEV_NUM];

enum _eLOG_TYPE {
	_LOG_DBG = 0,
	/* currently, only used at ipl_buf_ctrl. to protect critical section */
	_LOG_INF = 1,
	_LOG_ERR = 2,
	_LOG_MAX = 3,
};
#ifndef __WPE_EP_NO_MMDVFS__
static inline void WPE_MMDVFS_Set_Bandwidth(unsigned int devno, bool En)
{
	struct WPE_device *WPE_dev;

	if (devno >= WPE_DEV_NUM) {
		LOG_ERR("%s(%d) failed with wrong devno, %d", __func__, En, devno);
		return;
	}

	WPE_dev = &(WPE_devs[devno]);
	if (En) {
		mtk_icc_set_bw(WPE_dev->path_vppwpe_rdma0,
			Bps_to_icc(RDMA0_BW_FOR_8195_DPTZ_WORSTCASE), 0);
		mtk_icc_set_bw(WPE_dev->path_vppwpe_rdma1,
			Bps_to_icc(RDMA1_BW_FOR_8195_DPTZ_WORSTCASE), 0);
		mtk_icc_set_bw(WPE_dev->path_vppwpe_wdma,
			Bps_to_icc(WDMA_BW_FOR_8195_DPTZ_WORSTCASE), 0);
	} else {
		mtk_icc_set_bw(WPE_dev->path_vppwpe_rdma0, Bps_to_icc(0), 0);
		mtk_icc_set_bw(WPE_dev->path_vppwpe_rdma1, Bps_to_icc(0), 0);
		mtk_icc_set_bw(WPE_dev->path_vppwpe_wdma,  Bps_to_icc(0), 0);
	}
}

static inline int WPE_MMDVFS_Set_Frequency(unsigned long freq)
{
	/* For mt8195 WPESYS,
	 *	1. 273MHZ / MAINPLL_D4_D2 / 0.55v / default
	 *	2. 364MHZ / MAINPLL_D6 / 0.60v
	 *	3. 436MHZ / MAINPLL_D5 / 0.65v
	 *	4. 624MHZ / UNIVPLL_D4 / 0.75v
	 */
	struct dev_pm_opp *opp;
	int volt, ret = 0;

	opp = dev_pm_opp_find_freq_ceil(WPE_devs->dev, &freq);

	if (IS_ERR(opp)) /* It means freq is over the highest available frequency */
		opp = dev_pm_opp_find_freq_floor(WPE_devs->dev, &freq);

	volt = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);

	ret = regulator_set_voltage(WPE_devs->regu, volt, INT_MAX);
	if (ret)
		LOG_ERR("Set voltage(%d) and frequency(%d) fail for WPESYS", volt, freq);

	return ret;
}
#endif
static inline int WPE_Prepare_Enable_ccf_clock(unsigned int devno)
{
	int j, ret = 0;
	struct WPE_device *WPE_dev;

	if (devno >= WPE_DEV_NUM) {
		LOG_ERR("%s failed with wrong devno, %d", __func__, devno);
		return -1;
	}

	if (devno == 0)
		pm_runtime_get_sync(WPE_devs->dev); // only wpe_a
	WPE_dev = &(WPE_devs[devno]);
	for (j = 0; j < WPE_dev->clk_cnt; j++) {
		ret = clk_prepare_enable(WPE_dev->clk_list[j]);
		if (ret) {
			LOG_ERR("cannot prepare_enable (%d,%d) clock", devno, j);
			return ret;
		}
	}
#ifndef __WPE_EP_NO_MMDVFS__
	if (devno == 0) {
		ret = WPE_MMDVFS_Set_Frequency(WPESYS_CLOCKRATE_624MHZ); // only wpe_a
		if (ret) {
			LOG_ERR("cannot Set_Frequency (WPE %d, CLOCK %d) clock",
				devno, WPESYS_CLOCKRATE_624MHZ);
			return ret;
		}
	}
	WPE_MMDVFS_Set_Bandwidth(devno, MTRUE);
#endif
	return ret;
}

static inline void WPE_Disable_Unprepare_ccf_clock(unsigned int devno)
{
	int j;
	struct WPE_device *WPE_dev;

	if (devno >= WPE_DEV_NUM) {
		LOG_ERR("failed with wrong devno, %d", devno);
		return;
	}
#ifndef __WPE_EP_NO_MMDVFS__
	int ret = 0;

	if (devno == 0) {
		ret = WPE_MMDVFS_Set_Frequency(WPESYS_CLOCKRATE_273MHZ); // only wpe_a
		if (ret)
			LOG_ERR("cannot Set_Frequency (WPE %d, CLOCK %d) clock",
				devno, WPESYS_CLOCKRATE_273MHZ);
	}
	WPE_MMDVFS_Set_Bandwidth(devno, MFALSE);
#endif

	WPE_dev = &(WPE_devs[devno]);
	for (j = (WPE_dev->clk_cnt - 1) ; j >= 0 ; j--)
		clk_disable_unprepare(WPE_dev->clk_list[j]);
	if (devno == 0)
		pm_runtime_put_sync(WPE_devs->dev); // only wpe_a
	return;
}


/***********************************************************************
 *
 ***********************************************************************/
static void WPE_EnableClock(unsigned int devno, bool En)
{
	int ret = 0;

	if (devno >= WPE_DEV_NUM) {
		LOG_ERR("Enable(%d) failed with wrong devno, %d", En, devno);
		return;
	}

	if (En) {		/* Enable clock. */
		mutex_lock(&gWpeClkMutex);
		if (g_u4EnableClockCount[devno] == 0) {
			LOG_DBG("WPE[%d] clock enbled. g_u4EnableClockCount: %d."
				, devno, g_u4EnableClockCount[devno]);
			g_u4EnableClockCount[devno]++;
			mutex_unlock(&gWpeClkMutex);

			ret = WPE_Prepare_Enable_ccf_clock(devno);
			if (ret) {
				LOG_ERR("WPE_Prepare_Enable_ccf_clock fail. (WPE %d)", devno);
				mutex_lock(&gWpeClkMutex);
				g_u4EnableClockCount[devno]--;
				mutex_unlock(&gWpeClkMutex);
			}
		} else {
			g_u4EnableClockCount[devno]++;
			mutex_unlock(&gWpeClkMutex);
		}
	} else {		/* Disable clock. */
		mutex_lock(&gWpeClkMutex);
		g_u4EnableClockCount[devno]--;
		if (g_u4EnableClockCount[devno] == 0) {
			LOG_DBG("WPE[%d] clock disabled. g_u4EnableClockCount: %d."
				, devno, g_u4EnableClockCount[devno]);
			mutex_unlock(&gWpeClkMutex);

			WPE_Disable_Unprepare_ccf_clock(devno);
		} else {
			mutex_unlock(&gWpeClkMutex);
		}
	}
}

/***********************************************************************
 *
 ***********************************************************************/
static signed int WPE_open(struct inode *pInode, struct file *pFile)
{
	signed int Ret = 0;
	struct WPE_USER_INFO_STRUCT *pUserInfo;
	void *wpesys_reg;
	unsigned int devno = iminor(pInode);

	if (devno >= WPE_DEV_NUM) {
		LOG_ERR("iminor failed with devno = %d", devno);
		return -ENXIO;
	}

	LOG_DBG("- E. UserCount: %d.", WPEInfo[devno].UserCount);

	/*  */
	spin_lock(&(WPEInfo[devno].SpinLockWPERef));

	pFile->private_data = NULL;
	pFile->private_data = kmalloc(
		sizeof(struct WPE_USER_INFO_STRUCT), GFP_ATOMIC);

	if (pFile->private_data == NULL) {
		LOG_DBG(
			"ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)",
			current->comm,
			current->pid,
			current->tgid);
		Ret = -ENOMEM;
	} else {
		pUserInfo =
			(struct WPE_USER_INFO_STRUCT *) pFile->private_data;
		pUserInfo->Pid = current->pid;
		pUserInfo->Tid = current->tgid;
	}
	/*  */
	if (WPEInfo[devno].UserCount > 0) {
		WPEInfo[devno].UserCount++;
		spin_unlock(&(WPEInfo[devno].SpinLockWPERef));
		LOG_DBG(
			"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist",
			WPEInfo[devno].UserCount, current->comm,
			current->pid, current->tgid);
		goto EXIT;
	} else {
		WPEInfo[devno].UserCount++;
		spin_unlock(&(WPEInfo[devno].SpinLockWPERef));
		LOG_DBG(
			"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), first user",
			WPEInfo[devno].UserCount,
			current->comm,
			current->pid,
			current->tgid);
	}

	/* First User to setup */
	/* Enable clock */
	WPE_EnableClock(devno, MTRUE);
	g_u4WpeCnt[devno] = 0;
	LOG_INF("WPE[%d] open g_u4EnableClockCount: %d", devno, g_u4EnableClockCount[devno]);

	/* Enable WPESYS_EVENT_EN */
	wpesys_reg = ioremap_nocache(WPESYS_TOP_REG, 0x30);
	WPE_WR32(wpesys_reg + WPESYS_RG_004_OFFSET, 0xFFFF0000);
	iounmap(wpesys_reg);


EXIT:
	LOG_DBG("- X. Ret: %d. UserCount: %d.", Ret, WPEInfo[devno].UserCount);
	return Ret;

}

/***********************************************************************
 *
 ***********************************************************************/
static signed int WPE_release(struct inode *pInode, struct file *pFile)
{
	struct WPE_USER_INFO_STRUCT *pUserInfo;
	unsigned int devno = iminor(pInode);

	if (devno >= WPE_DEV_NUM) {
		LOG_ERR("iminor failed with devno = %d", devno);
		return -ENXIO;
	}

	LOG_DBG("- E. UserCount: %d.", WPEInfo[devno].UserCount);

	/*  */
	if (pFile->private_data != NULL) {
		pUserInfo =
			(struct WPE_USER_INFO_STRUCT *) pFile->private_data;

		kfree(pFile->private_data);
		pFile->private_data = NULL;
	}
	/*  */
	spin_lock(&(WPEInfo[devno].SpinLockWPERef));
	WPEInfo[devno].UserCount--;

	if (WPEInfo[devno].UserCount > 0) {
		spin_unlock(&(WPEInfo[devno].SpinLockWPERef));
		LOG_DBG(
			"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist",
			WPEInfo[devno].UserCount,
			current->comm,
			current->pid,
			current->tgid);
		goto EXIT;
	} else
		spin_unlock(&(WPEInfo[devno].SpinLockWPERef));
	/*  */
	LOG_DBG(
	"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), last user",
		WPEInfo[devno].UserCount,
		current->comm,
		current->pid,
		current->tgid);

	/* Disable clock. */
	WPE_EnableClock(devno, MFALSE);
	LOG_INF("WPE[%d] release g_u4EnableClockCount: %d", devno, g_u4EnableClockCount[devno]);

	/*  */
EXIT:

	LOG_DBG("- X. UserCount: %d.", WPEInfo[devno].UserCount);
	return 0;
}

/***********************************************************************
 *
 ***********************************************************************/

static dev_t WPEDevNo;
static unsigned int WPEDev_major;
static struct cdev *pWPECharDrv;
static struct class *pWPEClass;

static const struct file_operations WPEFileOper = {
	.owner = THIS_MODULE,
	.open = WPE_open,
	.release = WPE_release,
};

/***********************************************************************
 *
 ***********************************************************************/
static inline void WPE_UnregCharDev(void)
{
	LOG_DBG("- E.");
	/*  */
	/* Release char driver */
	if (pWPECharDrv != NULL) {
		cdev_del(pWPECharDrv);
		pWPECharDrv = NULL;
	}
	/*  */
	unregister_chrdev_region(WPEDevNo, WPE_DEV_NUM);
}

/***********************************************************************
 *
 ***********************************************************************/
static inline signed int WPE_RegCharDev(void)
{
	signed int Ret = 0;
	/*  */
	LOG_DBG("- E.");
	/*  */
	Ret = alloc_chrdev_region(&WPEDevNo, 0, WPE_DEV_NUM, WPE_DEV_NAME);
	WPEDev_major = MAJOR(WPEDevNo);
	if (Ret < 0) {
		LOG_ERR("alloc_chrdev_region failed, %d", Ret);
		return Ret;
	}
	/* Allocate driver */
	pWPECharDrv = cdev_alloc();
	if (pWPECharDrv == NULL) {
		LOG_ERR("cdev_alloc failed");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/* Attatch file operation. */
	cdev_init(pWPECharDrv, &WPEFileOper);
	/*  */
	pWPECharDrv->owner = THIS_MODULE;
	/* Add to system */
	Ret = cdev_add(pWPECharDrv, WPEDevNo, WPE_DEV_NUM);
	if (Ret < 0) {
		LOG_ERR("Attatch file operation failed, %d", Ret);
		goto EXIT;
	}
	/*  */
EXIT:
	if (Ret < 0)
		WPE_UnregCharDev();

	/*  */

	LOG_DBG("- X.");
	return Ret;
}

/***********************************************************************
 *
 ***********************************************************************/
static signed int WPE_probe(struct platform_device *pDev)
{
	signed int Ret = 0;
	struct device *dev = NULL;
	struct WPE_device *_wpe_dev;
	struct WPE_device *WPE_dev;
	struct clk **_clk_list;
	int clk_cnt, clk_i, dev_i;
	long ret;
#ifndef __WPE_EP_NO_MMDVFS__
	struct regulator *regu;
#endif

	LOG_INF("- E. WPE driver probe.");

	/* Check platform_device parameters */

	if (pDev == NULL) {
		LOG_ERR("[ERROR] pDev is NULL");
		return -ENXIO;
	}

	/* Init Global Struct */
	nr_WPE_devs += 1;
	_wpe_dev = krealloc(WPE_devs,
		sizeof(struct WPE_device) * nr_WPE_devs, GFP_KERNEL);

	if (!_wpe_dev) {
		LOG_ERR("[ERROR] Unable to allocate WPE_devs\n");
		return -ENOMEM;
	}
	WPE_devs = _wpe_dev;

	WPE_dev = &(WPE_devs[nr_WPE_devs - 1]);
	WPE_dev->dev = &pDev->dev;

	clk_cnt = of_count_phandle_with_args(pDev->dev.of_node, "clocks",
			"#clock-cells");
	LOG_INF("clk_cnt:%d\n", clk_cnt);

	if (clk_cnt > 0) {
		_clk_list = kcalloc(clk_cnt, sizeof(*_clk_list), GFP_KERNEL);
		if (!_clk_list) {
			LOG_ERR("[ERROR] Unable to allocate clk_list\n");
			return 1;
		}
	}

	for (clk_i = 0; clk_i < clk_cnt; clk_i++) {
		_clk_list[clk_i] = of_clk_get(pDev->dev.of_node, clk_i);
		if (IS_ERR(_clk_list[clk_i])) {
			ret = PTR_ERR(_clk_list[clk_i]);
			if (ret == -EPROBE_DEFER)
				LOG_ERR("clk %d is not ready\n", clk_i);
			else
				LOG_ERR("get clk %d fail, ret=%d, clk_cnt=%d\n",
				       clk_i, (int)ret, clk_cnt);
			clk_i--;
			goto EXIT_CLK;
		}
	}

	WPE_dev->clk_cnt = clk_cnt;
	WPE_dev->clk_list = _clk_list;

	/* Enable power */
	pm_runtime_enable(WPE_dev->dev);
	/*   */
#ifndef __WPE_EP_NO_MMDVFS__
	WPE_dev->path_vppwpe_rdma0 = of_mtk_icc_get(WPE_dev->dev, "vppwpe_rdma0");
	WPE_dev->path_vppwpe_rdma1 = of_mtk_icc_get(WPE_dev->dev, "vppwpe_rdma1");
	WPE_dev->path_vppwpe_wdma = of_mtk_icc_get(WPE_dev->dev, "vppwpe_wdma");
#endif
	/*   */

#if IS_ENABLED(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
		*(WPE_dev->dev->dma_mask) =
			(u64)DMA_BIT_MASK(CONFIG_MTK_IOMMU_PGTABLE_EXT);
		WPE_dev->dev->coherent_dma_mask =
			(u64)DMA_BIT_MASK(CONFIG_MTK_IOMMU_PGTABLE_EXT);
#endif

	LOG_INF("nr_WPE_devs=%d, devnode(%s)\n", nr_WPE_devs,
		pDev->dev.of_node->name);

	/* Only register char driver in the 1st time */
	if (nr_WPE_devs == 1) {

		/* Register char driver */
		Ret = WPE_RegCharDev();
		if (Ret) {
			LOG_ERR("[ERROR] register char failed");
			return Ret;
		}
		/* Create class register */
		pWPEClass = class_create(THIS_MODULE, "WPEdrv");
		if (IS_ERR(pWPEClass)) {
			Ret = PTR_ERR(pWPEClass);
			LOG_ERR("Unable to create class, err = %d", Ret);
			goto EXIT;
		}

		for (dev_i = 0 ; dev_i < WPE_DEV_NUM ; dev_i++) {
			char wpe_name[WPE_DEV_NAME_LEN + 5] /*= {0}*/;

			if (sprintf(wpe_name, WPE_DEV_NAME) < 0)
				LOG_ERR("sprintf fail");
			strcat(wpe_name, (dev_i == 0)?"":"-b");

			dev = device_create(pWPEClass, NULL,
						MKDEV(WPEDev_major, dev_i), NULL, wpe_name);
			if (IS_ERR(dev)) {
				Ret = PTR_ERR(dev);
				LOG_ERR(
					"[ERROR] Failed to create device: /dev/%s, err = %d",
					wpe_name, Ret);
				goto EXIT;
			}
		}

		for (dev_i = 0 ; dev_i < WPE_DEV_NUM ; dev_i++) {
			/* Init spinlocks */
			spin_lock_init(&(WPEInfo[dev_i].SpinLockWPERef));

			/* Init WPEInfo */
			spin_lock(&(WPEInfo[dev_i].SpinLockWPERef));
			WPEInfo[dev_i].UserCount = 0;
			spin_unlock(&(WPEInfo[dev_i].SpinLockWPERef));
		}
#ifndef __WPE_EP_NO_MMDVFS__
		/* Create opp table from dts */
		dev_pm_opp_of_add_table(WPE_dev->dev);

		/* Get regulator instance by name. */
		regu = devm_regulator_get(WPE_dev->dev, "dvfsrc-vcore");
		WPE_dev->regu = regu;
#endif
	}

EXIT:
	if (Ret < 0)
		WPE_UnregCharDev();

	LOG_INF("- X. WPE driver probe.");

	return Ret;

EXIT_CLK:
	while (clk_i > 0) {
		clk_put(_clk_list[clk_i]);
		clk_i--;
	}

	kfree(_clk_list);

	return ret;
}

/***********************************************************************
 * Called when the device is being detached from the driver
 ***********************************************************************/
static signed int WPE_remove(struct platform_device *pDev)
{
	int i, dev_i;
	struct WPE_device *WPE_dev;
	/*struct resource *pRes; */
	/*  */
	LOG_DBG("- E.");
	/* unregister char driver. */
	WPE_UnregCharDev();

	/*  */
	for (dev_i = 0 ; dev_i < WPE_DEV_NUM ; dev_i++)
		device_destroy(pWPEClass, MKDEV(WPEDev_major, dev_i));
	/*  */
	class_destroy(pWPEClass);
	pWPEClass = NULL;
	/*  */
	for (i = 0 ; i < nr_WPE_devs ; i++) {
		WPE_dev = &(WPE_devs[i]);
		kfree(WPE_dev->clk_list);
		WPE_dev->clk_list = NULL;
	}
	/* Disable power */
	pm_runtime_disable(WPE_devs->dev);

	/*  */
	return 0;
}

/***********************************************************************
 *
 ***********************************************************************/
static signed int bPass1_On_In_Resume_TG1;

static signed int WPE_suspend(
		struct platform_device *pDev, pm_message_t Mesg)
{
	unsigned int dev_i;

	/*LOG_DBG("bPass1_On_In_Resume_TG1(%d)\n", bPass1_On_In_Resume_TG1);*/

	bPass1_On_In_Resume_TG1 = 0;

	for (dev_i = 0 ; dev_i < WPE_DEV_NUM ; dev_i++) {
		if (g_u4EnableClockCount[dev_i] > 0) {
			WPE_EnableClock(dev_i, MFALSE);
			g_u4WpeCnt[dev_i]++;
		}

		LOG_INF("%s: WPE[%d] suspend g_u4EnableClockCount: %d, g_u4WpeCnt: %d",
			 __func__, dev_i, g_u4EnableClockCount[dev_i], g_u4WpeCnt[dev_i]);
	}

	return 0;
}

/***********************************************************************
 *
 ***********************************************************************/
static signed int WPE_resume(struct platform_device *pDev)
{
	unsigned int dev_i;

	/*LOG_DBG("bPass1_On_In_Resume_TG1(%d).\n", bPass1_On_In_Resume_TG1);*/
	for (dev_i = 0 ; dev_i < WPE_DEV_NUM ; dev_i++) {
		if (g_u4WpeCnt[dev_i] > 0) {
			WPE_EnableClock(dev_i, MTRUE);
			g_u4WpeCnt[dev_i]--;
		}

		LOG_INF("%s: WPE[%d] resume g_u4EnableClockCount: %d, g_u4WpeCnt: %d",
			 __func__, dev_i, g_u4EnableClockCount[dev_i], g_u4WpeCnt[dev_i]);
	}



	return 0;
}

/*---------------------------------------------------------------------*/
#if IS_ENABLED(CONFIG_PM)
/*---------------------------------------------------------------------*/
int WPE_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return WPE_suspend(pdev, PMSG_SUSPEND);
}

int WPE_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return WPE_resume(pdev);
}

int WPE_pm_restore_noirq(struct device *device)
{
	pr_debug("calling %s()\n", __func__);
	return 0;

}

/*---------------------------------------------------------------------*/
#else				/*CONFIG_PM */
/*---------------------------------------------------------------------*/
#define WPE_pm_suspend NULL
#define WPE_pm_resume  NULL
#define WPE_pm_restore_noirq NULL
/*---------------------------------------------------------------------*/
#endif				/*CONFIG_PM */
/*---------------------------------------------------------------------*/
/*
 * Note!!! The order and member of .compatible must be the same with that in
 *  "WPE_DEV_NODE_ENUM" in camera_WPE.h
 */
static const struct of_device_id WPE_of_ids[] = {
	{.compatible = "mediatek,wpe",},
	{}
};

const struct dev_pm_ops WPE_pm_ops = {
	.suspend = WPE_pm_suspend,
	.resume = WPE_pm_resume,
	.freeze = WPE_pm_suspend,
	.thaw = WPE_pm_resume,
	.poweroff = WPE_pm_suspend,
	.restore = WPE_pm_resume,
	.restore_noirq = WPE_pm_restore_noirq,
};


/***********************************************************************
 *
 ***********************************************************************/
static struct platform_driver WPEDriver = {
	.probe = WPE_probe,
	.remove = WPE_remove,
	.suspend = WPE_suspend,
	.resume = WPE_resume,
	.driver = {
		   .name = WPE_DEV_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = WPE_of_ids,
#ifdef CONFIG_PM
		   .pm = &WPE_pm_ops,
#endif
		}
};


static signed int __init WPE_Init(void)
{
	signed int Ret = 0;

	/*  */
	LOG_INF("- E. WPE_DEV_NUM=%d", WPE_DEV_NUM);
	/*  */
	Ret = platform_driver_register(&WPEDriver);
	if (Ret < 0) {
		LOG_ERR("platform_driver_register fail for VPP WPE");
		return Ret;
	}


	LOG_DBG("- X. Ret: %d.", Ret);
	return Ret;
}

/***********************************************************************
 *
 ***********************************************************************/
static void __exit WPE_Exit(void)
{
	LOG_DBG("- E.");
	/*  */
	platform_driver_unregister(&WPEDriver);
}


/***********************************************************************
 *
 ***********************************************************************/
module_init(WPE_Init);
module_exit(WPE_Exit);
MODULE_DESCRIPTION("Camera WPE driver");
MODULE_AUTHOR("SW1SS7");
MODULE_LICENSE("GPL");
