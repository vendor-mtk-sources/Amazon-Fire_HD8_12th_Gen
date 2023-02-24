// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/irqdomain.h>
#include <linux/of_irq.h>
#include <dt-bindings/interrupt-controller/arm-gic.h>

#include "tz_cross/trustzone.h"
#include "tz_cross/ta_system.h"
#include "tz_cross/ta_irq.h"
#include "trustzone/kree/system.h"
#include "kree_int.h"

/* #define IRQ_HANDLING_LATENCY_MEASUREMENT */
#ifdef IRQ_HANDLING_LATENCY_MEASUREMENT
#include <linux/ktime.h>
#define TOPHALF_IRQT    0
#define BOTTOMHALF_IRQT 1
#define HANDLED_IRQT    2
#define NUM_IRQT        3
#endif

static struct irq_domain *sysirq;
static struct device_node *sysirq_node;

struct irq_hndl_data {
	unsigned int irq_no;
	KREE_SESSION_HANDLE irq_ssh;
#ifdef IRQ_HANDLING_LATENCY_MEASUREMENT
	ktime_t irqt[NUM_IRQT];
#endif
};
/*************************************************************
 *           REE Service
 *************************************************************/
static irqreturn_t KREE_HwIrqHandler(int virq, void *dev)
{
#ifdef IRQ_HANDLING_LATENCY_MEASUREMENT
	struct irq_hndl_data *ihd = (struct irq_hndl_data *)dev;

	ihd->irqt[TOPHALF_IRQT] = ktime_get();
#endif
	disable_irq_nosync(virq);
	return IRQ_WAKE_THREAD;
}

static irqreturn_t KREE_IrqHandler(int virq, void *dev)
{
	struct irq_data *data = irq_get_irq_data(virq);
	uint32_t paramTypes;
	union MTEEC_PARAM param[4];
	int ret;
	struct irq_hndl_data *ihd = (struct irq_hndl_data *)dev;

	if (!data)
		return IRQ_NONE;

#ifdef IRQ_HANDLING_LATENCY_MEASUREMENT
	ihd->irqt[BOTTOMHALF_IRQT] = ktime_get();
#endif
	param[0].value.a = (uint32_t) irqd_to_hwirq(data);
	paramTypes = TZ_ParamTypes1(TZPT_VALUE_INPUT);
	ret = KREE_TeeServiceCall(
			ihd->irq_ssh,
			TZCMD_SYS_IRQ, paramTypes, param);
#ifdef IRQ_HANDLING_LATENCY_MEASUREMENT
	ihd->irqt[HANDLED_IRQT] = ktime_get();
	pr_debug("irq %d, lat1=%lld ns, lat2=%lld ns\n",
		 irqd_to_hwirq(data),
		 ktime_to_ns(ktime_sub(ihd->irqt[BOTTOMHALF_IRQT],
				       ihd->irqt[TOPHALF_IRQT])),
		 ktime_to_ns(ktime_sub(ihd->irqt[HANDLED_IRQT],
				       ihd->irqt[BOTTOMHALF_IRQT])));
#endif
	enable_irq(virq);
	return (ret == TZ_RESULT_SUCCESS) ? IRQ_HANDLED : IRQ_NONE;
}

static unsigned int tz_hwirq_to_virq(unsigned int hwirq, unsigned long flags)
{
	struct of_phandle_args oirq;

	if (sysirq_node) {
		oirq.np = sysirq_node;
		oirq.args_count = 3;
		oirq.args[0] = GIC_SPI;
		oirq.args[1] = hwirq - 32;
		oirq.args[2] = flags;
		return irq_create_of_mapping(&oirq);
	}

	return hwirq;
}

int KREE_ServRequestIrq(u32 op, u8 uparam[REE_SERVICE_BUFFER_SIZE])
{
	struct ree_service_irq *param = (struct ree_service_irq *)uparam;
	int rret;
	unsigned long flags;
	int ret = TZ_RESULT_SUCCESS;
	unsigned int virq;
	struct irq_hndl_data *ihd = NULL;

	if (param->enable) {
		flags = 0;
		if (param->flags & MTIRQF_SHARED)
			flags |= IRQF_SHARED;
		if (param->flags & MTIRQF_TRIGGER_LOW)
			flags |= IRQF_TRIGGER_LOW;
		else if (param->flags & MTIRQF_TRIGGER_HIGH)
			flags |= IRQF_TRIGGER_HIGH;
		else if (param->flags & MTIRQF_TRIGGER_RISING)
			flags |= IRQF_TRIGGER_RISING;
		else if (param->flags & MTIRQF_TRIGGER_FALLING)
			flags |= IRQF_TRIGGER_FALLING;

		/* Generate a token if not already exists.
		 * Token is used as device for Share IRQ
		 */
		if (!param->token64) {
			ihd = kmalloc(sizeof(struct irq_hndl_data), GFP_KERNEL);
			if (!ihd)
				return TZ_RESULT_ERROR_OUT_OF_MEMORY;
			ihd->irq_no = param->irq;
			if (KREE_CreateSession(PTA_SYSTEM_UUID_STRING,
					       &ihd->irq_ssh)
					      != TZ_RESULT_SUCCESS) {
				kfree(ihd);
				return TZ_RESULT_ERROR_GENERIC;
			}
			param->token64 = (uint64_t)(unsigned long)ihd;
		}

		virq = tz_hwirq_to_virq(param->irq, flags);

		if (virq > 0)
			rret = request_threaded_irq(virq, KREE_HwIrqHandler,
					KREE_IrqHandler, flags,
					"TEE IRQ",
					(void *)(unsigned long)param->token64);
		else
			rret = -EINVAL;

		if (rret) {
			kfree(ihd);
			pr_warn("%s (virq:%d, hwirq:%d) return error: %d\n",
				__func__, virq, param->irq, rret);
			if (rret == -ENOMEM)
				ret = TZ_RESULT_ERROR_OUT_OF_MEMORY;
			else
				ret = TZ_RESULT_ERROR_BAD_PARAMETERS;
		}
	} else {
		if (sysirq)
			virq = irq_find_mapping(sysirq, param->irq);
		else
			virq = param->irq;

		if (virq) {
			free_irq(virq, (void *)(unsigned long)param->token64);
			if (param->token64) {
				ihd = (struct irq_hndl_data *)(unsigned long)
				param->token64;
				if (ihd->irq_no == param->irq) {
					KREE_CloseSession(ihd->irq_ssh);
					kfree(ihd);
				}
			}
		}
	}
	return ret;
}

int KREE_ServEnableIrq(u32 op, u8 uparam[REE_SERVICE_BUFFER_SIZE])
{
	struct ree_service_irq *param = (struct ree_service_irq *)uparam;
	unsigned int virq;

	virq = tz_hwirq_to_virq(param->irq, 0);
	if (virq > 0) {
		if (param->enable)
			enable_irq(virq);
		else
			disable_irq(virq);

		return TZ_RESULT_SUCCESS;
	}

	return TZ_RESULT_ERROR_BAD_PARAMETERS;
}

void kree_set_sysirq_node(struct device_node *pnode)
{
	if (pnode) {
		sysirq_node = pnode;
		sysirq = irq_find_host(sysirq_node);
	}
}
