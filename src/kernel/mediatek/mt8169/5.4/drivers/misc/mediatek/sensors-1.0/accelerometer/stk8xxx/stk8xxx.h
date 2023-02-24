/* SPDX-License-Identifier: GPL-2.0 */
/*
 * stk8xxx.c - Linux driver for sensortek stk8xx accelerometer
 * Copyright (C) 2021 Sensortek
 */
#ifndef STK8XXX_H
#define STK8XXX_H
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/vmalloc.h>


/*****************************************************************************
 * stk8xxx register, start
 *****************************************************************************/
#define STK8XXX_REG_CHIPID          0x00
#define STK8XXX_REG_XOUT1           0x02
#define STK8XXX_REG_XOUT2           0x03
#define STK8XXX_REG_YOUT1           0x04
#define STK8XXX_REG_YOUT2           0x05
#define STK8XXX_REG_ZOUT1           0x06
#define STK8XXX_REG_ZOUT2           0x07
#define STK8XXX_REG_INTSTS1         0x09
#define STK8XXX_REG_INTSTS2         0x0A
#define STK8XXX_REG_FIFOSTS         0x0C
#define STK8XXX_REG_STEPOUT1        0x0D
#define STK8XXX_REG_STEPOUT2        0x0E
#define STK8XXX_REG_RANGESEL        0x0F
#define STK8XXX_REG_BWSEL           0x10
#define STK8XXX_REG_POWMODE         0x11
#define STK8XXX_REG_ODRMODE         0x12
#define STK8XXX_REG_SWRST           0x14
#define STK8XXX_REG_INTEN1          0x16
#define STK8XXX_REG_INTEN2          0x17
#define STK8XXX_REG_INTMAP1         0x19
#define STK8XXX_REG_INTMAP2         0x1A
#define STK8XXX_REG_INTCFG1         0x20
#define STK8XXX_REG_INTCFG2         0x21
#define STK8XXX_REG_SLOPEDLY        0x27
#define STK8XXX_REG_SLOPETHD        0x28
#define STK8XXX_REG_SIGMOT1         0x29
#define STK8XXX_REG_SIGMOT2         0x2A
#define STK8XXX_REG_SIGMOT3         0x2B
#define STK8XXX_REG_STEPCNT1        0x2C
#define STK8XXX_REG_STEPCNT2        0x2D
#define STK8XXX_REG_STEPTHD         0x2E
#define STK8XXX_REG_STEPDEB         0x2F
#define STK8XXX_REG_STEPMAXTW       0x31
#define STK8XXX_REG_INTFCFG         0x34
#define STK8XXX_REG_OFSTCOMP1       0x36
#define STK8XXX_REG_OFSTX           0x38
#define STK8XXX_REG_OFSTY           0x39
#define STK8XXX_REG_OFSTZ           0x3A
#define STK8XXX_REG_CFG1            0x3D
#define STK8XXX_REG_CFG2            0x3E
#define STK8XXX_REG_FIFOOUT         0x3F

/* STK8XXX_REG_CHIPID */
#define STK8BA50_R_ID                       0x86
#define STK8BA53_ID                         0x87
#define STK8323_ID                          0x23 /* include for STK8321 */
#define STK8327_ID                          0x26
#define STK8329_ID                          0x25
static const u8 STK_ID[] = {STK8BA50_R_ID, STK8BA53_ID, STK8323_ID, STK8327_ID, STK8329_ID};

/* STK8XXX_REG_INTSTS1 */
#define STK8XXX_INTSTS1_SIG_MOT_STS         0x1
#define STK8XXX_INTSTS1_ANY_MOT_STS         0x4

/* STK8XXX_REG_INTSTS2 */
#define STK8XXX_INTSTS2_FWM_STS_MASK        0x40

/* STK8XXX_REG_FIFOSTS */
#define STK8XXX_FIFOSTS_FIFOOVER            0x80
#define STK8XXX_FIFOSTS_FIFO_FRAME_CNT_MASK 0x7F

/* STK8XXX_REG_RANGESEL */
#define STK8XXX_RANGESEL_2G                 0x3
#define STK8XXX_RANGESEL_4G                 0x5
#define STK8XXX_RANGESEL_8G                 0x8
#define STK8XXX_RANGESEL_16G                0xc
#define STK8XXX_RANGESEL_BW_MASK            0xF
#define STK8XXX_RANGESEL_DEF                STK8XXX_RANGESEL_4G
typedef enum {
	STK_2G = STK8XXX_RANGESEL_2G,
	STK_4G = STK8XXX_RANGESEL_4G,
	STK_8G = STK8XXX_RANGESEL_8G,
	STK_16G = STK8XXX_RANGESEL_16G
} stk_rangesel;

/* STK8XXX_REG_BWSEL */
#define STK8XXX_BWSEL_BW_7_81               0x08    /* ODR = BW x 2 = 15.62Hz */
#define STK8XXX_BWSEL_BW_15_63              0x09    /* ODR = BW x 2 = 31.26Hz */
#define STK8XXX_BWSEL_BW_31_25              0x0A    /* ODR = BW x 2 = 62.5Hz */
#define STK8XXX_BWSEL_BW_62_5               0x0B    /* ODR = BW x 2 = 125Hz */
#define STK8XXX_BWSEL_BW_125                0x0C    /* ODR = BW x 2 = 250Hz */
#define STK8XXX_BWSEL_BW_250                0x0D    /* ODR = BW x 2 = 500Hz */
#define STK8XXX_BWSEL_BW_500                0x0E    /* ODR = BW x 2 = 1000Hz */

/* STK8XXX_REG_POWMODE */
#define STK8XXX_PWMD_SUSPEND                0x80
#define STK8XXX_PWMD_LOWPOWER               0x40
#define STK8XXX_PWMD_SLEEP_TIMER            0x20
#define STK8XXX_PWMD_NORMAL                 0x00
/* STK8XXX_REG_ODRMODE */
#define STK8XXX_ODR_NORMODE                 0x00
#define STK8XXX_ODR_ESMMODE                 0x08

/* STK8XXX_SLEEP_DUR */
#define STK8XXX_PWMD_1                      0x1E
#define STK8XXX_PWMD_2                      0x1C
#define STK8XXX_PWMD_10                     0x1A
#define STK8XXX_PWMD_20                     0x18
#define STK8XXX_PWMD_25                     0x16
#define STK8XXX_PWMD_50                     0x14
#define STK8XXX_PWMD_100                    0x12
#define STK8XXX_PWMD_163                    0x10
#define STK8XXX_PWMD_200                    0x0E
#define STK8XXX_PWMD_300                    0x0C
#define STK8XXX_PWMD_SLP_MASK               0x7E

/* STK8XXX_REG_SWRST */
#define STK8XXX_SWRST_VAL                   0xB6

/* STK8XXX_REG_INTEN1 */
#define STK8XXX_INTEN1_SLP_EN_XYZ           0x07

/* STK8XXX_REG_INTEN2 */
#define STK8XXX_INTEN2_DATA_EN              0x10
#define STK8XXX_INTEN2_FWM_EN               0x40

/* STK8XXX_REG_INTMAP1 */
#define STK8XXX_INTMAP1_SIGMOT2INT1         0x01
#define STK8XXX_INTMAP1_ANYMOT2INT1         0x04

/* STK8XXX_REG_INTMAP2 */
#define STK8XXX_INTMAP2_DATA2INT1           0x01
#define STK8XXX_INTMAP2_FWM2INT1            0x02
#define STK8XXX_INTMAP2_FWM2INT2            0x40

/* STK8XXX_REG_INTCFG1 */
#define STK8XXX_INTCFG1_INT1_ACTIVE_H       0x01
#define STK8XXX_INTCFG1_INT1_OD_PUSHPULL    0x00
#define STK8XXX_INTCFG1_INT2_ACTIVE_H       0x04
#define STK8XXX_INTCFG1_INT2_OD_PUSHPULL    0x00

/* STK8XXX_REG_INTCFG2 */
#define STK8XXX_INTCFG2_NOLATCHED           0x00
#define STK8XXX_INTCFG2_LATCHED             0x0F
#define STK8XXX_INTCFG2_INT_RST             0x80

/* STK8XXX_REG_SLOPETHD */
#define STK8XXX_SLOPETHD_DEF                0x14

/* STK8XXX_REG_SIGMOT1 */
#define STK8XXX_SIGMOT1_SKIP_TIME_3SEC      0x96    /* default value */

/* STK8XXX_REG_SIGMOT2 */
#define STK8XXX_SIGMOT2_SIG_MOT_EN          0x02
#define STK8XXX_SIGMOT2_ANY_MOT_EN          0x04

/* STK8XXX_REG_SIGMOT3 */
#define STK8XXX_SIGMOT3_PROOF_TIME_1SEC     0x32    /* default value */

/* STK8XXX_REG_STEPCNT2 */
#define STK8XXX_STEPCNT2_RST_CNT            0x04
#define STK8XXX_STEPCNT2_STEP_CNT_EN        0x08

/* STK8XXX_REG_INTFCFG */
#define STK8XXX_INTFCFG_I2C_WDT_EN          0x04

/* STK8XXX_REG_OFSTCOMP1 */
#define STK8XXX_OFSTCOMP1_OFST_RST          0x80

/* STK8XXX_REG_CFG1 */
/* the maximum space for FIFO is 32*3 bytes */
#define STK8XXX_CFG1_XYZ_FRAME_MAX          32

/* STK8XXX_REG_CFG2 */
#define STK8XXX_CFG2_FIFO_MODE_BYPASS       0x0
#define STK8XXX_CFG2_FIFO_MODE_FIFO         0x1
#define STK8XXX_CFG2_FIFO_MODE_SHIFT        5
#define STK8XXX_CFG2_FIFO_DATA_SEL_XYZ      0x0
#define STK8XXX_CFG2_FIFO_DATA_SEL_X        0x1
#define STK8XXX_CFG2_FIFO_DATA_SEL_Y        0x2
#define STK8XXX_CFG2_FIFO_DATA_SEL_Z        0x3
#define STK8XXX_CFG2_FIFO_DATA_SEL_MASK     0x3

/* STK8XXX_REG_OFSTx */
#define STK8XXX_OFST_LSB                    128     /* 8 bits for +-1G */


/* selftest usage */
#define STK_SELFTEST_SAMPLE_NUM             100
#define STK_SELFTEST_RESULT_NA              0
#define STK_SELFTEST_RESULT_RUNNING         (1 << 0)
#define STK_SELFTEST_RESULT_NO_ERROR        (1 << 1)
#define STK_SELFTEST_RESULT_DRIVER_ERROR    (1 << 2)
#define STK_SELFTEST_RESULT_FAIL_X          (1 << 3)
#define STK_SELFTEST_RESULT_FAIL_Y          (1 << 4)
#define STK_SELFTEST_RESULT_FAIL_Z          (1 << 5)
#define STK_SELFTEST_RESULT_NO_OUTPUT       (1 << 6)

#define STK8XXX_SUCCESS                                     0
#define STK8XXX_ERR_I2C                                     -1
#define STK8XXX_ERR_CLIENT                             -2
#define STK8XXX_ERR_STATUS                            -3
#define STK8XXX_ERR_SETUP_FAILURE          -4
#define STK8XXX_ERR_GETGSENSORDATA     -5
#define STK8XXX_ERR_IDENTIFICATION          -6
/*****************************************************************************
 * stk8xxx register, end
 *****************************************************************************/

#endif
