/* SPDX-License-Identifier: GPL-2.0 */
/**
 * @file acenna_kmod.h
 * @brief Kernel Module header for ACE NNA
 */
#ifndef ACENNAMOD_H_
#define ACENNAMOD_H_

#include <linux/ioctl.h>
#include <linux/types.h>

#define DRIVER_VERSION     "1.6.0"
#define DRIVER_NAME        "acenna"
#define DEVICE_NAME_FORMAT "acenna%d"
#define DEVICE_MAX_NUM      256

/**
 * @brief Struct used to make IOCTL calls for Memory allocation and
 * de-allocation requests
 * @ingroup ACENNA_KERNEL_IOCTL_MSG
 */
typedef struct stMemAreaRequest
{
    /**
     * Identifier of Client making the request. Valid values are between 0 and
     * %NNA_MAX_CLIENTS%
     */
    int clientid;
    /**
     * memid for request. Valid values are between 0 and
     * %NNA_MAX_ALLOCATED_MEMAREAS_PER_CLIENT%
     */
    int memid;
    /**
     * Size of allocation requested. Maximum allowed NNA_MAX_MEMAREA_SIZE
     */
    int size;
    /**
     * physical address of requested buffers. Only valid if 0 is returned.
     *
     */
    uint64_t phy_addr;
    /**
     * Cache synchronization mode.
     * 0 - Let flags used in open() decide (default)
     * 1 - Noncached
     * 2 - Write Combined
     * 3 - DMA Coherent
     * 
     */
    int sync_mode;
} stMemAreaRequest;

typedef struct stMemAreaSync
{
    /**
     * Identifier of Client making the request. Valid values are between 0 and
     * %NNA_MAX_CLIENTS%
     */
    int clientid;
    /**
     * memid for request. Valid values are between 0 and
     * %NNA_MAX_ALLOCATED_MEMAREAS_PER_CLIENT%
     */
    int memid;
    /**
     * Size of area to by synced
     */
    int size;
    /**
     * Offset of area to be synced from buffer start
     * 
     */
    int offset;
    /**
     * Direction of DMA data flow
     * 0 - Data moves bidirectional (default)
     * 1 - Data moves from main memory to the device
     * 2 - Data moves from the device to main memory
     * 3 - None (Debug only)
     * 
     */
    int data_direction;
} stMemAreaSync;

/**
 * @brief Struct used to make IOCTL calls for ACE NNA register access requests
 * @ingroup ACENNA_KERNEL_IOCTL_MSG
 */
typedef struct regAccessRequest_s
{
    int32_t offset;     /** Register offset to read/write */
    uint32_t value;     /** For write requests, value to write */
} regAccessRequest_t;

/**
 * @brief Struct used to make IOCTL call for timestamps from kernel
 * @ingroup ACENNA_KERNEL_IOCTL_MSG
 */
typedef struct timeval_exch
{
    __aligned_u64 tv_sec;   /** Seconds */
    __aligned_u64 tv_usec;  /** Micro seconds */
} timeval_exch;

/**
 *  IOCTL Magic for Memory allocation requests
 * @ingroup ACENNA_KERNEL_IOCTL_MAGIC
 */
#define NNA_IOCTL_ALLOC         _IOW('a', 1, stMemAreaRequest)
/**
 *  IOCTL Magic for Memory de-allocation requests
 * @ingroup ACENNA_KERNEL_IOCTL_MAGIC
 */
#define NNA_IOCTL_DEALLOC       _IOW('a', 2, stMemAreaRequest)
/**
 *  IOCTL Magic to clear interrupt flags in kernel.
 * \deprecated Register writes are sniffed to implicitly clear interrupt flags.
 * No explicit userspace command needed.
 * @ingroup ACENNA_KERNEL_IOCTL_MAGIC
 */
#define NNA_IOCTL_TRIGGER       _IO('a', 3)
/**
 *  IOCTL Magic to request last interrupt timestamp
 * @ingroup ACENNA_KERNEL_IOCTL_MAGIC
 */
#define NNA_IOCTL_HW_INT_TIME   _IOR('a', 4, timeval_exch)
/**
 *  IOCTL Magic for Register Write requests
 * @ingroup ACENNA_KERNEL_IOCTL_MAGIC
 */
#define NNA_IOCTL_REG_WRITE     _IOW('a', 5, regAccessRequest_t)
/**
 *  IOCTL Magic for Register Read requests
 * @ingroup ACENNA_KERNEL_IOCTL_MAGIC
 */
#define NNA_IOCTL_REG_READ      _IOR('a', 6, regAccessRequest_t)
/**
 *  IOCTL Magic to clear POLL Event flag
 * @ingroup ACENNA_KERNEL_IOCTL_MAGIC
 */
#define NNA_IOCTL_CLEAR_POLL    _IO('a', 7)

#define NNA_IOCTL_SYNC_FOR_DEVICE _IOW('a', 8, stMemAreaSync)

#define NNA_IOCTL_SYNC_FOR_CPU _IOW('a', 9, stMemAreaSync)

/**
 *  Max number of clients that can be connected to NNA driver
 */
#define NNA_MAX_CLIENTS 16

/**
 *  Max number of CMA areas that a client can ask from NNA driver
 */
#define NNA_MAX_ALLOCATED_MEMAREAS_PER_CLIENT 128

/**
 *  NNA Max mem area size
 */
#define NNA_MAX_MEMAREA_SIZE (128*1024*1024)

#endif //   ACENNAMOD_H_
