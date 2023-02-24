/*
 * amzn_sign_of_life_rtc.h
 *
 * Copyright 2021 Amazon Technologies, Inc. All Rights Reserved.
 *
 * The code contained herein is licensed under the GNU General Public
 * License Version 2.
 * You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef __AMZN_SIGN_OF_LIFE_RTC_H
#define __AMZN_SIGN_OF_LIFE_RTC_H

#include <linux/mfd/mt6397/rtc.h>

/*
 * Define the registers used by sign of life driver.
 */
#define BOOT_REASON_REG			RTC_SPAR0
#define SHUTDOWN_REASON_REG		RTC_SPAR0
#define THERMAL_SHUTDOWN_REASON_REG	RTC_AL_DOW
#define SPECIAL_MODE_REASON_REG		RTC_AL_DOW
#define PARITY_REG			RTC_AL_DOW
#define QUIE_REG			RTC_AL_DOW

/*
 * Define the bits in the registers used by sign of life driver.
 */

/*
 * RTC_SPAR0:
 *	bit 0 - 5 : SEC in power-on time
 *	bit 6 : 32K less bit. True:with 32K, False:Without 32K
 *	bit 7 : LP_DET
 *	bit 8 : Enter KPOC
 *	bit 9 : Enter SW LPRST
 *	bit 10 - 15: Use for life cycle reason
 *		bit 10 - bit 12: life cycle boot reason
 *		bit 13 - bit 15: life cycle shutdown reason
 */
#define RTC_BOOT_MASK			0x1C00
#define RTC_BOOT_SHIFT			10

#define RTC_SHUTDOWN_MASK		0xE000
#define RTC_SHUTDOWN_SHIFT		13

/*
 * RTC_AL_DOW:
 *	bit 8 - 14 : Use for life cycle reason
 *		bit 8 -  10 : life cycle thermal shutdown reason
 *		bit 11 - 13 : life cycle special mode reason
 *		bit 14: life cycle parity check bit
 *	bit 15 : Use for Slient OTA
 */
#define RTC_THERMAL_SHUTDOWN_MASK	0x0700
#define RTC_THERMAL_SHUTDOWN_SHIFT	8

#define RTC_SPECIAL_MODE_MASK		0x3800
#define RTC_SPECIAL_MODE_SHIFT		11

#define RTC_LIFE_CYCLE_REASON_PARITY_MASK	0x4000
#define RTC_LIFE_CYCLE_REASON_PARITY_SHIFT	14

#define PMIC_RTC_QUIESCENT        (1U << 15)
#define PMIC_RTC_QUIESCENT_MASK   (1U << 15)


/*
 * Define the life cycle reasons used in this product.
 */

/* boot reason */
typedef enum {
	RTC_BOOT_NONE           = 0,
	/* Device Boot Reason */
	RTC_WARMBOOT_BY_KERNEL_PANIC,
	RTC_WARMBOOT_BY_KERNEL_WATCHDOG,
	RTC_WARMBOOT_BY_HW_WATCHDOG,
	RTC_WARMBOOT_BY_SW,
} rtc_boot_reason_t;

/* shutdown reason */
typedef enum {
	RTC_SHUTDOWN_NONE       = 0,
	/* Device Shutdown Reason */
	RTC_SHUTDOWN_BY_LONG_PWR_KEY_PRESS,
	RTC_SHUTDOWN_BY_SW,
	RTC_SHUTDOWN_BY_PWR_KEY,
	RTC_SHUTDOWN_BY_SUDDEN_POWER_LOSS,
	RTC_SHUTDOWN_BY_UNKNOWN_REASONS,
} rtc_shutdown_reason_t;

/* thermal shtudown reason */
typedef enum {
	RTC_THERMAL_SHUTDOWN_NONE       = 0,
	/* Device Thermal Shutdown Reason */
	RTC_THERMAL_SHUTDOWN_REASON_BATTERY,
	RTC_THERMAL_SHUTDOWN_REASON_PMIC,
	RTC_THERMAL_SHUTDOWN_REASON_SOC,
	RTC_THERMAL_SHUTDOWN_REASON_MODEM,
	RTC_THERMAL_SHUTDOWN_REASON_WIFI,
	RTC_THERMAL_SHUTDOWN_REASON_PCB,
	RTC_THERMAL_SHUTDOWN_REASON_BTS,
} rtc_thermal_shutdown_reason_t;

/* special mode reason */
typedef enum {
	RTC_LIFE_CYCLE_SMODE_NONE       = 0,
	/* LIFE CYCLE Special Mode */
	RTC_LIFE_CYCLE_SMODE_LOW_BATTERY,
	RTC_LIFE_CYCLE_SMODE_WARM_BOOT_USB_CONNECTED,
	RTC_LIFE_CYCLE_SMODE_OTA,
	RTC_LIFE_CYCLE_SMODE_FACTORY_RESET,
} rtc_special_mode_reason_t;

/*
 * android boot reason charaters.
 */
#define POWER_KEY_BOOT		"=PowerKey"
#define USB_BOOT		"=usb"
#define WATCHDOG_SW		"=Watchdog"
#define WATCHDOG_HW		"=wdt_hw"
#define KERNEL_PANIC		"=kernel_panic"

#define lcr_rtc_lock		mtk_rtc_acquire_lock
#define lcr_rtc_unlock		mtk_rtc_release_lock

#define lcr_rtc_write		mtk_rtc_write
#define lcr_rtc_read		mtk_rtc_read
#define lcr_rtc_write_trigger	mtk_rtc_write_trigger_out

#endif
