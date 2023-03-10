/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt6357.h  --  mt6357 ALSA SoC audio codec driver
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: KaiChieh Chuang <kaichieh.chuang@mediatek.com>
 */

#ifndef __MT6357_H__
#define __MT6357_H__

/* Reg bit define */
/* MT6357_DCXO_CW14 */
#define RG_XO_AUDIO_EN_M_SFT 13

/* MT6357_AUD_TOP_CKPDN_CON0 */
#define RG_AUDNCP_CK_PDN_SFT                              6
#define RG_AUDNCP_CK_PDN_MASK                             0x1
#define RG_AUDNCP_CK_PDN_MASK_SFT                         (0x1 << 6)
#define RG_ZCD13M_CK_PDN_SFT                              5
#define RG_ZCD13M_CK_PDN_MASK                             0x1
#define RG_ZCD13M_CK_PDN_MASK_SFT                         (0x1 << 5)
#define RG_AUDIF_CK_PDN_SFT                               2
#define RG_AUDIF_CK_PDN_MASK                              0x1
#define RG_AUDIF_CK_PDN_MASK_SFT                          (0x1 << 2)
#define RG_AUD_CK_PDN_SFT                                 1
#define RG_AUD_CK_PDN_MASK                                0x1
#define RG_AUD_CK_PDN_MASK_SFT                            (0x1 << 1)
#define RG_ACCDET_CK_PDN_SFT                              0
#define RG_ACCDET_CK_PDN_MASK                             0x1
#define RG_ACCDET_CK_PDN_MASK_SFT                         (0x1 << 0)

/* MT6357_AUD_TOP_CKPDN_CON0_SET */
#define RG_AUD_TOP_CKPDN_CON0_SET_SFT                     0
#define RG_AUD_TOP_CKPDN_CON0_SET_MASK                    0x3fff
#define RG_AUD_TOP_CKPDN_CON0_SET_MASK_SFT                (0x3fff << 0)

/* MT6357_AUD_TOP_CKPDN_CON0_CLR */
#define RG_AUD_TOP_CKPDN_CON0_CLR_SFT                     0
#define RG_AUD_TOP_CKPDN_CON0_CLR_MASK                    0x3fff
#define RG_AUD_TOP_CKPDN_CON0_CLR_MASK_SFT                (0x3fff << 0)

/* MT6357_AUD_TOP_CKSEL_CON0 */
#define RG_AUDIF_CK_CKSEL_SFT                             3
#define RG_AUDIF_CK_CKSEL_MASK                            0x1
#define RG_AUDIF_CK_CKSEL_MASK_SFT                        (0x1 << 3)
#define RG_AUD_CK_CKSEL_SFT                               2
#define RG_AUD_CK_CKSEL_MASK                              0x1
#define RG_AUD_CK_CKSEL_MASK_SFT                          (0x1 << 2)

/* MT6357_AUD_TOP_CKSEL_CON0_SET */
#define RG_AUD_TOP_CKSEL_CON0_SET_SFT                     0
#define RG_AUD_TOP_CKSEL_CON0_SET_MASK                    0xf
#define RG_AUD_TOP_CKSEL_CON0_SET_MASK_SFT                (0xf << 0)

/* MT6357_AUD_TOP_CKSEL_CON0_CLR */
#define RG_AUD_TOP_CKSEL_CON0_CLR_SFT                     0
#define RG_AUD_TOP_CKSEL_CON0_CLR_MASK                    0xf
#define RG_AUD_TOP_CKSEL_CON0_CLR_MASK_SFT                (0xf << 0)

/* MT6357_AUD_TOP_CKTST_CON0 */
#define RG_AUD26M_CK_TSTSEL_SFT                           4
#define RG_AUD26M_CK_TSTSEL_MASK                          0x1
#define RG_AUD26M_CK_TSTSEL_MASK_SFT                      (0x1 << 4)
#define RG_AUDIF_CK_TSTSEL_SFT                            3
#define RG_AUDIF_CK_TSTSEL_MASK                           0x1
#define RG_AUDIF_CK_TSTSEL_MASK_SFT                       (0x1 << 3)
#define RG_AUD_CK_TSTSEL_SFT                              2
#define RG_AUD_CK_TSTSEL_MASK                             0x1
#define RG_AUD_CK_TSTSEL_MASK_SFT                         (0x1 << 2)
#define RG_AUD26M_CK_TST_DIS_SFT                          0
#define RG_AUD26M_CK_TST_DIS_MASK                         0x1
#define RG_AUD26M_CK_TST_DIS_MASK_SFT                     (0x1 << 0)

/* MT6357_AUD_TOP_RST_CON0 */
#define RG_AUDNCP_RST_SFT                                 3
#define RG_AUDNCP_RST_MASK                                0x1
#define RG_AUDNCP_RST_MASK_SFT                            (0x1 << 3)
#define RG_ZCD_RST_SFT                                    2
#define RG_ZCD_RST_MASK                                   0x1
#define RG_ZCD_RST_MASK_SFT                               (0x1 << 2)
#define RG_ACCDET_RST_SFT                                 1
#define RG_ACCDET_RST_MASK                                0x1
#define RG_ACCDET_RST_MASK_SFT                            (0x1 << 1)
#define RG_AUDIO_RST_SFT                                  0
#define RG_AUDIO_RST_MASK                                 0x1
#define RG_AUDIO_RST_MASK_SFT                             (0x1 << 0)

/* MT6357_AUD_TOP_RST_CON0_SET */
#define RG_AUD_TOP_RST_CON0_SET_SFT                       0
#define RG_AUD_TOP_RST_CON0_SET_MASK                      0xf
#define RG_AUD_TOP_RST_CON0_SET_MASK_SFT                  (0xf << 0)

/* MT6357_AUD_TOP_RST_CON0_CLR */
#define RG_AUD_TOP_RST_CON0_CLR_SFT                       0
#define RG_AUD_TOP_RST_CON0_CLR_MASK                      0xf
#define RG_AUD_TOP_RST_CON0_CLR_MASK_SFT                  (0xf << 0)

/* MT6357_AUD_TOP_RST_BANK_CON0 */
#define BANK_AUDZCD_SWRST_SFT                             2
#define BANK_AUDZCD_SWRST_MASK                            0x1
#define BANK_AUDZCD_SWRST_MASK_SFT                        (0x1 << 2)
#define BANK_AUDIO_SWRST_SFT                              1
#define BANK_AUDIO_SWRST_MASK                             0x1
#define BANK_AUDIO_SWRST_MASK_SFT                         (0x1 << 1)
#define BANK_ACCDET_SWRST_SFT                             0
#define BANK_ACCDET_SWRST_MASK                            0x1
#define BANK_ACCDET_SWRST_MASK_SFT                        (0x1 << 0)

/* MT6357_AUD_TOP_INT_CON0 */
#define RG_INT_EN_AUDIO_SFT                               0
#define RG_INT_EN_AUDIO_MASK                              0x1
#define RG_INT_EN_AUDIO_MASK_SFT                          (0x1 << 0)
#define RG_INT_EN_ACCDET_SFT                              5
#define RG_INT_EN_ACCDET_MASK                             0x1
#define RG_INT_EN_ACCDET_MASK_SFT                         (0x1 << 5)
#define RG_INT_EN_ACCDET_EINT0_SFT                        6
#define RG_INT_EN_ACCDET_EINT0_MASK                       0x1
#define RG_INT_EN_ACCDET_EINT0_MASK_SFT                   (0x1 << 6)
#define RG_INT_EN_ACCDET_EINT1_SFT                        7
#define RG_INT_EN_ACCDET_EINT1_MASK                       0x1
#define RG_INT_EN_ACCDET_EINT1_MASK_SFT                   (0x1 << 7)

/* MT6357_AUD_TOP_INT_CON0_SET */
#define RG_AUD_INT_CON0_SET_SFT                           0
#define RG_AUD_INT_CON0_SET_MASK                          0xffff
#define RG_AUD_INT_CON0_SET_MASK_SFT                      (0xffff << 0)

/* MT6357_AUD_TOP_INT_CON0_CLR */
#define RG_AUD_INT_CON0_CLR_SFT                           0
#define RG_AUD_INT_CON0_CLR_MASK                          0xffff
#define RG_AUD_INT_CON0_CLR_MASK_SFT                      (0xffff << 0)

/* MT6357_AUD_TOP_INT_MASK_CON0 */
#define RG_INT_MASK_AUDIO_SFT                             0
#define RG_INT_MASK_AUDIO_MASK                            0x1
#define RG_INT_MASK_AUDIO_MASK_SFT                        (0x1 << 0)
#define RG_INT_MASK_ACCDET_SFT                            5
#define RG_INT_MASK_ACCDET_MASK                           0x1
#define RG_INT_MASK_ACCDET_MASK_SFT                       (0x1 << 5)
#define RG_INT_MASK_ACCDET_EINT0_SFT                      6
#define RG_INT_MASK_ACCDET_EINT0_MASK                     0x1
#define RG_INT_MASK_ACCDET_EINT0_MASK_SFT                 (0x1 << 6)
#define RG_INT_MASK_ACCDET_EINT1_SFT                      7
#define RG_INT_MASK_ACCDET_EINT1_MASK                     0x1
#define RG_INT_MASK_ACCDET_EINT1_MASK_SFT                 (0x1 << 7)

/* MT6357_AUD_TOP_INT_MASK_CON0_SET */
#define RG_AUD_INT_MASK_CON0_SET_SFT                      0
#define RG_AUD_INT_MASK_CON0_SET_MASK                     0xff
#define RG_AUD_INT_MASK_CON0_SET_MASK_SFT                 (0xff << 0)

/* MT6357_AUD_TOP_INT_MASK_CON0_CLR */
#define RG_AUD_INT_MASK_CON0_CLR_SFT                      0
#define RG_AUD_INT_MASK_CON0_CLR_MASK                     0xff
#define RG_AUD_INT_MASK_CON0_CLR_MASK_SFT                 (0xff << 0)

/* MT6357_AUD_TOP_INT_STATUS0 */
#define RG_INT_STATUS_AUDIO_SFT                           0
#define RG_INT_STATUS_AUDIO_MASK                          0x1
#define RG_INT_STATUS_AUDIO_MASK_SFT                      (0x1 << 0)
#define RG_INT_STATUS_ACCDET_SFT                          5
#define RG_INT_STATUS_ACCDET_MASK                         0x1
#define RG_INT_STATUS_ACCDET_MASK_SFT                     (0x1 << 5)
#define RG_INT_STATUS_ACCDET_EINT0_SFT                    6
#define RG_INT_STATUS_ACCDET_EINT0_MASK                   0x1
#define RG_INT_STATUS_ACCDET_EINT0_MASK_SFT               (0x1 << 6)
#define RG_INT_STATUS_ACCDET_EINT1_SFT                    7
#define RG_INT_STATUS_ACCDET_EINT1_MASK                   0x1
#define RG_INT_STATUS_ACCDET_EINT1_MASK_SFT               (0x1 << 7)

/* MT6357_AUD_TOP_INT_RAW_STATUS0 */
#define RG_INT_RAW_STATUS_AUDIO_SFT                       0
#define RG_INT_RAW_STATUS_AUDIO_MASK                      0x1
#define RG_INT_RAW_STATUS_AUDIO_MASK_SFT                  (0x1 << 0)
#define RG_INT_RAW_STATUS_ACCDET_SFT                      5
#define RG_INT_RAW_STATUS_ACCDET_MASK                     0x1
#define RG_INT_RAW_STATUS_ACCDET_MASK_SFT                 (0x1 << 5)
#define RG_INT_RAW_STATUS_ACCDET_EINT0_SFT                6
#define RG_INT_RAW_STATUS_ACCDET_EINT0_MASK               0x1
#define RG_INT_RAW_STATUS_ACCDET_EINT0_MASK_SFT           (0x1 << 6)
#define RG_INT_RAW_STATUS_ACCDET_EINT1_SFT                7
#define RG_INT_RAW_STATUS_ACCDET_EINT1_MASK               0x1
#define RG_INT_RAW_STATUS_ACCDET_EINT1_MASK_SFT           (0x1 << 7)

/* MT6357_AUD_TOP_INT_MISC_CON0 */
#define RG_AUD_TOP_INT_POLARITY_SFT                       0
#define RG_AUD_TOP_INT_POLARITY_MASK                      0x1
#define RG_AUD_TOP_INT_POLARITY_MASK_SFT                  (0x1 << 0)

/* MT6357_AUDNCP_CLKDIV_CON0 */
#define RG_DIVCKS_CHG_SFT                                 0
#define RG_DIVCKS_CHG_MASK                                0x1
#define RG_DIVCKS_CHG_MASK_SFT                            (0x1 << 0)

/* MT6357_AUDNCP_CLKDIV_CON1 */
#define RG_DIVCKS_ON_SFT                                  0
#define RG_DIVCKS_ON_MASK                                 0x1
#define RG_DIVCKS_ON_MASK_SFT                             (0x1 << 0)

/* MT6357_AUDNCP_CLKDIV_CON2 */
#define RG_DIVCKS_PRG_SFT                                 0
#define RG_DIVCKS_PRG_MASK                                0x1ff
#define RG_DIVCKS_PRG_MASK_SFT                            (0x1ff << 0)

/* MT6357_AUDNCP_CLKDIV_CON3 */
#define RG_DIVCKS_PWD_NCP_SFT                             0
#define RG_DIVCKS_PWD_NCP_MASK                            0x1
#define RG_DIVCKS_PWD_NCP_MASK_SFT                        (0x1 << 0)

/* MT6357_AUDNCP_CLKDIV_CON4 */
#define RG_DIVCKS_PWD_NCP_ST_SEL_SFT                      0
#define RG_DIVCKS_PWD_NCP_ST_SEL_MASK                     0x3
#define RG_DIVCKS_PWD_NCP_ST_SEL_MASK_SFT                 (0x3 << 0)

/* MT6357_AUD_TOP_MON_CON0 */
#define RG_AUD_TOP_MON_SEL_SFT                            0
#define RG_AUD_TOP_MON_SEL_MASK                           0x7
#define RG_AUD_TOP_MON_SEL_MASK_SFT                       (0x7 << 0)
#define RG_AUD_CLK_INT_MON_FLAG_SEL_SFT                   3
#define RG_AUD_CLK_INT_MON_FLAG_SEL_MASK                  0xff
#define RG_AUD_CLK_INT_MON_FLAG_SEL_MASK_SFT              (0xff << 3)
#define RG_AUD_CLK_INT_MON_FLAG_EN_SFT                    11
#define RG_AUD_CLK_INT_MON_FLAG_EN_MASK                   0x1
#define RG_AUD_CLK_INT_MON_FLAG_EN_MASK_SFT               (0x1 << 11)

/* MT6357_AFE_UL_DL_CON0 */
#define AFE_UL_LR_SWAP_SFT                                15
#define AFE_UL_LR_SWAP_MASK                               0x1
#define AFE_UL_LR_SWAP_MASK_SFT                           (0x1 << 15)
#define AFE_DL_LR_SWAP_SFT                                14
#define AFE_DL_LR_SWAP_MASK                               0x1
#define AFE_DL_LR_SWAP_MASK_SFT                           (0x1 << 14)
#define AFE_ON_SFT                                        0
#define AFE_ON_MASK                                       0x1
#define AFE_ON_MASK_SFT                                   (0x1 << 0)

/* MT6357_AFE_DL_SRC2_CON0_L */
#define DL_2_SRC_ON_TMP_CTL_PRE_SFT                       0
#define DL_2_SRC_ON_TMP_CTL_PRE_MASK                      0x1
#define DL_2_SRC_ON_TMP_CTL_PRE_MASK_SFT                  (0x1 << 0)

/* MT6357_AFE_UL_SRC_CON0_H */
#define C_DIGMIC_PHASE_SEL_CH1_CTL_SFT                    11
#define C_DIGMIC_PHASE_SEL_CH1_CTL_MASK                   0x7
#define C_DIGMIC_PHASE_SEL_CH1_CTL_MASK_SFT               (0x7 << 11)
#define C_DIGMIC_PHASE_SEL_CH2_CTL_SFT                    8
#define C_DIGMIC_PHASE_SEL_CH2_CTL_MASK                   0x7
#define C_DIGMIC_PHASE_SEL_CH2_CTL_MASK_SFT               (0x7 << 8)
#define C_TWO_DIGITAL_MIC_CTL_SFT                         7
#define C_TWO_DIGITAL_MIC_CTL_MASK                        0x1
#define C_TWO_DIGITAL_MIC_CTL_MASK_SFT                    (0x1 << 7)

/* MT6357_AFE_UL_SRC_CON0_L */
#define DMIC_LOW_POWER_MODE_CTL_SFT                       14
#define DMIC_LOW_POWER_MODE_CTL_MASK                      0x3
#define DMIC_LOW_POWER_MODE_CTL_MASK_SFT                  (0x3 << 14)
#define DIGMIC_3P25M_1P625M_SEL_CTL_SFT                   5
#define DIGMIC_3P25M_1P625M_SEL_CTL_MASK                  0x1
#define DIGMIC_3P25M_1P625M_SEL_CTL_MASK_SFT              (0x1 << 5)
#define UL_LOOP_BACK_MODE_CTL_SFT                         2
#define UL_LOOP_BACK_MODE_CTL_MASK                        0x1
#define UL_LOOP_BACK_MODE_CTL_MASK_SFT                    (0x1 << 2)
#define UL_SDM_3_LEVEL_CTL_SFT                            1
#define UL_SDM_3_LEVEL_CTL_MASK                           0x1
#define UL_SDM_3_LEVEL_CTL_MASK_SFT                       (0x1 << 1)
#define UL_SRC_ON_TMP_CTL_SFT                             0
#define UL_SRC_ON_TMP_CTL_MASK                            0x1
#define UL_SRC_ON_TMP_CTL_MASK_SFT                        (0x1 << 0)

/* MT6357_AFE_TOP_CON0 */
#define MTKAIF_SINE_ON_SFT                                2
#define MTKAIF_SINE_ON_MASK                               0x1
#define MTKAIF_SINE_ON_MASK_SFT                           (0x1 << 2)
#define UL_SINE_ON_SFT                                    1
#define UL_SINE_ON_MASK                                   0x1
#define UL_SINE_ON_MASK_SFT                               (0x1 << 1)
#define DL_SINE_ON_SFT                                    0
#define DL_SINE_ON_MASK                                   0x1
#define DL_SINE_ON_MASK_SFT                               (0x1 << 0)

/* MT6357_AUDIO_TOP_CON0 */
#define PDN_AFE_CTL_SFT                                   7
#define PDN_AFE_CTL_MASK                                  0x1
#define PDN_AFE_CTL_MASK_SFT                              (0x1 << 7)
#define PDN_DAC_CTL_SFT                                   6
#define PDN_DAC_CTL_MASK                                  0x1
#define PDN_DAC_CTL_MASK_SFT                              (0x1 << 6)
#define PDN_ADC_CTL_SFT                                   5
#define PDN_ADC_CTL_MASK                                  0x1
#define PDN_ADC_CTL_MASK_SFT                              (0x1 << 5)
#define PDN_I2S_DL_CTL_SFT                                3
#define PDN_I2S_DL_CTL_MASK                               0x1
#define PDN_I2S_DL_CTL_MASK_SFT                           (0x1 << 3)
#define PWR_CLK_DIS_CTL_SFT                               2
#define PWR_CLK_DIS_CTL_MASK                              0x1
#define PWR_CLK_DIS_CTL_MASK_SFT                          (0x1 << 2)
#define PDN_AFE_TESTMODEL_CTL_SFT                         1
#define PDN_AFE_TESTMODEL_CTL_MASK                        0x1
#define PDN_AFE_TESTMODEL_CTL_MASK_SFT                    (0x1 << 1)
#define PDN_RESERVED_SFT                                  0
#define PDN_RESERVED_MASK                                 0x1
#define PDN_RESERVED_MASK_SFT                             (0x1 << 0)

/* MT6357_AFE_MON_DEBUG0 */
#define AUDIO_SYS_TOP_MON_SWAP_SFT                        14
#define AUDIO_SYS_TOP_MON_SWAP_MASK                       0x3
#define AUDIO_SYS_TOP_MON_SWAP_MASK_SFT                   (0x3 << 14)
#define AUDIO_SYS_TOP_MON_SEL_SFT                         8
#define AUDIO_SYS_TOP_MON_SEL_MASK                        0x1f
#define AUDIO_SYS_TOP_MON_SEL_MASK_SFT                    (0x1f << 8)
#define AFE_MON_SEL_SFT                                   0
#define AFE_MON_SEL_MASK                                  0xff
#define AFE_MON_SEL_MASK_SFT                              (0xff << 0)

/* MT6357_AFUNC_AUD_CON0 */
#define CCI_AUD_ANACK_SEL_SFT                             15
#define CCI_AUD_ANACK_SEL_MASK                            0x1
#define CCI_AUD_ANACK_SEL_MASK_SFT                        (0x1 << 15)
#define CCI_AUDIO_FIFO_WPTR_SFT                           12
#define CCI_AUDIO_FIFO_WPTR_MASK                          0x7
#define CCI_AUDIO_FIFO_WPTR_MASK_SFT                      (0x7 << 12)
#define CCI_SCRAMBLER_CG_EN_SFT                           11
#define CCI_SCRAMBLER_CG_EN_MASK                          0x1
#define CCI_SCRAMBLER_CG_EN_MASK_SFT                      (0x1 << 11)
#define CCI_LCH_INV_SFT                                   10
#define CCI_LCH_INV_MASK                                  0x1
#define CCI_LCH_INV_MASK_SFT                              (0x1 << 10)
#define CCI_RAND_EN_SFT                                   9
#define CCI_RAND_EN_MASK                                  0x1
#define CCI_RAND_EN_MASK_SFT                              (0x1 << 9)
#define CCI_SPLT_SCRMB_CLK_ON_SFT                         8
#define CCI_SPLT_SCRMB_CLK_ON_MASK                        0x1
#define CCI_SPLT_SCRMB_CLK_ON_MASK_SFT                    (0x1 << 8)
#define CCI_SPLT_SCRMB_ON_SFT                             7
#define CCI_SPLT_SCRMB_ON_MASK                            0x1
#define CCI_SPLT_SCRMB_ON_MASK_SFT                        (0x1 << 7)
#define CCI_AUD_IDAC_TEST_EN_SFT                          6
#define CCI_AUD_IDAC_TEST_EN_MASK                         0x1
#define CCI_AUD_IDAC_TEST_EN_MASK_SFT                     (0x1 << 6)
#define CCI_ZERO_PAD_DISABLE_SFT                          5
#define CCI_ZERO_PAD_DISABLE_MASK                         0x1
#define CCI_ZERO_PAD_DISABLE_MASK_SFT                     (0x1 << 5)
#define CCI_AUD_SPLIT_TEST_EN_SFT                         4
#define CCI_AUD_SPLIT_TEST_EN_MASK                        0x1
#define CCI_AUD_SPLIT_TEST_EN_MASK_SFT                    (0x1 << 4)
#define CCI_AUD_SDM_MUTEL_SFT                             3
#define CCI_AUD_SDM_MUTEL_MASK                            0x1
#define CCI_AUD_SDM_MUTEL_MASK_SFT                        (0x1 << 3)
#define CCI_AUD_SDM_MUTER_SFT                             2
#define CCI_AUD_SDM_MUTER_MASK                            0x1
#define CCI_AUD_SDM_MUTER_MASK_SFT                        (0x1 << 2)
#define CCI_AUD_SDM_7BIT_SEL_SFT                          1
#define CCI_AUD_SDM_7BIT_SEL_MASK                         0x1
#define CCI_AUD_SDM_7BIT_SEL_MASK_SFT                     (0x1 << 1)
#define CCI_SCRAMBLER_EN_SFT                              0
#define CCI_SCRAMBLER_EN_MASK                             0x1
#define CCI_SCRAMBLER_EN_MASK_SFT                         (0x1 << 0)

/* MT6357_AFUNC_AUD_CON1 */
#define AUD_SDM_TEST_L_SFT                                8
#define AUD_SDM_TEST_L_MASK                               0xff
#define AUD_SDM_TEST_L_MASK_SFT                           (0xff << 8)
#define AUD_SDM_TEST_R_SFT                                0
#define AUD_SDM_TEST_R_MASK                               0xff
#define AUD_SDM_TEST_R_MASK_SFT                           (0xff << 0)

/* MT6357_AFUNC_AUD_CON2 */
#define CCI_AUD_DAC_ANA_MUTE_SFT                          7
#define CCI_AUD_DAC_ANA_MUTE_MASK                         0x1
#define CCI_AUD_DAC_ANA_MUTE_MASK_SFT                     (0x1 << 7)
#define CCI_AUD_DAC_ANA_RSTB_SEL_SFT                      6
#define CCI_AUD_DAC_ANA_RSTB_SEL_MASK                     0x1
#define CCI_AUD_DAC_ANA_RSTB_SEL_MASK_SFT                 (0x1 << 6)
#define CCI_AUDIO_FIFO_CLKIN_INV_SFT                      4
#define CCI_AUDIO_FIFO_CLKIN_INV_MASK                     0x1
#define CCI_AUDIO_FIFO_CLKIN_INV_MASK_SFT                 (0x1 << 4)
#define CCI_AUDIO_FIFO_ENABLE_SFT                         3
#define CCI_AUDIO_FIFO_ENABLE_MASK                        0x1
#define CCI_AUDIO_FIFO_ENABLE_MASK_SFT                    (0x1 << 3)
#define CCI_ACD_MODE_SFT                                  2
#define CCI_ACD_MODE_MASK                                 0x1
#define CCI_ACD_MODE_MASK_SFT                             (0x1 << 2)
#define CCI_AFIFO_CLK_PWDB_SFT                            1
#define CCI_AFIFO_CLK_PWDB_MASK                           0x1
#define CCI_AFIFO_CLK_PWDB_MASK_SFT                       (0x1 << 1)
#define CCI_ACD_FUNC_RSTB_SFT                             0
#define CCI_ACD_FUNC_RSTB_MASK                            0x1
#define CCI_ACD_FUNC_RSTB_MASK_SFT                        (0x1 << 0)

/* MT6357_AFUNC_AUD_CON3 */
#define SDM_ANA13M_TESTCK_SEL_SFT                         15
#define SDM_ANA13M_TESTCK_SEL_MASK                        0x1
#define SDM_ANA13M_TESTCK_SEL_MASK_SFT                    (0x1 << 15)
#define SDM_ANA13M_TESTCK_SRC_SEL_SFT                     12
#define SDM_ANA13M_TESTCK_SRC_SEL_MASK                    0x7
#define SDM_ANA13M_TESTCK_SRC_SEL_MASK_SFT                (0x7 << 12)
#define SDM_TESTCK_SRC_SEL_SFT                            8
#define SDM_TESTCK_SRC_SEL_MASK                           0x7
#define SDM_TESTCK_SRC_SEL_MASK_SFT                       (0x7 << 8)
#define DIGMIC_TESTCK_SRC_SEL_SFT                         4
#define DIGMIC_TESTCK_SRC_SEL_MASK                        0x7
#define DIGMIC_TESTCK_SRC_SEL_MASK_SFT                    (0x7 << 4)
#define DIGMIC_TESTCK_SEL_SFT                             0
#define DIGMIC_TESTCK_SEL_MASK                            0x1
#define DIGMIC_TESTCK_SEL_MASK_SFT                        (0x1 << 0)

/* MT6357_AFUNC_AUD_CON4 */
#define UL_FIFO_WCLK_INV_SFT                              8
#define UL_FIFO_WCLK_INV_MASK                             0x1
#define UL_FIFO_WCLK_INV_MASK_SFT                         (0x1 << 8)
#define UL_FIFO_DIGMIC_WDATA_TESTSRC_SEL_SFT              6
#define UL_FIFO_DIGMIC_WDATA_TESTSRC_SEL_MASK             0x1
#define UL_FIFO_DIGMIC_WDATA_TESTSRC_SEL_MASK_SFT         (0x1 << 6)
#define UL_FIFO_WDATA_TESTEN_SFT                          5
#define UL_FIFO_WDATA_TESTEN_MASK                         0x1
#define UL_FIFO_WDATA_TESTEN_MASK_SFT                     (0x1 << 5)
#define UL_FIFO_WDATA_TESTSRC_SEL_SFT                     4
#define UL_FIFO_WDATA_TESTSRC_SEL_MASK                    0x1
#define UL_FIFO_WDATA_TESTSRC_SEL_MASK_SFT                (0x1 << 4)
#define UL_FIFO_WCLK_6P5M_TESTCK_SEL_SFT                  3
#define UL_FIFO_WCLK_6P5M_TESTCK_SEL_MASK                 0x1
#define UL_FIFO_WCLK_6P5M_TESTCK_SEL_MASK_SFT             (0x1 << 3)
#define UL_FIFO_WCLK_6P5M_TESTCK_SRC_SEL_SFT              0
#define UL_FIFO_WCLK_6P5M_TESTCK_SRC_SEL_MASK             0x7
#define UL_FIFO_WCLK_6P5M_TESTCK_SRC_SEL_MASK_SFT         (0x7 << 0)

/* MT6357_AFUNC_AUD_CON5 */
#define R_AUD_DAC_POS_LARGE_MONO_SFT                      8
#define R_AUD_DAC_POS_LARGE_MONO_MASK                     0xff
#define R_AUD_DAC_POS_LARGE_MONO_MASK_SFT                 (0xff << 8)
#define R_AUD_DAC_NEG_LARGE_MONO_SFT                      0
#define R_AUD_DAC_NEG_LARGE_MONO_MASK                     0xff
#define R_AUD_DAC_NEG_LARGE_MONO_MASK_SFT                 (0xff << 0)

/* MT6357_AFUNC_AUD_CON6 */
#define R_AUD_DAC_POS_SMALL_MONO_SFT                      12
#define R_AUD_DAC_POS_SMALL_MONO_MASK                     0xf
#define R_AUD_DAC_POS_SMALL_MONO_MASK_SFT                 (0xf << 12)
#define R_AUD_DAC_NEG_SMALL_MONO_SFT                      8
#define R_AUD_DAC_NEG_SMALL_MONO_MASK                     0xf
#define R_AUD_DAC_NEG_SMALL_MONO_MASK_SFT                 (0xf << 8)
#define R_AUD_DAC_POS_TINY_MONO_SFT                       6
#define R_AUD_DAC_POS_TINY_MONO_MASK                      0x3
#define R_AUD_DAC_POS_TINY_MONO_MASK_SFT                  (0x3 << 6)
#define R_AUD_DAC_NEG_TINY_MONO_SFT                       4
#define R_AUD_DAC_NEG_TINY_MONO_MASK                      0x3
#define R_AUD_DAC_NEG_TINY_MONO_MASK_SFT                  (0x3 << 4)
#define R_AUD_DAC_MONO_SEL_SFT                            3
#define R_AUD_DAC_MONO_SEL_MASK                           0x1
#define R_AUD_DAC_MONO_SEL_MASK_SFT                       (0x1 << 3)
#define R_AUD_DAC_SW_RSTB_SFT                             0
#define R_AUD_DAC_SW_RSTB_MASK                            0x1
#define R_AUD_DAC_SW_RSTB_MASK_SFT                        (0x1 << 0)

/* MT6357_AFUNC_AUD_MON0 */
#define AUD_SCR_OUT_L_SFT                                 8
#define AUD_SCR_OUT_L_MASK                                0xff
#define AUD_SCR_OUT_L_MASK_SFT                            (0xff << 8)
#define AUD_SCR_OUT_R_SFT                                 0
#define AUD_SCR_OUT_R_MASK                                0xff
#define AUD_SCR_OUT_R_MASK_SFT                            (0xff << 0)

/* MT6357_AUDRC_TUNE_MON0 */
#define ASYNC_TEST_OUT_BCK_SFT                            15
#define ASYNC_TEST_OUT_BCK_MASK                           0x1
#define ASYNC_TEST_OUT_BCK_MASK_SFT                       (0x1 << 15)
#define RGS_AUDRCTUNE1READ_SFT                            8
#define RGS_AUDRCTUNE1READ_MASK                           0x1f
#define RGS_AUDRCTUNE1READ_MASK_SFT                       (0x1f << 8)
#define RGS_AUDRCTUNE0READ_SFT                            0
#define RGS_AUDRCTUNE0READ_MASK                           0x1f
#define RGS_AUDRCTUNE0READ_MASK_SFT                       (0x1f << 0)

/* MT6357_AFE_ADDA_MTKAIF_FIFO_CFG0 */
#define AFE_RESERVED_SFT                                  1
#define AFE_RESERVED_MASK                                 0x7fff
#define AFE_RESERVED_MASK_SFT                             (0x7fff << 1)
#define RG_MTKAIF_RXIF_FIFO_INTEN_SFT                     0
#define RG_MTKAIF_RXIF_FIFO_INTEN_MASK                    0x1
#define RG_MTKAIF_RXIF_FIFO_INTEN_MASK_SFT                (0x1 << 0)

/* MT6357_AFE_ADDA_MTKAIF_FIFO_LOG_MON1 */
#define MTKAIF_RXIF_WR_FULL_STATUS_SFT                    1
#define MTKAIF_RXIF_WR_FULL_STATUS_MASK                   0x1
#define MTKAIF_RXIF_WR_FULL_STATUS_MASK_SFT               (0x1 << 1)
#define MTKAIF_RXIF_RD_EMPTY_STATUS_SFT                   0
#define MTKAIF_RXIF_RD_EMPTY_STATUS_MASK                  0x1
#define MTKAIF_RXIF_RD_EMPTY_STATUS_MASK_SFT              (0x1 << 0)

/* MT6357_AFE_ADDA_MTKAIF_MON0 */
#define MTKAIFTX_V3_SYNC_OUT_SFT                          14
#define MTKAIFTX_V3_SYNC_OUT_MASK                         0x1
#define MTKAIFTX_V3_SYNC_OUT_MASK_SFT                     (0x1 << 14)
#define MTKAIFTX_V3_SDATA_OUT2_SFT                        13
#define MTKAIFTX_V3_SDATA_OUT2_MASK                       0x1
#define MTKAIFTX_V3_SDATA_OUT2_MASK_SFT                   (0x1 << 13)
#define MTKAIFTX_V3_SDATA_OUT1_SFT                        12
#define MTKAIFTX_V3_SDATA_OUT1_MASK                       0x1
#define MTKAIFTX_V3_SDATA_OUT1_MASK_SFT                   (0x1 << 12)
#define MTKAIF_RXIF_FIFO_STATUS_SFT                       0
#define MTKAIF_RXIF_FIFO_STATUS_MASK                      0xfff
#define MTKAIF_RXIF_FIFO_STATUS_MASK_SFT                  (0xfff << 0)

/* MT6357_AFE_ADDA_MTKAIF_MON1 */
#define MTKAIFRX_V3_SYNC_IN_SFT                           14
#define MTKAIFRX_V3_SYNC_IN_MASK                          0x1
#define MTKAIFRX_V3_SYNC_IN_MASK_SFT                      (0x1 << 14)
#define MTKAIFRX_V3_SDATA_IN2_SFT                         13
#define MTKAIFRX_V3_SDATA_IN2_MASK                        0x1
#define MTKAIFRX_V3_SDATA_IN2_MASK_SFT                    (0x1 << 13)
#define MTKAIFRX_V3_SDATA_IN1_SFT                         12
#define MTKAIFRX_V3_SDATA_IN1_MASK                        0x1
#define MTKAIFRX_V3_SDATA_IN1_MASK_SFT                    (0x1 << 12)
#define MTKAIF_RXIF_SEARCH_FAIL_FLAG_SFT                  11
#define MTKAIF_RXIF_SEARCH_FAIL_FLAG_MASK                 0x1
#define MTKAIF_RXIF_SEARCH_FAIL_FLAG_MASK_SFT             (0x1 << 11)
#define MTKAIF_RXIF_INVALID_FLAG_SFT                      8
#define MTKAIF_RXIF_INVALID_FLAG_MASK                     0x1
#define MTKAIF_RXIF_INVALID_FLAG_MASK_SFT                 (0x1 << 8)
#define MTKAIF_RXIF_INVALID_CYCLE_SFT                     0
#define MTKAIF_RXIF_INVALID_CYCLE_MASK                    0xff
#define MTKAIF_RXIF_INVALID_CYCLE_MASK_SFT                (0xff << 0)

/* MT6357_AFE_ADDA_MTKAIF_MON2 */
#define MTKAIF_TXIF_IN_CH2_SFT                            8
#define MTKAIF_TXIF_IN_CH2_MASK                           0xff
#define MTKAIF_TXIF_IN_CH2_MASK_SFT                       (0xff << 8)
#define MTKAIF_TXIF_IN_CH1_SFT                            0
#define MTKAIF_TXIF_IN_CH1_MASK                           0xff
#define MTKAIF_TXIF_IN_CH1_MASK_SFT                       (0xff << 0)

/* MT6357_AFE_ADDA_MTKAIF_MON3 */
#define MTKAIF_RXIF_OUT_CH2_SFT                           8
#define MTKAIF_RXIF_OUT_CH2_MASK                          0xff
#define MTKAIF_RXIF_OUT_CH2_MASK_SFT                      (0xff << 8)
#define MTKAIF_RXIF_OUT_CH1_SFT                           0
#define MTKAIF_RXIF_OUT_CH1_MASK                          0xff
#define MTKAIF_RXIF_OUT_CH1_MASK_SFT                      (0xff << 0)

/* MT6357_AFE_ADDA_MTKAIF_CFG0 */
#define RG_MTKAIF_RXIF_CLKINV_SFT                         15
#define RG_MTKAIF_RXIF_CLKINV_MASK                        0x1
#define RG_MTKAIF_RXIF_CLKINV_MASK_SFT                    (0x1 << 15)
#define RG_MTKAIF_RXIF_PROTOCOL2_SFT                      8
#define RG_MTKAIF_RXIF_PROTOCOL2_MASK                     0x1
#define RG_MTKAIF_RXIF_PROTOCOL2_MASK_SFT                 (0x1 << 8)
#define RG_MTKAIF_BYPASS_SRC_MODE_SFT                     6
#define RG_MTKAIF_BYPASS_SRC_MODE_MASK                    0x3
#define RG_MTKAIF_BYPASS_SRC_MODE_MASK_SFT                (0x3 << 6)
#define RG_MTKAIF_BYPASS_SRC_TEST_SFT                     5
#define RG_MTKAIF_BYPASS_SRC_TEST_MASK                    0x1
#define RG_MTKAIF_BYPASS_SRC_TEST_MASK_SFT                (0x1 << 5)
#define RG_MTKAIF_TXIF_PROTOCOL2_SFT                      4
#define RG_MTKAIF_TXIF_PROTOCOL2_MASK                     0x1
#define RG_MTKAIF_TXIF_PROTOCOL2_MASK_SFT                 (0x1 << 4)
#define RG_MTKAIF_PMIC_TXIF_8TO5_SFT                      2
#define RG_MTKAIF_PMIC_TXIF_8TO5_MASK                     0x1
#define RG_MTKAIF_PMIC_TXIF_8TO5_MASK_SFT                 (0x1 << 2)
#define RG_MTKAIF_LOOPBACK_TEST2_SFT                      1
#define RG_MTKAIF_LOOPBACK_TEST2_MASK                     0x1
#define RG_MTKAIF_LOOPBACK_TEST2_MASK_SFT                 (0x1 << 1)
#define RG_MTKAIF_LOOPBACK_TEST1_SFT                      0
#define RG_MTKAIF_LOOPBACK_TEST1_MASK                     0x1
#define RG_MTKAIF_LOOPBACK_TEST1_MASK_SFT                 (0x1 << 0)

/* MT6357_AFE_ADDA_MTKAIF_RX_CFG0 */
#define RG_MTKAIF_RXIF_VOICE_MODE_SFT                     12
#define RG_MTKAIF_RXIF_VOICE_MODE_MASK                    0xf
#define RG_MTKAIF_RXIF_VOICE_MODE_MASK_SFT                (0xf << 12)
#define RG_MTKAIF_RXIF_DATA_BIT_SFT                       8
#define RG_MTKAIF_RXIF_DATA_BIT_MASK                      0x7
#define RG_MTKAIF_RXIF_DATA_BIT_MASK_SFT                  (0x7 << 8)
#define RG_MTKAIF_RXIF_FIFO_RSP_SFT                       4
#define RG_MTKAIF_RXIF_FIFO_RSP_MASK                      0x7
#define RG_MTKAIF_RXIF_FIFO_RSP_MASK_SFT                  (0x7 << 4)
#define RG_MTKAIF_RXIF_DETECT_ON_SFT                      3
#define RG_MTKAIF_RXIF_DETECT_ON_MASK                     0x1
#define RG_MTKAIF_RXIF_DETECT_ON_MASK_SFT                 (0x1 << 3)
#define RG_MTKAIF_RXIF_DATA_MODE_SFT                      0
#define RG_MTKAIF_RXIF_DATA_MODE_MASK                     0x1
#define RG_MTKAIF_RXIF_DATA_MODE_MASK_SFT                 (0x1 << 0)

/* MT6357_AFE_ADDA_MTKAIF_RX_CFG1 */
#define RG_MTKAIF_RXIF_SYNC_SEARCH_TABLE_SFT              12
#define RG_MTKAIF_RXIF_SYNC_SEARCH_TABLE_MASK             0xf
#define RG_MTKAIF_RXIF_SYNC_SEARCH_TABLE_MASK_SFT         (0xf << 12)
#define RG_MTKAIF_RXIF_INVALID_SYNC_CHECK_ROUND_SFT       8
#define RG_MTKAIF_RXIF_INVALID_SYNC_CHECK_ROUND_MASK      0xf
#define RG_MTKAIF_RXIF_INVALID_SYNC_CHECK_ROUND_MASK_SFT  (0xf << 8)
#define RG_MTKAIF_RXIF_SYNC_CHECK_ROUND_SFT               4
#define RG_MTKAIF_RXIF_SYNC_CHECK_ROUND_MASK              0xf
#define RG_MTKAIF_RXIF_SYNC_CHECK_ROUND_MASK_SFT          (0xf << 4)
#define RG_MTKAIF_RXIF_VOICE_MODE_PROTOCOL2_SFT           0
#define RG_MTKAIF_RXIF_VOICE_MODE_PROTOCOL2_MASK          0xf
#define RG_MTKAIF_RXIF_VOICE_MODE_PROTOCOL2_MASK_SFT      (0xf << 0)

/* MT6357_AFE_ADDA_MTKAIF_RX_CFG2 */
#define RG_MTKAIF_RXIF_CLEAR_SYNC_FAIL_SFT                12
#define RG_MTKAIF_RXIF_CLEAR_SYNC_FAIL_MASK               0x1
#define RG_MTKAIF_RXIF_CLEAR_SYNC_FAIL_MASK_SFT           (0x1 << 12)
#define RG_MTKAIF_RXIF_SYNC_CNT_TABLE_SFT                 0
#define RG_MTKAIF_RXIF_SYNC_CNT_TABLE_MASK                0xfff
#define RG_MTKAIF_RXIF_SYNC_CNT_TABLE_MASK_SFT            (0xfff << 0)

/* MT6357_AFE_ADDA_MTKAIF_RX_CFG3 */
#define RG_MTKAIF_RXIF_FIFO_RSP_PROTOCOL2_SFT             4
#define RG_MTKAIF_RXIF_FIFO_RSP_PROTOCOL2_MASK            0x7
#define RG_MTKAIF_RXIF_FIFO_RSP_PROTOCOL2_MASK_SFT        (0x7 << 4)
#define RG_MTKAIF_RXIF_DETECT_ON_PROTOCOL2_SFT            3
#define RG_MTKAIF_RXIF_DETECT_ON_PROTOCOL2_MASK           0x1
#define RG_MTKAIF_RXIF_DETECT_ON_PROTOCOL2_MASK_SFT       (0x1 << 3)

/* MT6357_AFE_ADDA_MTKAIF_TX_CFG1 */
#define RG_MTKAIF_SYNC_WORD2_SFT                          4
#define RG_MTKAIF_SYNC_WORD2_MASK                         0x7
#define RG_MTKAIF_SYNC_WORD2_MASK_SFT                     (0x7 << 4)
#define RG_MTKAIF_SYNC_WORD1_SFT                          0
#define RG_MTKAIF_SYNC_WORD1_MASK                         0x7
#define RG_MTKAIF_SYNC_WORD1_MASK_SFT                     (0x7 << 0)

/* MT6357_AFE_SGEN_CFG0 */
#define SGEN_AMP_DIV_CH1_CTL_SFT                          12
#define SGEN_AMP_DIV_CH1_CTL_MASK                         0xf
#define SGEN_AMP_DIV_CH1_CTL_MASK_SFT                     (0xf << 12)
#define SGEN_DAC_EN_CTL_SFT                               7
#define SGEN_DAC_EN_CTL_MASK                              0x1
#define SGEN_DAC_EN_CTL_MASK_SFT                          (0x1 << 7)
#define SGEN_MUTE_SW_CTL_SFT                              6
#define SGEN_MUTE_SW_CTL_MASK                             0x1
#define SGEN_MUTE_SW_CTL_MASK_SFT                         (0x1 << 6)

/* MT6357_AFE_SGEN_CFG1 */
#define C_SGEN_RCH_INV_5BIT_SFT                           15
#define C_SGEN_RCH_INV_5BIT_MASK                          0x1
#define C_SGEN_RCH_INV_5BIT_MASK_SFT                      (0x1 << 15)
#define C_SGEN_RCH_INV_8BIT_SFT                           14
#define C_SGEN_RCH_INV_8BIT_MASK                          0x1
#define C_SGEN_RCH_INV_8BIT_MASK_SFT                      (0x1 << 14)
#define SGEN_FREQ_DIV_CH1_CTL_SFT                         0
#define SGEN_FREQ_DIV_CH1_CTL_MASK                        0x1f
#define SGEN_FREQ_DIV_CH1_CTL_MASK_SFT                    (0x1f << 0)

/* MT6357_AFE_ADC_ASYNC_FIFO_CFG */
#define RG_UL_ASYNC_FIFO_SOFT_RST_EN_SFT                  5
#define RG_UL_ASYNC_FIFO_SOFT_RST_EN_MASK                 0x1
#define RG_UL_ASYNC_FIFO_SOFT_RST_EN_MASK_SFT             (0x1 << 5)
#define RG_UL_ASYNC_FIFO_SOFT_RST_SFT                     4
#define RG_UL_ASYNC_FIFO_SOFT_RST_MASK                    0x1
#define RG_UL_ASYNC_FIFO_SOFT_RST_MASK_SFT                (0x1 << 4)
#define RG_AMIC_UL_ADC_CLK_SEL_SFT                        1
#define RG_AMIC_UL_ADC_CLK_SEL_MASK                       0x1
#define RG_AMIC_UL_ADC_CLK_SEL_MASK_SFT                   (0x1 << 1)

/* MT6357_AFE_DCCLK_CFG0 */
#define DCCLK_DIV_SFT                                     5
#define DCCLK_DIV_MASK                                    0x7ff
#define DCCLK_DIV_MASK_SFT                                (0x7ff << 5)
#define DCCLK_INV_SFT                                     4
#define DCCLK_INV_MASK                                    0x1
#define DCCLK_INV_MASK_SFT                                (0x1 << 4)
#define DCCLK_PDN_SFT                                     1
#define DCCLK_PDN_MASK                                    0x1
#define DCCLK_PDN_MASK_SFT                                (0x1 << 1)
#define DCCLK_GEN_ON_SFT                                  0
#define DCCLK_GEN_ON_MASK                                 0x1
#define DCCLK_GEN_ON_MASK_SFT                             (0x1 << 0)

/* MT6357_AFE_DCCLK_CFG1 */
#define RESYNC_SRC_SEL_SFT                                10
#define RESYNC_SRC_SEL_MASK                               0x3
#define RESYNC_SRC_SEL_MASK_SFT                           (0x3 << 10)
#define RESYNC_SRC_CK_INV_SFT                             9
#define RESYNC_SRC_CK_INV_MASK                            0x1
#define RESYNC_SRC_CK_INV_MASK_SFT                        (0x1 << 9)
#define DCCLK_RESYNC_BYPASS_SFT                           8
#define DCCLK_RESYNC_BYPASS_MASK                          0x1
#define DCCLK_RESYNC_BYPASS_MASK_SFT                      (0x1 << 8)
#define DCCLK_PHASE_SEL_SFT                               4
#define DCCLK_PHASE_SEL_MASK                              0xf
#define DCCLK_PHASE_SEL_MASK_SFT                          (0xf << 4)

/* MT6357_AUDIO_DIG_CFG */
#define RG_AUD_PAD_TOP_DAT_MISO2_LOOPBACK_SFT             15
#define RG_AUD_PAD_TOP_DAT_MISO2_LOOPBACK_MASK            0x1
#define RG_AUD_PAD_TOP_DAT_MISO2_LOOPBACK_MASK_SFT        (0x1 << 15)
#define RG_AUD_PAD_TOP_PHASE_MODE2_SFT                    8
#define RG_AUD_PAD_TOP_PHASE_MODE2_MASK                   0x7f
#define RG_AUD_PAD_TOP_PHASE_MODE2_MASK_SFT               (0x7f << 8)
#define RG_AUD_PAD_TOP_DAT_MISO_LOOPBACK_SFT              7
#define RG_AUD_PAD_TOP_DAT_MISO_LOOPBACK_MASK             0x1
#define RG_AUD_PAD_TOP_DAT_MISO_LOOPBACK_MASK_SFT         (0x1 << 7)
#define RG_AUD_PAD_TOP_PHASE_MODE_SFT                     0
#define RG_AUD_PAD_TOP_PHASE_MODE_MASK                    0x7f
#define RG_AUD_PAD_TOP_PHASE_MODE_MASK_SFT                (0x7f << 0)

/* MT6357_AFE_AUD_PAD_TOP */
#define RG_AUD_PAD_TOP_TX_FIFO_RSP_SFT                    12
#define RG_AUD_PAD_TOP_TX_FIFO_RSP_MASK                   0x7
#define RG_AUD_PAD_TOP_TX_FIFO_RSP_MASK_SFT               (0x7 << 12)
#define RG_AUD_PAD_TOP_MTKAIF_CLK_PROTOCOL2_SFT           11
#define RG_AUD_PAD_TOP_MTKAIF_CLK_PROTOCOL2_MASK          0x1
#define RG_AUD_PAD_TOP_MTKAIF_CLK_PROTOCOL2_MASK_SFT      (0x1 << 11)
#define RG_AUD_PAD_TOP_TX_FIFO_ON_SFT                     8
#define RG_AUD_PAD_TOP_TX_FIFO_ON_MASK                    0x1
#define RG_AUD_PAD_TOP_TX_FIFO_ON_MASK_SFT                (0x1 << 8)

/* MT6357_AFE_AUD_PAD_TOP_MON */
#define ADDA_AUD_PAD_TOP_MON_SFT                          0
#define ADDA_AUD_PAD_TOP_MON_MASK                         0xffff
#define ADDA_AUD_PAD_TOP_MON_MASK_SFT                     (0xffff << 0)

/* MT6357_AFE_AUD_PAD_TOP_MON1 */
#define ADDA_AUD_PAD_TOP_MON1_SFT                         0
#define ADDA_AUD_PAD_TOP_MON1_MASK                        0xffff
#define ADDA_AUD_PAD_TOP_MON1_MASK_SFT                    (0xffff << 0)

/* MT6357_AUDENC_DSN_ID */
#define AUDENC_ANA_ID_SFT                                 0
#define AUDENC_ANA_ID_MASK                                0xff
#define AUDENC_ANA_ID_MASK_SFT                            (0xff << 0)
#define AUDENC_DIG_ID_SFT                                 8
#define AUDENC_DIG_ID_MASK                                0xff
#define AUDENC_DIG_ID_MASK_SFT                            (0xff << 8)

/* MT6357_AUDENC_DSN_REV0 */
#define AUDENC_ANA_MINOR_REV_SFT                          0
#define AUDENC_ANA_MINOR_REV_MASK                         0xf
#define AUDENC_ANA_MINOR_REV_MASK_SFT                     (0xf << 0)
#define AUDENC_ANA_MAJOR_REV_SFT                          4
#define AUDENC_ANA_MAJOR_REV_MASK                         0xf
#define AUDENC_ANA_MAJOR_REV_MASK_SFT                     (0xf << 4)
#define AUDENC_DIG_MINOR_REV_SFT                          8
#define AUDENC_DIG_MINOR_REV_MASK                         0xf
#define AUDENC_DIG_MINOR_REV_MASK_SFT                     (0xf << 8)
#define AUDENC_DIG_MAJOR_REV_SFT                          12
#define AUDENC_DIG_MAJOR_REV_MASK                         0xf
#define AUDENC_DIG_MAJOR_REV_MASK_SFT                     (0xf << 12)

/* MT6357_AUDENC_DSN_DBI */
#define AUDENC_DSN_CBS_SFT                                0
#define AUDENC_DSN_CBS_MASK                               0x3
#define AUDENC_DSN_CBS_MASK_SFT                           (0x3 << 0)
#define AUDENC_DSN_BIX_SFT                                2
#define AUDENC_DSN_BIX_MASK                               0x3
#define AUDENC_DSN_BIX_MASK_SFT                           (0x3 << 2)
#define AUDENC_DSN_ESP_SFT                                8
#define AUDENC_DSN_ESP_MASK                               0xff
#define AUDENC_DSN_ESP_MASK_SFT                           (0xff << 8)

/* MT6357_AUDENC_DSN_FPI */
#define AUDENC_DSN_FPI_SFT                                0
#define AUDENC_DSN_FPI_MASK                               0xff
#define AUDENC_DSN_FPI_MASK_SFT                           (0xff << 0)

/* MT6357_AUDENC_ANA_CON0 */
#define RG_AUDPREAMPLON_SFT                               0
#define RG_AUDPREAMPLON_MASK                              0x1
#define RG_AUDPREAMPLON_MASK_SFT                          (0x1 << 0)
#define RG_AUDPREAMPLDCCEN_SFT                            1
#define RG_AUDPREAMPLDCCEN_MASK                           0x1
#define RG_AUDPREAMPLDCCEN_MASK_SFT                       (0x1 << 1)
#define RG_AUDPREAMPLDCPRECHARGE_SFT                      2
#define RG_AUDPREAMPLDCPRECHARGE_MASK                     0x1
#define RG_AUDPREAMPLDCPRECHARGE_MASK_SFT                 (0x1 << 2)
#define RG_AUDPREAMPLPGATEST_SFT                          3
#define RG_AUDPREAMPLPGATEST_MASK                         0x1
#define RG_AUDPREAMPLPGATEST_MASK_SFT                     (0x1 << 3)
#define RG_AUDPREAMPLVSCALE_SFT                           4
#define RG_AUDPREAMPLVSCALE_MASK                          0x3
#define RG_AUDPREAMPLVSCALE_MASK_SFT                      (0x3 << 4)
#define RG_AUDPREAMPLINPUTSEL_SFT                         6
#define RG_AUDPREAMPLINPUTSEL_MASK                        0x3
#define RG_AUDPREAMPLINPUTSEL_MASK_SFT                    (0x3 << 6)
#define RG_AUDPREAMPLGAIN_SFT                             8
#define RG_AUDPREAMPLGAIN_MASK                            0x7
#define RG_AUDPREAMPLGAIN_MASK_SFT                        (0x7 << 8)
#define RG_AUDADCLPWRUP_SFT                               12
#define RG_AUDADCLPWRUP_MASK                              0x1
#define RG_AUDADCLPWRUP_MASK_SFT                          (0x1 << 12)
#define RG_AUDADCLINPUTSEL_SFT                            13
#define RG_AUDADCLINPUTSEL_MASK                           0x3
#define RG_AUDADCLINPUTSEL_MASK_SFT                       (0x3 << 13)

/* MT6357_AUDENC_ANA_CON1 */
#define RG_AUDPREAMPRON_SFT                               0
#define RG_AUDPREAMPRON_MASK                              0x1
#define RG_AUDPREAMPRON_MASK_SFT                          (0x1 << 0)
#define RG_AUDPREAMPRDCCEN_SFT                            1
#define RG_AUDPREAMPRDCCEN_MASK                           0x1
#define RG_AUDPREAMPRDCCEN_MASK_SFT                       (0x1 << 1)
#define RG_AUDPREAMPRDCPRECHARGE_SFT                      2
#define RG_AUDPREAMPRDCPRECHARGE_MASK                     0x1
#define RG_AUDPREAMPRDCPRECHARGE_MASK_SFT                 (0x1 << 2)
#define RG_AUDPREAMPRPGATEST_SFT                          3
#define RG_AUDPREAMPRPGATEST_MASK                         0x1
#define RG_AUDPREAMPRPGATEST_MASK_SFT                     (0x1 << 3)
#define RG_AUDPREAMPRVSCALE_SFT                           4
#define RG_AUDPREAMPRVSCALE_MASK                          0x3
#define RG_AUDPREAMPRVSCALE_MASK_SFT                      (0x3 << 4)
#define RG_AUDPREAMPRINPUTSEL_SFT                         6
#define RG_AUDPREAMPRINPUTSEL_MASK                        0x3
#define RG_AUDPREAMPRINPUTSEL_MASK_SFT                    (0x3 << 6)
#define RG_AUDPREAMPRGAIN_SFT                             8
#define RG_AUDPREAMPRGAIN_MASK                            0x7
#define RG_AUDPREAMPRGAIN_MASK_SFT                        (0x7 << 8)
#define RG_AUDADCRPWRUP_SFT                               12
#define RG_AUDADCRPWRUP_MASK                              0x1
#define RG_AUDADCRPWRUP_MASK_SFT                          (0x1 << 12)
#define RG_AUDADCRINPUTSEL_SFT                            13
#define RG_AUDADCRINPUTSEL_MASK                           0x3
#define RG_AUDADCRINPUTSEL_MASK_SFT                       (0x3 << 13)

/* MT6357_AUDENC_ANA_CON2 */
#define RG_AUDPREAMPIDDTEST_SFT                           6
#define RG_AUDPREAMPIDDTEST_MASK                          0x3
#define RG_AUDPREAMPIDDTEST_MASK_SFT                      (0x3 << 6)
#define RG_AUDADC1STSTAGEIDDTEST_SFT                      8
#define RG_AUDADC1STSTAGEIDDTEST_MASK                     0x3
#define RG_AUDADC1STSTAGEIDDTEST_MASK_SFT                 (0x3 << 8)
#define RG_AUDADC2NDSTAGEIDDTEST_SFT                      10
#define RG_AUDADC2NDSTAGEIDDTEST_MASK                     0x3
#define RG_AUDADC2NDSTAGEIDDTEST_MASK_SFT                 (0x3 << 10)
#define RG_AUDADCREFBUFIDDTEST_SFT                        12
#define RG_AUDADCREFBUFIDDTEST_MASK                       0x3
#define RG_AUDADCREFBUFIDDTEST_MASK_SFT                   (0x3 << 12)
#define RG_AUDADCFLASHIDDTEST_SFT                         14
#define RG_AUDADCFLASHIDDTEST_MASK                        0x3
#define RG_AUDADCFLASHIDDTEST_MASK_SFT                    (0x3 << 14)

/* MT6357_AUDENC_ANA_CON3 */
#define RG_AUDADCDAC0P25FS_SFT                            0
#define RG_AUDADCDAC0P25FS_MASK                           0x1
#define RG_AUDADCDAC0P25FS_MASK_SFT                       (0x1 << 0)
#define RG_AUDADCCLKSEL_SFT                               1
#define RG_AUDADCCLKSEL_MASK                              0x1
#define RG_AUDADCCLKSEL_MASK_SFT                          (0x1 << 1)
#define RG_AUDADCCLKSOURCE_SFT                            2
#define RG_AUDADCCLKSOURCE_MASK                           0x3
#define RG_AUDADCCLKSOURCE_MASK_SFT                       (0x3 << 2)
#define RG_AUDPREAMPAAFEN_SFT                             8
#define RG_AUDPREAMPAAFEN_MASK                            0x1
#define RG_AUDPREAMPAAFEN_MASK_SFT                        (0x1 << 8)
#define RG_CMSTBENH_SFT                                   11
#define RG_CMSTBENH_MASK                                  0x1
#define RG_CMSTBENH_MASK_SFT                              (0x1 << 11)
#define RG_PGABODYSW_SFT                                  12
#define RG_PGABODYSW_MASK                                 0x1
#define RG_PGABODYSW_MASK_SFT                             (0x1 << 12)

/* MT6357_AUDENC_ANA_CON4 */
#define RG_AUDADC1STSTAGESDENB_SFT                        0
#define RG_AUDADC1STSTAGESDENB_MASK                       0x1
#define RG_AUDADC1STSTAGESDENB_MASK_SFT                   (0x1 << 0)
#define RG_AUDADC2NDSTAGERESET_SFT                        1
#define RG_AUDADC2NDSTAGERESET_MASK                       0x1
#define RG_AUDADC2NDSTAGERESET_MASK_SFT                   (0x1 << 1)
#define RG_AUDADC3RDSTAGERESET_SFT                        2
#define RG_AUDADC3RDSTAGERESET_MASK                       0x1
#define RG_AUDADC3RDSTAGERESET_MASK_SFT                   (0x1 << 2)
#define RG_AUDADCFSRESET_SFT                              3
#define RG_AUDADCFSRESET_MASK                             0x1
#define RG_AUDADCFSRESET_MASK_SFT                         (0x1 << 3)
#define RG_AUDADCWIDECM_SFT                               4
#define RG_AUDADCWIDECM_MASK                              0x1
#define RG_AUDADCWIDECM_MASK_SFT                          (0x1 << 4)
#define RG_AUDADCNOPATEST_SFT                             5
#define RG_AUDADCNOPATEST_MASK                            0x1
#define RG_AUDADCNOPATEST_MASK_SFT                        (0x1 << 5)
#define RG_AUDADCBYPASS_SFT                               6
#define RG_AUDADCBYPASS_MASK                              0x1
#define RG_AUDADCBYPASS_MASK_SFT                          (0x1 << 6)
#define RG_AUDADCFFBYPASS_SFT                             7
#define RG_AUDADCFFBYPASS_MASK                            0x1
#define RG_AUDADCFFBYPASS_MASK_SFT                        (0x1 << 7)
#define RG_AUDADCDACFBCURRENT_SFT                         8
#define RG_AUDADCDACFBCURRENT_MASK                        0x1
#define RG_AUDADCDACFBCURRENT_MASK_SFT                    (0x1 << 8)
#define RG_AUDADCDACIDDTEST_SFT                           9
#define RG_AUDADCDACIDDTEST_MASK                          0x3
#define RG_AUDADCDACIDDTEST_MASK_SFT                      (0x3 << 9)
#define RG_AUDADCDACNRZ_SFT                               11
#define RG_AUDADCDACNRZ_MASK                              0x1
#define RG_AUDADCDACNRZ_MASK_SFT                          (0x1 << 11)
#define RG_AUDADCNODEM_SFT                                12
#define RG_AUDADCNODEM_MASK                               0x1
#define RG_AUDADCNODEM_MASK_SFT                           (0x1 << 12)
#define RG_AUDADCDACTEST_SFT                              13
#define RG_AUDADCDACTEST_MASK                             0x1
#define RG_AUDADCDACTEST_MASK_SFT                         (0x1 << 13)

/* MT6357_AUDENC_ANA_CON5 */
#define RG_AUDRCTUNEL_SFT                                 0
#define RG_AUDRCTUNEL_MASK                                0x1f
#define RG_AUDRCTUNEL_MASK_SFT                            (0x1f << 0)
#define RG_AUDRCTUNELSEL_SFT                              5
#define RG_AUDRCTUNELSEL_MASK                             0x1
#define RG_AUDRCTUNELSEL_MASK_SFT                         (0x1 << 5)
#define RG_AUDRCTUNER_SFT                                 8
#define RG_AUDRCTUNER_MASK                                0x1f
#define RG_AUDRCTUNER_MASK_SFT                            (0x1f << 8)
#define RG_AUDRCTUNERSEL_SFT                              13
#define RG_AUDRCTUNERSEL_MASK                             0x1
#define RG_AUDRCTUNERSEL_MASK_SFT                         (0x1 << 13)

/* MT6357_AUDENC_ANA_CON6 */
#define RG_CLKSQ_EN_SFT                                   0
#define RG_CLKSQ_EN_MASK                                  0x1
#define RG_CLKSQ_EN_MASK_SFT                              (0x1 << 0)
#define RG_CLKSQ_IN_SEL_TEST_SFT                          1
#define RG_CLKSQ_IN_SEL_TEST_MASK                         0x1
#define RG_CLKSQ_IN_SEL_TEST_MASK_SFT                     (0x1 << 1)
#define RG_CM_REFGENSEL_SFT                               2
#define RG_CM_REFGENSEL_MASK                              0x1
#define RG_CM_REFGENSEL_MASK_SFT                          (0x1 << 2)
#define RG_AUDSPARE_SFT                                   4
#define RG_AUDSPARE_MASK                                  0xf
#define RG_AUDSPARE_MASK_SFT                              (0xf << 4)
#define RG_AUDENCSPARE_SFT                                8
#define RG_AUDENCSPARE_MASK                               0x3f
#define RG_AUDENCSPARE_MASK_SFT                           (0x3f << 8)

/* MT6357_AUDENC_ANA_CON7 */
#define RG_AUDDIGMICEN_SFT                                0
#define RG_AUDDIGMICEN_MASK                               0x1
#define RG_AUDDIGMICEN_MASK_SFT                           (0x1 << 0)
#define RG_AUDDIGMICBIAS_SFT                              1
#define RG_AUDDIGMICBIAS_MASK                             0x3
#define RG_AUDDIGMICBIAS_MASK_SFT                         (0x3 << 1)
#define RG_DMICHPCLKEN_SFT                                3
#define RG_DMICHPCLKEN_MASK                               0x1
#define RG_DMICHPCLKEN_MASK_SFT                           (0x1 << 3)
#define RG_AUDDIGMICPDUTY_SFT                             4
#define RG_AUDDIGMICPDUTY_MASK                            0x3
#define RG_AUDDIGMICPDUTY_MASK_SFT                        (0x3 << 4)
#define RG_AUDDIGMICNDUTY_SFT                             6
#define RG_AUDDIGMICNDUTY_MASK                            0x3
#define RG_AUDDIGMICNDUTY_MASK_SFT                        (0x3 << 6)
#define RG_DMICMONEN_SFT                                  8
#define RG_DMICMONEN_MASK                                 0x1
#define RG_DMICMONEN_MASK_SFT                             (0x1 << 8)
#define RG_DMICMONSEL_SFT                                 9
#define RG_DMICMONSEL_MASK                                0x7
#define RG_DMICMONSEL_MASK_SFT                            (0x7 << 9)
#define RG_AUDSPAREVMIC_SFT                               12
#define RG_AUDSPAREVMIC_MASK                              0xf
#define RG_AUDSPAREVMIC_MASK_SFT                          (0xf << 12)

/* MT6357_AUDENC_ANA_CON8 */
#define RG_AUDPWDBMICBIAS0_SFT                            0
#define RG_AUDPWDBMICBIAS0_MASK                           0x1
#define RG_AUDPWDBMICBIAS0_MASK_SFT                       (0x1 << 0)
#define RG_AUDMICBIAS0BYPASSEN_SFT                        1
#define RG_AUDMICBIAS0BYPASSEN_MASK                       0x1
#define RG_AUDMICBIAS0BYPASSEN_MASK_SFT                   (0x1 << 1)
#define RG_AUDMICBIAS0VREF_SFT                            4
#define RG_AUDMICBIAS0VREF_MASK                           0x7
#define RG_AUDMICBIAS0VREF_MASK_SFT                       (0x7 << 4)
#define RG_AUDMICBIAS0DCSW0P1EN_SFT                       8
#define RG_AUDMICBIAS0DCSW0P1EN_MASK                      0x1
#define RG_AUDMICBIAS0DCSW0P1EN_MASK_SFT                  (0x1 << 8)
#define RG_AUDMICBIAS0DCSW0P2EN_SFT                       9
#define RG_AUDMICBIAS0DCSW0P2EN_MASK                      0x1
#define RG_AUDMICBIAS0DCSW0P2EN_MASK_SFT                  (0x1 << 9)
#define RG_AUDMICBIAS0DCSW0NEN_SFT                        10
#define RG_AUDMICBIAS0DCSW0NEN_MASK                       0x1
#define RG_AUDMICBIAS0DCSW0NEN_MASK_SFT                   (0x1 << 10)
#define RG_AUDMICBIAS0DCSW2P1EN_SFT                       12
#define RG_AUDMICBIAS0DCSW2P1EN_MASK                      0x1
#define RG_AUDMICBIAS0DCSW2P1EN_MASK_SFT                  (0x1 << 12)
#define RG_AUDMICBIAS0DCSW2P2EN_SFT                       13
#define RG_AUDMICBIAS0DCSW2P2EN_MASK                      0x1
#define RG_AUDMICBIAS0DCSW2P2EN_MASK_SFT                  (0x1 << 13)
#define RG_AUDMICBIAS0DCSW2NEN_SFT                        14
#define RG_AUDMICBIAS0DCSW2NEN_MASK                       0x1
#define RG_AUDMICBIAS0DCSW2NEN_MASK_SFT                   (0x1 << 14)

/* MT6357_AUDENC_ANA_CON9 */
#define RG_AUDPWDBMICBIAS1_SFT                            0
#define RG_AUDPWDBMICBIAS1_MASK                           0x1
#define RG_AUDPWDBMICBIAS1_MASK_SFT                       (0x1 << 0)
#define RG_AUDMICBIAS1BYPASSEN_SFT                        1
#define RG_AUDMICBIAS1BYPASSEN_MASK                       0x1
#define RG_AUDMICBIAS1BYPASSEN_MASK_SFT                   (0x1 << 1)
#define RG_AUDMICBIAS1VREF_SFT                            4
#define RG_AUDMICBIAS1VREF_MASK                           0x7
#define RG_AUDMICBIAS1VREF_MASK_SFT                       (0x7 << 4)
#define RG_AUDMICBIAS1DCSW1PEN_SFT                        8
#define RG_AUDMICBIAS1DCSW1PEN_MASK                       0x1
#define RG_AUDMICBIAS1DCSW1PEN_MASK_SFT                   (0x1 << 8)
#define RG_AUDMICBIAS1DCSW1NEN_SFT                        9
#define RG_AUDMICBIAS1DCSW1NEN_MASK                       0x1
#define RG_AUDMICBIAS1DCSW1NEN_MASK_SFT                   (0x1 << 9)
#define RG_BANDGAPGEN_SFT                                 12
#define RG_BANDGAPGEN_MASK                                0x1
#define RG_BANDGAPGEN_MASK_SFT                            (0x1 << 12)
#define RG_MTEST_EN_SFT                                   13
#define RG_MTEST_EN_MASK                                  0x1
#define RG_MTEST_EN_MASK_SFT                              (0x1 << 13)
#define RG_MTEST_SEL_SFT                                  14
#define RG_MTEST_SEL_MASK                                 0x1
#define RG_MTEST_SEL_MASK_SFT                             (0x1 << 14)
#define RG_MTEST_CURRENT_SFT                              15
#define RG_MTEST_CURRENT_MASK                             0x1
#define RG_MTEST_CURRENT_MASK_SFT                         (0x1 << 15)

/* MT6357_AUDENC_ANA_CON10 */
#define RG_AUDACCDETMICBIAS0PULLLOW_SFT                   0
#define RG_AUDACCDETMICBIAS0PULLLOW_MASK                  0x1
#define RG_AUDACCDETMICBIAS0PULLLOW_MASK_SFT              (0x1 << 0)
#define RG_AUDACCDETMICBIAS1PULLLOW_SFT                   1
#define RG_AUDACCDETMICBIAS1PULLLOW_MASK                  0x1
#define RG_AUDACCDETMICBIAS1PULLLOW_MASK_SFT              (0x1 << 1)
#define RG_AUDACCDETVIN1PULLLOW_SFT                       2
#define RG_AUDACCDETVIN1PULLLOW_MASK                      0x1
#define RG_AUDACCDETVIN1PULLLOW_MASK_SFT                  (0x1 << 2)
#define RG_AUDACCDETVTHACAL_SFT                           4
#define RG_AUDACCDETVTHACAL_MASK                          0x1
#define RG_AUDACCDETVTHACAL_MASK_SFT                      (0x1 << 4)
#define RG_AUDACCDETVTHBCAL_SFT                           5
#define RG_AUDACCDETVTHBCAL_MASK                          0x1
#define RG_AUDACCDETVTHBCAL_MASK_SFT                      (0x1 << 5)
#define RG_AUDACCDETTVDET_SFT                             6
#define RG_AUDACCDETTVDET_MASK                            0x1
#define RG_AUDACCDETTVDET_MASK_SFT                        (0x1 << 6)
#define RG_ACCDETSEL_SFT                                  7
#define RG_ACCDETSEL_MASK                                 0x1
#define RG_ACCDETSEL_MASK_SFT                             (0x1 << 7)
#define RG_SWBUFMODSEL_SFT                                8
#define RG_SWBUFMODSEL_MASK                               0x1
#define RG_SWBUFMODSEL_MASK_SFT                           (0x1 << 8)
#define RG_SWBUFSWEN_SFT                                  9
#define RG_SWBUFSWEN_MASK                                 0x1
#define RG_SWBUFSWEN_MASK_SFT                             (0x1 << 9)
#define RG_EINTCOMPVTH_SFT                                10
#define RG_EINTCOMPVTH_MASK                               0x1
#define RG_EINTCOMPVTH_MASK_SFT                           (0x1 << 10)
#define RG_EINTCONFIGACCDET_SFT                           11
#define RG_EINTCONFIGACCDET_MASK                          0x1
#define RG_EINTCONFIGACCDET_MASK_SFT                      (0x1 << 11)
#define RG_EINTHIRENB_SFT                                 12
#define RG_EINTHIRENB_MASK                                0x1
#define RG_EINTHIRENB_MASK_SFT                            (0x1 << 12)
#define RG_ACCDET2AUXRESBYPASS_SFT                        13
#define RG_ACCDET2AUXRESBYPASS_MASK                       0x1
#define RG_ACCDET2AUXRESBYPASS_MASK_SFT                   (0x1 << 13)
#define RG_ACCDET2AUXBUFFERBYPASS_SFT                     14
#define RG_ACCDET2AUXBUFFERBYPASS_MASK                    0x1
#define RG_ACCDET2AUXBUFFERBYPASS_MASK_SFT                (0x1 << 14)
#define RG_ACCDET2AUXSWEN_SFT                             15
#define RG_ACCDET2AUXSWEN_MASK                            0x1
#define RG_ACCDET2AUXSWEN_MASK_SFT                        (0x1 << 15)

/* MT6357_AUDENC_ANA_CON11 */
#define RGS_AUDRCTUNELREAD_SFT                            0
#define RGS_AUDRCTUNELREAD_MASK                           0x1f
#define RGS_AUDRCTUNELREAD_MASK_SFT                       (0x1f << 0)
#define RGS_AUDRCTUNERREAD_SFT                            8
#define RGS_AUDRCTUNERREAD_MASK                           0x1f
#define RGS_AUDRCTUNERREAD_MASK_SFT                       (0x1f << 8)

/* MT6357_AUDDEC_DSN_ID */
#define AUDDEC_ANA_ID_SFT                                 0
#define AUDDEC_ANA_ID_MASK                                0xff
#define AUDDEC_ANA_ID_MASK_SFT                            (0xff << 0)
#define AUDDEC_DIG_ID_SFT                                 8
#define AUDDEC_DIG_ID_MASK                                0xff
#define AUDDEC_DIG_ID_MASK_SFT                            (0xff << 8)

/* MT6357_AUDDEC_DSN_REV0 */
#define AUDDEC_ANA_MINOR_REV_SFT                          0
#define AUDDEC_ANA_MINOR_REV_MASK                         0xf
#define AUDDEC_ANA_MINOR_REV_MASK_SFT                     (0xf << 0)
#define AUDDEC_ANA_MAJOR_REV_SFT                          4
#define AUDDEC_ANA_MAJOR_REV_MASK                         0xf
#define AUDDEC_ANA_MAJOR_REV_MASK_SFT                     (0xf << 4)
#define AUDDEC_DIG_MINOR_REV_SFT                          8
#define AUDDEC_DIG_MINOR_REV_MASK                         0xf
#define AUDDEC_DIG_MINOR_REV_MASK_SFT                     (0xf << 8)
#define AUDDEC_DIG_MAJOR_REV_SFT                          12
#define AUDDEC_DIG_MAJOR_REV_MASK                         0xf
#define AUDDEC_DIG_MAJOR_REV_MASK_SFT                     (0xf << 12)

/* MT6357_AUDDEC_DSN_DBI */
#define AUDDEC_DSN_CBS_SFT                                0
#define AUDDEC_DSN_CBS_MASK                               0x3
#define AUDDEC_DSN_CBS_MASK_SFT                           (0x3 << 0)
#define AUDDEC_DSN_BIX_SFT                                2
#define AUDDEC_DSN_BIX_MASK                               0x3
#define AUDDEC_DSN_BIX_MASK_SFT                           (0x3 << 2)
#define AUDDEC_DSN_ESP_SFT                                8
#define AUDDEC_DSN_ESP_MASK                               0xff
#define AUDDEC_DSN_ESP_MASK_SFT                           (0xff << 8)

/* MT6357_AUDDEC_DSN_FPI */
#define AUDDEC_DSN_FPI_SFT                                0
#define AUDDEC_DSN_FPI_MASK                               0xff
#define AUDDEC_DSN_FPI_MASK_SFT                           (0xff << 0)

/* MT6357_AUDDEC_ANA_CON0 */
#define RG_AUDDACLPWRUP_VAUDP15_SFT                       0
#define RG_AUDDACLPWRUP_VAUDP15_MASK                      0x1
#define RG_AUDDACLPWRUP_VAUDP15_MASK_SFT                  (0x1 << 0)
#define RG_AUDDACRPWRUP_VAUDP15_SFT                       1
#define RG_AUDDACRPWRUP_VAUDP15_MASK                      0x1
#define RG_AUDDACRPWRUP_VAUDP15_MASK_SFT                  (0x1 << 1)
#define RG_AUD_DAC_PWR_UP_VA28_SFT                        2
#define RG_AUD_DAC_PWR_UP_VA28_MASK                       0x1
#define RG_AUD_DAC_PWR_UP_VA28_MASK_SFT                   (0x1 << 2)
#define RG_AUD_DAC_PWL_UP_VA28_SFT                        3
#define RG_AUD_DAC_PWL_UP_VA28_MASK                       0x1
#define RG_AUD_DAC_PWL_UP_VA28_MASK_SFT                   (0x1 << 3)
#define RG_AUDHPLPWRUP_VAUDP15_SFT                        4
#define RG_AUDHPLPWRUP_VAUDP15_MASK                       0x1
#define RG_AUDHPLPWRUP_VAUDP15_MASK_SFT                   (0x1 << 4)
#define RG_AUDHPRPWRUP_VAUDP15_SFT                        5
#define RG_AUDHPRPWRUP_VAUDP15_MASK                       0x1
#define RG_AUDHPRPWRUP_VAUDP15_MASK_SFT                   (0x1 << 5)
#define RG_AUDHPLPWRUP_IBIAS_VAUDP15_SFT                  6
#define RG_AUDHPLPWRUP_IBIAS_VAUDP15_MASK                 0x1
#define RG_AUDHPLPWRUP_IBIAS_VAUDP15_MASK_SFT             (0x1 << 6)
#define RG_AUDHPRPWRUP_IBIAS_VAUDP15_SFT                  7
#define RG_AUDHPRPWRUP_IBIAS_VAUDP15_MASK                 0x1
#define RG_AUDHPRPWRUP_IBIAS_VAUDP15_MASK_SFT             (0x1 << 7)
#define RG_AUDHPLMUXINPUTSEL_VAUDP15_SFT                  8
#define RG_AUDHPLMUXINPUTSEL_VAUDP15_MASK                 0x3
#define RG_AUDHPLMUXINPUTSEL_VAUDP15_MASK_SFT             (0x3 << 8)
#define RG_AUDHPRMUXINPUTSEL_VAUDP15_SFT                  10
#define RG_AUDHPRMUXINPUTSEL_VAUDP15_MASK                 0x3
#define RG_AUDHPRMUXINPUTSEL_VAUDP15_MASK_SFT             (0x3 << 10)
#define RG_AUDHPLSCDISABLE_VAUDP15_SFT                    12
#define RG_AUDHPLSCDISABLE_VAUDP15_MASK                   0x1
#define RG_AUDHPLSCDISABLE_VAUDP15_MASK_SFT               (0x1 << 12)
#define RG_AUDHPRSCDISABLE_VAUDP15_SFT                    13
#define RG_AUDHPRSCDISABLE_VAUDP15_MASK                   0x1
#define RG_AUDHPRSCDISABLE_VAUDP15_MASK_SFT               (0x1 << 13)
#define RG_AUDHPLBSCCURRENT_VAUDP15_SFT                   14
#define RG_AUDHPLBSCCURRENT_VAUDP15_MASK                  0x1
#define RG_AUDHPLBSCCURRENT_VAUDP15_MASK_SFT              (0x1 << 14)
#define RG_AUDHPRBSCCURRENT_VAUDP15_SFT                   15
#define RG_AUDHPRBSCCURRENT_VAUDP15_MASK                  0x1
#define RG_AUDHPRBSCCURRENT_VAUDP15_MASK_SFT              (0x1 << 15)

/* MT6357_AUDDEC_ANA_CON1 */
#define RG_AUDHPLOUTPWRUP_VAUDP15_SFT                     0
#define RG_AUDHPLOUTPWRUP_VAUDP15_MASK                    0x1
#define RG_AUDHPLOUTPWRUP_VAUDP15_MASK_SFT                (0x1 << 0)
#define RG_AUDHPROUTPWRUP_VAUDP15_SFT                     1
#define RG_AUDHPROUTPWRUP_VAUDP15_MASK                    0x1
#define RG_AUDHPROUTPWRUP_VAUDP15_MASK_SFT                (0x1 << 1)
#define RG_AUDHPLOUTAUXPWRUP_VAUDP15_SFT                  2
#define RG_AUDHPLOUTAUXPWRUP_VAUDP15_MASK                 0x1
#define RG_AUDHPLOUTAUXPWRUP_VAUDP15_MASK_SFT             (0x1 << 2)
#define RG_AUDHPROUTAUXPWRUP_VAUDP15_SFT                  3
#define RG_AUDHPROUTAUXPWRUP_VAUDP15_MASK                 0x1
#define RG_AUDHPROUTAUXPWRUP_VAUDP15_MASK_SFT             (0x1 << 3)
#define RG_HPLAUXFBRSW_EN_VAUDP15_SFT                     4
#define RG_HPLAUXFBRSW_EN_VAUDP15_MASK                    0x1
#define RG_HPLAUXFBRSW_EN_VAUDP15_MASK_SFT                (0x1 << 4)
#define RG_HPRAUXFBRSW_EN_VAUDP15_SFT                     5
#define RG_HPRAUXFBRSW_EN_VAUDP15_MASK                    0x1
#define RG_HPRAUXFBRSW_EN_VAUDP15_MASK_SFT                (0x1 << 5)
#define RG_HPLSHORT2HPLAUX_EN_VAUDP15_SFT                 6
#define RG_HPLSHORT2HPLAUX_EN_VAUDP15_MASK                0x1
#define RG_HPLSHORT2HPLAUX_EN_VAUDP15_MASK_SFT            (0x1 << 6)
#define RG_HPRSHORT2HPRAUX_EN_VAUDP15_SFT                 7
#define RG_HPRSHORT2HPRAUX_EN_VAUDP15_MASK                0x1
#define RG_HPRSHORT2HPRAUX_EN_VAUDP15_MASK_SFT            (0x1 << 7)
#define RG_HPLOUTSTGCTRL_VAUDP15_SFT                      8
#define RG_HPLOUTSTGCTRL_VAUDP15_MASK                     0x7
#define RG_HPLOUTSTGCTRL_VAUDP15_MASK_SFT                 (0x7 << 8)
#define RG_HPROUTSTGCTRL_VAUDP15_SFT                      12
#define RG_HPROUTSTGCTRL_VAUDP15_MASK                     0x7
#define RG_HPROUTSTGCTRL_VAUDP15_MASK_SFT                 (0x7 << 12)

/* MT6357_AUDDEC_ANA_CON2 */
#define RG_HPLOUTPUTSTBENH_VAUDP15_SFT                    0
#define RG_HPLOUTPUTSTBENH_VAUDP15_MASK                   0x7
#define RG_HPLOUTPUTSTBENH_VAUDP15_MASK_SFT               (0x7 << 0)
#define RG_HPROUTPUTSTBENH_VAUDP15_SFT                    4
#define RG_HPROUTPUTSTBENH_VAUDP15_MASK                   0x7
#define RG_HPROUTPUTSTBENH_VAUDP15_MASK_SFT               (0x7 << 4)
#define RG_AUDHPSTARTUP_VAUDP15_SFT                       8
#define RG_AUDHPSTARTUP_VAUDP15_MASK                      0x1
#define RG_AUDHPSTARTUP_VAUDP15_MASK_SFT                  (0x1 << 8)
#define RG_AUDREFN_DERES_EN_VAUDP15_SFT                   9
#define RG_AUDREFN_DERES_EN_VAUDP15_MASK                  0x1
#define RG_AUDREFN_DERES_EN_VAUDP15_MASK_SFT              (0x1 << 9)
#define RG_HPPSHORT2VCM_VAUDP15_SFT                       10
#define RG_HPPSHORT2VCM_VAUDP15_MASK                      0x1
#define RG_HPPSHORT2VCM_VAUDP15_MASK_SFT                  (0x1 << 10)
#define RG_HPINPUTSTBENH_VAUDP15_SFT                      12
#define RG_HPINPUTSTBENH_VAUDP15_MASK                     0x1
#define RG_HPINPUTSTBENH_VAUDP15_MASK_SFT                 (0x1 << 12)
#define RG_HPINPUTRESET0_VAUDP15_SFT                      13
#define RG_HPINPUTRESET0_VAUDP15_MASK                     0x1
#define RG_HPINPUTRESET0_VAUDP15_MASK_SFT                 (0x1 << 13)
#define RG_HPOUTPUTRESET0_VAUDP15_SFT                     14
#define RG_HPOUTPUTRESET0_VAUDP15_MASK                    0x1
#define RG_HPOUTPUTRESET0_VAUDP15_MASK_SFT                (0x1 << 14)

/* MT6357_AUDDEC_ANA_CON3 */
#define RG_AUDHSPWRUP_VAUDP15_SFT                         0
#define RG_AUDHSPWRUP_VAUDP15_MASK                        0x1
#define RG_AUDHSPWRUP_VAUDP15_MASK_SFT                    (0x1 << 0)
#define RG_AUDHSPWRUP_IBIAS_VAUDP15_SFT                   1
#define RG_AUDHSPWRUP_IBIAS_VAUDP15_MASK                  0x1
#define RG_AUDHSPWRUP_IBIAS_VAUDP15_MASK_SFT              (0x1 << 1)
#define RG_AUDHSMUXINPUTSEL_VAUDP15_SFT                   2
#define RG_AUDHSMUXINPUTSEL_VAUDP15_MASK                  0x3
#define RG_AUDHSMUXINPUTSEL_VAUDP15_MASK_SFT              (0x3 << 2)
#define RG_AUDHSSCDISABLE_VAUDP15_SFT                     4
#define RG_AUDHSSCDISABLE_VAUDP15_MASK                    0x1
#define RG_AUDHSSCDISABLE_VAUDP15_MASK_SFT                (0x1 << 4)
#define RG_AUDHSBSCCURRENT_VAUDP15_SFT                    5
#define RG_AUDHSBSCCURRENT_VAUDP15_MASK                   0x1
#define RG_AUDHSBSCCURRENT_VAUDP15_MASK_SFT               (0x1 << 5)
#define RG_AUDHSSTARTUP_VAUDP15_SFT                       6
#define RG_AUDHSSTARTUP_VAUDP15_MASK                      0x1
#define RG_AUDHSSTARTUP_VAUDP15_MASK_SFT                  (0x1 << 6)
#define RG_HSOUTPUTSTBENH_VAUDP15_SFT                     7
#define RG_HSOUTPUTSTBENH_VAUDP15_MASK                    0x1
#define RG_HSOUTPUTSTBENH_VAUDP15_MASK_SFT                (0x1 << 7)
#define RG_HSINPUTSTBENH_VAUDP15_SFT                      8
#define RG_HSINPUTSTBENH_VAUDP15_MASK                     0x1
#define RG_HSINPUTSTBENH_VAUDP15_MASK_SFT                 (0x1 << 8)
#define RG_HSINPUTRESET0_VAUDP15_SFT                      9
#define RG_HSINPUTRESET0_VAUDP15_MASK                     0x1
#define RG_HSINPUTRESET0_VAUDP15_MASK_SFT                 (0x1 << 9)
#define RG_HSOUTPUTRESET0_VAUDP15_SFT                     10
#define RG_HSOUTPUTRESET0_VAUDP15_MASK                    0x1
#define RG_HSOUTPUTRESET0_VAUDP15_MASK_SFT                (0x1 << 10)
#define RG_HSOUT_SHORTVCM_VAUDP15_SFT                     11
#define RG_HSOUT_SHORTVCM_VAUDP15_MASK                    0x1
#define RG_HSOUT_SHORTVCM_VAUDP15_MASK_SFT                (0x1 << 11)

/* MT6357_AUDDEC_ANA_CON4 */
#define RG_AUDLOLPWRUP_VAUDP15_SFT                        0
#define RG_AUDLOLPWRUP_VAUDP15_MASK                       0x1
#define RG_AUDLOLPWRUP_VAUDP15_MASK_SFT                   (0x1 << 0)
#define RG_AUDLOLPWRUP_IBIAS_VAUDP15_SFT                  1
#define RG_AUDLOLPWRUP_IBIAS_VAUDP15_MASK                 0x1
#define RG_AUDLOLPWRUP_IBIAS_VAUDP15_MASK_SFT             (0x1 << 1)
#define RG_AUDLOLMUXINPUTSEL_VAUDP15_SFT                  2
#define RG_AUDLOLMUXINPUTSEL_VAUDP15_MASK                 0x3
#define RG_AUDLOLMUXINPUTSEL_VAUDP15_MASK_SFT             (0x3 << 2)
#define RG_AUDLOLSCDISABLE_VAUDP15_SFT                    4
#define RG_AUDLOLSCDISABLE_VAUDP15_MASK                   0x1
#define RG_AUDLOLSCDISABLE_VAUDP15_MASK_SFT               (0x1 << 4)
#define RG_AUDLOLBSCCURRENT_VAUDP15_SFT                   5
#define RG_AUDLOLBSCCURRENT_VAUDP15_MASK                  0x1
#define RG_AUDLOLBSCCURRENT_VAUDP15_MASK_SFT              (0x1 << 5)
#define RG_AUDLOSTARTUP_VAUDP15_SFT                       6
#define RG_AUDLOSTARTUP_VAUDP15_MASK                      0x1
#define RG_AUDLOSTARTUP_VAUDP15_MASK_SFT                  (0x1 << 6)
#define RG_LOINPUTSTBENH_VAUDP15_SFT                      7
#define RG_LOINPUTSTBENH_VAUDP15_MASK                     0x1
#define RG_LOINPUTSTBENH_VAUDP15_MASK_SFT                 (0x1 << 7)
#define RG_LOOUTPUTSTBENH_VAUDP15_SFT                     8
#define RG_LOOUTPUTSTBENH_VAUDP15_MASK                    0x1
#define RG_LOOUTPUTSTBENH_VAUDP15_MASK_SFT                (0x1 << 8)
#define RG_LOINPUTRESET0_VAUDP15_SFT                      9
#define RG_LOINPUTRESET0_VAUDP15_MASK                     0x1
#define RG_LOINPUTRESET0_VAUDP15_MASK_SFT                 (0x1 << 9)
#define RG_LOOUTPUTRESET0_VAUDP15_SFT                     10
#define RG_LOOUTPUTRESET0_VAUDP15_MASK                    0x1
#define RG_LOOUTPUTRESET0_VAUDP15_MASK_SFT                (0x1 << 10)
#define RG_LOOUT_SHORTVCM_VAUDP15_SFT                     11
#define RG_LOOUT_SHORTVCM_VAUDP15_MASK                    0x1
#define RG_LOOUT_SHORTVCM_VAUDP15_MASK_SFT                (0x1 << 11)

/* MT6357_AUDDEC_ANA_CON5 */
#define RG_AUDTRIMBUF_INPUTMUXSEL_VAUDP15_SFT             0
#define RG_AUDTRIMBUF_INPUTMUXSEL_VAUDP15_MASK            0xf
#define RG_AUDTRIMBUF_INPUTMUXSEL_VAUDP15_MASK_SFT        (0xf << 0)
#define RG_AUDTRIMBUF_GAINSEL_VAUDP15_SFT                 4
#define RG_AUDTRIMBUF_GAINSEL_VAUDP15_MASK                0x3
#define RG_AUDTRIMBUF_GAINSEL_VAUDP15_MASK_SFT            (0x3 << 4)
#define RG_AUDTRIMBUF_EN_VAUDP15_SFT                      6
#define RG_AUDTRIMBUF_EN_VAUDP15_MASK                     0x1
#define RG_AUDTRIMBUF_EN_VAUDP15_MASK_SFT                 (0x1 << 6)
#define RG_AUDHPSPKDET_INPUTMUXSEL_VAUDP15_SFT            8
#define RG_AUDHPSPKDET_INPUTMUXSEL_VAUDP15_MASK           0x3
#define RG_AUDHPSPKDET_INPUTMUXSEL_VAUDP15_MASK_SFT       (0x3 << 8)
#define RG_AUDHPSPKDET_OUTPUTMUXSEL_VAUDP15_SFT           10
#define RG_AUDHPSPKDET_OUTPUTMUXSEL_VAUDP15_MASK          0x3
#define RG_AUDHPSPKDET_OUTPUTMUXSEL_VAUDP15_MASK_SFT      (0x3 << 10)
#define RG_AUDHPSPKDET_EN_VAUDP15_SFT                     12
#define RG_AUDHPSPKDET_EN_VAUDP15_MASK                    0x1
#define RG_AUDHPSPKDET_EN_VAUDP15_MASK_SFT                (0x1 << 12)

/* MT6357_AUDDEC_ANA_CON6 */
#define RG_ABIDEC_RSVD0_VA28_SFT                          0
#define RG_ABIDEC_RSVD0_VA28_MASK                         0xff
#define RG_ABIDEC_RSVD0_VA28_MASK_SFT                     (0xff << 0)
#define RG_ABIDEC_RSVD0_VAUDP15_SFT                       8
#define RG_ABIDEC_RSVD0_VAUDP15_MASK                      0xff
#define RG_ABIDEC_RSVD0_VAUDP15_MASK_SFT                  (0xff << 8)

/* MT6357_AUDDEC_ANA_CON7 */
#define RG_ABIDEC_RSVD1_VAUDP15_SFT                       0
#define RG_ABIDEC_RSVD1_VAUDP15_MASK                      0xff
#define RG_ABIDEC_RSVD1_VAUDP15_MASK_SFT                  (0xff << 0)
#define RG_ABIDEC_RSVD2_VAUDP15_SFT                       8
#define RG_ABIDEC_RSVD2_VAUDP15_MASK                      0xff
#define RG_ABIDEC_RSVD2_VAUDP15_MASK_SFT                  (0xff << 8)

/* MT6357_AUDDEC_ANA_CON8 */
#define RG_AUDZCDMUXSEL_VAUDP15_SFT                       0
#define RG_AUDZCDMUXSEL_VAUDP15_MASK                      0x7
#define RG_AUDZCDMUXSEL_VAUDP15_MASK_SFT                  (0x7 << 0)
#define RG_AUDZCDCLKSEL_VAUDP15_SFT                       3
#define RG_AUDZCDCLKSEL_VAUDP15_MASK                      0x1
#define RG_AUDZCDCLKSEL_VAUDP15_MASK_SFT                  (0x1 << 3)

/* MT6357_AUDDEC_ANA_CON9 */
#define RG_AUDBIASADJ_0_VAUDP15_SFT                       7
#define RG_AUDBIASADJ_0_VAUDP15_MASK                      0x1ff
#define RG_AUDBIASADJ_0_VAUDP15_MASK_SFT                  (0x1ff << 7)

/* MT6357_AUDDEC_ANA_CON10 */
#define RG_AUDBIASADJ_1_VAUDP15_SFT                       0
#define RG_AUDBIASADJ_1_VAUDP15_MASK                      0xff
#define RG_AUDBIASADJ_1_VAUDP15_MASK_SFT                  (0xff << 0)
#define RG_AUDIBIASPWRDN_VAUDP15_SFT                      8
#define RG_AUDIBIASPWRDN_VAUDP15_MASK                     0x1
#define RG_AUDIBIASPWRDN_VAUDP15_MASK_SFT                 (0x1 << 8)

/* MT6357_AUDDEC_ANA_CON11 */
#define RG_RSTB_DECODER_VA28_SFT                          0
#define RG_RSTB_DECODER_VA28_MASK                         0x1
#define RG_RSTB_DECODER_VA28_MASK_SFT                     (0x1 << 0)
#define RG_SEL_DECODER_96K_VA28_SFT                       1
#define RG_SEL_DECODER_96K_VA28_MASK                      0x1
#define RG_SEL_DECODER_96K_VA28_MASK_SFT                  (0x1 << 1)
#define RG_SEL_DELAY_VCORE_SFT                            2
#define RG_SEL_DELAY_VCORE_MASK                           0x1
#define RG_SEL_DELAY_VCORE_MASK_SFT                       (0x1 << 2)
#define RG_AUDGLB_PWRDN_VA28_SFT                          4
#define RG_AUDGLB_PWRDN_VA28_MASK                         0x1
#define RG_AUDGLB_PWRDN_VA28_MASK_SFT                     (0x1 << 4)
#define RG_RSTB_ENCODER_VA28_SFT                          5
#define RG_RSTB_ENCODER_VA28_MASK                         0x1
#define RG_RSTB_ENCODER_VA28_MASK_SFT                     (0x1 << 5)
#define RG_SEL_ENCODER_96K_VA28_SFT                       6
#define RG_SEL_ENCODER_96K_VA28_MASK                      0x1
#define RG_SEL_ENCODER_96K_VA28_MASK_SFT                  (0x1 << 6)

/* MT6357_AUDDEC_ANA_CON12 */
#define RG_HCLDO_EN_VA18_SFT                              0
#define RG_HCLDO_EN_VA18_MASK                             0x1
#define RG_HCLDO_EN_VA18_MASK_SFT                         (0x1 << 0)
#define RG_HCLDO_PDDIS_EN_VA18_SFT                        1
#define RG_HCLDO_PDDIS_EN_VA18_MASK                       0x1
#define RG_HCLDO_PDDIS_EN_VA18_MASK_SFT                   (0x1 << 1)
#define RG_HCLDO_REMOTE_SENSE_VA18_SFT                    2
#define RG_HCLDO_REMOTE_SENSE_VA18_MASK                   0x1
#define RG_HCLDO_REMOTE_SENSE_VA18_MASK_SFT               (0x1 << 2)
#define RG_LCLDO_EN_VA18_SFT                              4
#define RG_LCLDO_EN_VA18_MASK                             0x1
#define RG_LCLDO_EN_VA18_MASK_SFT                         (0x1 << 4)
#define RG_LCLDO_PDDIS_EN_VA18_SFT                        5
#define RG_LCLDO_PDDIS_EN_VA18_MASK                       0x1
#define RG_LCLDO_PDDIS_EN_VA18_MASK_SFT                   (0x1 << 5)
#define RG_LCLDO_REMOTE_SENSE_VA18_SFT                    6
#define RG_LCLDO_REMOTE_SENSE_VA18_MASK                   0x1
#define RG_LCLDO_REMOTE_SENSE_VA18_MASK_SFT               (0x1 << 6)
#define RG_LCLDO_ENC_EN_VA28_SFT                          8
#define RG_LCLDO_ENC_EN_VA28_MASK                         0x1
#define RG_LCLDO_ENC_EN_VA28_MASK_SFT                     (0x1 << 8)
#define RG_LCLDO_ENC_PDDIS_EN_VA28_SFT                    9
#define RG_LCLDO_ENC_PDDIS_EN_VA28_MASK                   0x1
#define RG_LCLDO_ENC_PDDIS_EN_VA28_MASK_SFT               (0x1 << 9)
#define RG_LCLDO_ENC_REMOTE_SENSE_VA28_SFT                10
#define RG_LCLDO_ENC_REMOTE_SENSE_VA28_MASK               0x1
#define RG_LCLDO_ENC_REMOTE_SENSE_VA28_MASK_SFT           (0x1 << 10)
#define RG_VA33REFGEN_EN_VA18_SFT                         12
#define RG_VA33REFGEN_EN_VA18_MASK                        0x1
#define RG_VA33REFGEN_EN_VA18_MASK_SFT                    (0x1 << 12)
#define RG_VA28REFGEN_EN_VA28_SFT                         13
#define RG_VA28REFGEN_EN_VA28_MASK                        0x1
#define RG_VA28REFGEN_EN_VA28_MASK_SFT                    (0x1 << 13)
#define RG_HCLDO_VOSEL_VA18_SFT                           14
#define RG_HCLDO_VOSEL_VA18_MASK                          0x1
#define RG_HCLDO_VOSEL_VA18_MASK_SFT                      (0x1 << 14)
#define RG_LCLDO_VOSEL_VA18_SFT                           15
#define RG_LCLDO_VOSEL_VA18_MASK                          0x1
#define RG_LCLDO_VOSEL_VA18_MASK_SFT                      (0x1 << 15)

/* MT6357_AUDDEC_ANA_CON13 */
#define RG_NVREG_EN_VAUDP15_SFT                           0
#define RG_NVREG_EN_VAUDP15_MASK                          0x1
#define RG_NVREG_EN_VAUDP15_MASK_SFT                      (0x1 << 0)
#define RG_NVREG_PULL0V_VAUDP15_SFT                       1
#define RG_NVREG_PULL0V_VAUDP15_MASK                      0x1
#define RG_NVREG_PULL0V_VAUDP15_MASK_SFT                  (0x1 << 1)
#define RG_AUDPMU_RSD0_VAUDP15_SFT                        4
#define RG_AUDPMU_RSD0_VAUDP15_MASK                       0xf
#define RG_AUDPMU_RSD0_VAUDP15_MASK_SFT                   (0xf << 4)
#define RG_AUDPMU_RSD0_VA18_SFT                           8
#define RG_AUDPMU_RSD0_VA18_MASK                          0xf
#define RG_AUDPMU_RSD0_VA18_MASK_SFT                      (0xf << 8)
#define RG_AUDPMU_RSD0_VA28_SFT                           12
#define RG_AUDPMU_RSD0_VA28_MASK                          0xf
#define RG_AUDPMU_RSD0_VA28_MASK_SFT                      (0xf << 12)

/* MT6357_ZCD_CON0 */
#define RG_AUDZCDENABLE_SFT                               0
#define RG_AUDZCDENABLE_MASK                              0x1
#define RG_AUDZCDENABLE_MASK_SFT                          (0x1 << 0)
#define RG_AUDZCDGAINSTEPTIME_SFT                         1
#define RG_AUDZCDGAINSTEPTIME_MASK                        0x7
#define RG_AUDZCDGAINSTEPTIME_MASK_SFT                    (0x7 << 1)
#define RG_AUDZCDGAINSTEPSIZE_SFT                         4
#define RG_AUDZCDGAINSTEPSIZE_MASK                        0x3
#define RG_AUDZCDGAINSTEPSIZE_MASK_SFT                    (0x3 << 4)
#define RG_AUDZCDTIMEOUTMODESEL_SFT                       6
#define RG_AUDZCDTIMEOUTMODESEL_MASK                      0x1
#define RG_AUDZCDTIMEOUTMODESEL_MASK_SFT                  (0x1 << 6)

/* MT6357_ZCD_CON1 */
#define RG_AUDLOLGAIN_SFT                                 0
#define RG_AUDLOLGAIN_MASK                                0x1f
#define RG_AUDLOLGAIN_MASK_SFT                            (0x1f << 0)
#define RG_AUDLORGAIN_SFT                                 7
#define RG_AUDLORGAIN_MASK                                0x1f
#define RG_AUDLORGAIN_MASK_SFT                            (0x1f << 7)

/* MT6357_ZCD_CON2 */
#define RG_AUDHPLGAIN_SFT                                 0
#define RG_AUDHPLGAIN_MASK                                0x1f
#define RG_AUDHPLGAIN_MASK_SFT                            (0x1f << 0)
#define RG_AUDHPRGAIN_SFT                                 7
#define RG_AUDHPRGAIN_MASK                                0x1f
#define RG_AUDHPRGAIN_MASK_SFT                            (0x1f << 7)

/* MT6357_ZCD_CON3 */
#define RG_AUDHSGAIN_SFT                                  0
#define RG_AUDHSGAIN_MASK                                 0x1f
#define RG_AUDHSGAIN_MASK_SFT                             (0x1f << 0)

/* MT6357_ZCD_CON4 */
#define RG_AUDIVLGAIN_SFT                                 0
#define RG_AUDIVLGAIN_MASK                                0x7
#define RG_AUDIVLGAIN_MASK_SFT                            (0x7 << 0)
#define RG_AUDIVRGAIN_SFT                                 8
#define RG_AUDIVRGAIN_MASK                                0x7
#define RG_AUDIVRGAIN_MASK_SFT                            (0x7 << 8)

/* MT6357_ZCD_CON5 */
#define RG_AUDINTGAIN1_SFT                                0
#define RG_AUDINTGAIN1_MASK                               0x3f
#define RG_AUDINTGAIN1_MASK_SFT                           (0x3f << 0)
#define RG_AUDINTGAIN2_SFT                                8
#define RG_AUDINTGAIN2_MASK                               0x3f
#define RG_AUDINTGAIN2_MASK_SFT                           (0x3f << 8)

/* audio register */
#define MT6357_DRV_CON3            0x38
#define MT6357_GPIO_DIR0           0x88

#define MT6357_GPIO_MODE2          0xb6	/* mosi */
#define MT6357_GPIO_MODE2_SET      0xb8
#define MT6357_GPIO_MODE2_CLR      0xba

#define MT6357_GPIO_MODE3          0xbc	/* miso */
#define MT6357_GPIO_MODE3_SET      0xbe
#define MT6357_GPIO_MODE3_CLR      0xc0

#define MT6357_TOP_CKPDN_CON0      0x10c
#define MT6357_TOP_CKPDN_CON0_SET  0x10e
#define MT6357_TOP_CKPDN_CON0_CLR  0x110

#define MT6357_TOP_CKHWEN_CON0     0x12a
#define MT6357_TOP_CKHWEN_CON0_SET 0x12c
#define MT6357_TOP_CKHWEN_CON0_CLR 0x12e

#define MT6357_OTP_CON0            0x390
#define MT6357_OTP_CON8            0x3a0
#define MT6357_OTP_CON11           0x3a6
#define MT6357_OTP_CON12           0x3a8
#define MT6357_OTP_CON13           0x3aa

#define MT6357_DCXO_CW14           0x7ac

#define MT6357_AUXADC_CON10        0x1138

/* audio register */
#define MT6357_AUD_TOP_ID                    0x2080
#define MT6357_AUD_TOP_REV0                  0x2082
#define MT6357_AUD_TOP_DBI                   0x2084
#define MT6357_AUD_TOP_DXI                   0x2086
#define MT6357_AUD_TOP_CKPDN_TPM0            0x2088
#define MT6357_AUD_TOP_CKPDN_TPM1            0x208a
#define MT6357_AUD_TOP_CKPDN_CON0            0x208c
#define MT6357_AUD_TOP_CKPDN_CON0_SET        0x208e
#define MT6357_AUD_TOP_CKPDN_CON0_CLR        0x2090
#define MT6357_AUD_TOP_CKSEL_CON0            0x2092
#define MT6357_AUD_TOP_CKSEL_CON0_SET        0x2094
#define MT6357_AUD_TOP_CKSEL_CON0_CLR        0x2096
#define MT6357_AUD_TOP_CKTST_CON0            0x2098
#define MT6357_AUD_TOP_RST_CON0              0x209a
#define MT6357_AUD_TOP_RST_CON0_SET          0x209c
#define MT6357_AUD_TOP_RST_CON0_CLR          0x209e
#define MT6357_AUD_TOP_RST_BANK_CON0         0x20a0
#define MT6357_AUD_TOP_INT_CON0              0x20a2
#define MT6357_AUD_TOP_INT_CON0_SET          0x20a4
#define MT6357_AUD_TOP_INT_CON0_CLR          0x20a6
#define MT6357_AUD_TOP_INT_MASK_CON0         0x20a8
#define MT6357_AUD_TOP_INT_MASK_CON0_SET     0x20aa
#define MT6357_AUD_TOP_INT_MASK_CON0_CLR     0x20ac
#define MT6357_AUD_TOP_INT_STATUS0           0x20ae
#define MT6357_AUD_TOP_INT_RAW_STATUS0       0x20b0
#define MT6357_AUD_TOP_INT_MISC_CON0         0x20b2
#define MT6357_AUDNCP_CLKDIV_CON0            0x20b4
#define MT6357_AUDNCP_CLKDIV_CON1            0x20b6
#define MT6357_AUDNCP_CLKDIV_CON2            0x20b8
#define MT6357_AUDNCP_CLKDIV_CON3            0x20ba
#define MT6357_AUDNCP_CLKDIV_CON4            0x20bc
#define MT6357_AUD_TOP_MON_CON0              0x20be

#define MT6357_AFE_UL_DL_CON0                0x2108
#define MT6357_AFE_DL_SRC2_CON0_L            0x210a
#define MT6357_AFE_UL_SRC_CON0_H             0x210c
#define MT6357_AFE_UL_SRC_CON0_L             0x210e
#define MT6357_AFE_TOP_CON0                  0x2110
#define MT6357_AUDIO_TOP_CON0                0x2112
#define MT6357_AFE_MON_DEBUG0                0x2114
#define MT6357_AFUNC_AUD_CON0                0x2116
#define MT6357_AFUNC_AUD_CON1                0x2118
#define MT6357_AFUNC_AUD_CON2                0x211a
#define MT6357_AFUNC_AUD_CON3                0x211c
#define MT6357_AFUNC_AUD_CON4                0x211e
#define MT6357_AFUNC_AUD_CON5                0x2120
#define MT6357_AFUNC_AUD_CON6                0x2122
#define MT6357_AFUNC_AUD_MON0                0x2124
#define MT6357_AUDRC_TUNE_MON0               0x2126
#define MT6357_AFE_ADDA_MTKAIF_FIFO_CFG0     0x2128
#define MT6357_AFE_ADDA_MTKAIF_FIFO_LOG_MON1 0x212a
#define MT6357_AFE_ADDA_MTKAIF_MON0          0x212c
#define MT6357_AFE_ADDA_MTKAIF_MON1          0x212e
#define MT6357_AFE_ADDA_MTKAIF_MON2          0x2130
#define MT6357_AFE_ADDA_MTKAIF_MON3          0x2132
#define MT6357_AFE_ADDA_MTKAIF_CFG0          0x2134
#define MT6357_AFE_ADDA_MTKAIF_RX_CFG0       0x2136
#define MT6357_AFE_ADDA_MTKAIF_RX_CFG1       0x2138
#define MT6357_AFE_ADDA_MTKAIF_RX_CFG2       0x213a
#define MT6357_AFE_ADDA_MTKAIF_RX_CFG3       0x213c
#define MT6357_AFE_ADDA_MTKAIF_TX_CFG1       0x213e
#define MT6357_AFE_SGEN_CFG0                 0x2140
#define MT6357_AFE_SGEN_CFG1                 0x2142
#define MT6357_AFE_ADC_ASYNC_FIFO_CFG        0x2144
#define MT6357_AFE_DCCLK_CFG0                0x2146
#define MT6357_AFE_DCCLK_CFG1                0x2148
#define MT6357_AUDIO_DIG_CFG                 0x214a
#define MT6357_AFE_AUD_PAD_TOP               0x214c
#define MT6357_AFE_AUD_PAD_TOP_MON           0x214e
#define MT6357_AFE_AUD_PAD_TOP_MON1          0x2150
#define MT6357_AUDENC_DSN_ID                 0x2180
#define MT6357_AUDENC_DSN_REV0               0x2182
#define MT6357_AUDENC_DSN_DBI                0x2184
#define MT6357_AUDENC_DSN_FPI                0x2186
#define MT6357_AUDENC_ANA_CON0               0x2188
#define MT6357_AUDENC_ANA_CON1               0x218a
#define MT6357_AUDENC_ANA_CON2               0x218c
#define MT6357_AUDENC_ANA_CON3               0x218e
#define MT6357_AUDENC_ANA_CON4               0x2190
#define MT6357_AUDENC_ANA_CON5               0x2192
#define MT6357_AUDENC_ANA_CON6               0x2194
#define MT6357_AUDENC_ANA_CON7               0x2196
#define MT6357_AUDENC_ANA_CON8               0x2198
#define MT6357_AUDENC_ANA_CON9               0x219a
#define MT6357_AUDENC_ANA_CON10              0x219c
#define MT6357_AUDENC_ANA_CON11              0x219e
#define MT6357_AUDDEC_DSN_ID                 0x2200
#define MT6357_AUDDEC_DSN_REV0               0x2202
#define MT6357_AUDDEC_DSN_DBI                0x2204
#define MT6357_AUDDEC_DSN_FPI                0x2206
#define MT6357_AUDDEC_ANA_CON0               0x2208
#define MT6357_AUDDEC_ANA_CON1               0x220a
#define MT6357_AUDDEC_ANA_CON2               0x220c
#define MT6357_AUDDEC_ANA_CON3               0x220e
#define MT6357_AUDDEC_ANA_CON4               0x2210
#define MT6357_AUDDEC_ANA_CON5               0x2212
#define MT6357_AUDDEC_ANA_CON6               0x2214
#define MT6357_AUDDEC_ANA_CON7               0x2216
#define MT6357_AUDDEC_ANA_CON8               0x2218
#define MT6357_AUDDEC_ANA_CON9               0x221a
#define MT6357_AUDDEC_ANA_CON10              0x221c
#define MT6357_AUDDEC_ANA_CON11              0x221e
#define MT6357_AUDDEC_ANA_CON12              0x2220
#define MT6357_AUDDEC_ANA_CON13              0x2222
#define MT6357_AUDDEC_ELR_NUM                0x2224
#define MT6357_AUDDEC_ELR_0                  0x2226
#define MT6357_AUDZCD_DSN_ID                 0x2280
#define MT6357_AUDZCD_DSN_REV0               0x2282
#define MT6357_AUDZCD_DSN_DBI                0x2284
#define MT6357_AUDZCD_DSN_FPI                0x2286
#define MT6357_ZCD_CON0                      0x2288
#define MT6357_ZCD_CON1                      0x228a
#define MT6357_ZCD_CON2                      0x228c
#define MT6357_ZCD_CON3                      0x228e
#define MT6357_ZCD_CON4                      0x2290
#define MT6357_ZCD_CON5                      0x2292

#define MT6357_MAX_REGISTER MT6357_ZCD_CON5

enum {
	MT6357_MTKAIF_PROTOCOL_1 = 0,
	MT6357_MTKAIF_PROTOCOL_2,
	MT6357_MTKAIF_PROTOCOL_2_CLK_P2,
};

enum SPK_PA_DEVICE_ID {
	EXT_AMP_TPA2011,
	EXT_AMP_AD51562,
	EXT_AMP_AW87390,
};

/* codec name */
#define CODEC_MT6357_NAME "mtk-codec-mt6357"
#define DEVICE_MT6357_NAME "mt6357-sound"

#if IS_ENABLED(CONFIG_IDME)
/* spk codec gain calibration information */
#define SPK_CODEC_GAIN_CAL_L	"/idme/spk_gain_cal_l"
#define SPK_CODEC_GAIN_CAL_R	"/idme/spk_gain_cal_r"
char SpkMaxCodecGainVaildValues[3] = {'3','4','5'};
#endif

struct mt6357_codec_ops {
	int (*enable_dc_compensation)(bool enable);
	int (*set_lch_dc_compensation)(int value);
	int (*set_rch_dc_compensation)(int value);
	int (*adda_dl_gain_control)(bool mute);
};

struct mt8169_raspite_priv {
	struct gpio_desc *ext_amp1_gpio;
	struct gpio_desc *ext_amp2_gpio;
	struct gpio_desc *spk_pa_5v_en_gpio;
	struct gpio_desc *apa_sdn_gpio;
	struct gpio_desc *spk_pa_id1_gpio;
	struct gpio_desc *spk_pa_id2_gpio;
	int spk_pa_id;
	unsigned int ext_spk_amp_vdd_on_time_us;
	unsigned int fs1512n_start_work_time_us;
	unsigned int fs1512n_mode_setting_time_us;
	unsigned int fs1512n_power_down_time_us;
	int spk_codec_gain_left;
	int spk_codec_gain_right;
};

/* set only during init */
int mt6357_set_mtkaif_protocol(struct snd_soc_component *cmpnt,
			       int mtkaif_protocol);
int mt6357_mtkaif_calibration_enable(struct snd_soc_component *cmpnt);
int mt6357_mtkaif_calibration_disable(struct snd_soc_component *cmpnt);
int mt6357_set_mtkaif_calibration_phase(struct snd_soc_component *cmpnt,
					int phase_1, int phase_2);
#endif /* __MT6357_H__ */
