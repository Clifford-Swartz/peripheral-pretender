/*
 * Component description for RTC
 *
 * Copyright (c) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * Subject to your compliance with these terms, you may use Microchip software and any derivatives
 * exclusively with Microchip products. It is your responsibility to comply with third party license
 * terms applicable to your use of third party software (including open source software) that may
 * accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY,
 * APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
 * LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF
 * MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE FULLEST EXTENT
 * ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT
 * EXCEED THE AMOUNT OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 */

/* file generated from device description version 2021-03-23T20:32:05Z */
#ifndef _PIC32CMMC00_RTC_COMPONENT_H_
#define _PIC32CMMC00_RTC_COMPONENT_H_

/* ************************************************************************** */
/*   SOFTWARE API DEFINITION FOR RTC                                          */
/* ************************************************************************** */

/* -------- RTC_MODE0_CTRLA : (RTC Offset: 0x00) (R/W 16) MODE0 Control A -------- */
#define RTC_MODE0_CTRLA_RESETVALUE            _UINT16_(0x00)                                       /*  (RTC_MODE0_CTRLA) MODE0 Control A  Reset Value */

#define RTC_MODE0_CTRLA_SWRST_Pos             _UINT16_(0)                                          /* (RTC_MODE0_CTRLA) Software Reset Position */
#define RTC_MODE0_CTRLA_SWRST_Msk             (_UINT16_(0x1) << RTC_MODE0_CTRLA_SWRST_Pos)         /* (RTC_MODE0_CTRLA) Software Reset Mask */
#define RTC_MODE0_CTRLA_SWRST(value)          (RTC_MODE0_CTRLA_SWRST_Msk & (_UINT16_(value) << RTC_MODE0_CTRLA_SWRST_Pos)) /* Assigment of value for SWRST in the RTC_MODE0_CTRLA register */
#define RTC_MODE0_CTRLA_ENABLE_Pos            _UINT16_(1)                                          /* (RTC_MODE0_CTRLA) Enable Position */
#define RTC_MODE0_CTRLA_ENABLE_Msk            (_UINT16_(0x1) << RTC_MODE0_CTRLA_ENABLE_Pos)        /* (RTC_MODE0_CTRLA) Enable Mask */
#define RTC_MODE0_CTRLA_ENABLE(value)         (RTC_MODE0_CTRLA_ENABLE_Msk & (_UINT16_(value) << RTC_MODE0_CTRLA_ENABLE_Pos)) /* Assigment of value for ENABLE in the RTC_MODE0_CTRLA register */
#define RTC_MODE0_CTRLA_MODE_Pos              _UINT16_(2)                                          /* (RTC_MODE0_CTRLA) Operating Mode Position */
#define RTC_MODE0_CTRLA_MODE_Msk              (_UINT16_(0x3) << RTC_MODE0_CTRLA_MODE_Pos)          /* (RTC_MODE0_CTRLA) Operating Mode Mask */
#define RTC_MODE0_CTRLA_MODE(value)           (RTC_MODE0_CTRLA_MODE_Msk & (_UINT16_(value) << RTC_MODE0_CTRLA_MODE_Pos)) /* Assigment of value for MODE in the RTC_MODE0_CTRLA register */
#define   RTC_MODE0_CTRLA_MODE_COUNT32_Val    _UINT16_(0x0)                                        /* (RTC_MODE0_CTRLA) Mode 0: 32-bit Counter  */
#define   RTC_MODE0_CTRLA_MODE_COUNT16_Val    _UINT16_(0x1)                                        /* (RTC_MODE0_CTRLA) Mode 1: 16-bit Counter  */
#define   RTC_MODE0_CTRLA_MODE_CLOCK_Val      _UINT16_(0x2)                                        /* (RTC_MODE0_CTRLA) Mode 2: Clock/Calendar  */
#define RTC_MODE0_CTRLA_MODE_COUNT32          (RTC_MODE0_CTRLA_MODE_COUNT32_Val << RTC_MODE0_CTRLA_MODE_Pos) /* (RTC_MODE0_CTRLA) Mode 0: 32-bit Counter Position  */
#define RTC_MODE0_CTRLA_MODE_COUNT16          (RTC_MODE0_CTRLA_MODE_COUNT16_Val << RTC_MODE0_CTRLA_MODE_Pos) /* (RTC_MODE0_CTRLA) Mode 1: 16-bit Counter Position  */
#define RTC_MODE0_CTRLA_MODE_CLOCK            (RTC_MODE0_CTRLA_MODE_CLOCK_Val << RTC_MODE0_CTRLA_MODE_Pos) /* (RTC_MODE0_CTRLA) Mode 2: Clock/Calendar Position  */
#define RTC_MODE0_CTRLA_MATCHCLR_Pos          _UINT16_(7)                                          /* (RTC_MODE0_CTRLA) Clear on Match Position */
#define RTC_MODE0_CTRLA_MATCHCLR_Msk          (_UINT16_(0x1) << RTC_MODE0_CTRLA_MATCHCLR_Pos)      /* (RTC_MODE0_CTRLA) Clear on Match Mask */
#define RTC_MODE0_CTRLA_MATCHCLR(value)       (RTC_MODE0_CTRLA_MATCHCLR_Msk & (_UINT16_(value) << RTC_MODE0_CTRLA_MATCHCLR_Pos)) /* Assigment of value for MATCHCLR in the RTC_MODE0_CTRLA register */
#define RTC_MODE0_CTRLA_PRESCALER_Pos         _UINT16_(8)                                          /* (RTC_MODE0_CTRLA) Prescaler Position */
#define RTC_MODE0_CTRLA_PRESCALER_Msk         (_UINT16_(0xF) << RTC_MODE0_CTRLA_PRESCALER_Pos)     /* (RTC_MODE0_CTRLA) Prescaler Mask */
#define RTC_MODE0_CTRLA_PRESCALER(value)      (RTC_MODE0_CTRLA_PRESCALER_Msk & (_UINT16_(value) << RTC_MODE0_CTRLA_PRESCALER_Pos)) /* Assigment of value for PRESCALER in the RTC_MODE0_CTRLA register */
#define   RTC_MODE0_CTRLA_PRESCALER_OFF_Val   _UINT16_(0x0)                                        /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/1  */
#define   RTC_MODE0_CTRLA_PRESCALER_DIV1_Val  _UINT16_(0x1)                                        /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/1  */
#define   RTC_MODE0_CTRLA_PRESCALER_DIV2_Val  _UINT16_(0x2)                                        /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/2  */
#define   RTC_MODE0_CTRLA_PRESCALER_DIV4_Val  _UINT16_(0x3)                                        /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/4  */
#define   RTC_MODE0_CTRLA_PRESCALER_DIV8_Val  _UINT16_(0x4)                                        /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/8  */
#define   RTC_MODE0_CTRLA_PRESCALER_DIV16_Val _UINT16_(0x5)                                        /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/16  */
#define   RTC_MODE0_CTRLA_PRESCALER_DIV32_Val _UINT16_(0x6)                                        /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/32  */
#define   RTC_MODE0_CTRLA_PRESCALER_DIV64_Val _UINT16_(0x7)                                        /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/64  */
#define   RTC_MODE0_CTRLA_PRESCALER_DIV128_Val _UINT16_(0x8)                                        /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/128  */
#define   RTC_MODE0_CTRLA_PRESCALER_DIV256_Val _UINT16_(0x9)                                        /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/256  */
#define   RTC_MODE0_CTRLA_PRESCALER_DIV512_Val _UINT16_(0xA)                                        /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/512  */
#define   RTC_MODE0_CTRLA_PRESCALER_DIV1024_Val _UINT16_(0xB)                                        /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/1024  */
#define RTC_MODE0_CTRLA_PRESCALER_OFF         (RTC_MODE0_CTRLA_PRESCALER_OFF_Val << RTC_MODE0_CTRLA_PRESCALER_Pos) /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/1 Position  */
#define RTC_MODE0_CTRLA_PRESCALER_DIV1        (RTC_MODE0_CTRLA_PRESCALER_DIV1_Val << RTC_MODE0_CTRLA_PRESCALER_Pos) /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/1 Position  */
#define RTC_MODE0_CTRLA_PRESCALER_DIV2        (RTC_MODE0_CTRLA_PRESCALER_DIV2_Val << RTC_MODE0_CTRLA_PRESCALER_Pos) /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/2 Position  */
#define RTC_MODE0_CTRLA_PRESCALER_DIV4        (RTC_MODE0_CTRLA_PRESCALER_DIV4_Val << RTC_MODE0_CTRLA_PRESCALER_Pos) /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/4 Position  */
#define RTC_MODE0_CTRLA_PRESCALER_DIV8        (RTC_MODE0_CTRLA_PRESCALER_DIV8_Val << RTC_MODE0_CTRLA_PRESCALER_Pos) /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/8 Position  */
#define RTC_MODE0_CTRLA_PRESCALER_DIV16       (RTC_MODE0_CTRLA_PRESCALER_DIV16_Val << RTC_MODE0_CTRLA_PRESCALER_Pos) /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/16 Position  */
#define RTC_MODE0_CTRLA_PRESCALER_DIV32       (RTC_MODE0_CTRLA_PRESCALER_DIV32_Val << RTC_MODE0_CTRLA_PRESCALER_Pos) /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/32 Position  */
#define RTC_MODE0_CTRLA_PRESCALER_DIV64       (RTC_MODE0_CTRLA_PRESCALER_DIV64_Val << RTC_MODE0_CTRLA_PRESCALER_Pos) /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/64 Position  */
#define RTC_MODE0_CTRLA_PRESCALER_DIV128      (RTC_MODE0_CTRLA_PRESCALER_DIV128_Val << RTC_MODE0_CTRLA_PRESCALER_Pos) /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/128 Position  */
#define RTC_MODE0_CTRLA_PRESCALER_DIV256      (RTC_MODE0_CTRLA_PRESCALER_DIV256_Val << RTC_MODE0_CTRLA_PRESCALER_Pos) /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/256 Position  */
#define RTC_MODE0_CTRLA_PRESCALER_DIV512      (RTC_MODE0_CTRLA_PRESCALER_DIV512_Val << RTC_MODE0_CTRLA_PRESCALER_Pos) /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/512 Position  */
#define RTC_MODE0_CTRLA_PRESCALER_DIV1024     (RTC_MODE0_CTRLA_PRESCALER_DIV1024_Val << RTC_MODE0_CTRLA_PRESCALER_Pos) /* (RTC_MODE0_CTRLA) CLK_RTC_CNT = GCLK_RTC/1024 Position  */
#define RTC_MODE0_CTRLA_COUNTSYNC_Pos         _UINT16_(15)                                         /* (RTC_MODE0_CTRLA) Count Read Synchronization Enable Position */
#define RTC_MODE0_CTRLA_COUNTSYNC_Msk         (_UINT16_(0x1) << RTC_MODE0_CTRLA_COUNTSYNC_Pos)     /* (RTC_MODE0_CTRLA) Count Read Synchronization Enable Mask */
#define RTC_MODE0_CTRLA_COUNTSYNC(value)      (RTC_MODE0_CTRLA_COUNTSYNC_Msk & (_UINT16_(value) << RTC_MODE0_CTRLA_COUNTSYNC_Pos)) /* Assigment of value for COUNTSYNC in the RTC_MODE0_CTRLA register */
#define RTC_MODE0_CTRLA_Msk                   _UINT16_(0x8F8F)                                     /* (RTC_MODE0_CTRLA) Register Mask  */


/* -------- RTC_MODE1_CTRLA : (RTC Offset: 0x00) (R/W 16) MODE1 Control A -------- */
#define RTC_MODE1_CTRLA_RESETVALUE            _UINT16_(0x00)                                       /*  (RTC_MODE1_CTRLA) MODE1 Control A  Reset Value */

#define RTC_MODE1_CTRLA_SWRST_Pos             _UINT16_(0)                                          /* (RTC_MODE1_CTRLA) Software Reset Position */
#define RTC_MODE1_CTRLA_SWRST_Msk             (_UINT16_(0x1) << RTC_MODE1_CTRLA_SWRST_Pos)         /* (RTC_MODE1_CTRLA) Software Reset Mask */
#define RTC_MODE1_CTRLA_SWRST(value)          (RTC_MODE1_CTRLA_SWRST_Msk & (_UINT16_(value) << RTC_MODE1_CTRLA_SWRST_Pos)) /* Assigment of value for SWRST in the RTC_MODE1_CTRLA register */
#define RTC_MODE1_CTRLA_ENABLE_Pos            _UINT16_(1)                                          /* (RTC_MODE1_CTRLA) Enable Position */
#define RTC_MODE1_CTRLA_ENABLE_Msk            (_UINT16_(0x1) << RTC_MODE1_CTRLA_ENABLE_Pos)        /* (RTC_MODE1_CTRLA) Enable Mask */
#define RTC_MODE1_CTRLA_ENABLE(value)         (RTC_MODE1_CTRLA_ENABLE_Msk & (_UINT16_(value) << RTC_MODE1_CTRLA_ENABLE_Pos)) /* Assigment of value for ENABLE in the RTC_MODE1_CTRLA register */
#define RTC_MODE1_CTRLA_MODE_Pos              _UINT16_(2)                                          /* (RTC_MODE1_CTRLA) Operating Mode Position */
#define RTC_MODE1_CTRLA_MODE_Msk              (_UINT16_(0x3) << RTC_MODE1_CTRLA_MODE_Pos)          /* (RTC_MODE1_CTRLA) Operating Mode Mask */
#define RTC_MODE1_CTRLA_MODE(value)           (RTC_MODE1_CTRLA_MODE_Msk & (_UINT16_(value) << RTC_MODE1_CTRLA_MODE_Pos)) /* Assigment of value for MODE in the RTC_MODE1_CTRLA register */
#define   RTC_MODE1_CTRLA_MODE_COUNT32_Val    _UINT16_(0x0)                                        /* (RTC_MODE1_CTRLA) Mode 0: 32-bit Counter  */
#define   RTC_MODE1_CTRLA_MODE_COUNT16_Val    _UINT16_(0x1)                                        /* (RTC_MODE1_CTRLA) Mode 1: 16-bit Counter  */
#define   RTC_MODE1_CTRLA_MODE_CLOCK_Val      _UINT16_(0x2)                                        /* (RTC_MODE1_CTRLA) Mode 2: Clock/Calendar  */
#define RTC_MODE1_CTRLA_MODE_COUNT32          (RTC_MODE1_CTRLA_MODE_COUNT32_Val << RTC_MODE1_CTRLA_MODE_Pos) /* (RTC_MODE1_CTRLA) Mode 0: 32-bit Counter Position  */
#define RTC_MODE1_CTRLA_MODE_COUNT16          (RTC_MODE1_CTRLA_MODE_COUNT16_Val << RTC_MODE1_CTRLA_MODE_Pos) /* (RTC_MODE1_CTRLA) Mode 1: 16-bit Counter Position  */
#define RTC_MODE1_CTRLA_MODE_CLOCK            (RTC_MODE1_CTRLA_MODE_CLOCK_Val << RTC_MODE1_CTRLA_MODE_Pos) /* (RTC_MODE1_CTRLA) Mode 2: Clock/Calendar Position  */
#define RTC_MODE1_CTRLA_PRESCALER_Pos         _UINT16_(8)                                          /* (RTC_MODE1_CTRLA) Prescaler Position */
#define RTC_MODE1_CTRLA_PRESCALER_Msk         (_UINT16_(0xF) << RTC_MODE1_CTRLA_PRESCALER_Pos)     /* (RTC_MODE1_CTRLA) Prescaler Mask */
#define RTC_MODE1_CTRLA_PRESCALER(value)      (RTC_MODE1_CTRLA_PRESCALER_Msk & (_UINT16_(value) << RTC_MODE1_CTRLA_PRESCALER_Pos)) /* Assigment of value for PRESCALER in the RTC_MODE1_CTRLA register */
#define   RTC_MODE1_CTRLA_PRESCALER_OFF_Val   _UINT16_(0x0)                                        /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/1  */
#define   RTC_MODE1_CTRLA_PRESCALER_DIV1_Val  _UINT16_(0x1)                                        /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/1  */
#define   RTC_MODE1_CTRLA_PRESCALER_DIV2_Val  _UINT16_(0x2)                                        /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/2  */
#define   RTC_MODE1_CTRLA_PRESCALER_DIV4_Val  _UINT16_(0x3)                                        /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/4  */
#define   RTC_MODE1_CTRLA_PRESCALER_DIV8_Val  _UINT16_(0x4)                                        /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/8  */
#define   RTC_MODE1_CTRLA_PRESCALER_DIV16_Val _UINT16_(0x5)                                        /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/16  */
#define   RTC_MODE1_CTRLA_PRESCALER_DIV32_Val _UINT16_(0x6)                                        /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/32  */
#define   RTC_MODE1_CTRLA_PRESCALER_DIV64_Val _UINT16_(0x7)                                        /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/64  */
#define   RTC_MODE1_CTRLA_PRESCALER_DIV128_Val _UINT16_(0x8)                                        /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/128  */
#define   RTC_MODE1_CTRLA_PRESCALER_DIV256_Val _UINT16_(0x9)                                        /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/256  */
#define   RTC_MODE1_CTRLA_PRESCALER_DIV512_Val _UINT16_(0xA)                                        /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/512  */
#define   RTC_MODE1_CTRLA_PRESCALER_DIV1024_Val _UINT16_(0xB)                                        /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/1024  */
#define RTC_MODE1_CTRLA_PRESCALER_OFF         (RTC_MODE1_CTRLA_PRESCALER_OFF_Val << RTC_MODE1_CTRLA_PRESCALER_Pos) /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/1 Position  */
#define RTC_MODE1_CTRLA_PRESCALER_DIV1        (RTC_MODE1_CTRLA_PRESCALER_DIV1_Val << RTC_MODE1_CTRLA_PRESCALER_Pos) /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/1 Position  */
#define RTC_MODE1_CTRLA_PRESCALER_DIV2        (RTC_MODE1_CTRLA_PRESCALER_DIV2_Val << RTC_MODE1_CTRLA_PRESCALER_Pos) /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/2 Position  */
#define RTC_MODE1_CTRLA_PRESCALER_DIV4        (RTC_MODE1_CTRLA_PRESCALER_DIV4_Val << RTC_MODE1_CTRLA_PRESCALER_Pos) /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/4 Position  */
#define RTC_MODE1_CTRLA_PRESCALER_DIV8        (RTC_MODE1_CTRLA_PRESCALER_DIV8_Val << RTC_MODE1_CTRLA_PRESCALER_Pos) /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/8 Position  */
#define RTC_MODE1_CTRLA_PRESCALER_DIV16       (RTC_MODE1_CTRLA_PRESCALER_DIV16_Val << RTC_MODE1_CTRLA_PRESCALER_Pos) /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/16 Position  */
#define RTC_MODE1_CTRLA_PRESCALER_DIV32       (RTC_MODE1_CTRLA_PRESCALER_DIV32_Val << RTC_MODE1_CTRLA_PRESCALER_Pos) /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/32 Position  */
#define RTC_MODE1_CTRLA_PRESCALER_DIV64       (RTC_MODE1_CTRLA_PRESCALER_DIV64_Val << RTC_MODE1_CTRLA_PRESCALER_Pos) /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/64 Position  */
#define RTC_MODE1_CTRLA_PRESCALER_DIV128      (RTC_MODE1_CTRLA_PRESCALER_DIV128_Val << RTC_MODE1_CTRLA_PRESCALER_Pos) /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/128 Position  */
#define RTC_MODE1_CTRLA_PRESCALER_DIV256      (RTC_MODE1_CTRLA_PRESCALER_DIV256_Val << RTC_MODE1_CTRLA_PRESCALER_Pos) /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/256 Position  */
#define RTC_MODE1_CTRLA_PRESCALER_DIV512      (RTC_MODE1_CTRLA_PRESCALER_DIV512_Val << RTC_MODE1_CTRLA_PRESCALER_Pos) /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/512 Position  */
#define RTC_MODE1_CTRLA_PRESCALER_DIV1024     (RTC_MODE1_CTRLA_PRESCALER_DIV1024_Val << RTC_MODE1_CTRLA_PRESCALER_Pos) /* (RTC_MODE1_CTRLA) CLK_RTC_CNT = GCLK_RTC/1024 Position  */
#define RTC_MODE1_CTRLA_COUNTSYNC_Pos         _UINT16_(15)                                         /* (RTC_MODE1_CTRLA) Count Read Synchronization Enable Position */
#define RTC_MODE1_CTRLA_COUNTSYNC_Msk         (_UINT16_(0x1) << RTC_MODE1_CTRLA_COUNTSYNC_Pos)     /* (RTC_MODE1_CTRLA) Count Read Synchronization Enable Mask */
#define RTC_MODE1_CTRLA_COUNTSYNC(value)      (RTC_MODE1_CTRLA_COUNTSYNC_Msk & (_UINT16_(value) << RTC_MODE1_CTRLA_COUNTSYNC_Pos)) /* Assigment of value for COUNTSYNC in the RTC_MODE1_CTRLA register */
#define RTC_MODE1_CTRLA_Msk                   _UINT16_(0x8F0F)                                     /* (RTC_MODE1_CTRLA) Register Mask  */


/* -------- RTC_MODE2_CTRLA : (RTC Offset: 0x00) (R/W 16) MODE2 Control A -------- */
#define RTC_MODE2_CTRLA_RESETVALUE            _UINT16_(0x00)                                       /*  (RTC_MODE2_CTRLA) MODE2 Control A  Reset Value */

#define RTC_MODE2_CTRLA_SWRST_Pos             _UINT16_(0)                                          /* (RTC_MODE2_CTRLA) Software Reset Position */
#define RTC_MODE2_CTRLA_SWRST_Msk             (_UINT16_(0x1) << RTC_MODE2_CTRLA_SWRST_Pos)         /* (RTC_MODE2_CTRLA) Software Reset Mask */
#define RTC_MODE2_CTRLA_SWRST(value)          (RTC_MODE2_CTRLA_SWRST_Msk & (_UINT16_(value) << RTC_MODE2_CTRLA_SWRST_Pos)) /* Assigment of value for SWRST in the RTC_MODE2_CTRLA register */
#define RTC_MODE2_CTRLA_ENABLE_Pos            _UINT16_(1)                                          /* (RTC_MODE2_CTRLA) Enable Position */
#define RTC_MODE2_CTRLA_ENABLE_Msk            (_UINT16_(0x1) << RTC_MODE2_CTRLA_ENABLE_Pos)        /* (RTC_MODE2_CTRLA) Enable Mask */
#define RTC_MODE2_CTRLA_ENABLE(value)         (RTC_MODE2_CTRLA_ENABLE_Msk & (_UINT16_(value) << RTC_MODE2_CTRLA_ENABLE_Pos)) /* Assigment of value for ENABLE in the RTC_MODE2_CTRLA register */
#define RTC_MODE2_CTRLA_MODE_Pos              _UINT16_(2)                                          /* (RTC_MODE2_CTRLA) Operating Mode Position */
#define RTC_MODE2_CTRLA_MODE_Msk              (_UINT16_(0x3) << RTC_MODE2_CTRLA_MODE_Pos)          /* (RTC_MODE2_CTRLA) Operating Mode Mask */
#define RTC_MODE2_CTRLA_MODE(value)           (RTC_MODE2_CTRLA_MODE_Msk & (_UINT16_(value) << RTC_MODE2_CTRLA_MODE_Pos)) /* Assigment of value for MODE in the RTC_MODE2_CTRLA register */
#define   RTC_MODE2_CTRLA_MODE_COUNT32_Val    _UINT16_(0x0)                                        /* (RTC_MODE2_CTRLA) Mode 0: 32-bit Counter  */
#define   RTC_MODE2_CTRLA_MODE_COUNT16_Val    _UINT16_(0x1)                                        /* (RTC_MODE2_CTRLA) Mode 1: 16-bit Counter  */
#define   RTC_MODE2_CTRLA_MODE_CLOCK_Val      _UINT16_(0x2)                                        /* (RTC_MODE2_CTRLA) Mode 2: Clock/Calendar  */
#define RTC_MODE2_CTRLA_MODE_COUNT32          (RTC_MODE2_CTRLA_MODE_COUNT32_Val << RTC_MODE2_CTRLA_MODE_Pos) /* (RTC_MODE2_CTRLA) Mode 0: 32-bit Counter Position  */
#define RTC_MODE2_CTRLA_MODE_COUNT16          (RTC_MODE2_CTRLA_MODE_COUNT16_Val << RTC_MODE2_CTRLA_MODE_Pos) /* (RTC_MODE2_CTRLA) Mode 1: 16-bit Counter Position  */
#define RTC_MODE2_CTRLA_MODE_CLOCK            (RTC_MODE2_CTRLA_MODE_CLOCK_Val << RTC_MODE2_CTRLA_MODE_Pos) /* (RTC_MODE2_CTRLA) Mode 2: Clock/Calendar Position  */
#define RTC_MODE2_CTRLA_CLKREP_Pos            _UINT16_(6)                                          /* (RTC_MODE2_CTRLA) Clock Representation Position */
#define RTC_MODE2_CTRLA_CLKREP_Msk            (_UINT16_(0x1) << RTC_MODE2_CTRLA_CLKREP_Pos)        /* (RTC_MODE2_CTRLA) Clock Representation Mask */
#define RTC_MODE2_CTRLA_CLKREP(value)         (RTC_MODE2_CTRLA_CLKREP_Msk & (_UINT16_(value) << RTC_MODE2_CTRLA_CLKREP_Pos)) /* Assigment of value for CLKREP in the RTC_MODE2_CTRLA register */
#define RTC_MODE2_CTRLA_MATCHCLR_Pos          _UINT16_(7)                                          /* (RTC_MODE2_CTRLA) Clear on Match Position */
#define RTC_MODE2_CTRLA_MATCHCLR_Msk          (_UINT16_(0x1) << RTC_MODE2_CTRLA_MATCHCLR_Pos)      /* (RTC_MODE2_CTRLA) Clear on Match Mask */
#define RTC_MODE2_CTRLA_MATCHCLR(value)       (RTC_MODE2_CTRLA_MATCHCLR_Msk & (_UINT16_(value) << RTC_MODE2_CTRLA_MATCHCLR_Pos)) /* Assigment of value for MATCHCLR in the RTC_MODE2_CTRLA register */
#define RTC_MODE2_CTRLA_PRESCALER_Pos         _UINT16_(8)                                          /* (RTC_MODE2_CTRLA) Prescaler Position */
#define RTC_MODE2_CTRLA_PRESCALER_Msk         (_UINT16_(0xF) << RTC_MODE2_CTRLA_PRESCALER_Pos)     /* (RTC_MODE2_CTRLA) Prescaler Mask */
#define RTC_MODE2_CTRLA_PRESCALER(value)      (RTC_MODE2_CTRLA_PRESCALER_Msk & (_UINT16_(value) << RTC_MODE2_CTRLA_PRESCALER_Pos)) /* Assigment of value for PRESCALER in the RTC_MODE2_CTRLA register */
#define   RTC_MODE2_CTRLA_PRESCALER_OFF_Val   _UINT16_(0x0)                                        /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/1  */
#define   RTC_MODE2_CTRLA_PRESCALER_DIV1_Val  _UINT16_(0x1)                                        /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/1  */
#define   RTC_MODE2_CTRLA_PRESCALER_DIV2_Val  _UINT16_(0x2)                                        /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/2  */
#define   RTC_MODE2_CTRLA_PRESCALER_DIV4_Val  _UINT16_(0x3)                                        /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/4  */
#define   RTC_MODE2_CTRLA_PRESCALER_DIV8_Val  _UINT16_(0x4)                                        /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/8  */
#define   RTC_MODE2_CTRLA_PRESCALER_DIV16_Val _UINT16_(0x5)                                        /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/16  */
#define   RTC_MODE2_CTRLA_PRESCALER_DIV32_Val _UINT16_(0x6)                                        /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/32  */
#define   RTC_MODE2_CTRLA_PRESCALER_DIV64_Val _UINT16_(0x7)                                        /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/64  */
#define   RTC_MODE2_CTRLA_PRESCALER_DIV128_Val _UINT16_(0x8)                                        /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/128  */
#define   RTC_MODE2_CTRLA_PRESCALER_DIV256_Val _UINT16_(0x9)                                        /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/256  */
#define   RTC_MODE2_CTRLA_PRESCALER_DIV512_Val _UINT16_(0xA)                                        /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/512  */
#define   RTC_MODE2_CTRLA_PRESCALER_DIV1024_Val _UINT16_(0xB)                                        /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/1024  */
#define RTC_MODE2_CTRLA_PRESCALER_OFF         (RTC_MODE2_CTRLA_PRESCALER_OFF_Val << RTC_MODE2_CTRLA_PRESCALER_Pos) /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/1 Position  */
#define RTC_MODE2_CTRLA_PRESCALER_DIV1        (RTC_MODE2_CTRLA_PRESCALER_DIV1_Val << RTC_MODE2_CTRLA_PRESCALER_Pos) /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/1 Position  */
#define RTC_MODE2_CTRLA_PRESCALER_DIV2        (RTC_MODE2_CTRLA_PRESCALER_DIV2_Val << RTC_MODE2_CTRLA_PRESCALER_Pos) /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/2 Position  */
#define RTC_MODE2_CTRLA_PRESCALER_DIV4        (RTC_MODE2_CTRLA_PRESCALER_DIV4_Val << RTC_MODE2_CTRLA_PRESCALER_Pos) /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/4 Position  */
#define RTC_MODE2_CTRLA_PRESCALER_DIV8        (RTC_MODE2_CTRLA_PRESCALER_DIV8_Val << RTC_MODE2_CTRLA_PRESCALER_Pos) /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/8 Position  */
#define RTC_MODE2_CTRLA_PRESCALER_DIV16       (RTC_MODE2_CTRLA_PRESCALER_DIV16_Val << RTC_MODE2_CTRLA_PRESCALER_Pos) /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/16 Position  */
#define RTC_MODE2_CTRLA_PRESCALER_DIV32       (RTC_MODE2_CTRLA_PRESCALER_DIV32_Val << RTC_MODE2_CTRLA_PRESCALER_Pos) /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/32 Position  */
#define RTC_MODE2_CTRLA_PRESCALER_DIV64       (RTC_MODE2_CTRLA_PRESCALER_DIV64_Val << RTC_MODE2_CTRLA_PRESCALER_Pos) /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/64 Position  */
#define RTC_MODE2_CTRLA_PRESCALER_DIV128      (RTC_MODE2_CTRLA_PRESCALER_DIV128_Val << RTC_MODE2_CTRLA_PRESCALER_Pos) /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/128 Position  */
#define RTC_MODE2_CTRLA_PRESCALER_DIV256      (RTC_MODE2_CTRLA_PRESCALER_DIV256_Val << RTC_MODE2_CTRLA_PRESCALER_Pos) /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/256 Position  */
#define RTC_MODE2_CTRLA_PRESCALER_DIV512      (RTC_MODE2_CTRLA_PRESCALER_DIV512_Val << RTC_MODE2_CTRLA_PRESCALER_Pos) /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/512 Position  */
#define RTC_MODE2_CTRLA_PRESCALER_DIV1024     (RTC_MODE2_CTRLA_PRESCALER_DIV1024_Val << RTC_MODE2_CTRLA_PRESCALER_Pos) /* (RTC_MODE2_CTRLA) CLK_RTC_CNT = GCLK_RTC/1024 Position  */
#define RTC_MODE2_CTRLA_CLOCKSYNC_Pos         _UINT16_(15)                                         /* (RTC_MODE2_CTRLA) Clock Read Synchronization Enable Position */
#define RTC_MODE2_CTRLA_CLOCKSYNC_Msk         (_UINT16_(0x1) << RTC_MODE2_CTRLA_CLOCKSYNC_Pos)     /* (RTC_MODE2_CTRLA) Clock Read Synchronization Enable Mask */
#define RTC_MODE2_CTRLA_CLOCKSYNC(value)      (RTC_MODE2_CTRLA_CLOCKSYNC_Msk & (_UINT16_(value) << RTC_MODE2_CTRLA_CLOCKSYNC_Pos)) /* Assigment of value for CLOCKSYNC in the RTC_MODE2_CTRLA register */
#define RTC_MODE2_CTRLA_Msk                   _UINT16_(0x8FCF)                                     /* (RTC_MODE2_CTRLA) Register Mask  */


/* -------- RTC_MODE0_EVCTRL : (RTC Offset: 0x04) (R/W 32) MODE0 Event Control -------- */
#define RTC_MODE0_EVCTRL_RESETVALUE           _UINT32_(0x00)                                       /*  (RTC_MODE0_EVCTRL) MODE0 Event Control  Reset Value */

#define RTC_MODE0_EVCTRL_PEREO0_Pos           _UINT32_(0)                                          /* (RTC_MODE0_EVCTRL) Periodic Interval 0 Event Output Enable Position */
#define RTC_MODE0_EVCTRL_PEREO0_Msk           (_UINT32_(0x1) << RTC_MODE0_EVCTRL_PEREO0_Pos)       /* (RTC_MODE0_EVCTRL) Periodic Interval 0 Event Output Enable Mask */
#define RTC_MODE0_EVCTRL_PEREO0(value)        (RTC_MODE0_EVCTRL_PEREO0_Msk & (_UINT32_(value) << RTC_MODE0_EVCTRL_PEREO0_Pos)) /* Assigment of value for PEREO0 in the RTC_MODE0_EVCTRL register */
#define RTC_MODE0_EVCTRL_PEREO1_Pos           _UINT32_(1)                                          /* (RTC_MODE0_EVCTRL) Periodic Interval 1 Event Output Enable Position */
#define RTC_MODE0_EVCTRL_PEREO1_Msk           (_UINT32_(0x1) << RTC_MODE0_EVCTRL_PEREO1_Pos)       /* (RTC_MODE0_EVCTRL) Periodic Interval 1 Event Output Enable Mask */
#define RTC_MODE0_EVCTRL_PEREO1(value)        (RTC_MODE0_EVCTRL_PEREO1_Msk & (_UINT32_(value) << RTC_MODE0_EVCTRL_PEREO1_Pos)) /* Assigment of value for PEREO1 in the RTC_MODE0_EVCTRL register */
#define RTC_MODE0_EVCTRL_PEREO2_Pos           _UINT32_(2)                                          /* (RTC_MODE0_EVCTRL) Periodic Interval 2 Event Output Enable Position */
#define RTC_MODE0_EVCTRL_PEREO2_Msk           (_UINT32_(0x1) << RTC_MODE0_EVCTRL_PEREO2_Pos)       /* (RTC_MODE0_EVCTRL) Periodic Interval 2 Event Output Enable Mask */
#define RTC_MODE0_EVCTRL_PEREO2(value)        (RTC_MODE0_EVCTRL_PEREO2_Msk & (_UINT32_(value) << RTC_MODE0_EVCTRL_PEREO2_Pos)) /* Assigment of value for PEREO2 in the RTC_MODE0_EVCTRL register */
#define RTC_MODE0_EVCTRL_PEREO3_Pos           _UINT32_(3)                                          /* (RTC_MODE0_EVCTRL) Periodic Interval 3 Event Output Enable Position */
#define RTC_MODE0_EVCTRL_PEREO3_Msk           (_UINT32_(0x1) << RTC_MODE0_EVCTRL_PEREO3_Pos)       /* (RTC_MODE0_EVCTRL) Periodic Interval 3 Event Output Enable Mask */
#define RTC_MODE0_EVCTRL_PEREO3(value)        (RTC_MODE0_EVCTRL_PEREO3_Msk & (_UINT32_(value) << RTC_MODE0_EVCTRL_PEREO3_Pos)) /* Assigment of value for PEREO3 in the RTC_MODE0_EVCTRL register */
#define RTC_MODE0_EVCTRL_PEREO4_Pos           _UINT32_(4)                                          /* (RTC_MODE0_EVCTRL) Periodic Interval 4 Event Output Enable Position */
#define RTC_MODE0_EVCTRL_PEREO4_Msk           (_UINT32_(0x1) << RTC_MODE0_EVCTRL_PEREO4_Pos)       /* (RTC_MODE0_EVCTRL) Periodic Interval 4 Event Output Enable Mask */
#define RTC_MODE0_EVCTRL_PEREO4(value)        (RTC_MODE0_EVCTRL_PEREO4_Msk & (_UINT32_(value) << RTC_MODE0_EVCTRL_PEREO4_Pos)) /* Assigment of value for PEREO4 in the RTC_MODE0_EVCTRL register */
#define RTC_MODE0_EVCTRL_PEREO5_Pos           _UINT32_(5)                                          /* (RTC_MODE0_EVCTRL) Periodic Interval 5 Event Output Enable Position */
#define RTC_MODE0_EVCTRL_PEREO5_Msk           (_UINT32_(0x1) << RTC_MODE0_EVCTRL_PEREO5_Pos)       /* (RTC_MODE0_EVCTRL) Periodic Interval 5 Event Output Enable Mask */
#define RTC_MODE0_EVCTRL_PEREO5(value)        (RTC_MODE0_EVCTRL_PEREO5_Msk & (_UINT32_(value) << RTC_MODE0_EVCTRL_PEREO5_Pos)) /* Assigment of value for PEREO5 in the RTC_MODE0_EVCTRL register */
#define RTC_MODE0_EVCTRL_PEREO6_Pos           _UINT32_(6)                                          /* (RTC_MODE0_EVCTRL) Periodic Interval 6 Event Output Enable Position */
#define RTC_MODE0_EVCTRL_PEREO6_Msk           (_UINT32_(0x1) << RTC_MODE0_EVCTRL_PEREO6_Pos)       /* (RTC_MODE0_EVCTRL) Periodic Interval 6 Event Output Enable Mask */
#define RTC_MODE0_EVCTRL_PEREO6(value)        (RTC_MODE0_EVCTRL_PEREO6_Msk & (_UINT32_(value) << RTC_MODE0_EVCTRL_PEREO6_Pos)) /* Assigment of value for PEREO6 in the RTC_MODE0_EVCTRL register */
#define RTC_MODE0_EVCTRL_PEREO7_Pos           _UINT32_(7)                                          /* (RTC_MODE0_EVCTRL) Periodic Interval 7 Event Output Enable Position */
#define RTC_MODE0_EVCTRL_PEREO7_Msk           (_UINT32_(0x1) << RTC_MODE0_EVCTRL_PEREO7_Pos)       /* (RTC_MODE0_EVCTRL) Periodic Interval 7 Event Output Enable Mask */
#define RTC_MODE0_EVCTRL_PEREO7(value)        (RTC_MODE0_EVCTRL_PEREO7_Msk & (_UINT32_(value) << RTC_MODE0_EVCTRL_PEREO7_Pos)) /* Assigment of value for PEREO7 in the RTC_MODE0_EVCTRL register */
#define RTC_MODE0_EVCTRL_CMPEO0_Pos           _UINT32_(8)                                          /* (RTC_MODE0_EVCTRL) Compare 0 Event Output Enable Position */
#define RTC_MODE0_EVCTRL_CMPEO0_Msk           (_UINT32_(0x1) << RTC_MODE0_EVCTRL_CMPEO0_Pos)       /* (RTC_MODE0_EVCTRL) Compare 0 Event Output Enable Mask */
#define RTC_MODE0_EVCTRL_CMPEO0(value)        (RTC_MODE0_EVCTRL_CMPEO0_Msk & (_UINT32_(value) << RTC_MODE0_EVCTRL_CMPEO0_Pos)) /* Assigment of value for CMPEO0 in the RTC_MODE0_EVCTRL register */
#define RTC_MODE0_EVCTRL_OVFEO_Pos            _UINT32_(15)                                         /* (RTC_MODE0_EVCTRL) Overflow Event Output Enable Position */
#define RTC_MODE0_EVCTRL_OVFEO_Msk            (_UINT32_(0x1) << RTC_MODE0_EVCTRL_OVFEO_Pos)        /* (RTC_MODE0_EVCTRL) Overflow Event Output Enable Mask */
#define RTC_MODE0_EVCTRL_OVFEO(value)         (RTC_MODE0_EVCTRL_OVFEO_Msk & (_UINT32_(value) << RTC_MODE0_EVCTRL_OVFEO_Pos)) /* Assigment of value for OVFEO in the RTC_MODE0_EVCTRL register */
#define RTC_MODE0_EVCTRL_Msk                  _UINT32_(0x000081FF)                                 /* (RTC_MODE0_EVCTRL) Register Mask  */

#define RTC_MODE0_EVCTRL_PEREO_Pos            _UINT32_(0)                                          /* (RTC_MODE0_EVCTRL Position) Periodic Interval x Event Output Enable */
#define RTC_MODE0_EVCTRL_PEREO_Msk            (_UINT32_(0xFF) << RTC_MODE0_EVCTRL_PEREO_Pos)       /* (RTC_MODE0_EVCTRL Mask) PEREO */
#define RTC_MODE0_EVCTRL_PEREO(value)         (RTC_MODE0_EVCTRL_PEREO_Msk & (_UINT32_(value) << RTC_MODE0_EVCTRL_PEREO_Pos)) 
#define RTC_MODE0_EVCTRL_CMPEO_Pos            _UINT32_(8)                                          /* (RTC_MODE0_EVCTRL Position) Compare x Event Output Enable */
#define RTC_MODE0_EVCTRL_CMPEO_Msk            (_UINT32_(0x1) << RTC_MODE0_EVCTRL_CMPEO_Pos)        /* (RTC_MODE0_EVCTRL Mask) CMPEO */
#define RTC_MODE0_EVCTRL_CMPEO(value)         (RTC_MODE0_EVCTRL_CMPEO_Msk & (_UINT32_(value) << RTC_MODE0_EVCTRL_CMPEO_Pos)) 

/* -------- RTC_MODE1_EVCTRL : (RTC Offset: 0x04) (R/W 32) MODE1 Event Control -------- */
#define RTC_MODE1_EVCTRL_RESETVALUE           _UINT32_(0x00)                                       /*  (RTC_MODE1_EVCTRL) MODE1 Event Control  Reset Value */

#define RTC_MODE1_EVCTRL_PEREO0_Pos           _UINT32_(0)                                          /* (RTC_MODE1_EVCTRL) Periodic Interval 0 Event Output Enable Position */
#define RTC_MODE1_EVCTRL_PEREO0_Msk           (_UINT32_(0x1) << RTC_MODE1_EVCTRL_PEREO0_Pos)       /* (RTC_MODE1_EVCTRL) Periodic Interval 0 Event Output Enable Mask */
#define RTC_MODE1_EVCTRL_PEREO0(value)        (RTC_MODE1_EVCTRL_PEREO0_Msk & (_UINT32_(value) << RTC_MODE1_EVCTRL_PEREO0_Pos)) /* Assigment of value for PEREO0 in the RTC_MODE1_EVCTRL register */
#define RTC_MODE1_EVCTRL_PEREO1_Pos           _UINT32_(1)                                          /* (RTC_MODE1_EVCTRL) Periodic Interval 1 Event Output Enable Position */
#define RTC_MODE1_EVCTRL_PEREO1_Msk           (_UINT32_(0x1) << RTC_MODE1_EVCTRL_PEREO1_Pos)       /* (RTC_MODE1_EVCTRL) Periodic Interval 1 Event Output Enable Mask */
#define RTC_MODE1_EVCTRL_PEREO1(value)        (RTC_MODE1_EVCTRL_PEREO1_Msk & (_UINT32_(value) << RTC_MODE1_EVCTRL_PEREO1_Pos)) /* Assigment of value for PEREO1 in the RTC_MODE1_EVCTRL register */
#define RTC_MODE1_EVCTRL_PEREO2_Pos           _UINT32_(2)                                          /* (RTC_MODE1_EVCTRL) Periodic Interval 2 Event Output Enable Position */
#define RTC_MODE1_EVCTRL_PEREO2_Msk           (_UINT32_(0x1) << RTC_MODE1_EVCTRL_PEREO2_Pos)       /* (RTC_MODE1_EVCTRL) Periodic Interval 2 Event Output Enable Mask */
#define RTC_MODE1_EVCTRL_PEREO2(value)        (RTC_MODE1_EVCTRL_PEREO2_Msk & (_UINT32_(value) << RTC_MODE1_EVCTRL_PEREO2_Pos)) /* Assigment of value for PEREO2 in the RTC_MODE1_EVCTRL register */
#define RTC_MODE1_EVCTRL_PEREO3_Pos           _UINT32_(3)                                          /* (RTC_MODE1_EVCTRL) Periodic Interval 3 Event Output Enable Position */
#define RTC_MODE1_EVCTRL_PEREO3_Msk           (_UINT32_(0x1) << RTC_MODE1_EVCTRL_PEREO3_Pos)       /* (RTC_MODE1_EVCTRL) Periodic Interval 3 Event Output Enable Mask */
#define RTC_MODE1_EVCTRL_PEREO3(value)        (RTC_MODE1_EVCTRL_PEREO3_Msk & (_UINT32_(value) << RTC_MODE1_EVCTRL_PEREO3_Pos)) /* Assigment of value for PEREO3 in the RTC_MODE1_EVCTRL register */
#define RTC_MODE1_EVCTRL_PEREO4_Pos           _UINT32_(4)                                          /* (RTC_MODE1_EVCTRL) Periodic Interval 4 Event Output Enable Position */
#define RTC_MODE1_EVCTRL_PEREO4_Msk           (_UINT32_(0x1) << RTC_MODE1_EVCTRL_PEREO4_Pos)       /* (RTC_MODE1_EVCTRL) Periodic Interval 4 Event Output Enable Mask */
#define RTC_MODE1_EVCTRL_PEREO4(value)        (RTC_MODE1_EVCTRL_PEREO4_Msk & (_UINT32_(value) << RTC_MODE1_EVCTRL_PEREO4_Pos)) /* Assigment of value for PEREO4 in the RTC_MODE1_EVCTRL register */
#define RTC_MODE1_EVCTRL_PEREO5_Pos           _UINT32_(5)                                          /* (RTC_MODE1_EVCTRL) Periodic Interval 5 Event Output Enable Position */
#define RTC_MODE1_EVCTRL_PEREO5_Msk           (_UINT32_(0x1) << RTC_MODE1_EVCTRL_PEREO5_Pos)       /* (RTC_MODE1_EVCTRL) Periodic Interval 5 Event Output Enable Mask */
#define RTC_MODE1_EVCTRL_PEREO5(value)        (RTC_MODE1_EVCTRL_PEREO5_Msk & (_UINT32_(value) << RTC_MODE1_EVCTRL_PEREO5_Pos)) /* Assigment of value for PEREO5 in the RTC_MODE1_EVCTRL register */
#define RTC_MODE1_EVCTRL_PEREO6_Pos           _UINT32_(6)                                          /* (RTC_MODE1_EVCTRL) Periodic Interval 6 Event Output Enable Position */
#define RTC_MODE1_EVCTRL_PEREO6_Msk           (_UINT32_(0x1) << RTC_MODE1_EVCTRL_PEREO6_Pos)       /* (RTC_MODE1_EVCTRL) Periodic Interval 6 Event Output Enable Mask */
#define RTC_MODE1_EVCTRL_PEREO6(value)        (RTC_MODE1_EVCTRL_PEREO6_Msk & (_UINT32_(value) << RTC_MODE1_EVCTRL_PEREO6_Pos)) /* Assigment of value for PEREO6 in the RTC_MODE1_EVCTRL register */
#define RTC_MODE1_EVCTRL_PEREO7_Pos           _UINT32_(7)                                          /* (RTC_MODE1_EVCTRL) Periodic Interval 7 Event Output Enable Position */
#define RTC_MODE1_EVCTRL_PEREO7_Msk           (_UINT32_(0x1) << RTC_MODE1_EVCTRL_PEREO7_Pos)       /* (RTC_MODE1_EVCTRL) Periodic Interval 7 Event Output Enable Mask */
#define RTC_MODE1_EVCTRL_PEREO7(value)        (RTC_MODE1_EVCTRL_PEREO7_Msk & (_UINT32_(value) << RTC_MODE1_EVCTRL_PEREO7_Pos)) /* Assigment of value for PEREO7 in the RTC_MODE1_EVCTRL register */
#define RTC_MODE1_EVCTRL_CMPEO0_Pos           _UINT32_(8)                                          /* (RTC_MODE1_EVCTRL) Compare 0 Event Output Enable Position */
#define RTC_MODE1_EVCTRL_CMPEO0_Msk           (_UINT32_(0x1) << RTC_MODE1_EVCTRL_CMPEO0_Pos)       /* (RTC_MODE1_EVCTRL) Compare 0 Event Output Enable Mask */
#define RTC_MODE1_EVCTRL_CMPEO0(value)        (RTC_MODE1_EVCTRL_CMPEO0_Msk & (_UINT32_(value) << RTC_MODE1_EVCTRL_CMPEO0_Pos)) /* Assigment of value for CMPEO0 in the RTC_MODE1_EVCTRL register */
#define RTC_MODE1_EVCTRL_CMPEO1_Pos           _UINT32_(9)                                          /* (RTC_MODE1_EVCTRL) Compare 1 Event Output Enable Position */
#define RTC_MODE1_EVCTRL_CMPEO1_Msk           (_UINT32_(0x1) << RTC_MODE1_EVCTRL_CMPEO1_Pos)       /* (RTC_MODE1_EVCTRL) Compare 1 Event Output Enable Mask */
#define RTC_MODE1_EVCTRL_CMPEO1(value)        (RTC_MODE1_EVCTRL_CMPEO1_Msk & (_UINT32_(value) << RTC_MODE1_EVCTRL_CMPEO1_Pos)) /* Assigment of value for CMPEO1 in the RTC_MODE1_EVCTRL register */
#define RTC_MODE1_EVCTRL_OVFEO_Pos            _UINT32_(15)                                         /* (RTC_MODE1_EVCTRL) Overflow Event Output Enable Position */
#define RTC_MODE1_EVCTRL_OVFEO_Msk            (_UINT32_(0x1) << RTC_MODE1_EVCTRL_OVFEO_Pos)        /* (RTC_MODE1_EVCTRL) Overflow Event Output Enable Mask */
#define RTC_MODE1_EVCTRL_OVFEO(value)         (RTC_MODE1_EVCTRL_OVFEO_Msk & (_UINT32_(value) << RTC_MODE1_EVCTRL_OVFEO_Pos)) /* Assigment of value for OVFEO in the RTC_MODE1_EVCTRL register */
#define RTC_MODE1_EVCTRL_Msk                  _UINT32_(0x000083FF)                                 /* (RTC_MODE1_EVCTRL) Register Mask  */

#define RTC_MODE1_EVCTRL_PEREO_Pos            _UINT32_(0)                                          /* (RTC_MODE1_EVCTRL Position) Periodic Interval x Event Output Enable */
#define RTC_MODE1_EVCTRL_PEREO_Msk            (_UINT32_(0xFF) << RTC_MODE1_EVCTRL_PEREO_Pos)       /* (RTC_MODE1_EVCTRL Mask) PEREO */
#define RTC_MODE1_EVCTRL_PEREO(value)         (RTC_MODE1_EVCTRL_PEREO_Msk & (_UINT32_(value) << RTC_MODE1_EVCTRL_PEREO_Pos)) 
#define RTC_MODE1_EVCTRL_CMPEO_Pos            _UINT32_(8)                                          /* (RTC_MODE1_EVCTRL Position) Compare x Event Output Enable */
#define RTC_MODE1_EVCTRL_CMPEO_Msk            (_UINT32_(0x3) << RTC_MODE1_EVCTRL_CMPEO_Pos)        /* (RTC_MODE1_EVCTRL Mask) CMPEO */
#define RTC_MODE1_EVCTRL_CMPEO(value)         (RTC_MODE1_EVCTRL_CMPEO_Msk & (_UINT32_(value) << RTC_MODE1_EVCTRL_CMPEO_Pos)) 

/* -------- RTC_MODE2_EVCTRL : (RTC Offset: 0x04) (R/W 32) MODE2 Event Control -------- */
#define RTC_MODE2_EVCTRL_RESETVALUE           _UINT32_(0x00)                                       /*  (RTC_MODE2_EVCTRL) MODE2 Event Control  Reset Value */

#define RTC_MODE2_EVCTRL_PEREO0_Pos           _UINT32_(0)                                          /* (RTC_MODE2_EVCTRL) Periodic Interval 0 Event Output Enable Position */
#define RTC_MODE2_EVCTRL_PEREO0_Msk           (_UINT32_(0x1) << RTC_MODE2_EVCTRL_PEREO0_Pos)       /* (RTC_MODE2_EVCTRL) Periodic Interval 0 Event Output Enable Mask */
#define RTC_MODE2_EVCTRL_PEREO0(value)        (RTC_MODE2_EVCTRL_PEREO0_Msk & (_UINT32_(value) << RTC_MODE2_EVCTRL_PEREO0_Pos)) /* Assigment of value for PEREO0 in the RTC_MODE2_EVCTRL register */
#define RTC_MODE2_EVCTRL_PEREO1_Pos           _UINT32_(1)                                          /* (RTC_MODE2_EVCTRL) Periodic Interval 1 Event Output Enable Position */
#define RTC_MODE2_EVCTRL_PEREO1_Msk           (_UINT32_(0x1) << RTC_MODE2_EVCTRL_PEREO1_Pos)       /* (RTC_MODE2_EVCTRL) Periodic Interval 1 Event Output Enable Mask */
#define RTC_MODE2_EVCTRL_PEREO1(value)        (RTC_MODE2_EVCTRL_PEREO1_Msk & (_UINT32_(value) << RTC_MODE2_EVCTRL_PEREO1_Pos)) /* Assigment of value for PEREO1 in the RTC_MODE2_EVCTRL register */
#define RTC_MODE2_EVCTRL_PEREO2_Pos           _UINT32_(2)                                          /* (RTC_MODE2_EVCTRL) Periodic Interval 2 Event Output Enable Position */
#define RTC_MODE2_EVCTRL_PEREO2_Msk           (_UINT32_(0x1) << RTC_MODE2_EVCTRL_PEREO2_Pos)       /* (RTC_MODE2_EVCTRL) Periodic Interval 2 Event Output Enable Mask */
#define RTC_MODE2_EVCTRL_PEREO2(value)        (RTC_MODE2_EVCTRL_PEREO2_Msk & (_UINT32_(value) << RTC_MODE2_EVCTRL_PEREO2_Pos)) /* Assigment of value for PEREO2 in the RTC_MODE2_EVCTRL register */
#define RTC_MODE2_EVCTRL_PEREO3_Pos           _UINT32_(3)                                          /* (RTC_MODE2_EVCTRL) Periodic Interval 3 Event Output Enable Position */
#define RTC_MODE2_EVCTRL_PEREO3_Msk           (_UINT32_(0x1) << RTC_MODE2_EVCTRL_PEREO3_Pos)       /* (RTC_MODE2_EVCTRL) Periodic Interval 3 Event Output Enable Mask */
#define RTC_MODE2_EVCTRL_PEREO3(value)        (RTC_MODE2_EVCTRL_PEREO3_Msk & (_UINT32_(value) << RTC_MODE2_EVCTRL_PEREO3_Pos)) /* Assigment of value for PEREO3 in the RTC_MODE2_EVCTRL register */
#define RTC_MODE2_EVCTRL_PEREO4_Pos           _UINT32_(4)                                          /* (RTC_MODE2_EVCTRL) Periodic Interval 4 Event Output Enable Position */
#define RTC_MODE2_EVCTRL_PEREO4_Msk           (_UINT32_(0x1) << RTC_MODE2_EVCTRL_PEREO4_Pos)       /* (RTC_MODE2_EVCTRL) Periodic Interval 4 Event Output Enable Mask */
#define RTC_MODE2_EVCTRL_PEREO4(value)        (RTC_MODE2_EVCTRL_PEREO4_Msk & (_UINT32_(value) << RTC_MODE2_EVCTRL_PEREO4_Pos)) /* Assigment of value for PEREO4 in the RTC_MODE2_EVCTRL register */
#define RTC_MODE2_EVCTRL_PEREO5_Pos           _UINT32_(5)                                          /* (RTC_MODE2_EVCTRL) Periodic Interval 5 Event Output Enable Position */
#define RTC_MODE2_EVCTRL_PEREO5_Msk           (_UINT32_(0x1) << RTC_MODE2_EVCTRL_PEREO5_Pos)       /* (RTC_MODE2_EVCTRL) Periodic Interval 5 Event Output Enable Mask */
#define RTC_MODE2_EVCTRL_PEREO5(value)        (RTC_MODE2_EVCTRL_PEREO5_Msk & (_UINT32_(value) << RTC_MODE2_EVCTRL_PEREO5_Pos)) /* Assigment of value for PEREO5 in the RTC_MODE2_EVCTRL register */
#define RTC_MODE2_EVCTRL_PEREO6_Pos           _UINT32_(6)                                          /* (RTC_MODE2_EVCTRL) Periodic Interval 6 Event Output Enable Position */
#define RTC_MODE2_EVCTRL_PEREO6_Msk           (_UINT32_(0x1) << RTC_MODE2_EVCTRL_PEREO6_Pos)       /* (RTC_MODE2_EVCTRL) Periodic Interval 6 Event Output Enable Mask */
#define RTC_MODE2_EVCTRL_PEREO6(value)        (RTC_MODE2_EVCTRL_PEREO6_Msk & (_UINT32_(value) << RTC_MODE2_EVCTRL_PEREO6_Pos)) /* Assigment of value for PEREO6 in the RTC_MODE2_EVCTRL register */
#define RTC_MODE2_EVCTRL_PEREO7_Pos           _UINT32_(7)                                          /* (RTC_MODE2_EVCTRL) Periodic Interval 7 Event Output Enable Position */
#define RTC_MODE2_EVCTRL_PEREO7_Msk           (_UINT32_(0x1) << RTC_MODE2_EVCTRL_PEREO7_Pos)       /* (RTC_MODE2_EVCTRL) Periodic Interval 7 Event Output Enable Mask */
#define RTC_MODE2_EVCTRL_PEREO7(value)        (RTC_MODE2_EVCTRL_PEREO7_Msk & (_UINT32_(value) << RTC_MODE2_EVCTRL_PEREO7_Pos)) /* Assigment of value for PEREO7 in the RTC_MODE2_EVCTRL register */
#define RTC_MODE2_EVCTRL_ALARMEO0_Pos         _UINT32_(8)                                          /* (RTC_MODE2_EVCTRL) Alarm 0 Event Output Enable Position */
#define RTC_MODE2_EVCTRL_ALARMEO0_Msk         (_UINT32_(0x1) << RTC_MODE2_EVCTRL_ALARMEO0_Pos)     /* (RTC_MODE2_EVCTRL) Alarm 0 Event Output Enable Mask */
#define RTC_MODE2_EVCTRL_ALARMEO0(value)      (RTC_MODE2_EVCTRL_ALARMEO0_Msk & (_UINT32_(value) << RTC_MODE2_EVCTRL_ALARMEO0_Pos)) /* Assigment of value for ALARMEO0 in the RTC_MODE2_EVCTRL register */
#define RTC_MODE2_EVCTRL_OVFEO_Pos            _UINT32_(15)                                         /* (RTC_MODE2_EVCTRL) Overflow Event Output Enable Position */
#define RTC_MODE2_EVCTRL_OVFEO_Msk            (_UINT32_(0x1) << RTC_MODE2_EVCTRL_OVFEO_Pos)        /* (RTC_MODE2_EVCTRL) Overflow Event Output Enable Mask */
#define RTC_MODE2_EVCTRL_OVFEO(value)         (RTC_MODE2_EVCTRL_OVFEO_Msk & (_UINT32_(value) << RTC_MODE2_EVCTRL_OVFEO_Pos)) /* Assigment of value for OVFEO in the RTC_MODE2_EVCTRL register */
#define RTC_MODE2_EVCTRL_Msk                  _UINT32_(0x000081FF)                                 /* (RTC_MODE2_EVCTRL) Register Mask  */

#define RTC_MODE2_EVCTRL_PEREO_Pos            _UINT32_(0)                                          /* (RTC_MODE2_EVCTRL Position) Periodic Interval x Event Output Enable */
#define RTC_MODE2_EVCTRL_PEREO_Msk            (_UINT32_(0xFF) << RTC_MODE2_EVCTRL_PEREO_Pos)       /* (RTC_MODE2_EVCTRL Mask) PEREO */
#define RTC_MODE2_EVCTRL_PEREO(value)         (RTC_MODE2_EVCTRL_PEREO_Msk & (_UINT32_(value) << RTC_MODE2_EVCTRL_PEREO_Pos)) 
#define RTC_MODE2_EVCTRL_ALARMEO_Pos          _UINT32_(8)                                          /* (RTC_MODE2_EVCTRL Position) Alarm x Event Output Enable */
#define RTC_MODE2_EVCTRL_ALARMEO_Msk          (_UINT32_(0x1) << RTC_MODE2_EVCTRL_ALARMEO_Pos)      /* (RTC_MODE2_EVCTRL Mask) ALARMEO */
#define RTC_MODE2_EVCTRL_ALARMEO(value)       (RTC_MODE2_EVCTRL_ALARMEO_Msk & (_UINT32_(value) << RTC_MODE2_EVCTRL_ALARMEO_Pos)) 

/* -------- RTC_MODE0_INTENCLR : (RTC Offset: 0x08) (R/W 16) MODE0 Interrupt Enable Clear -------- */
#define RTC_MODE0_INTENCLR_RESETVALUE         _UINT16_(0x00)                                       /*  (RTC_MODE0_INTENCLR) MODE0 Interrupt Enable Clear  Reset Value */

#define RTC_MODE0_INTENCLR_PER0_Pos           _UINT16_(0)                                          /* (RTC_MODE0_INTENCLR) Periodic Interval 0 Interrupt Enable Position */
#define RTC_MODE0_INTENCLR_PER0_Msk           (_UINT16_(0x1) << RTC_MODE0_INTENCLR_PER0_Pos)       /* (RTC_MODE0_INTENCLR) Periodic Interval 0 Interrupt Enable Mask */
#define RTC_MODE0_INTENCLR_PER0(value)        (RTC_MODE0_INTENCLR_PER0_Msk & (_UINT16_(value) << RTC_MODE0_INTENCLR_PER0_Pos)) /* Assigment of value for PER0 in the RTC_MODE0_INTENCLR register */
#define RTC_MODE0_INTENCLR_PER1_Pos           _UINT16_(1)                                          /* (RTC_MODE0_INTENCLR) Periodic Interval 1 Interrupt Enable Position */
#define RTC_MODE0_INTENCLR_PER1_Msk           (_UINT16_(0x1) << RTC_MODE0_INTENCLR_PER1_Pos)       /* (RTC_MODE0_INTENCLR) Periodic Interval 1 Interrupt Enable Mask */
#define RTC_MODE0_INTENCLR_PER1(value)        (RTC_MODE0_INTENCLR_PER1_Msk & (_UINT16_(value) << RTC_MODE0_INTENCLR_PER1_Pos)) /* Assigment of value for PER1 in the RTC_MODE0_INTENCLR register */
#define RTC_MODE0_INTENCLR_PER2_Pos           _UINT16_(2)                                          /* (RTC_MODE0_INTENCLR) Periodic Interval 2 Interrupt Enable Position */
#define RTC_MODE0_INTENCLR_PER2_Msk           (_UINT16_(0x1) << RTC_MODE0_INTENCLR_PER2_Pos)       /* (RTC_MODE0_INTENCLR) Periodic Interval 2 Interrupt Enable Mask */
#define RTC_MODE0_INTENCLR_PER2(value)        (RTC_MODE0_INTENCLR_PER2_Msk & (_UINT16_(value) << RTC_MODE0_INTENCLR_PER2_Pos)) /* Assigment of value for PER2 in the RTC_MODE0_INTENCLR register */
#define RTC_MODE0_INTENCLR_PER3_Pos           _UINT16_(3)                                          /* (RTC_MODE0_INTENCLR) Periodic Interval 3 Interrupt Enable Position */
#define RTC_MODE0_INTENCLR_PER3_Msk           (_UINT16_(0x1) << RTC_MODE0_INTENCLR_PER3_Pos)       /* (RTC_MODE0_INTENCLR) Periodic Interval 3 Interrupt Enable Mask */
#define RTC_MODE0_INTENCLR_PER3(value)        (RTC_MODE0_INTENCLR_PER3_Msk & (_UINT16_(value) << RTC_MODE0_INTENCLR_PER3_Pos)) /* Assigment of value for PER3 in the RTC_MODE0_INTENCLR register */
#define RTC_MODE0_INTENCLR_PER4_Pos           _UINT16_(4)                                          /* (RTC_MODE0_INTENCLR) Periodic Interval 4 Interrupt Enable Position */
#define RTC_MODE0_INTENCLR_PER4_Msk           (_UINT16_(0x1) << RTC_MODE0_INTENCLR_PER4_Pos)       /* (RTC_MODE0_INTENCLR) Periodic Interval 4 Interrupt Enable Mask */
#define RTC_MODE0_INTENCLR_PER4(value)        (RTC_MODE0_INTENCLR_PER4_Msk & (_UINT16_(value) << RTC_MODE0_INTENCLR_PER4_Pos)) /* Assigment of value for PER4 in the RTC_MODE0_INTENCLR register */
#define RTC_MODE0_INTENCLR_PER5_Pos           _UINT16_(5)                                          /* (RTC_MODE0_INTENCLR) Periodic Interval 5 Interrupt Enable Position */
#define RTC_MODE0_INTENCLR_PER5_Msk           (_UINT16_(0x1) << RTC_MODE0_INTENCLR_PER5_Pos)       /* (RTC_MODE0_INTENCLR) Periodic Interval 5 Interrupt Enable Mask */
#define RTC_MODE0_INTENCLR_PER5(value)        (RTC_MODE0_INTENCLR_PER5_Msk & (_UINT16_(value) << RTC_MODE0_INTENCLR_PER5_Pos)) /* Assigment of value for PER5 in the RTC_MODE0_INTENCLR register */
#define RTC_MODE0_INTENCLR_PER6_Pos           _UINT16_(6)                                          /* (RTC_MODE0_INTENCLR) Periodic Interval 6 Interrupt Enable Position */
#define RTC_MODE0_INTENCLR_PER6_Msk           (_UINT16_(0x1) << RTC_MODE0_INTENCLR_PER6_Pos)       /* (RTC_MODE0_INTENCLR) Periodic Interval 6 Interrupt Enable Mask */
#define RTC_MODE0_INTENCLR_PER6(value)        (RTC_MODE0_INTENCLR_PER6_Msk & (_UINT16_(value) << RTC_MODE0_INTENCLR_PER6_Pos)) /* Assigment of value for PER6 in the RTC_MODE0_INTENCLR register */
#define RTC_MODE0_INTENCLR_PER7_Pos           _UINT16_(7)                                          /* (RTC_MODE0_INTENCLR) Periodic Interval 7 Interrupt Enable Position */
#define RTC_MODE0_INTENCLR_PER7_Msk           (_UINT16_(0x1) << RTC_MODE0_INTENCLR_PER7_Pos)       /* (RTC_MODE0_INTENCLR) Periodic Interval 7 Interrupt Enable Mask */
#define RTC_MODE0_INTENCLR_PER7(value)        (RTC_MODE0_INTENCLR_PER7_Msk & (_UINT16_(value) << RTC_MODE0_INTENCLR_PER7_Pos)) /* Assigment of value for PER7 in the RTC_MODE0_INTENCLR register */
#define RTC_MODE0_INTENCLR_CMP0_Pos           _UINT16_(8)                                          /* (RTC_MODE0_INTENCLR) Compare 0 Interrupt Enable Position */
#define RTC_MODE0_INTENCLR_CMP0_Msk           (_UINT16_(0x1) << RTC_MODE0_INTENCLR_CMP0_Pos)       /* (RTC_MODE0_INTENCLR) Compare 0 Interrupt Enable Mask */
#define RTC_MODE0_INTENCLR_CMP0(value)        (RTC_MODE0_INTENCLR_CMP0_Msk & (_UINT16_(value) << RTC_MODE0_INTENCLR_CMP0_Pos)) /* Assigment of value for CMP0 in the RTC_MODE0_INTENCLR register */
#define RTC_MODE0_INTENCLR_OVF_Pos            _UINT16_(15)                                         /* (RTC_MODE0_INTENCLR) Overflow Interrupt Enable Position */
#define RTC_MODE0_INTENCLR_OVF_Msk            (_UINT16_(0x1) << RTC_MODE0_INTENCLR_OVF_Pos)        /* (RTC_MODE0_INTENCLR) Overflow Interrupt Enable Mask */
#define RTC_MODE0_INTENCLR_OVF(value)         (RTC_MODE0_INTENCLR_OVF_Msk & (_UINT16_(value) << RTC_MODE0_INTENCLR_OVF_Pos)) /* Assigment of value for OVF in the RTC_MODE0_INTENCLR register */
#define RTC_MODE0_INTENCLR_Msk                _UINT16_(0x81FF)                                     /* (RTC_MODE0_INTENCLR) Register Mask  */

#define RTC_MODE0_INTENCLR_PER_Pos            _UINT16_(0)                                          /* (RTC_MODE0_INTENCLR Position) Periodic Interval x Interrupt Enable */
#define RTC_MODE0_INTENCLR_PER_Msk            (_UINT16_(0xFF) << RTC_MODE0_INTENCLR_PER_Pos)       /* (RTC_MODE0_INTENCLR Mask) PER */
#define RTC_MODE0_INTENCLR_PER(value)         (RTC_MODE0_INTENCLR_PER_Msk & (_UINT16_(value) << RTC_MODE0_INTENCLR_PER_Pos)) 
#define RTC_MODE0_INTENCLR_CMP_Pos            _UINT16_(8)                                          /* (RTC_MODE0_INTENCLR Position) Compare x Interrupt Enable */
#define RTC_MODE0_INTENCLR_CMP_Msk            (_UINT16_(0x1) << RTC_MODE0_INTENCLR_CMP_Pos)        /* (RTC_MODE0_INTENCLR Mask) CMP */
#define RTC_MODE0_INTENCLR_CMP(value)         (RTC_MODE0_INTENCLR_CMP_Msk & (_UINT16_(value) << RTC_MODE0_INTENCLR_CMP_Pos)) 

/* -------- RTC_MODE1_INTENCLR : (RTC Offset: 0x08) (R/W 16) MODE1 Interrupt Enable Clear -------- */
#define RTC_MODE1_INTENCLR_RESETVALUE         _UINT16_(0x00)                                       /*  (RTC_MODE1_INTENCLR) MODE1 Interrupt Enable Clear  Reset Value */

#define RTC_MODE1_INTENCLR_PER0_Pos           _UINT16_(0)                                          /* (RTC_MODE1_INTENCLR) Periodic Interval 0 Interrupt Enable Position */
#define RTC_MODE1_INTENCLR_PER0_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENCLR_PER0_Pos)       /* (RTC_MODE1_INTENCLR) Periodic Interval 0 Interrupt Enable Mask */
#define RTC_MODE1_INTENCLR_PER0(value)        (RTC_MODE1_INTENCLR_PER0_Msk & (_UINT16_(value) << RTC_MODE1_INTENCLR_PER0_Pos)) /* Assigment of value for PER0 in the RTC_MODE1_INTENCLR register */
#define RTC_MODE1_INTENCLR_PER1_Pos           _UINT16_(1)                                          /* (RTC_MODE1_INTENCLR) Periodic Interval 1 Interrupt Enable Position */
#define RTC_MODE1_INTENCLR_PER1_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENCLR_PER1_Pos)       /* (RTC_MODE1_INTENCLR) Periodic Interval 1 Interrupt Enable Mask */
#define RTC_MODE1_INTENCLR_PER1(value)        (RTC_MODE1_INTENCLR_PER1_Msk & (_UINT16_(value) << RTC_MODE1_INTENCLR_PER1_Pos)) /* Assigment of value for PER1 in the RTC_MODE1_INTENCLR register */
#define RTC_MODE1_INTENCLR_PER2_Pos           _UINT16_(2)                                          /* (RTC_MODE1_INTENCLR) Periodic Interval 2 Interrupt Enable Position */
#define RTC_MODE1_INTENCLR_PER2_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENCLR_PER2_Pos)       /* (RTC_MODE1_INTENCLR) Periodic Interval 2 Interrupt Enable Mask */
#define RTC_MODE1_INTENCLR_PER2(value)        (RTC_MODE1_INTENCLR_PER2_Msk & (_UINT16_(value) << RTC_MODE1_INTENCLR_PER2_Pos)) /* Assigment of value for PER2 in the RTC_MODE1_INTENCLR register */
#define RTC_MODE1_INTENCLR_PER3_Pos           _UINT16_(3)                                          /* (RTC_MODE1_INTENCLR) Periodic Interval 3 Interrupt Enable Position */
#define RTC_MODE1_INTENCLR_PER3_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENCLR_PER3_Pos)       /* (RTC_MODE1_INTENCLR) Periodic Interval 3 Interrupt Enable Mask */
#define RTC_MODE1_INTENCLR_PER3(value)        (RTC_MODE1_INTENCLR_PER3_Msk & (_UINT16_(value) << RTC_MODE1_INTENCLR_PER3_Pos)) /* Assigment of value for PER3 in the RTC_MODE1_INTENCLR register */
#define RTC_MODE1_INTENCLR_PER4_Pos           _UINT16_(4)                                          /* (RTC_MODE1_INTENCLR) Periodic Interval 4 Interrupt Enable Position */
#define RTC_MODE1_INTENCLR_PER4_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENCLR_PER4_Pos)       /* (RTC_MODE1_INTENCLR) Periodic Interval 4 Interrupt Enable Mask */
#define RTC_MODE1_INTENCLR_PER4(value)        (RTC_MODE1_INTENCLR_PER4_Msk & (_UINT16_(value) << RTC_MODE1_INTENCLR_PER4_Pos)) /* Assigment of value for PER4 in the RTC_MODE1_INTENCLR register */
#define RTC_MODE1_INTENCLR_PER5_Pos           _UINT16_(5)                                          /* (RTC_MODE1_INTENCLR) Periodic Interval 5 Interrupt Enable Position */
#define RTC_MODE1_INTENCLR_PER5_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENCLR_PER5_Pos)       /* (RTC_MODE1_INTENCLR) Periodic Interval 5 Interrupt Enable Mask */
#define RTC_MODE1_INTENCLR_PER5(value)        (RTC_MODE1_INTENCLR_PER5_Msk & (_UINT16_(value) << RTC_MODE1_INTENCLR_PER5_Pos)) /* Assigment of value for PER5 in the RTC_MODE1_INTENCLR register */
#define RTC_MODE1_INTENCLR_PER6_Pos           _UINT16_(6)                                          /* (RTC_MODE1_INTENCLR) Periodic Interval 6 Interrupt Enable Position */
#define RTC_MODE1_INTENCLR_PER6_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENCLR_PER6_Pos)       /* (RTC_MODE1_INTENCLR) Periodic Interval 6 Interrupt Enable Mask */
#define RTC_MODE1_INTENCLR_PER6(value)        (RTC_MODE1_INTENCLR_PER6_Msk & (_UINT16_(value) << RTC_MODE1_INTENCLR_PER6_Pos)) /* Assigment of value for PER6 in the RTC_MODE1_INTENCLR register */
#define RTC_MODE1_INTENCLR_PER7_Pos           _UINT16_(7)                                          /* (RTC_MODE1_INTENCLR) Periodic Interval 7 Interrupt Enable Position */
#define RTC_MODE1_INTENCLR_PER7_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENCLR_PER7_Pos)       /* (RTC_MODE1_INTENCLR) Periodic Interval 7 Interrupt Enable Mask */
#define RTC_MODE1_INTENCLR_PER7(value)        (RTC_MODE1_INTENCLR_PER7_Msk & (_UINT16_(value) << RTC_MODE1_INTENCLR_PER7_Pos)) /* Assigment of value for PER7 in the RTC_MODE1_INTENCLR register */
#define RTC_MODE1_INTENCLR_CMP0_Pos           _UINT16_(8)                                          /* (RTC_MODE1_INTENCLR) Compare 0 Interrupt Enable Position */
#define RTC_MODE1_INTENCLR_CMP0_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENCLR_CMP0_Pos)       /* (RTC_MODE1_INTENCLR) Compare 0 Interrupt Enable Mask */
#define RTC_MODE1_INTENCLR_CMP0(value)        (RTC_MODE1_INTENCLR_CMP0_Msk & (_UINT16_(value) << RTC_MODE1_INTENCLR_CMP0_Pos)) /* Assigment of value for CMP0 in the RTC_MODE1_INTENCLR register */
#define RTC_MODE1_INTENCLR_CMP1_Pos           _UINT16_(9)                                          /* (RTC_MODE1_INTENCLR) Compare 1 Interrupt Enable Position */
#define RTC_MODE1_INTENCLR_CMP1_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENCLR_CMP1_Pos)       /* (RTC_MODE1_INTENCLR) Compare 1 Interrupt Enable Mask */
#define RTC_MODE1_INTENCLR_CMP1(value)        (RTC_MODE1_INTENCLR_CMP1_Msk & (_UINT16_(value) << RTC_MODE1_INTENCLR_CMP1_Pos)) /* Assigment of value for CMP1 in the RTC_MODE1_INTENCLR register */
#define RTC_MODE1_INTENCLR_OVF_Pos            _UINT16_(15)                                         /* (RTC_MODE1_INTENCLR) Overflow Interrupt Enable Position */
#define RTC_MODE1_INTENCLR_OVF_Msk            (_UINT16_(0x1) << RTC_MODE1_INTENCLR_OVF_Pos)        /* (RTC_MODE1_INTENCLR) Overflow Interrupt Enable Mask */
#define RTC_MODE1_INTENCLR_OVF(value)         (RTC_MODE1_INTENCLR_OVF_Msk & (_UINT16_(value) << RTC_MODE1_INTENCLR_OVF_Pos)) /* Assigment of value for OVF in the RTC_MODE1_INTENCLR register */
#define RTC_MODE1_INTENCLR_Msk                _UINT16_(0x83FF)                                     /* (RTC_MODE1_INTENCLR) Register Mask  */

#define RTC_MODE1_INTENCLR_PER_Pos            _UINT16_(0)                                          /* (RTC_MODE1_INTENCLR Position) Periodic Interval x Interrupt Enable */
#define RTC_MODE1_INTENCLR_PER_Msk            (_UINT16_(0xFF) << RTC_MODE1_INTENCLR_PER_Pos)       /* (RTC_MODE1_INTENCLR Mask) PER */
#define RTC_MODE1_INTENCLR_PER(value)         (RTC_MODE1_INTENCLR_PER_Msk & (_UINT16_(value) << RTC_MODE1_INTENCLR_PER_Pos)) 
#define RTC_MODE1_INTENCLR_CMP_Pos            _UINT16_(8)                                          /* (RTC_MODE1_INTENCLR Position) Compare x Interrupt Enable */
#define RTC_MODE1_INTENCLR_CMP_Msk            (_UINT16_(0x3) << RTC_MODE1_INTENCLR_CMP_Pos)        /* (RTC_MODE1_INTENCLR Mask) CMP */
#define RTC_MODE1_INTENCLR_CMP(value)         (RTC_MODE1_INTENCLR_CMP_Msk & (_UINT16_(value) << RTC_MODE1_INTENCLR_CMP_Pos)) 

/* -------- RTC_MODE2_INTENCLR : (RTC Offset: 0x08) (R/W 16) MODE2 Interrupt Enable Clear -------- */
#define RTC_MODE2_INTENCLR_RESETVALUE         _UINT16_(0x00)                                       /*  (RTC_MODE2_INTENCLR) MODE2 Interrupt Enable Clear  Reset Value */

#define RTC_MODE2_INTENCLR_PER0_Pos           _UINT16_(0)                                          /* (RTC_MODE2_INTENCLR) Periodic Interval 0 Interrupt Enable Position */
#define RTC_MODE2_INTENCLR_PER0_Msk           (_UINT16_(0x1) << RTC_MODE2_INTENCLR_PER0_Pos)       /* (RTC_MODE2_INTENCLR) Periodic Interval 0 Interrupt Enable Mask */
#define RTC_MODE2_INTENCLR_PER0(value)        (RTC_MODE2_INTENCLR_PER0_Msk & (_UINT16_(value) << RTC_MODE2_INTENCLR_PER0_Pos)) /* Assigment of value for PER0 in the RTC_MODE2_INTENCLR register */
#define RTC_MODE2_INTENCLR_PER1_Pos           _UINT16_(1)                                          /* (RTC_MODE2_INTENCLR) Periodic Interval 1 Interrupt Enable Position */
#define RTC_MODE2_INTENCLR_PER1_Msk           (_UINT16_(0x1) << RTC_MODE2_INTENCLR_PER1_Pos)       /* (RTC_MODE2_INTENCLR) Periodic Interval 1 Interrupt Enable Mask */
#define RTC_MODE2_INTENCLR_PER1(value)        (RTC_MODE2_INTENCLR_PER1_Msk & (_UINT16_(value) << RTC_MODE2_INTENCLR_PER1_Pos)) /* Assigment of value for PER1 in the RTC_MODE2_INTENCLR register */
#define RTC_MODE2_INTENCLR_PER2_Pos           _UINT16_(2)                                          /* (RTC_MODE2_INTENCLR) Periodic Interval 2 Interrupt Enable Position */
#define RTC_MODE2_INTENCLR_PER2_Msk           (_UINT16_(0x1) << RTC_MODE2_INTENCLR_PER2_Pos)       /* (RTC_MODE2_INTENCLR) Periodic Interval 2 Interrupt Enable Mask */
#define RTC_MODE2_INTENCLR_PER2(value)        (RTC_MODE2_INTENCLR_PER2_Msk & (_UINT16_(value) << RTC_MODE2_INTENCLR_PER2_Pos)) /* Assigment of value for PER2 in the RTC_MODE2_INTENCLR register */
#define RTC_MODE2_INTENCLR_PER3_Pos           _UINT16_(3)                                          /* (RTC_MODE2_INTENCLR) Periodic Interval 3 Interrupt Enable Position */
#define RTC_MODE2_INTENCLR_PER3_Msk           (_UINT16_(0x1) << RTC_MODE2_INTENCLR_PER3_Pos)       /* (RTC_MODE2_INTENCLR) Periodic Interval 3 Interrupt Enable Mask */
#define RTC_MODE2_INTENCLR_PER3(value)        (RTC_MODE2_INTENCLR_PER3_Msk & (_UINT16_(value) << RTC_MODE2_INTENCLR_PER3_Pos)) /* Assigment of value for PER3 in the RTC_MODE2_INTENCLR register */
#define RTC_MODE2_INTENCLR_PER4_Pos           _UINT16_(4)                                          /* (RTC_MODE2_INTENCLR) Periodic Interval 4 Interrupt Enable Position */
#define RTC_MODE2_INTENCLR_PER4_Msk           (_UINT16_(0x1) << RTC_MODE2_INTENCLR_PER4_Pos)       /* (RTC_MODE2_INTENCLR) Periodic Interval 4 Interrupt Enable Mask */
#define RTC_MODE2_INTENCLR_PER4(value)        (RTC_MODE2_INTENCLR_PER4_Msk & (_UINT16_(value) << RTC_MODE2_INTENCLR_PER4_Pos)) /* Assigment of value for PER4 in the RTC_MODE2_INTENCLR register */
#define RTC_MODE2_INTENCLR_PER5_Pos           _UINT16_(5)                                          /* (RTC_MODE2_INTENCLR) Periodic Interval 5 Interrupt Enable Position */
#define RTC_MODE2_INTENCLR_PER5_Msk           (_UINT16_(0x1) << RTC_MODE2_INTENCLR_PER5_Pos)       /* (RTC_MODE2_INTENCLR) Periodic Interval 5 Interrupt Enable Mask */
#define RTC_MODE2_INTENCLR_PER5(value)        (RTC_MODE2_INTENCLR_PER5_Msk & (_UINT16_(value) << RTC_MODE2_INTENCLR_PER5_Pos)) /* Assigment of value for PER5 in the RTC_MODE2_INTENCLR register */
#define RTC_MODE2_INTENCLR_PER6_Pos           _UINT16_(6)                                          /* (RTC_MODE2_INTENCLR) Periodic Interval 6 Interrupt Enable Position */
#define RTC_MODE2_INTENCLR_PER6_Msk           (_UINT16_(0x1) << RTC_MODE2_INTENCLR_PER6_Pos)       /* (RTC_MODE2_INTENCLR) Periodic Interval 6 Interrupt Enable Mask */
#define RTC_MODE2_INTENCLR_PER6(value)        (RTC_MODE2_INTENCLR_PER6_Msk & (_UINT16_(value) << RTC_MODE2_INTENCLR_PER6_Pos)) /* Assigment of value for PER6 in the RTC_MODE2_INTENCLR register */
#define RTC_MODE2_INTENCLR_PER7_Pos           _UINT16_(7)                                          /* (RTC_MODE2_INTENCLR) Periodic Interval 7 Interrupt Enable Position */
#define RTC_MODE2_INTENCLR_PER7_Msk           (_UINT16_(0x1) << RTC_MODE2_INTENCLR_PER7_Pos)       /* (RTC_MODE2_INTENCLR) Periodic Interval 7 Interrupt Enable Mask */
#define RTC_MODE2_INTENCLR_PER7(value)        (RTC_MODE2_INTENCLR_PER7_Msk & (_UINT16_(value) << RTC_MODE2_INTENCLR_PER7_Pos)) /* Assigment of value for PER7 in the RTC_MODE2_INTENCLR register */
#define RTC_MODE2_INTENCLR_ALARM0_Pos         _UINT16_(8)                                          /* (RTC_MODE2_INTENCLR) Alarm 0 Interrupt Enable Position */
#define RTC_MODE2_INTENCLR_ALARM0_Msk         (_UINT16_(0x1) << RTC_MODE2_INTENCLR_ALARM0_Pos)     /* (RTC_MODE2_INTENCLR) Alarm 0 Interrupt Enable Mask */
#define RTC_MODE2_INTENCLR_ALARM0(value)      (RTC_MODE2_INTENCLR_ALARM0_Msk & (_UINT16_(value) << RTC_MODE2_INTENCLR_ALARM0_Pos)) /* Assigment of value for ALARM0 in the RTC_MODE2_INTENCLR register */
#define RTC_MODE2_INTENCLR_OVF_Pos            _UINT16_(15)                                         /* (RTC_MODE2_INTENCLR) Overflow Interrupt Enable Position */
#define RTC_MODE2_INTENCLR_OVF_Msk            (_UINT16_(0x1) << RTC_MODE2_INTENCLR_OVF_Pos)        /* (RTC_MODE2_INTENCLR) Overflow Interrupt Enable Mask */
#define RTC_MODE2_INTENCLR_OVF(value)         (RTC_MODE2_INTENCLR_OVF_Msk & (_UINT16_(value) << RTC_MODE2_INTENCLR_OVF_Pos)) /* Assigment of value for OVF in the RTC_MODE2_INTENCLR register */
#define RTC_MODE2_INTENCLR_Msk                _UINT16_(0x81FF)                                     /* (RTC_MODE2_INTENCLR) Register Mask  */

#define RTC_MODE2_INTENCLR_PER_Pos            _UINT16_(0)                                          /* (RTC_MODE2_INTENCLR Position) Periodic Interval x Interrupt Enable */
#define RTC_MODE2_INTENCLR_PER_Msk            (_UINT16_(0xFF) << RTC_MODE2_INTENCLR_PER_Pos)       /* (RTC_MODE2_INTENCLR Mask) PER */
#define RTC_MODE2_INTENCLR_PER(value)         (RTC_MODE2_INTENCLR_PER_Msk & (_UINT16_(value) << RTC_MODE2_INTENCLR_PER_Pos)) 
#define RTC_MODE2_INTENCLR_ALARM_Pos          _UINT16_(8)                                          /* (RTC_MODE2_INTENCLR Position) Alarm x Interrupt Enable */
#define RTC_MODE2_INTENCLR_ALARM_Msk          (_UINT16_(0x1) << RTC_MODE2_INTENCLR_ALARM_Pos)      /* (RTC_MODE2_INTENCLR Mask) ALARM */
#define RTC_MODE2_INTENCLR_ALARM(value)       (RTC_MODE2_INTENCLR_ALARM_Msk & (_UINT16_(value) << RTC_MODE2_INTENCLR_ALARM_Pos)) 

/* -------- RTC_MODE0_INTENSET : (RTC Offset: 0x0A) (R/W 16) MODE0 Interrupt Enable Set -------- */
#define RTC_MODE0_INTENSET_RESETVALUE         _UINT16_(0x00)                                       /*  (RTC_MODE0_INTENSET) MODE0 Interrupt Enable Set  Reset Value */

#define RTC_MODE0_INTENSET_PER0_Pos           _UINT16_(0)                                          /* (RTC_MODE0_INTENSET) Periodic Interval 0 Interrupt Enable Position */
#define RTC_MODE0_INTENSET_PER0_Msk           (_UINT16_(0x1) << RTC_MODE0_INTENSET_PER0_Pos)       /* (RTC_MODE0_INTENSET) Periodic Interval 0 Interrupt Enable Mask */
#define RTC_MODE0_INTENSET_PER0(value)        (RTC_MODE0_INTENSET_PER0_Msk & (_UINT16_(value) << RTC_MODE0_INTENSET_PER0_Pos)) /* Assigment of value for PER0 in the RTC_MODE0_INTENSET register */
#define RTC_MODE0_INTENSET_PER1_Pos           _UINT16_(1)                                          /* (RTC_MODE0_INTENSET) Periodic Interval 1 Interrupt Enable Position */
#define RTC_MODE0_INTENSET_PER1_Msk           (_UINT16_(0x1) << RTC_MODE0_INTENSET_PER1_Pos)       /* (RTC_MODE0_INTENSET) Periodic Interval 1 Interrupt Enable Mask */
#define RTC_MODE0_INTENSET_PER1(value)        (RTC_MODE0_INTENSET_PER1_Msk & (_UINT16_(value) << RTC_MODE0_INTENSET_PER1_Pos)) /* Assigment of value for PER1 in the RTC_MODE0_INTENSET register */
#define RTC_MODE0_INTENSET_PER2_Pos           _UINT16_(2)                                          /* (RTC_MODE0_INTENSET) Periodic Interval 2 Interrupt Enable Position */
#define RTC_MODE0_INTENSET_PER2_Msk           (_UINT16_(0x1) << RTC_MODE0_INTENSET_PER2_Pos)       /* (RTC_MODE0_INTENSET) Periodic Interval 2 Interrupt Enable Mask */
#define RTC_MODE0_INTENSET_PER2(value)        (RTC_MODE0_INTENSET_PER2_Msk & (_UINT16_(value) << RTC_MODE0_INTENSET_PER2_Pos)) /* Assigment of value for PER2 in the RTC_MODE0_INTENSET register */
#define RTC_MODE0_INTENSET_PER3_Pos           _UINT16_(3)                                          /* (RTC_MODE0_INTENSET) Periodic Interval 3 Interrupt Enable Position */
#define RTC_MODE0_INTENSET_PER3_Msk           (_UINT16_(0x1) << RTC_MODE0_INTENSET_PER3_Pos)       /* (RTC_MODE0_INTENSET) Periodic Interval 3 Interrupt Enable Mask */
#define RTC_MODE0_INTENSET_PER3(value)        (RTC_MODE0_INTENSET_PER3_Msk & (_UINT16_(value) << RTC_MODE0_INTENSET_PER3_Pos)) /* Assigment of value for PER3 in the RTC_MODE0_INTENSET register */
#define RTC_MODE0_INTENSET_PER4_Pos           _UINT16_(4)                                          /* (RTC_MODE0_INTENSET) Periodic Interval 4 Interrupt Enable Position */
#define RTC_MODE0_INTENSET_PER4_Msk           (_UINT16_(0x1) << RTC_MODE0_INTENSET_PER4_Pos)       /* (RTC_MODE0_INTENSET) Periodic Interval 4 Interrupt Enable Mask */
#define RTC_MODE0_INTENSET_PER4(value)        (RTC_MODE0_INTENSET_PER4_Msk & (_UINT16_(value) << RTC_MODE0_INTENSET_PER4_Pos)) /* Assigment of value for PER4 in the RTC_MODE0_INTENSET register */
#define RTC_MODE0_INTENSET_PER5_Pos           _UINT16_(5)                                          /* (RTC_MODE0_INTENSET) Periodic Interval 5 Interrupt Enable Position */
#define RTC_MODE0_INTENSET_PER5_Msk           (_UINT16_(0x1) << RTC_MODE0_INTENSET_PER5_Pos)       /* (RTC_MODE0_INTENSET) Periodic Interval 5 Interrupt Enable Mask */
#define RTC_MODE0_INTENSET_PER5(value)        (RTC_MODE0_INTENSET_PER5_Msk & (_UINT16_(value) << RTC_MODE0_INTENSET_PER5_Pos)) /* Assigment of value for PER5 in the RTC_MODE0_INTENSET register */
#define RTC_MODE0_INTENSET_PER6_Pos           _UINT16_(6)                                          /* (RTC_MODE0_INTENSET) Periodic Interval 6 Interrupt Enable Position */
#define RTC_MODE0_INTENSET_PER6_Msk           (_UINT16_(0x1) << RTC_MODE0_INTENSET_PER6_Pos)       /* (RTC_MODE0_INTENSET) Periodic Interval 6 Interrupt Enable Mask */
#define RTC_MODE0_INTENSET_PER6(value)        (RTC_MODE0_INTENSET_PER6_Msk & (_UINT16_(value) << RTC_MODE0_INTENSET_PER6_Pos)) /* Assigment of value for PER6 in the RTC_MODE0_INTENSET register */
#define RTC_MODE0_INTENSET_PER7_Pos           _UINT16_(7)                                          /* (RTC_MODE0_INTENSET) Periodic Interval 7 Interrupt Enable Position */
#define RTC_MODE0_INTENSET_PER7_Msk           (_UINT16_(0x1) << RTC_MODE0_INTENSET_PER7_Pos)       /* (RTC_MODE0_INTENSET) Periodic Interval 7 Interrupt Enable Mask */
#define RTC_MODE0_INTENSET_PER7(value)        (RTC_MODE0_INTENSET_PER7_Msk & (_UINT16_(value) << RTC_MODE0_INTENSET_PER7_Pos)) /* Assigment of value for PER7 in the RTC_MODE0_INTENSET register */
#define RTC_MODE0_INTENSET_CMP0_Pos           _UINT16_(8)                                          /* (RTC_MODE0_INTENSET) Compare 0 Interrupt Enable Position */
#define RTC_MODE0_INTENSET_CMP0_Msk           (_UINT16_(0x1) << RTC_MODE0_INTENSET_CMP0_Pos)       /* (RTC_MODE0_INTENSET) Compare 0 Interrupt Enable Mask */
#define RTC_MODE0_INTENSET_CMP0(value)        (RTC_MODE0_INTENSET_CMP0_Msk & (_UINT16_(value) << RTC_MODE0_INTENSET_CMP0_Pos)) /* Assigment of value for CMP0 in the RTC_MODE0_INTENSET register */
#define RTC_MODE0_INTENSET_OVF_Pos            _UINT16_(15)                                         /* (RTC_MODE0_INTENSET) Overflow Interrupt Enable Position */
#define RTC_MODE0_INTENSET_OVF_Msk            (_UINT16_(0x1) << RTC_MODE0_INTENSET_OVF_Pos)        /* (RTC_MODE0_INTENSET) Overflow Interrupt Enable Mask */
#define RTC_MODE0_INTENSET_OVF(value)         (RTC_MODE0_INTENSET_OVF_Msk & (_UINT16_(value) << RTC_MODE0_INTENSET_OVF_Pos)) /* Assigment of value for OVF in the RTC_MODE0_INTENSET register */
#define RTC_MODE0_INTENSET_Msk                _UINT16_(0x81FF)                                     /* (RTC_MODE0_INTENSET) Register Mask  */

#define RTC_MODE0_INTENSET_PER_Pos            _UINT16_(0)                                          /* (RTC_MODE0_INTENSET Position) Periodic Interval x Interrupt Enable */
#define RTC_MODE0_INTENSET_PER_Msk            (_UINT16_(0xFF) << RTC_MODE0_INTENSET_PER_Pos)       /* (RTC_MODE0_INTENSET Mask) PER */
#define RTC_MODE0_INTENSET_PER(value)         (RTC_MODE0_INTENSET_PER_Msk & (_UINT16_(value) << RTC_MODE0_INTENSET_PER_Pos)) 
#define RTC_MODE0_INTENSET_CMP_Pos            _UINT16_(8)                                          /* (RTC_MODE0_INTENSET Position) Compare x Interrupt Enable */
#define RTC_MODE0_INTENSET_CMP_Msk            (_UINT16_(0x1) << RTC_MODE0_INTENSET_CMP_Pos)        /* (RTC_MODE0_INTENSET Mask) CMP */
#define RTC_MODE0_INTENSET_CMP(value)         (RTC_MODE0_INTENSET_CMP_Msk & (_UINT16_(value) << RTC_MODE0_INTENSET_CMP_Pos)) 

/* -------- RTC_MODE1_INTENSET : (RTC Offset: 0x0A) (R/W 16) MODE1 Interrupt Enable Set -------- */
#define RTC_MODE1_INTENSET_RESETVALUE         _UINT16_(0x00)                                       /*  (RTC_MODE1_INTENSET) MODE1 Interrupt Enable Set  Reset Value */

#define RTC_MODE1_INTENSET_PER0_Pos           _UINT16_(0)                                          /* (RTC_MODE1_INTENSET) Periodic Interval 0 Interrupt Enable Position */
#define RTC_MODE1_INTENSET_PER0_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENSET_PER0_Pos)       /* (RTC_MODE1_INTENSET) Periodic Interval 0 Interrupt Enable Mask */
#define RTC_MODE1_INTENSET_PER0(value)        (RTC_MODE1_INTENSET_PER0_Msk & (_UINT16_(value) << RTC_MODE1_INTENSET_PER0_Pos)) /* Assigment of value for PER0 in the RTC_MODE1_INTENSET register */
#define RTC_MODE1_INTENSET_PER1_Pos           _UINT16_(1)                                          /* (RTC_MODE1_INTENSET) Periodic Interval 1 Interrupt Enable Position */
#define RTC_MODE1_INTENSET_PER1_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENSET_PER1_Pos)       /* (RTC_MODE1_INTENSET) Periodic Interval 1 Interrupt Enable Mask */
#define RTC_MODE1_INTENSET_PER1(value)        (RTC_MODE1_INTENSET_PER1_Msk & (_UINT16_(value) << RTC_MODE1_INTENSET_PER1_Pos)) /* Assigment of value for PER1 in the RTC_MODE1_INTENSET register */
#define RTC_MODE1_INTENSET_PER2_Pos           _UINT16_(2)                                          /* (RTC_MODE1_INTENSET) Periodic Interval 2 Interrupt Enable Position */
#define RTC_MODE1_INTENSET_PER2_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENSET_PER2_Pos)       /* (RTC_MODE1_INTENSET) Periodic Interval 2 Interrupt Enable Mask */
#define RTC_MODE1_INTENSET_PER2(value)        (RTC_MODE1_INTENSET_PER2_Msk & (_UINT16_(value) << RTC_MODE1_INTENSET_PER2_Pos)) /* Assigment of value for PER2 in the RTC_MODE1_INTENSET register */
#define RTC_MODE1_INTENSET_PER3_Pos           _UINT16_(3)                                          /* (RTC_MODE1_INTENSET) Periodic Interval 3 Interrupt Enable Position */
#define RTC_MODE1_INTENSET_PER3_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENSET_PER3_Pos)       /* (RTC_MODE1_INTENSET) Periodic Interval 3 Interrupt Enable Mask */
#define RTC_MODE1_INTENSET_PER3(value)        (RTC_MODE1_INTENSET_PER3_Msk & (_UINT16_(value) << RTC_MODE1_INTENSET_PER3_Pos)) /* Assigment of value for PER3 in the RTC_MODE1_INTENSET register */
#define RTC_MODE1_INTENSET_PER4_Pos           _UINT16_(4)                                          /* (RTC_MODE1_INTENSET) Periodic Interval 4 Interrupt Enable Position */
#define RTC_MODE1_INTENSET_PER4_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENSET_PER4_Pos)       /* (RTC_MODE1_INTENSET) Periodic Interval 4 Interrupt Enable Mask */
#define RTC_MODE1_INTENSET_PER4(value)        (RTC_MODE1_INTENSET_PER4_Msk & (_UINT16_(value) << RTC_MODE1_INTENSET_PER4_Pos)) /* Assigment of value for PER4 in the RTC_MODE1_INTENSET register */
#define RTC_MODE1_INTENSET_PER5_Pos           _UINT16_(5)                                          /* (RTC_MODE1_INTENSET) Periodic Interval 5 Interrupt Enable Position */
#define RTC_MODE1_INTENSET_PER5_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENSET_PER5_Pos)       /* (RTC_MODE1_INTENSET) Periodic Interval 5 Interrupt Enable Mask */
#define RTC_MODE1_INTENSET_PER5(value)        (RTC_MODE1_INTENSET_PER5_Msk & (_UINT16_(value) << RTC_MODE1_INTENSET_PER5_Pos)) /* Assigment of value for PER5 in the RTC_MODE1_INTENSET register */
#define RTC_MODE1_INTENSET_PER6_Pos           _UINT16_(6)                                          /* (RTC_MODE1_INTENSET) Periodic Interval 6 Interrupt Enable Position */
#define RTC_MODE1_INTENSET_PER6_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENSET_PER6_Pos)       /* (RTC_MODE1_INTENSET) Periodic Interval 6 Interrupt Enable Mask */
#define RTC_MODE1_INTENSET_PER6(value)        (RTC_MODE1_INTENSET_PER6_Msk & (_UINT16_(value) << RTC_MODE1_INTENSET_PER6_Pos)) /* Assigment of value for PER6 in the RTC_MODE1_INTENSET register */
#define RTC_MODE1_INTENSET_PER7_Pos           _UINT16_(7)                                          /* (RTC_MODE1_INTENSET) Periodic Interval 7 Interrupt Enable Position */
#define RTC_MODE1_INTENSET_PER7_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENSET_PER7_Pos)       /* (RTC_MODE1_INTENSET) Periodic Interval 7 Interrupt Enable Mask */
#define RTC_MODE1_INTENSET_PER7(value)        (RTC_MODE1_INTENSET_PER7_Msk & (_UINT16_(value) << RTC_MODE1_INTENSET_PER7_Pos)) /* Assigment of value for PER7 in the RTC_MODE1_INTENSET register */
#define RTC_MODE1_INTENSET_CMP0_Pos           _UINT16_(8)                                          /* (RTC_MODE1_INTENSET) Compare 0 Interrupt Enable Position */
#define RTC_MODE1_INTENSET_CMP0_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENSET_CMP0_Pos)       /* (RTC_MODE1_INTENSET) Compare 0 Interrupt Enable Mask */
#define RTC_MODE1_INTENSET_CMP0(value)        (RTC_MODE1_INTENSET_CMP0_Msk & (_UINT16_(value) << RTC_MODE1_INTENSET_CMP0_Pos)) /* Assigment of value for CMP0 in the RTC_MODE1_INTENSET register */
#define RTC_MODE1_INTENSET_CMP1_Pos           _UINT16_(9)                                          /* (RTC_MODE1_INTENSET) Compare 1 Interrupt Enable Position */
#define RTC_MODE1_INTENSET_CMP1_Msk           (_UINT16_(0x1) << RTC_MODE1_INTENSET_CMP1_Pos)       /* (RTC_MODE1_INTENSET) Compare 1 Interrupt Enable Mask */
#define RTC_MODE1_INTENSET_CMP1(value)        (RTC_MODE1_INTENSET_CMP1_Msk & (_UINT16_(value) << RTC_MODE1_INTENSET_CMP1_Pos)) /* Assigment of value for CMP1 in the RTC_MODE1_INTENSET register */
#define RTC_MODE1_INTENSET_OVF_Pos            _UINT16_(15)                                         /* (RTC_MODE1_INTENSET) Overflow Interrupt Enable Position */
#define RTC_MODE1_INTENSET_OVF_Msk            (_UINT16_(0x1) << RTC_MODE1_INTENSET_OVF_Pos)        /* (RTC_MODE1_INTENSET) Overflow Interrupt Enable Mask */
#define RTC_MODE1_INTENSET_OVF(value)         (RTC_MODE1_INTENSET_OVF_Msk & (_UINT16_(value) << RTC_MODE1_INTENSET_OVF_Pos)) /* Assigment of value for OVF in the RTC_MODE1_INTENSET register */
#define RTC_MODE1_INTENSET_Msk                _UINT16_(0x83FF)                                     /* (RTC_MODE1_INTENSET) Register Mask  */

#define RTC_MODE1_INTENSET_PER_Pos            _UINT16_(0)                                          /* (RTC_MODE1_INTENSET Position) Periodic Interval x Interrupt Enable */
#define RTC_MODE1_INTENSET_PER_Msk            (_UINT16_(0xFF) << RTC_MODE1_INTENSET_PER_Pos)       /* (RTC_MODE1_INTENSET Mask) PER */
#define RTC_MODE1_INTENSET_PER(value)         (RTC_MODE1_INTENSET_PER_Msk & (_UINT16_(value) << RTC_MODE1_INTENSET_PER_Pos)) 
#define RTC_MODE1_INTENSET_CMP_Pos            _UINT16_(8)                                          /* (RTC_MODE1_INTENSET Position) Compare x Interrupt Enable */
#define RTC_MODE1_INTENSET_CMP_Msk            (_UINT16_(0x3) << RTC_MODE1_INTENSET_CMP_Pos)        /* (RTC_MODE1_INTENSET Mask) CMP */
#define RTC_MODE1_INTENSET_CMP(value)         (RTC_MODE1_INTENSET_CMP_Msk & (_UINT16_(value) << RTC_MODE1_INTENSET_CMP_Pos)) 

/* -------- RTC_MODE2_INTENSET : (RTC Offset: 0x0A) (R/W 16) MODE2 Interrupt Enable Set -------- */
#define RTC_MODE2_INTENSET_RESETVALUE         _UINT16_(0x00)                                       /*  (RTC_MODE2_INTENSET) MODE2 Interrupt Enable Set  Reset Value */

#define RTC_MODE2_INTENSET_PER0_Pos           _UINT16_(0)                                          /* (RTC_MODE2_INTENSET) Periodic Interval 0 Enable Position */
#define RTC_MODE2_INTENSET_PER0_Msk           (_UINT16_(0x1) << RTC_MODE2_INTENSET_PER0_Pos)       /* (RTC_MODE2_INTENSET) Periodic Interval 0 Enable Mask */
#define RTC_MODE2_INTENSET_PER0(value)        (RTC_MODE2_INTENSET_PER0_Msk & (_UINT16_(value) << RTC_MODE2_INTENSET_PER0_Pos)) /* Assigment of value for PER0 in the RTC_MODE2_INTENSET register */
#define RTC_MODE2_INTENSET_PER1_Pos           _UINT16_(1)                                          /* (RTC_MODE2_INTENSET) Periodic Interval 1 Enable Position */
#define RTC_MODE2_INTENSET_PER1_Msk           (_UINT16_(0x1) << RTC_MODE2_INTENSET_PER1_Pos)       /* (RTC_MODE2_INTENSET) Periodic Interval 1 Enable Mask */
#define RTC_MODE2_INTENSET_PER1(value)        (RTC_MODE2_INTENSET_PER1_Msk & (_UINT16_(value) << RTC_MODE2_INTENSET_PER1_Pos)) /* Assigment of value for PER1 in the RTC_MODE2_INTENSET register */
#define RTC_MODE2_INTENSET_PER2_Pos           _UINT16_(2)                                          /* (RTC_MODE2_INTENSET) Periodic Interval 2 Enable Position */
#define RTC_MODE2_INTENSET_PER2_Msk           (_UINT16_(0x1) << RTC_MODE2_INTENSET_PER2_Pos)       /* (RTC_MODE2_INTENSET) Periodic Interval 2 Enable Mask */
#define RTC_MODE2_INTENSET_PER2(value)        (RTC_MODE2_INTENSET_PER2_Msk & (_UINT16_(value) << RTC_MODE2_INTENSET_PER2_Pos)) /* Assigment of value for PER2 in the RTC_MODE2_INTENSET register */
#define RTC_MODE2_INTENSET_PER3_Pos           _UINT16_(3)                                          /* (RTC_MODE2_INTENSET) Periodic Interval 3 Enable Position */
#define RTC_MODE2_INTENSET_PER3_Msk           (_UINT16_(0x1) << RTC_MODE2_INTENSET_PER3_Pos)       /* (RTC_MODE2_INTENSET) Periodic Interval 3 Enable Mask */
#define RTC_MODE2_INTENSET_PER3(value)        (RTC_MODE2_INTENSET_PER3_Msk & (_UINT16_(value) << RTC_MODE2_INTENSET_PER3_Pos)) /* Assigment of value for PER3 in the RTC_MODE2_INTENSET register */
#define RTC_MODE2_INTENSET_PER4_Pos           _UINT16_(4)                                          /* (RTC_MODE2_INTENSET) Periodic Interval 4 Enable Position */
#define RTC_MODE2_INTENSET_PER4_Msk           (_UINT16_(0x1) << RTC_MODE2_INTENSET_PER4_Pos)       /* (RTC_MODE2_INTENSET) Periodic Interval 4 Enable Mask */
#define RTC_MODE2_INTENSET_PER4(value)        (RTC_MODE2_INTENSET_PER4_Msk & (_UINT16_(value) << RTC_MODE2_INTENSET_PER4_Pos)) /* Assigment of value for PER4 in the RTC_MODE2_INTENSET register */
#define RTC_MODE2_INTENSET_PER5_Pos           _UINT16_(5)                                          /* (RTC_MODE2_INTENSET) Periodic Interval 5 Enable Position */
#define RTC_MODE2_INTENSET_PER5_Msk           (_UINT16_(0x1) << RTC_MODE2_INTENSET_PER5_Pos)       /* (RTC_MODE2_INTENSET) Periodic Interval 5 Enable Mask */
#define RTC_MODE2_INTENSET_PER5(value)        (RTC_MODE2_INTENSET_PER5_Msk & (_UINT16_(value) << RTC_MODE2_INTENSET_PER5_Pos)) /* Assigment of value for PER5 in the RTC_MODE2_INTENSET register */
#define RTC_MODE2_INTENSET_PER6_Pos           _UINT16_(6)                                          /* (RTC_MODE2_INTENSET) Periodic Interval 6 Enable Position */
#define RTC_MODE2_INTENSET_PER6_Msk           (_UINT16_(0x1) << RTC_MODE2_INTENSET_PER6_Pos)       /* (RTC_MODE2_INTENSET) Periodic Interval 6 Enable Mask */
#define RTC_MODE2_INTENSET_PER6(value)        (RTC_MODE2_INTENSET_PER6_Msk & (_UINT16_(value) << RTC_MODE2_INTENSET_PER6_Pos)) /* Assigment of value for PER6 in the RTC_MODE2_INTENSET register */
#define RTC_MODE2_INTENSET_PER7_Pos           _UINT16_(7)                                          /* (RTC_MODE2_INTENSET) Periodic Interval 7 Enable Position */
#define RTC_MODE2_INTENSET_PER7_Msk           (_UINT16_(0x1) << RTC_MODE2_INTENSET_PER7_Pos)       /* (RTC_MODE2_INTENSET) Periodic Interval 7 Enable Mask */
#define RTC_MODE2_INTENSET_PER7(value)        (RTC_MODE2_INTENSET_PER7_Msk & (_UINT16_(value) << RTC_MODE2_INTENSET_PER7_Pos)) /* Assigment of value for PER7 in the RTC_MODE2_INTENSET register */
#define RTC_MODE2_INTENSET_ALARM0_Pos         _UINT16_(8)                                          /* (RTC_MODE2_INTENSET) Alarm 0 Interrupt Enable Position */
#define RTC_MODE2_INTENSET_ALARM0_Msk         (_UINT16_(0x1) << RTC_MODE2_INTENSET_ALARM0_Pos)     /* (RTC_MODE2_INTENSET) Alarm 0 Interrupt Enable Mask */
#define RTC_MODE2_INTENSET_ALARM0(value)      (RTC_MODE2_INTENSET_ALARM0_Msk & (_UINT16_(value) << RTC_MODE2_INTENSET_ALARM0_Pos)) /* Assigment of value for ALARM0 in the RTC_MODE2_INTENSET register */
#define RTC_MODE2_INTENSET_OVF_Pos            _UINT16_(15)                                         /* (RTC_MODE2_INTENSET) Overflow Interrupt Enable Position */
#define RTC_MODE2_INTENSET_OVF_Msk            (_UINT16_(0x1) << RTC_MODE2_INTENSET_OVF_Pos)        /* (RTC_MODE2_INTENSET) Overflow Interrupt Enable Mask */
#define RTC_MODE2_INTENSET_OVF(value)         (RTC_MODE2_INTENSET_OVF_Msk & (_UINT16_(value) << RTC_MODE2_INTENSET_OVF_Pos)) /* Assigment of value for OVF in the RTC_MODE2_INTENSET register */
#define RTC_MODE2_INTENSET_Msk                _UINT16_(0x81FF)                                     /* (RTC_MODE2_INTENSET) Register Mask  */

#define RTC_MODE2_INTENSET_PER_Pos            _UINT16_(0)                                          /* (RTC_MODE2_INTENSET Position) Periodic Interval x Enable */
#define RTC_MODE2_INTENSET_PER_Msk            (_UINT16_(0xFF) << RTC_MODE2_INTENSET_PER_Pos)       /* (RTC_MODE2_INTENSET Mask) PER */
#define RTC_MODE2_INTENSET_PER(value)         (RTC_MODE2_INTENSET_PER_Msk & (_UINT16_(value) << RTC_MODE2_INTENSET_PER_Pos)) 
#define RTC_MODE2_INTENSET_ALARM_Pos          _UINT16_(8)                                          /* (RTC_MODE2_INTENSET Position) Alarm x Interrupt Enable */
#define RTC_MODE2_INTENSET_ALARM_Msk          (_UINT16_(0x1) << RTC_MODE2_INTENSET_ALARM_Pos)      /* (RTC_MODE2_INTENSET Mask) ALARM */
#define RTC_MODE2_INTENSET_ALARM(value)       (RTC_MODE2_INTENSET_ALARM_Msk & (_UINT16_(value) << RTC_MODE2_INTENSET_ALARM_Pos)) 

/* -------- RTC_MODE0_INTFLAG : (RTC Offset: 0x0C) (R/W 16) MODE0 Interrupt Flag Status and Clear -------- */
#define RTC_MODE0_INTFLAG_RESETVALUE          _UINT16_(0x00)                                       /*  (RTC_MODE0_INTFLAG) MODE0 Interrupt Flag Status and Clear  Reset Value */

#define RTC_MODE0_INTFLAG_PER0_Pos            _UINT16_(0)                                          /* (RTC_MODE0_INTFLAG) Periodic Interval 0 Position */
#define RTC_MODE0_INTFLAG_PER0_Msk            (_UINT16_(0x1) << RTC_MODE0_INTFLAG_PER0_Pos)        /* (RTC_MODE0_INTFLAG) Periodic Interval 0 Mask */
#define RTC_MODE0_INTFLAG_PER0(value)         (RTC_MODE0_INTFLAG_PER0_Msk & (_UINT16_(value) << RTC_MODE0_INTFLAG_PER0_Pos)) /* Assigment of value for PER0 in the RTC_MODE0_INTFLAG register */
#define RTC_MODE0_INTFLAG_PER1_Pos            _UINT16_(1)                                          /* (RTC_MODE0_INTFLAG) Periodic Interval 1 Position */
#define RTC_MODE0_INTFLAG_PER1_Msk            (_UINT16_(0x1) << RTC_MODE0_INTFLAG_PER1_Pos)        /* (RTC_MODE0_INTFLAG) Periodic Interval 1 Mask */
#define RTC_MODE0_INTFLAG_PER1(value)         (RTC_MODE0_INTFLAG_PER1_Msk & (_UINT16_(value) << RTC_MODE0_INTFLAG_PER1_Pos)) /* Assigment of value for PER1 in the RTC_MODE0_INTFLAG register */
#define RTC_MODE0_INTFLAG_PER2_Pos            _UINT16_(2)                                          /* (RTC_MODE0_INTFLAG) Periodic Interval 2 Position */
#define RTC_MODE0_INTFLAG_PER2_Msk            (_UINT16_(0x1) << RTC_MODE0_INTFLAG_PER2_Pos)        /* (RTC_MODE0_INTFLAG) Periodic Interval 2 Mask */
#define RTC_MODE0_INTFLAG_PER2(value)         (RTC_MODE0_INTFLAG_PER2_Msk & (_UINT16_(value) << RTC_MODE0_INTFLAG_PER2_Pos)) /* Assigment of value for PER2 in the RTC_MODE0_INTFLAG register */
#define RTC_MODE0_INTFLAG_PER3_Pos            _UINT16_(3)                                          /* (RTC_MODE0_INTFLAG) Periodic Interval 3 Position */
#define RTC_MODE0_INTFLAG_PER3_Msk            (_UINT16_(0x1) << RTC_MODE0_INTFLAG_PER3_Pos)        /* (RTC_MODE0_INTFLAG) Periodic Interval 3 Mask */
#define RTC_MODE0_INTFLAG_PER3(value)         (RTC_MODE0_INTFLAG_PER3_Msk & (_UINT16_(value) << RTC_MODE0_INTFLAG_PER3_Pos)) /* Assigment of value for PER3 in the RTC_MODE0_INTFLAG register */
#define RTC_MODE0_INTFLAG_PER4_Pos            _UINT16_(4)                                          /* (RTC_MODE0_INTFLAG) Periodic Interval 4 Position */
#define RTC_MODE0_INTFLAG_PER4_Msk            (_UINT16_(0x1) << RTC_MODE0_INTFLAG_PER4_Pos)        /* (RTC_MODE0_INTFLAG) Periodic Interval 4 Mask */
#define RTC_MODE0_INTFLAG_PER4(value)         (RTC_MODE0_INTFLAG_PER4_Msk & (_UINT16_(value) << RTC_MODE0_INTFLAG_PER4_Pos)) /* Assigment of value for PER4 in the RTC_MODE0_INTFLAG register */
#define RTC_MODE0_INTFLAG_PER5_Pos            _UINT16_(5)                                          /* (RTC_MODE0_INTFLAG) Periodic Interval 5 Position */
#define RTC_MODE0_INTFLAG_PER5_Msk            (_UINT16_(0x1) << RTC_MODE0_INTFLAG_PER5_Pos)        /* (RTC_MODE0_INTFLAG) Periodic Interval 5 Mask */
#define RTC_MODE0_INTFLAG_PER5(value)         (RTC_MODE0_INTFLAG_PER5_Msk & (_UINT16_(value) << RTC_MODE0_INTFLAG_PER5_Pos)) /* Assigment of value for PER5 in the RTC_MODE0_INTFLAG register */
#define RTC_MODE0_INTFLAG_PER6_Pos            _UINT16_(6)                                          /* (RTC_MODE0_INTFLAG) Periodic Interval 6 Position */
#define RTC_MODE0_INTFLAG_PER6_Msk            (_UINT16_(0x1) << RTC_MODE0_INTFLAG_PER6_Pos)        /* (RTC_MODE0_INTFLAG) Periodic Interval 6 Mask */
#define RTC_MODE0_INTFLAG_PER6(value)         (RTC_MODE0_INTFLAG_PER6_Msk & (_UINT16_(value) << RTC_MODE0_INTFLAG_PER6_Pos)) /* Assigment of value for PER6 in the RTC_MODE0_INTFLAG register */
#define RTC_MODE0_INTFLAG_PER7_Pos            _UINT16_(7)                                          /* (RTC_MODE0_INTFLAG) Periodic Interval 7 Position */
#define RTC_MODE0_INTFLAG_PER7_Msk            (_UINT16_(0x1) << RTC_MODE0_INTFLAG_PER7_Pos)        /* (RTC_MODE0_INTFLAG) Periodic Interval 7 Mask */
#define RTC_MODE0_INTFLAG_PER7(value)         (RTC_MODE0_INTFLAG_PER7_Msk & (_UINT16_(value) << RTC_MODE0_INTFLAG_PER7_Pos)) /* Assigment of value for PER7 in the RTC_MODE0_INTFLAG register */
#define RTC_MODE0_INTFLAG_CMP0_Pos            _UINT16_(8)                                          /* (RTC_MODE0_INTFLAG) Compare 0 Position */
#define RTC_MODE0_INTFLAG_CMP0_Msk            (_UINT16_(0x1) << RTC_MODE0_INTFLAG_CMP0_Pos)        /* (RTC_MODE0_INTFLAG) Compare 0 Mask */
#define RTC_MODE0_INTFLAG_CMP0(value)         (RTC_MODE0_INTFLAG_CMP0_Msk & (_UINT16_(value) << RTC_MODE0_INTFLAG_CMP0_Pos)) /* Assigment of value for CMP0 in the RTC_MODE0_INTFLAG register */
#define RTC_MODE0_INTFLAG_OVF_Pos             _UINT16_(15)                                         /* (RTC_MODE0_INTFLAG) Overflow Position */
#define RTC_MODE0_INTFLAG_OVF_Msk             (_UINT16_(0x1) << RTC_MODE0_INTFLAG_OVF_Pos)         /* (RTC_MODE0_INTFLAG) Overflow Mask */
#define RTC_MODE0_INTFLAG_OVF(value)          (RTC_MODE0_INTFLAG_OVF_Msk & (_UINT16_(value) << RTC_MODE0_INTFLAG_OVF_Pos)) /* Assigment of value for OVF in the RTC_MODE0_INTFLAG register */
#define RTC_MODE0_INTFLAG_Msk                 _UINT16_(0x81FF)                                     /* (RTC_MODE0_INTFLAG) Register Mask  */

#define RTC_MODE0_INTFLAG_PER_Pos             _UINT16_(0)                                          /* (RTC_MODE0_INTFLAG Position) Periodic Interval x */
#define RTC_MODE0_INTFLAG_PER_Msk             (_UINT16_(0xFF) << RTC_MODE0_INTFLAG_PER_Pos)        /* (RTC_MODE0_INTFLAG Mask) PER */
#define RTC_MODE0_INTFLAG_PER(value)          (RTC_MODE0_INTFLAG_PER_Msk & (_UINT16_(value) << RTC_MODE0_INTFLAG_PER_Pos)) 
#define RTC_MODE0_INTFLAG_CMP_Pos             _UINT16_(8)                                          /* (RTC_MODE0_INTFLAG Position) Compare x */
#define RTC_MODE0_INTFLAG_CMP_Msk             (_UINT16_(0x1) << RTC_MODE0_INTFLAG_CMP_Pos)         /* (RTC_MODE0_INTFLAG Mask) CMP */
#define RTC_MODE0_INTFLAG_CMP(value)          (RTC_MODE0_INTFLAG_CMP_Msk & (_UINT16_(value) << RTC_MODE0_INTFLAG_CMP_Pos)) 

/* -------- RTC_MODE1_INTFLAG : (RTC Offset: 0x0C) (R/W 16) MODE1 Interrupt Flag Status and Clear -------- */
#define RTC_MODE1_INTFLAG_RESETVALUE          _UINT16_(0x00)                                       /*  (RTC_MODE1_INTFLAG) MODE1 Interrupt Flag Status and Clear  Reset Value */

#define RTC_MODE1_INTFLAG_PER0_Pos            _UINT16_(0)                                          /* (RTC_MODE1_INTFLAG) Periodic Interval 0 Position */
#define RTC_MODE1_INTFLAG_PER0_Msk            (_UINT16_(0x1) << RTC_MODE1_INTFLAG_PER0_Pos)        /* (RTC_MODE1_INTFLAG) Periodic Interval 0 Mask */
#define RTC_MODE1_INTFLAG_PER0(value)         (RTC_MODE1_INTFLAG_PER0_Msk & (_UINT16_(value) << RTC_MODE1_INTFLAG_PER0_Pos)) /* Assigment of value for PER0 in the RTC_MODE1_INTFLAG register */
#define RTC_MODE1_INTFLAG_PER1_Pos            _UINT16_(1)                                          /* (RTC_MODE1_INTFLAG) Periodic Interval 1 Position */
#define RTC_MODE1_INTFLAG_PER1_Msk            (_UINT16_(0x1) << RTC_MODE1_INTFLAG_PER1_Pos)        /* (RTC_MODE1_INTFLAG) Periodic Interval 1 Mask */
#define RTC_MODE1_INTFLAG_PER1(value)         (RTC_MODE1_INTFLAG_PER1_Msk & (_UINT16_(value) << RTC_MODE1_INTFLAG_PER1_Pos)) /* Assigment of value for PER1 in the RTC_MODE1_INTFLAG register */
#define RTC_MODE1_INTFLAG_PER2_Pos            _UINT16_(2)                                          /* (RTC_MODE1_INTFLAG) Periodic Interval 2 Position */
#define RTC_MODE1_INTFLAG_PER2_Msk            (_UINT16_(0x1) << RTC_MODE1_INTFLAG_PER2_Pos)        /* (RTC_MODE1_INTFLAG) Periodic Interval 2 Mask */
#define RTC_MODE1_INTFLAG_PER2(value)         (RTC_MODE1_INTFLAG_PER2_Msk & (_UINT16_(value) << RTC_MODE1_INTFLAG_PER2_Pos)) /* Assigment of value for PER2 in the RTC_MODE1_INTFLAG register */
#define RTC_MODE1_INTFLAG_PER3_Pos            _UINT16_(3)                                          /* (RTC_MODE1_INTFLAG) Periodic Interval 3 Position */
#define RTC_MODE1_INTFLAG_PER3_Msk            (_UINT16_(0x1) << RTC_MODE1_INTFLAG_PER3_Pos)        /* (RTC_MODE1_INTFLAG) Periodic Interval 3 Mask */
#define RTC_MODE1_INTFLAG_PER3(value)         (RTC_MODE1_INTFLAG_PER3_Msk & (_UINT16_(value) << RTC_MODE1_INTFLAG_PER3_Pos)) /* Assigment of value for PER3 in the RTC_MODE1_INTFLAG register */
#define RTC_MODE1_INTFLAG_PER4_Pos            _UINT16_(4)                                          /* (RTC_MODE1_INTFLAG) Periodic Interval 4 Position */
#define RTC_MODE1_INTFLAG_PER4_Msk            (_UINT16_(0x1) << RTC_MODE1_INTFLAG_PER4_Pos)        /* (RTC_MODE1_INTFLAG) Periodic Interval 4 Mask */
#define RTC_MODE1_INTFLAG_PER4(value)         (RTC_MODE1_INTFLAG_PER4_Msk & (_UINT16_(value) << RTC_MODE1_INTFLAG_PER4_Pos)) /* Assigment of value for PER4 in the RTC_MODE1_INTFLAG register */
#define RTC_MODE1_INTFLAG_PER5_Pos            _UINT16_(5)                                          /* (RTC_MODE1_INTFLAG) Periodic Interval 5 Position */
#define RTC_MODE1_INTFLAG_PER5_Msk            (_UINT16_(0x1) << RTC_MODE1_INTFLAG_PER5_Pos)        /* (RTC_MODE1_INTFLAG) Periodic Interval 5 Mask */
#define RTC_MODE1_INTFLAG_PER5(value)         (RTC_MODE1_INTFLAG_PER5_Msk & (_UINT16_(value) << RTC_MODE1_INTFLAG_PER5_Pos)) /* Assigment of value for PER5 in the RTC_MODE1_INTFLAG register */
#define RTC_MODE1_INTFLAG_PER6_Pos            _UINT16_(6)                                          /* (RTC_MODE1_INTFLAG) Periodic Interval 6 Position */
#define RTC_MODE1_INTFLAG_PER6_Msk            (_UINT16_(0x1) << RTC_MODE1_INTFLAG_PER6_Pos)        /* (RTC_MODE1_INTFLAG) Periodic Interval 6 Mask */
#define RTC_MODE1_INTFLAG_PER6(value)         (RTC_MODE1_INTFLAG_PER6_Msk & (_UINT16_(value) << RTC_MODE1_INTFLAG_PER6_Pos)) /* Assigment of value for PER6 in the RTC_MODE1_INTFLAG register */
#define RTC_MODE1_INTFLAG_PER7_Pos            _UINT16_(7)                                          /* (RTC_MODE1_INTFLAG) Periodic Interval 7 Position */
#define RTC_MODE1_INTFLAG_PER7_Msk            (_UINT16_(0x1) << RTC_MODE1_INTFLAG_PER7_Pos)        /* (RTC_MODE1_INTFLAG) Periodic Interval 7 Mask */
#define RTC_MODE1_INTFLAG_PER7(value)         (RTC_MODE1_INTFLAG_PER7_Msk & (_UINT16_(value) << RTC_MODE1_INTFLAG_PER7_Pos)) /* Assigment of value for PER7 in the RTC_MODE1_INTFLAG register */
#define RTC_MODE1_INTFLAG_CMP0_Pos            _UINT16_(8)                                          /* (RTC_MODE1_INTFLAG) Compare 0 Position */
#define RTC_MODE1_INTFLAG_CMP0_Msk            (_UINT16_(0x1) << RTC_MODE1_INTFLAG_CMP0_Pos)        /* (RTC_MODE1_INTFLAG) Compare 0 Mask */
#define RTC_MODE1_INTFLAG_CMP0(value)         (RTC_MODE1_INTFLAG_CMP0_Msk & (_UINT16_(value) << RTC_MODE1_INTFLAG_CMP0_Pos)) /* Assigment of value for CMP0 in the RTC_MODE1_INTFLAG register */
#define RTC_MODE1_INTFLAG_CMP1_Pos            _UINT16_(9)                                          /* (RTC_MODE1_INTFLAG) Compare 1 Position */
#define RTC_MODE1_INTFLAG_CMP1_Msk            (_UINT16_(0x1) << RTC_MODE1_INTFLAG_CMP1_Pos)        /* (RTC_MODE1_INTFLAG) Compare 1 Mask */
#define RTC_MODE1_INTFLAG_CMP1(value)         (RTC_MODE1_INTFLAG_CMP1_Msk & (_UINT16_(value) << RTC_MODE1_INTFLAG_CMP1_Pos)) /* Assigment of value for CMP1 in the RTC_MODE1_INTFLAG register */
#define RTC_MODE1_INTFLAG_OVF_Pos             _UINT16_(15)                                         /* (RTC_MODE1_INTFLAG) Overflow Position */
#define RTC_MODE1_INTFLAG_OVF_Msk             (_UINT16_(0x1) << RTC_MODE1_INTFLAG_OVF_Pos)         /* (RTC_MODE1_INTFLAG) Overflow Mask */
#define RTC_MODE1_INTFLAG_OVF(value)          (RTC_MODE1_INTFLAG_OVF_Msk & (_UINT16_(value) << RTC_MODE1_INTFLAG_OVF_Pos)) /* Assigment of value for OVF in the RTC_MODE1_INTFLAG register */
#define RTC_MODE1_INTFLAG_Msk                 _UINT16_(0x83FF)                                     /* (RTC_MODE1_INTFLAG) Register Mask  */

#define RTC_MODE1_INTFLAG_PER_Pos             _UINT16_(0)                                          /* (RTC_MODE1_INTFLAG Position) Periodic Interval x */
#define RTC_MODE1_INTFLAG_PER_Msk             (_UINT16_(0xFF) << RTC_MODE1_INTFLAG_PER_Pos)        /* (RTC_MODE1_INTFLAG Mask) PER */
#define RTC_MODE1_INTFLAG_PER(value)          (RTC_MODE1_INTFLAG_PER_Msk & (_UINT16_(value) << RTC_MODE1_INTFLAG_PER_Pos)) 
#define RTC_MODE1_INTFLAG_CMP_Pos             _UINT16_(8)                                          /* (RTC_MODE1_INTFLAG Position) Compare x */
#define RTC_MODE1_INTFLAG_CMP_Msk             (_UINT16_(0x3) << RTC_MODE1_INTFLAG_CMP_Pos)         /* (RTC_MODE1_INTFLAG Mask) CMP */
#define RTC_MODE1_INTFLAG_CMP(value)          (RTC_MODE1_INTFLAG_CMP_Msk & (_UINT16_(value) << RTC_MODE1_INTFLAG_CMP_Pos)) 

/* -------- RTC_MODE2_INTFLAG : (RTC Offset: 0x0C) (R/W 16) MODE2 Interrupt Flag Status and Clear -------- */
#define RTC_MODE2_INTFLAG_RESETVALUE          _UINT16_(0x00)                                       /*  (RTC_MODE2_INTFLAG) MODE2 Interrupt Flag Status and Clear  Reset Value */

#define RTC_MODE2_INTFLAG_PER0_Pos            _UINT16_(0)                                          /* (RTC_MODE2_INTFLAG) Periodic Interval 0 Position */
#define RTC_MODE2_INTFLAG_PER0_Msk            (_UINT16_(0x1) << RTC_MODE2_INTFLAG_PER0_Pos)        /* (RTC_MODE2_INTFLAG) Periodic Interval 0 Mask */
#define RTC_MODE2_INTFLAG_PER0(value)         (RTC_MODE2_INTFLAG_PER0_Msk & (_UINT16_(value) << RTC_MODE2_INTFLAG_PER0_Pos)) /* Assigment of value for PER0 in the RTC_MODE2_INTFLAG register */
#define RTC_MODE2_INTFLAG_PER1_Pos            _UINT16_(1)                                          /* (RTC_MODE2_INTFLAG) Periodic Interval 1 Position */
#define RTC_MODE2_INTFLAG_PER1_Msk            (_UINT16_(0x1) << RTC_MODE2_INTFLAG_PER1_Pos)        /* (RTC_MODE2_INTFLAG) Periodic Interval 1 Mask */
#define RTC_MODE2_INTFLAG_PER1(value)         (RTC_MODE2_INTFLAG_PER1_Msk & (_UINT16_(value) << RTC_MODE2_INTFLAG_PER1_Pos)) /* Assigment of value for PER1 in the RTC_MODE2_INTFLAG register */
#define RTC_MODE2_INTFLAG_PER2_Pos            _UINT16_(2)                                          /* (RTC_MODE2_INTFLAG) Periodic Interval 2 Position */
#define RTC_MODE2_INTFLAG_PER2_Msk            (_UINT16_(0x1) << RTC_MODE2_INTFLAG_PER2_Pos)        /* (RTC_MODE2_INTFLAG) Periodic Interval 2 Mask */
#define RTC_MODE2_INTFLAG_PER2(value)         (RTC_MODE2_INTFLAG_PER2_Msk & (_UINT16_(value) << RTC_MODE2_INTFLAG_PER2_Pos)) /* Assigment of value for PER2 in the RTC_MODE2_INTFLAG register */
#define RTC_MODE2_INTFLAG_PER3_Pos            _UINT16_(3)                                          /* (RTC_MODE2_INTFLAG) Periodic Interval 3 Position */
#define RTC_MODE2_INTFLAG_PER3_Msk            (_UINT16_(0x1) << RTC_MODE2_INTFLAG_PER3_Pos)        /* (RTC_MODE2_INTFLAG) Periodic Interval 3 Mask */
#define RTC_MODE2_INTFLAG_PER3(value)         (RTC_MODE2_INTFLAG_PER3_Msk & (_UINT16_(value) << RTC_MODE2_INTFLAG_PER3_Pos)) /* Assigment of value for PER3 in the RTC_MODE2_INTFLAG register */
#define RTC_MODE2_INTFLAG_PER4_Pos            _UINT16_(4)                                          /* (RTC_MODE2_INTFLAG) Periodic Interval 4 Position */
#define RTC_MODE2_INTFLAG_PER4_Msk            (_UINT16_(0x1) << RTC_MODE2_INTFLAG_PER4_Pos)        /* (RTC_MODE2_INTFLAG) Periodic Interval 4 Mask */
#define RTC_MODE2_INTFLAG_PER4(value)         (RTC_MODE2_INTFLAG_PER4_Msk & (_UINT16_(value) << RTC_MODE2_INTFLAG_PER4_Pos)) /* Assigment of value for PER4 in the RTC_MODE2_INTFLAG register */
#define RTC_MODE2_INTFLAG_PER5_Pos            _UINT16_(5)                                          /* (RTC_MODE2_INTFLAG) Periodic Interval 5 Position */
#define RTC_MODE2_INTFLAG_PER5_Msk            (_UINT16_(0x1) << RTC_MODE2_INTFLAG_PER5_Pos)        /* (RTC_MODE2_INTFLAG) Periodic Interval 5 Mask */
#define RTC_MODE2_INTFLAG_PER5(value)         (RTC_MODE2_INTFLAG_PER5_Msk & (_UINT16_(value) << RTC_MODE2_INTFLAG_PER5_Pos)) /* Assigment of value for PER5 in the RTC_MODE2_INTFLAG register */
#define RTC_MODE2_INTFLAG_PER6_Pos            _UINT16_(6)                                          /* (RTC_MODE2_INTFLAG) Periodic Interval 6 Position */
#define RTC_MODE2_INTFLAG_PER6_Msk            (_UINT16_(0x1) << RTC_MODE2_INTFLAG_PER6_Pos)        /* (RTC_MODE2_INTFLAG) Periodic Interval 6 Mask */
#define RTC_MODE2_INTFLAG_PER6(value)         (RTC_MODE2_INTFLAG_PER6_Msk & (_UINT16_(value) << RTC_MODE2_INTFLAG_PER6_Pos)) /* Assigment of value for PER6 in the RTC_MODE2_INTFLAG register */
#define RTC_MODE2_INTFLAG_PER7_Pos            _UINT16_(7)                                          /* (RTC_MODE2_INTFLAG) Periodic Interval 7 Position */
#define RTC_MODE2_INTFLAG_PER7_Msk            (_UINT16_(0x1) << RTC_MODE2_INTFLAG_PER7_Pos)        /* (RTC_MODE2_INTFLAG) Periodic Interval 7 Mask */
#define RTC_MODE2_INTFLAG_PER7(value)         (RTC_MODE2_INTFLAG_PER7_Msk & (_UINT16_(value) << RTC_MODE2_INTFLAG_PER7_Pos)) /* Assigment of value for PER7 in the RTC_MODE2_INTFLAG register */
#define RTC_MODE2_INTFLAG_ALARM0_Pos          _UINT16_(8)                                          /* (RTC_MODE2_INTFLAG) Alarm 0 Position */
#define RTC_MODE2_INTFLAG_ALARM0_Msk          (_UINT16_(0x1) << RTC_MODE2_INTFLAG_ALARM0_Pos)      /* (RTC_MODE2_INTFLAG) Alarm 0 Mask */
#define RTC_MODE2_INTFLAG_ALARM0(value)       (RTC_MODE2_INTFLAG_ALARM0_Msk & (_UINT16_(value) << RTC_MODE2_INTFLAG_ALARM0_Pos)) /* Assigment of value for ALARM0 in the RTC_MODE2_INTFLAG register */
#define RTC_MODE2_INTFLAG_OVF_Pos             _UINT16_(15)                                         /* (RTC_MODE2_INTFLAG) Overflow Position */
#define RTC_MODE2_INTFLAG_OVF_Msk             (_UINT16_(0x1) << RTC_MODE2_INTFLAG_OVF_Pos)         /* (RTC_MODE2_INTFLAG) Overflow Mask */
#define RTC_MODE2_INTFLAG_OVF(value)          (RTC_MODE2_INTFLAG_OVF_Msk & (_UINT16_(value) << RTC_MODE2_INTFLAG_OVF_Pos)) /* Assigment of value for OVF in the RTC_MODE2_INTFLAG register */
#define RTC_MODE2_INTFLAG_Msk                 _UINT16_(0x81FF)                                     /* (RTC_MODE2_INTFLAG) Register Mask  */

#define RTC_MODE2_INTFLAG_PER_Pos             _UINT16_(0)                                          /* (RTC_MODE2_INTFLAG Position) Periodic Interval x */
#define RTC_MODE2_INTFLAG_PER_Msk             (_UINT16_(0xFF) << RTC_MODE2_INTFLAG_PER_Pos)        /* (RTC_MODE2_INTFLAG Mask) PER */
#define RTC_MODE2_INTFLAG_PER(value)          (RTC_MODE2_INTFLAG_PER_Msk & (_UINT16_(value) << RTC_MODE2_INTFLAG_PER_Pos)) 
#define RTC_MODE2_INTFLAG_ALARM_Pos           _UINT16_(8)                                          /* (RTC_MODE2_INTFLAG Position) Alarm x */
#define RTC_MODE2_INTFLAG_ALARM_Msk           (_UINT16_(0x1) << RTC_MODE2_INTFLAG_ALARM_Pos)       /* (RTC_MODE2_INTFLAG Mask) ALARM */
#define RTC_MODE2_INTFLAG_ALARM(value)        (RTC_MODE2_INTFLAG_ALARM_Msk & (_UINT16_(value) << RTC_MODE2_INTFLAG_ALARM_Pos)) 

/* -------- RTC_DBGCTRL : (RTC Offset: 0x0E) (R/W 8) Debug Control -------- */
#define RTC_DBGCTRL_RESETVALUE                _UINT8_(0x00)                                        /*  (RTC_DBGCTRL) Debug Control  Reset Value */

#define RTC_DBGCTRL_DBGRUN_Pos                _UINT8_(0)                                           /* (RTC_DBGCTRL) Run During Debug Position */
#define RTC_DBGCTRL_DBGRUN_Msk                (_UINT8_(0x1) << RTC_DBGCTRL_DBGRUN_Pos)             /* (RTC_DBGCTRL) Run During Debug Mask */
#define RTC_DBGCTRL_DBGRUN(value)             (RTC_DBGCTRL_DBGRUN_Msk & (_UINT8_(value) << RTC_DBGCTRL_DBGRUN_Pos)) /* Assigment of value for DBGRUN in the RTC_DBGCTRL register */
#define RTC_DBGCTRL_Msk                       _UINT8_(0x01)                                        /* (RTC_DBGCTRL) Register Mask  */


/* -------- RTC_MODE0_SYNCBUSY : (RTC Offset: 0x10) ( R/ 32) MODE0 Synchronization Busy Status -------- */
#define RTC_MODE0_SYNCBUSY_RESETVALUE         _UINT32_(0x00)                                       /*  (RTC_MODE0_SYNCBUSY) MODE0 Synchronization Busy Status  Reset Value */

#define RTC_MODE0_SYNCBUSY_SWRST_Pos          _UINT32_(0)                                          /* (RTC_MODE0_SYNCBUSY) Software Reset Busy Position */
#define RTC_MODE0_SYNCBUSY_SWRST_Msk          (_UINT32_(0x1) << RTC_MODE0_SYNCBUSY_SWRST_Pos)      /* (RTC_MODE0_SYNCBUSY) Software Reset Busy Mask */
#define RTC_MODE0_SYNCBUSY_SWRST(value)       (RTC_MODE0_SYNCBUSY_SWRST_Msk & (_UINT32_(value) << RTC_MODE0_SYNCBUSY_SWRST_Pos)) /* Assigment of value for SWRST in the RTC_MODE0_SYNCBUSY register */
#define RTC_MODE0_SYNCBUSY_ENABLE_Pos         _UINT32_(1)                                          /* (RTC_MODE0_SYNCBUSY) Enable Bit Busy Position */
#define RTC_MODE0_SYNCBUSY_ENABLE_Msk         (_UINT32_(0x1) << RTC_MODE0_SYNCBUSY_ENABLE_Pos)     /* (RTC_MODE0_SYNCBUSY) Enable Bit Busy Mask */
#define RTC_MODE0_SYNCBUSY_ENABLE(value)      (RTC_MODE0_SYNCBUSY_ENABLE_Msk & (_UINT32_(value) << RTC_MODE0_SYNCBUSY_ENABLE_Pos)) /* Assigment of value for ENABLE in the RTC_MODE0_SYNCBUSY register */
#define RTC_MODE0_SYNCBUSY_FREQCORR_Pos       _UINT32_(2)                                          /* (RTC_MODE0_SYNCBUSY) FREQCORR Register Busy Position */
#define RTC_MODE0_SYNCBUSY_FREQCORR_Msk       (_UINT32_(0x1) << RTC_MODE0_SYNCBUSY_FREQCORR_Pos)   /* (RTC_MODE0_SYNCBUSY) FREQCORR Register Busy Mask */
#define RTC_MODE0_SYNCBUSY_FREQCORR(value)    (RTC_MODE0_SYNCBUSY_FREQCORR_Msk & (_UINT32_(value) << RTC_MODE0_SYNCBUSY_FREQCORR_Pos)) /* Assigment of value for FREQCORR in the RTC_MODE0_SYNCBUSY register */
#define RTC_MODE0_SYNCBUSY_COUNT_Pos          _UINT32_(3)                                          /* (RTC_MODE0_SYNCBUSY) COUNT Register Busy Position */
#define RTC_MODE0_SYNCBUSY_COUNT_Msk          (_UINT32_(0x1) << RTC_MODE0_SYNCBUSY_COUNT_Pos)      /* (RTC_MODE0_SYNCBUSY) COUNT Register Busy Mask */
#define RTC_MODE0_SYNCBUSY_COUNT(value)       (RTC_MODE0_SYNCBUSY_COUNT_Msk & (_UINT32_(value) << RTC_MODE0_SYNCBUSY_COUNT_Pos)) /* Assigment of value for COUNT in the RTC_MODE0_SYNCBUSY register */
#define RTC_MODE0_SYNCBUSY_COMP0_Pos          _UINT32_(5)                                          /* (RTC_MODE0_SYNCBUSY) COMP 0 Register Busy Position */
#define RTC_MODE0_SYNCBUSY_COMP0_Msk          (_UINT32_(0x1) << RTC_MODE0_SYNCBUSY_COMP0_Pos)      /* (RTC_MODE0_SYNCBUSY) COMP 0 Register Busy Mask */
#define RTC_MODE0_SYNCBUSY_COMP0(value)       (RTC_MODE0_SYNCBUSY_COMP0_Msk & (_UINT32_(value) << RTC_MODE0_SYNCBUSY_COMP0_Pos)) /* Assigment of value for COMP0 in the RTC_MODE0_SYNCBUSY register */
#define RTC_MODE0_SYNCBUSY_COUNTSYNC_Pos      _UINT32_(15)                                         /* (RTC_MODE0_SYNCBUSY) Count Read Synchronization Enable Bit Busy Position */
#define RTC_MODE0_SYNCBUSY_COUNTSYNC_Msk      (_UINT32_(0x1) << RTC_MODE0_SYNCBUSY_COUNTSYNC_Pos)  /* (RTC_MODE0_SYNCBUSY) Count Read Synchronization Enable Bit Busy Mask */
#define RTC_MODE0_SYNCBUSY_COUNTSYNC(value)   (RTC_MODE0_SYNCBUSY_COUNTSYNC_Msk & (_UINT32_(value) << RTC_MODE0_SYNCBUSY_COUNTSYNC_Pos)) /* Assigment of value for COUNTSYNC in the RTC_MODE0_SYNCBUSY register */
#define RTC_MODE0_SYNCBUSY_Msk                _UINT32_(0x0000802F)                                 /* (RTC_MODE0_SYNCBUSY) Register Mask  */

#define RTC_MODE0_SYNCBUSY_COMP_Pos           _UINT32_(5)                                          /* (RTC_MODE0_SYNCBUSY Position) COMP x Register Busy */
#define RTC_MODE0_SYNCBUSY_COMP_Msk           (_UINT32_(0x1) << RTC_MODE0_SYNCBUSY_COMP_Pos)       /* (RTC_MODE0_SYNCBUSY Mask) COMP */
#define RTC_MODE0_SYNCBUSY_COMP(value)        (RTC_MODE0_SYNCBUSY_COMP_Msk & (_UINT32_(value) << RTC_MODE0_SYNCBUSY_COMP_Pos)) 

/* -------- RTC_MODE1_SYNCBUSY : (RTC Offset: 0x10) ( R/ 32) MODE1 Synchronization Busy Status -------- */
#define RTC_MODE1_SYNCBUSY_RESETVALUE         _UINT32_(0x00)                                       /*  (RTC_MODE1_SYNCBUSY) MODE1 Synchronization Busy Status  Reset Value */

#define RTC_MODE1_SYNCBUSY_SWRST_Pos          _UINT32_(0)                                          /* (RTC_MODE1_SYNCBUSY) Software Reset Bit Busy Position */
#define RTC_MODE1_SYNCBUSY_SWRST_Msk          (_UINT32_(0x1) << RTC_MODE1_SYNCBUSY_SWRST_Pos)      /* (RTC_MODE1_SYNCBUSY) Software Reset Bit Busy Mask */
#define RTC_MODE1_SYNCBUSY_SWRST(value)       (RTC_MODE1_SYNCBUSY_SWRST_Msk & (_UINT32_(value) << RTC_MODE1_SYNCBUSY_SWRST_Pos)) /* Assigment of value for SWRST in the RTC_MODE1_SYNCBUSY register */
#define RTC_MODE1_SYNCBUSY_ENABLE_Pos         _UINT32_(1)                                          /* (RTC_MODE1_SYNCBUSY) Enable Bit Busy Position */
#define RTC_MODE1_SYNCBUSY_ENABLE_Msk         (_UINT32_(0x1) << RTC_MODE1_SYNCBUSY_ENABLE_Pos)     /* (RTC_MODE1_SYNCBUSY) Enable Bit Busy Mask */
#define RTC_MODE1_SYNCBUSY_ENABLE(value)      (RTC_MODE1_SYNCBUSY_ENABLE_Msk & (_UINT32_(value) << RTC_MODE1_SYNCBUSY_ENABLE_Pos)) /* Assigment of value for ENABLE in the RTC_MODE1_SYNCBUSY register */
#define RTC_MODE1_SYNCBUSY_FREQCORR_Pos       _UINT32_(2)                                          /* (RTC_MODE1_SYNCBUSY) FREQCORR Register Busy Position */
#define RTC_MODE1_SYNCBUSY_FREQCORR_Msk       (_UINT32_(0x1) << RTC_MODE1_SYNCBUSY_FREQCORR_Pos)   /* (RTC_MODE1_SYNCBUSY) FREQCORR Register Busy Mask */
#define RTC_MODE1_SYNCBUSY_FREQCORR(value)    (RTC_MODE1_SYNCBUSY_FREQCORR_Msk & (_UINT32_(value) << RTC_MODE1_SYNCBUSY_FREQCORR_Pos)) /* Assigment of value for FREQCORR in the RTC_MODE1_SYNCBUSY register */
#define RTC_MODE1_SYNCBUSY_COUNT_Pos          _UINT32_(3)                                          /* (RTC_MODE1_SYNCBUSY) COUNT Register Busy Position */
#define RTC_MODE1_SYNCBUSY_COUNT_Msk          (_UINT32_(0x1) << RTC_MODE1_SYNCBUSY_COUNT_Pos)      /* (RTC_MODE1_SYNCBUSY) COUNT Register Busy Mask */
#define RTC_MODE1_SYNCBUSY_COUNT(value)       (RTC_MODE1_SYNCBUSY_COUNT_Msk & (_UINT32_(value) << RTC_MODE1_SYNCBUSY_COUNT_Pos)) /* Assigment of value for COUNT in the RTC_MODE1_SYNCBUSY register */
#define RTC_MODE1_SYNCBUSY_PER_Pos            _UINT32_(4)                                          /* (RTC_MODE1_SYNCBUSY) PER Register Busy Position */
#define RTC_MODE1_SYNCBUSY_PER_Msk            (_UINT32_(0x1) << RTC_MODE1_SYNCBUSY_PER_Pos)        /* (RTC_MODE1_SYNCBUSY) PER Register Busy Mask */
#define RTC_MODE1_SYNCBUSY_PER(value)         (RTC_MODE1_SYNCBUSY_PER_Msk & (_UINT32_(value) << RTC_MODE1_SYNCBUSY_PER_Pos)) /* Assigment of value for PER in the RTC_MODE1_SYNCBUSY register */
#define RTC_MODE1_SYNCBUSY_COMP0_Pos          _UINT32_(5)                                          /* (RTC_MODE1_SYNCBUSY) COMP 0 Register Busy Position */
#define RTC_MODE1_SYNCBUSY_COMP0_Msk          (_UINT32_(0x1) << RTC_MODE1_SYNCBUSY_COMP0_Pos)      /* (RTC_MODE1_SYNCBUSY) COMP 0 Register Busy Mask */
#define RTC_MODE1_SYNCBUSY_COMP0(value)       (RTC_MODE1_SYNCBUSY_COMP0_Msk & (_UINT32_(value) << RTC_MODE1_SYNCBUSY_COMP0_Pos)) /* Assigment of value for COMP0 in the RTC_MODE1_SYNCBUSY register */
#define RTC_MODE1_SYNCBUSY_COMP1_Pos          _UINT32_(6)                                          /* (RTC_MODE1_SYNCBUSY) COMP 1 Register Busy Position */
#define RTC_MODE1_SYNCBUSY_COMP1_Msk          (_UINT32_(0x1) << RTC_MODE1_SYNCBUSY_COMP1_Pos)      /* (RTC_MODE1_SYNCBUSY) COMP 1 Register Busy Mask */
#define RTC_MODE1_SYNCBUSY_COMP1(value)       (RTC_MODE1_SYNCBUSY_COMP1_Msk & (_UINT32_(value) << RTC_MODE1_SYNCBUSY_COMP1_Pos)) /* Assigment of value for COMP1 in the RTC_MODE1_SYNCBUSY register */
#define RTC_MODE1_SYNCBUSY_COUNTSYNC_Pos      _UINT32_(15)                                         /* (RTC_MODE1_SYNCBUSY) Count Read Synchronization Enable Bit Busy Position */
#define RTC_MODE1_SYNCBUSY_COUNTSYNC_Msk      (_UINT32_(0x1) << RTC_MODE1_SYNCBUSY_COUNTSYNC_Pos)  /* (RTC_MODE1_SYNCBUSY) Count Read Synchronization Enable Bit Busy Mask */
#define RTC_MODE1_SYNCBUSY_COUNTSYNC(value)   (RTC_MODE1_SYNCBUSY_COUNTSYNC_Msk & (_UINT32_(value) << RTC_MODE1_SYNCBUSY_COUNTSYNC_Pos)) /* Assigment of value for COUNTSYNC in the RTC_MODE1_SYNCBUSY register */
#define RTC_MODE1_SYNCBUSY_Msk                _UINT32_(0x0000807F)                                 /* (RTC_MODE1_SYNCBUSY) Register Mask  */

#define RTC_MODE1_SYNCBUSY_COMP_Pos           _UINT32_(5)                                          /* (RTC_MODE1_SYNCBUSY Position) COMP x Register Busy */
#define RTC_MODE1_SYNCBUSY_COMP_Msk           (_UINT32_(0x3) << RTC_MODE1_SYNCBUSY_COMP_Pos)       /* (RTC_MODE1_SYNCBUSY Mask) COMP */
#define RTC_MODE1_SYNCBUSY_COMP(value)        (RTC_MODE1_SYNCBUSY_COMP_Msk & (_UINT32_(value) << RTC_MODE1_SYNCBUSY_COMP_Pos)) 

/* -------- RTC_MODE2_SYNCBUSY : (RTC Offset: 0x10) ( R/ 32) MODE2 Synchronization Busy Status -------- */
#define RTC_MODE2_SYNCBUSY_RESETVALUE         _UINT32_(0x00)                                       /*  (RTC_MODE2_SYNCBUSY) MODE2 Synchronization Busy Status  Reset Value */

#define RTC_MODE2_SYNCBUSY_SWRST_Pos          _UINT32_(0)                                          /* (RTC_MODE2_SYNCBUSY) Software Reset Bit Busy Position */
#define RTC_MODE2_SYNCBUSY_SWRST_Msk          (_UINT32_(0x1) << RTC_MODE2_SYNCBUSY_SWRST_Pos)      /* (RTC_MODE2_SYNCBUSY) Software Reset Bit Busy Mask */
#define RTC_MODE2_SYNCBUSY_SWRST(value)       (RTC_MODE2_SYNCBUSY_SWRST_Msk & (_UINT32_(value) << RTC_MODE2_SYNCBUSY_SWRST_Pos)) /* Assigment of value for SWRST in the RTC_MODE2_SYNCBUSY register */
#define RTC_MODE2_SYNCBUSY_ENABLE_Pos         _UINT32_(1)                                          /* (RTC_MODE2_SYNCBUSY) Enable Bit Busy Position */
#define RTC_MODE2_SYNCBUSY_ENABLE_Msk         (_UINT32_(0x1) << RTC_MODE2_SYNCBUSY_ENABLE_Pos)     /* (RTC_MODE2_SYNCBUSY) Enable Bit Busy Mask */
#define RTC_MODE2_SYNCBUSY_ENABLE(value)      (RTC_MODE2_SYNCBUSY_ENABLE_Msk & (_UINT32_(value) << RTC_MODE2_SYNCBUSY_ENABLE_Pos)) /* Assigment of value for ENABLE in the RTC_MODE2_SYNCBUSY register */
#define RTC_MODE2_SYNCBUSY_FREQCORR_Pos       _UINT32_(2)                                          /* (RTC_MODE2_SYNCBUSY) FREQCORR Register Busy Position */
#define RTC_MODE2_SYNCBUSY_FREQCORR_Msk       (_UINT32_(0x1) << RTC_MODE2_SYNCBUSY_FREQCORR_Pos)   /* (RTC_MODE2_SYNCBUSY) FREQCORR Register Busy Mask */
#define RTC_MODE2_SYNCBUSY_FREQCORR(value)    (RTC_MODE2_SYNCBUSY_FREQCORR_Msk & (_UINT32_(value) << RTC_MODE2_SYNCBUSY_FREQCORR_Pos)) /* Assigment of value for FREQCORR in the RTC_MODE2_SYNCBUSY register */
#define RTC_MODE2_SYNCBUSY_CLOCK_Pos          _UINT32_(3)                                          /* (RTC_MODE2_SYNCBUSY) CLOCK Register Busy Position */
#define RTC_MODE2_SYNCBUSY_CLOCK_Msk          (_UINT32_(0x1) << RTC_MODE2_SYNCBUSY_CLOCK_Pos)      /* (RTC_MODE2_SYNCBUSY) CLOCK Register Busy Mask */
#define RTC_MODE2_SYNCBUSY_CLOCK(value)       (RTC_MODE2_SYNCBUSY_CLOCK_Msk & (_UINT32_(value) << RTC_MODE2_SYNCBUSY_CLOCK_Pos)) /* Assigment of value for CLOCK in the RTC_MODE2_SYNCBUSY register */
#define RTC_MODE2_SYNCBUSY_ALARM0_Pos         _UINT32_(5)                                          /* (RTC_MODE2_SYNCBUSY) ALARM 0 Register Busy Position */
#define RTC_MODE2_SYNCBUSY_ALARM0_Msk         (_UINT32_(0x1) << RTC_MODE2_SYNCBUSY_ALARM0_Pos)     /* (RTC_MODE2_SYNCBUSY) ALARM 0 Register Busy Mask */
#define RTC_MODE2_SYNCBUSY_ALARM0(value)      (RTC_MODE2_SYNCBUSY_ALARM0_Msk & (_UINT32_(value) << RTC_MODE2_SYNCBUSY_ALARM0_Pos)) /* Assigment of value for ALARM0 in the RTC_MODE2_SYNCBUSY register */
#define RTC_MODE2_SYNCBUSY_MASK0_Pos          _UINT32_(11)                                         /* (RTC_MODE2_SYNCBUSY) MASK 0 Register Busy Position */
#define RTC_MODE2_SYNCBUSY_MASK0_Msk          (_UINT32_(0x1) << RTC_MODE2_SYNCBUSY_MASK0_Pos)      /* (RTC_MODE2_SYNCBUSY) MASK 0 Register Busy Mask */
#define RTC_MODE2_SYNCBUSY_MASK0(value)       (RTC_MODE2_SYNCBUSY_MASK0_Msk & (_UINT32_(value) << RTC_MODE2_SYNCBUSY_MASK0_Pos)) /* Assigment of value for MASK0 in the RTC_MODE2_SYNCBUSY register */
#define RTC_MODE2_SYNCBUSY_CLOCKSYNC_Pos      _UINT32_(15)                                         /* (RTC_MODE2_SYNCBUSY) Clock Read Synchronization Enable Bit Busy Position */
#define RTC_MODE2_SYNCBUSY_CLOCKSYNC_Msk      (_UINT32_(0x1) << RTC_MODE2_SYNCBUSY_CLOCKSYNC_Pos)  /* (RTC_MODE2_SYNCBUSY) Clock Read Synchronization Enable Bit Busy Mask */
#define RTC_MODE2_SYNCBUSY_CLOCKSYNC(value)   (RTC_MODE2_SYNCBUSY_CLOCKSYNC_Msk & (_UINT32_(value) << RTC_MODE2_SYNCBUSY_CLOCKSYNC_Pos)) /* Assigment of value for CLOCKSYNC in the RTC_MODE2_SYNCBUSY register */
#define RTC_MODE2_SYNCBUSY_Msk                _UINT32_(0x0000882F)                                 /* (RTC_MODE2_SYNCBUSY) Register Mask  */

#define RTC_MODE2_SYNCBUSY_ALARM_Pos          _UINT32_(5)                                          /* (RTC_MODE2_SYNCBUSY Position) ALARM x Register Busy */
#define RTC_MODE2_SYNCBUSY_ALARM_Msk          (_UINT32_(0x1) << RTC_MODE2_SYNCBUSY_ALARM_Pos)      /* (RTC_MODE2_SYNCBUSY Mask) ALARM */
#define RTC_MODE2_SYNCBUSY_ALARM(value)       (RTC_MODE2_SYNCBUSY_ALARM_Msk & (_UINT32_(value) << RTC_MODE2_SYNCBUSY_ALARM_Pos)) 
#define RTC_MODE2_SYNCBUSY_MASK_Pos           _UINT32_(11)                                         /* (RTC_MODE2_SYNCBUSY Position) MASK x Register Busy */
#define RTC_MODE2_SYNCBUSY_MASK_Msk           (_UINT32_(0x1) << RTC_MODE2_SYNCBUSY_MASK_Pos)       /* (RTC_MODE2_SYNCBUSY Mask) MASK */
#define RTC_MODE2_SYNCBUSY_MASK(value)        (RTC_MODE2_SYNCBUSY_MASK_Msk & (_UINT32_(value) << RTC_MODE2_SYNCBUSY_MASK_Pos)) 

/* -------- RTC_FREQCORR : (RTC Offset: 0x14) (R/W 8) Frequency Correction -------- */
#define RTC_FREQCORR_RESETVALUE               _UINT8_(0x00)                                        /*  (RTC_FREQCORR) Frequency Correction  Reset Value */

#define RTC_FREQCORR_VALUE_Pos                _UINT8_(0)                                           /* (RTC_FREQCORR) Correction Value Position */
#define RTC_FREQCORR_VALUE_Msk                (_UINT8_(0x7F) << RTC_FREQCORR_VALUE_Pos)            /* (RTC_FREQCORR) Correction Value Mask */
#define RTC_FREQCORR_VALUE(value)             (RTC_FREQCORR_VALUE_Msk & (_UINT8_(value) << RTC_FREQCORR_VALUE_Pos)) /* Assigment of value for VALUE in the RTC_FREQCORR register */
#define RTC_FREQCORR_SIGN_Pos                 _UINT8_(7)                                           /* (RTC_FREQCORR) Correction Sign Position */
#define RTC_FREQCORR_SIGN_Msk                 (_UINT8_(0x1) << RTC_FREQCORR_SIGN_Pos)              /* (RTC_FREQCORR) Correction Sign Mask */
#define RTC_FREQCORR_SIGN(value)              (RTC_FREQCORR_SIGN_Msk & (_UINT8_(value) << RTC_FREQCORR_SIGN_Pos)) /* Assigment of value for SIGN in the RTC_FREQCORR register */
#define RTC_FREQCORR_Msk                      _UINT8_(0xFF)                                        /* (RTC_FREQCORR) Register Mask  */


/* -------- RTC_MODE0_COUNT : (RTC Offset: 0x18) (R/W 32) MODE0 Counter Value -------- */
#define RTC_MODE0_COUNT_RESETVALUE            _UINT32_(0x00)                                       /*  (RTC_MODE0_COUNT) MODE0 Counter Value  Reset Value */

#define RTC_MODE0_COUNT_COUNT_Pos             _UINT32_(0)                                          /* (RTC_MODE0_COUNT) Counter Value Position */
#define RTC_MODE0_COUNT_COUNT_Msk             (_UINT32_(0xFFFFFFFF) << RTC_MODE0_COUNT_COUNT_Pos)  /* (RTC_MODE0_COUNT) Counter Value Mask */
#define RTC_MODE0_COUNT_COUNT(value)          (RTC_MODE0_COUNT_COUNT_Msk & (_UINT32_(value) << RTC_MODE0_COUNT_COUNT_Pos)) /* Assigment of value for COUNT in the RTC_MODE0_COUNT register */
#define RTC_MODE0_COUNT_Msk                   _UINT32_(0xFFFFFFFF)                                 /* (RTC_MODE0_COUNT) Register Mask  */


/* -------- RTC_MODE1_COUNT : (RTC Offset: 0x18) (R/W 16) MODE1 Counter Value -------- */
#define RTC_MODE1_COUNT_RESETVALUE            _UINT16_(0x00)                                       /*  (RTC_MODE1_COUNT) MODE1 Counter Value  Reset Value */

#define RTC_MODE1_COUNT_COUNT_Pos             _UINT16_(0)                                          /* (RTC_MODE1_COUNT) Counter Value Position */
#define RTC_MODE1_COUNT_COUNT_Msk             (_UINT16_(0xFFFF) << RTC_MODE1_COUNT_COUNT_Pos)      /* (RTC_MODE1_COUNT) Counter Value Mask */
#define RTC_MODE1_COUNT_COUNT(value)          (RTC_MODE1_COUNT_COUNT_Msk & (_UINT16_(value) << RTC_MODE1_COUNT_COUNT_Pos)) /* Assigment of value for COUNT in the RTC_MODE1_COUNT register */
#define RTC_MODE1_COUNT_Msk                   _UINT16_(0xFFFF)                                     /* (RTC_MODE1_COUNT) Register Mask  */


/* -------- RTC_MODE2_CLOCK : (RTC Offset: 0x18) (R/W 32) MODE2 Clock Value -------- */
#define RTC_MODE2_CLOCK_RESETVALUE            _UINT32_(0x00)                                       /*  (RTC_MODE2_CLOCK) MODE2 Clock Value  Reset Value */

#define RTC_MODE2_CLOCK_SECOND_Pos            _UINT32_(0)                                          /* (RTC_MODE2_CLOCK) Second Position */
#define RTC_MODE2_CLOCK_SECOND_Msk            (_UINT32_(0x3F) << RTC_MODE2_CLOCK_SECOND_Pos)       /* (RTC_MODE2_CLOCK) Second Mask */
#define RTC_MODE2_CLOCK_SECOND(value)         (RTC_MODE2_CLOCK_SECOND_Msk & (_UINT32_(value) << RTC_MODE2_CLOCK_SECOND_Pos)) /* Assigment of value for SECOND in the RTC_MODE2_CLOCK register */
#define RTC_MODE2_CLOCK_MINUTE_Pos            _UINT32_(6)                                          /* (RTC_MODE2_CLOCK) Minute Position */
#define RTC_MODE2_CLOCK_MINUTE_Msk            (_UINT32_(0x3F) << RTC_MODE2_CLOCK_MINUTE_Pos)       /* (RTC_MODE2_CLOCK) Minute Mask */
#define RTC_MODE2_CLOCK_MINUTE(value)         (RTC_MODE2_CLOCK_MINUTE_Msk & (_UINT32_(value) << RTC_MODE2_CLOCK_MINUTE_Pos)) /* Assigment of value for MINUTE in the RTC_MODE2_CLOCK register */
#define RTC_MODE2_CLOCK_HOUR_Pos              _UINT32_(12)                                         /* (RTC_MODE2_CLOCK) Hour Position */
#define RTC_MODE2_CLOCK_HOUR_Msk              (_UINT32_(0x1F) << RTC_MODE2_CLOCK_HOUR_Pos)         /* (RTC_MODE2_CLOCK) Hour Mask */
#define RTC_MODE2_CLOCK_HOUR(value)           (RTC_MODE2_CLOCK_HOUR_Msk & (_UINT32_(value) << RTC_MODE2_CLOCK_HOUR_Pos)) /* Assigment of value for HOUR in the RTC_MODE2_CLOCK register */
#define   RTC_MODE2_CLOCK_HOUR_AM_Val         _UINT32_(0x0)                                        /* (RTC_MODE2_CLOCK) AM when CLKREP in 12-hour  */
#define   RTC_MODE2_CLOCK_HOUR_PM_Val         _UINT32_(0x10)                                       /* (RTC_MODE2_CLOCK) PM when CLKREP in 12-hour  */
#define RTC_MODE2_CLOCK_HOUR_AM               (RTC_MODE2_CLOCK_HOUR_AM_Val << RTC_MODE2_CLOCK_HOUR_Pos) /* (RTC_MODE2_CLOCK) AM when CLKREP in 12-hour Position  */
#define RTC_MODE2_CLOCK_HOUR_PM               (RTC_MODE2_CLOCK_HOUR_PM_Val << RTC_MODE2_CLOCK_HOUR_Pos) /* (RTC_MODE2_CLOCK) PM when CLKREP in 12-hour Position  */
#define RTC_MODE2_CLOCK_DAY_Pos               _UINT32_(17)                                         /* (RTC_MODE2_CLOCK) Day Position */
#define RTC_MODE2_CLOCK_DAY_Msk               (_UINT32_(0x1F) << RTC_MODE2_CLOCK_DAY_Pos)          /* (RTC_MODE2_CLOCK) Day Mask */
#define RTC_MODE2_CLOCK_DAY(value)            (RTC_MODE2_CLOCK_DAY_Msk & (_UINT32_(value) << RTC_MODE2_CLOCK_DAY_Pos)) /* Assigment of value for DAY in the RTC_MODE2_CLOCK register */
#define RTC_MODE2_CLOCK_MONTH_Pos             _UINT32_(22)                                         /* (RTC_MODE2_CLOCK) Month Position */
#define RTC_MODE2_CLOCK_MONTH_Msk             (_UINT32_(0xF) << RTC_MODE2_CLOCK_MONTH_Pos)         /* (RTC_MODE2_CLOCK) Month Mask */
#define RTC_MODE2_CLOCK_MONTH(value)          (RTC_MODE2_CLOCK_MONTH_Msk & (_UINT32_(value) << RTC_MODE2_CLOCK_MONTH_Pos)) /* Assigment of value for MONTH in the RTC_MODE2_CLOCK register */
#define RTC_MODE2_CLOCK_YEAR_Pos              _UINT32_(26)                                         /* (RTC_MODE2_CLOCK) Year Position */
#define RTC_MODE2_CLOCK_YEAR_Msk              (_UINT32_(0x3F) << RTC_MODE2_CLOCK_YEAR_Pos)         /* (RTC_MODE2_CLOCK) Year Mask */
#define RTC_MODE2_CLOCK_YEAR(value)           (RTC_MODE2_CLOCK_YEAR_Msk & (_UINT32_(value) << RTC_MODE2_CLOCK_YEAR_Pos)) /* Assigment of value for YEAR in the RTC_MODE2_CLOCK register */
#define RTC_MODE2_CLOCK_Msk                   _UINT32_(0xFFFFFFFF)                                 /* (RTC_MODE2_CLOCK) Register Mask  */


/* -------- RTC_MODE1_PER : (RTC Offset: 0x1C) (R/W 16) MODE1 Counter Period -------- */
#define RTC_MODE1_PER_RESETVALUE              _UINT16_(0x00)                                       /*  (RTC_MODE1_PER) MODE1 Counter Period  Reset Value */

#define RTC_MODE1_PER_PER_Pos                 _UINT16_(0)                                          /* (RTC_MODE1_PER) Counter Period Position */
#define RTC_MODE1_PER_PER_Msk                 (_UINT16_(0xFFFF) << RTC_MODE1_PER_PER_Pos)          /* (RTC_MODE1_PER) Counter Period Mask */
#define RTC_MODE1_PER_PER(value)              (RTC_MODE1_PER_PER_Msk & (_UINT16_(value) << RTC_MODE1_PER_PER_Pos)) /* Assigment of value for PER in the RTC_MODE1_PER register */
#define RTC_MODE1_PER_Msk                     _UINT16_(0xFFFF)                                     /* (RTC_MODE1_PER) Register Mask  */


/* -------- RTC_MODE0_COMP : (RTC Offset: 0x20) (R/W 32) MODE0 Compare n Value -------- */
#define RTC_MODE0_COMP_RESETVALUE             _UINT32_(0x00)                                       /*  (RTC_MODE0_COMP) MODE0 Compare n Value  Reset Value */

#define RTC_MODE0_COMP_COMP_Pos               _UINT32_(0)                                          /* (RTC_MODE0_COMP) Compare Value Position */
#define RTC_MODE0_COMP_COMP_Msk               (_UINT32_(0xFFFFFFFF) << RTC_MODE0_COMP_COMP_Pos)    /* (RTC_MODE0_COMP) Compare Value Mask */
#define RTC_MODE0_COMP_COMP(value)            (RTC_MODE0_COMP_COMP_Msk & (_UINT32_(value) << RTC_MODE0_COMP_COMP_Pos)) /* Assigment of value for COMP in the RTC_MODE0_COMP register */
#define RTC_MODE0_COMP_Msk                    _UINT32_(0xFFFFFFFF)                                 /* (RTC_MODE0_COMP) Register Mask  */


/* -------- RTC_MODE1_COMP : (RTC Offset: 0x20) (R/W 16) MODE1 Compare n Value -------- */
#define RTC_MODE1_COMP_RESETVALUE             _UINT16_(0x00)                                       /*  (RTC_MODE1_COMP) MODE1 Compare n Value  Reset Value */

#define RTC_MODE1_COMP_COMP_Pos               _UINT16_(0)                                          /* (RTC_MODE1_COMP) Compare Value Position */
#define RTC_MODE1_COMP_COMP_Msk               (_UINT16_(0xFFFF) << RTC_MODE1_COMP_COMP_Pos)        /* (RTC_MODE1_COMP) Compare Value Mask */
#define RTC_MODE1_COMP_COMP(value)            (RTC_MODE1_COMP_COMP_Msk & (_UINT16_(value) << RTC_MODE1_COMP_COMP_Pos)) /* Assigment of value for COMP in the RTC_MODE1_COMP register */
#define RTC_MODE1_COMP_Msk                    _UINT16_(0xFFFF)                                     /* (RTC_MODE1_COMP) Register Mask  */


/* -------- RTC_MODE2_ALARM : (RTC Offset: 0x20) (R/W 32) MODE2_ALARM Alarm n Value -------- */
#define RTC_MODE2_ALARM_RESETVALUE            _UINT32_(0x00)                                       /*  (RTC_MODE2_ALARM) MODE2_ALARM Alarm n Value  Reset Value */

#define RTC_MODE2_ALARM_SECOND_Pos            _UINT32_(0)                                          /* (RTC_MODE2_ALARM) Second Position */
#define RTC_MODE2_ALARM_SECOND_Msk            (_UINT32_(0x3F) << RTC_MODE2_ALARM_SECOND_Pos)       /* (RTC_MODE2_ALARM) Second Mask */
#define RTC_MODE2_ALARM_SECOND(value)         (RTC_MODE2_ALARM_SECOND_Msk & (_UINT32_(value) << RTC_MODE2_ALARM_SECOND_Pos)) /* Assigment of value for SECOND in the RTC_MODE2_ALARM register */
#define RTC_MODE2_ALARM_MINUTE_Pos            _UINT32_(6)                                          /* (RTC_MODE2_ALARM) Minute Position */
#define RTC_MODE2_ALARM_MINUTE_Msk            (_UINT32_(0x3F) << RTC_MODE2_ALARM_MINUTE_Pos)       /* (RTC_MODE2_ALARM) Minute Mask */
#define RTC_MODE2_ALARM_MINUTE(value)         (RTC_MODE2_ALARM_MINUTE_Msk & (_UINT32_(value) << RTC_MODE2_ALARM_MINUTE_Pos)) /* Assigment of value for MINUTE in the RTC_MODE2_ALARM register */
#define RTC_MODE2_ALARM_HOUR_Pos              _UINT32_(12)                                         /* (RTC_MODE2_ALARM) Hour Position */
#define RTC_MODE2_ALARM_HOUR_Msk              (_UINT32_(0x1F) << RTC_MODE2_ALARM_HOUR_Pos)         /* (RTC_MODE2_ALARM) Hour Mask */
#define RTC_MODE2_ALARM_HOUR(value)           (RTC_MODE2_ALARM_HOUR_Msk & (_UINT32_(value) << RTC_MODE2_ALARM_HOUR_Pos)) /* Assigment of value for HOUR in the RTC_MODE2_ALARM register */
#define   RTC_MODE2_ALARM_HOUR_AM_Val         _UINT32_(0x0)                                        /* (RTC_MODE2_ALARM) Morning hour  */
#define   RTC_MODE2_ALARM_HOUR_PM_Val         _UINT32_(0x10)                                       /* (RTC_MODE2_ALARM) Afternoon hour  */
#define RTC_MODE2_ALARM_HOUR_AM               (RTC_MODE2_ALARM_HOUR_AM_Val << RTC_MODE2_ALARM_HOUR_Pos) /* (RTC_MODE2_ALARM) Morning hour Position  */
#define RTC_MODE2_ALARM_HOUR_PM               (RTC_MODE2_ALARM_HOUR_PM_Val << RTC_MODE2_ALARM_HOUR_Pos) /* (RTC_MODE2_ALARM) Afternoon hour Position  */
#define RTC_MODE2_ALARM_DAY_Pos               _UINT32_(17)                                         /* (RTC_MODE2_ALARM) Day Position */
#define RTC_MODE2_ALARM_DAY_Msk               (_UINT32_(0x1F) << RTC_MODE2_ALARM_DAY_Pos)          /* (RTC_MODE2_ALARM) Day Mask */
#define RTC_MODE2_ALARM_DAY(value)            (RTC_MODE2_ALARM_DAY_Msk & (_UINT32_(value) << RTC_MODE2_ALARM_DAY_Pos)) /* Assigment of value for DAY in the RTC_MODE2_ALARM register */
#define RTC_MODE2_ALARM_MONTH_Pos             _UINT32_(22)                                         /* (RTC_MODE2_ALARM) Month Position */
#define RTC_MODE2_ALARM_MONTH_Msk             (_UINT32_(0xF) << RTC_MODE2_ALARM_MONTH_Pos)         /* (RTC_MODE2_ALARM) Month Mask */
#define RTC_MODE2_ALARM_MONTH(value)          (RTC_MODE2_ALARM_MONTH_Msk & (_UINT32_(value) << RTC_MODE2_ALARM_MONTH_Pos)) /* Assigment of value for MONTH in the RTC_MODE2_ALARM register */
#define RTC_MODE2_ALARM_YEAR_Pos              _UINT32_(26)                                         /* (RTC_MODE2_ALARM) Year Position */
#define RTC_MODE2_ALARM_YEAR_Msk              (_UINT32_(0x3F) << RTC_MODE2_ALARM_YEAR_Pos)         /* (RTC_MODE2_ALARM) Year Mask */
#define RTC_MODE2_ALARM_YEAR(value)           (RTC_MODE2_ALARM_YEAR_Msk & (_UINT32_(value) << RTC_MODE2_ALARM_YEAR_Pos)) /* Assigment of value for YEAR in the RTC_MODE2_ALARM register */
#define RTC_MODE2_ALARM_Msk                   _UINT32_(0xFFFFFFFF)                                 /* (RTC_MODE2_ALARM) Register Mask  */


/* -------- RTC_MODE2_MASK : (RTC Offset: 0x24) (R/W 8) MODE2_ALARM Alarm n Mask -------- */
#define RTC_MODE2_MASK_RESETVALUE             _UINT8_(0x00)                                        /*  (RTC_MODE2_MASK) MODE2_ALARM Alarm n Mask  Reset Value */

#define RTC_MODE2_MASK_SEL_Pos                _UINT8_(0)                                           /* (RTC_MODE2_MASK) Alarm Mask Selection Position */
#define RTC_MODE2_MASK_SEL_Msk                (_UINT8_(0x7) << RTC_MODE2_MASK_SEL_Pos)             /* (RTC_MODE2_MASK) Alarm Mask Selection Mask */
#define RTC_MODE2_MASK_SEL(value)             (RTC_MODE2_MASK_SEL_Msk & (_UINT8_(value) << RTC_MODE2_MASK_SEL_Pos)) /* Assigment of value for SEL in the RTC_MODE2_MASK register */
#define   RTC_MODE2_MASK_SEL_OFF_Val          _UINT8_(0x0)                                         /* (RTC_MODE2_MASK) Alarm Disabled  */
#define   RTC_MODE2_MASK_SEL_SS_Val           _UINT8_(0x1)                                         /* (RTC_MODE2_MASK) Match seconds only  */
#define   RTC_MODE2_MASK_SEL_MMSS_Val         _UINT8_(0x2)                                         /* (RTC_MODE2_MASK) Match seconds and minutes only  */
#define   RTC_MODE2_MASK_SEL_HHMMSS_Val       _UINT8_(0x3)                                         /* (RTC_MODE2_MASK) Match seconds, minutes, and hours only  */
#define   RTC_MODE2_MASK_SEL_DDHHMMSS_Val     _UINT8_(0x4)                                         /* (RTC_MODE2_MASK) Match seconds, minutes, hours, and days only  */
#define   RTC_MODE2_MASK_SEL_MMDDHHMMSS_Val   _UINT8_(0x5)                                         /* (RTC_MODE2_MASK) Match seconds, minutes, hours, days, and months only  */
#define   RTC_MODE2_MASK_SEL_YYMMDDHHMMSS_Val _UINT8_(0x6)                                         /* (RTC_MODE2_MASK) Match seconds, minutes, hours, days, months, and years  */
#define RTC_MODE2_MASK_SEL_OFF                (RTC_MODE2_MASK_SEL_OFF_Val << RTC_MODE2_MASK_SEL_Pos) /* (RTC_MODE2_MASK) Alarm Disabled Position  */
#define RTC_MODE2_MASK_SEL_SS                 (RTC_MODE2_MASK_SEL_SS_Val << RTC_MODE2_MASK_SEL_Pos) /* (RTC_MODE2_MASK) Match seconds only Position  */
#define RTC_MODE2_MASK_SEL_MMSS               (RTC_MODE2_MASK_SEL_MMSS_Val << RTC_MODE2_MASK_SEL_Pos) /* (RTC_MODE2_MASK) Match seconds and minutes only Position  */
#define RTC_MODE2_MASK_SEL_HHMMSS             (RTC_MODE2_MASK_SEL_HHMMSS_Val << RTC_MODE2_MASK_SEL_Pos) /* (RTC_MODE2_MASK) Match seconds, minutes, and hours only Position  */
#define RTC_MODE2_MASK_SEL_DDHHMMSS           (RTC_MODE2_MASK_SEL_DDHHMMSS_Val << RTC_MODE2_MASK_SEL_Pos) /* (RTC_MODE2_MASK) Match seconds, minutes, hours, and days only Position  */
#define RTC_MODE2_MASK_SEL_MMDDHHMMSS         (RTC_MODE2_MASK_SEL_MMDDHHMMSS_Val << RTC_MODE2_MASK_SEL_Pos) /* (RTC_MODE2_MASK) Match seconds, minutes, hours, days, and months only Position  */
#define RTC_MODE2_MASK_SEL_YYMMDDHHMMSS       (RTC_MODE2_MASK_SEL_YYMMDDHHMMSS_Val << RTC_MODE2_MASK_SEL_Pos) /* (RTC_MODE2_MASK) Match seconds, minutes, hours, days, months, and years Position  */
#define RTC_MODE2_MASK_Msk                    _UINT8_(0x07)                                        /* (RTC_MODE2_MASK) Register Mask  */


/** \brief RTC register offsets definitions */
#define RTC_MODE0_CTRLA_REG_OFST       _UINT32_(0x00)      /* (RTC_MODE0_CTRLA) MODE0 Control A Offset */
#define RTC_MODE1_CTRLA_REG_OFST       _UINT32_(0x00)      /* (RTC_MODE1_CTRLA) MODE1 Control A Offset */
#define RTC_MODE2_CTRLA_REG_OFST       _UINT32_(0x00)      /* (RTC_MODE2_CTRLA) MODE2 Control A Offset */
#define RTC_MODE0_EVCTRL_REG_OFST      _UINT32_(0x04)      /* (RTC_MODE0_EVCTRL) MODE0 Event Control Offset */
#define RTC_MODE1_EVCTRL_REG_OFST      _UINT32_(0x04)      /* (RTC_MODE1_EVCTRL) MODE1 Event Control Offset */
#define RTC_MODE2_EVCTRL_REG_OFST      _UINT32_(0x04)      /* (RTC_MODE2_EVCTRL) MODE2 Event Control Offset */
#define RTC_MODE0_INTENCLR_REG_OFST    _UINT32_(0x08)      /* (RTC_MODE0_INTENCLR) MODE0 Interrupt Enable Clear Offset */
#define RTC_MODE1_INTENCLR_REG_OFST    _UINT32_(0x08)      /* (RTC_MODE1_INTENCLR) MODE1 Interrupt Enable Clear Offset */
#define RTC_MODE2_INTENCLR_REG_OFST    _UINT32_(0x08)      /* (RTC_MODE2_INTENCLR) MODE2 Interrupt Enable Clear Offset */
#define RTC_MODE0_INTENSET_REG_OFST    _UINT32_(0x0A)      /* (RTC_MODE0_INTENSET) MODE0 Interrupt Enable Set Offset */
#define RTC_MODE1_INTENSET_REG_OFST    _UINT32_(0x0A)      /* (RTC_MODE1_INTENSET) MODE1 Interrupt Enable Set Offset */
#define RTC_MODE2_INTENSET_REG_OFST    _UINT32_(0x0A)      /* (RTC_MODE2_INTENSET) MODE2 Interrupt Enable Set Offset */
#define RTC_MODE0_INTFLAG_REG_OFST     _UINT32_(0x0C)      /* (RTC_MODE0_INTFLAG) MODE0 Interrupt Flag Status and Clear Offset */
#define RTC_MODE1_INTFLAG_REG_OFST     _UINT32_(0x0C)      /* (RTC_MODE1_INTFLAG) MODE1 Interrupt Flag Status and Clear Offset */
#define RTC_MODE2_INTFLAG_REG_OFST     _UINT32_(0x0C)      /* (RTC_MODE2_INTFLAG) MODE2 Interrupt Flag Status and Clear Offset */
#define RTC_DBGCTRL_REG_OFST           _UINT32_(0x0E)      /* (RTC_DBGCTRL) Debug Control Offset */
#define RTC_MODE0_SYNCBUSY_REG_OFST    _UINT32_(0x10)      /* (RTC_MODE0_SYNCBUSY) MODE0 Synchronization Busy Status Offset */
#define RTC_MODE1_SYNCBUSY_REG_OFST    _UINT32_(0x10)      /* (RTC_MODE1_SYNCBUSY) MODE1 Synchronization Busy Status Offset */
#define RTC_MODE2_SYNCBUSY_REG_OFST    _UINT32_(0x10)      /* (RTC_MODE2_SYNCBUSY) MODE2 Synchronization Busy Status Offset */
#define RTC_FREQCORR_REG_OFST          _UINT32_(0x14)      /* (RTC_FREQCORR) Frequency Correction Offset */
#define RTC_MODE0_COUNT_REG_OFST       _UINT32_(0x18)      /* (RTC_MODE0_COUNT) MODE0 Counter Value Offset */
#define RTC_MODE1_COUNT_REG_OFST       _UINT32_(0x18)      /* (RTC_MODE1_COUNT) MODE1 Counter Value Offset */
#define RTC_MODE2_CLOCK_REG_OFST       _UINT32_(0x18)      /* (RTC_MODE2_CLOCK) MODE2 Clock Value Offset */
#define RTC_MODE1_PER_REG_OFST         _UINT32_(0x1C)      /* (RTC_MODE1_PER) MODE1 Counter Period Offset */
#define RTC_MODE0_COMP_REG_OFST        _UINT32_(0x20)      /* (RTC_MODE0_COMP) MODE0 Compare n Value Offset */
#define RTC_MODE1_COMP_REG_OFST        _UINT32_(0x20)      /* (RTC_MODE1_COMP) MODE1 Compare n Value Offset */
#define RTC_MODE1_COMP0_REG_OFST       _UINT32_(0x20)      /* (RTC_MODE1_COMP0) MODE1 Compare n Value Offset */
#define RTC_MODE1_COMP1_REG_OFST       _UINT32_(0x22)      /* (RTC_MODE1_COMP1) MODE1 Compare n Value Offset */
#define RTC_MODE2_ALARM_REG_OFST       _UINT32_(0x20)      /* (RTC_MODE2_ALARM) MODE2_ALARM Alarm n Value Offset */
#define RTC_MODE2_MASK_REG_OFST        _UINT32_(0x24)      /* (RTC_MODE2_MASK) MODE2_ALARM Alarm n Mask Offset */

#if !(defined(__ASSEMBLER__) || defined(__IAR_SYSTEMS_ASM__))
/** \brief RTC register API structure */
typedef struct
{  /* Real-Time Counter */
  __IO  uint16_t                       RTC_CTRLA;          /**< Offset: 0x00 (R/W  16) MODE0 Control A */
  __I   uint8_t                        Reserved1[0x02];
  __IO  uint32_t                       RTC_EVCTRL;         /**< Offset: 0x04 (R/W  32) MODE0 Event Control */
  __IO  uint16_t                       RTC_INTENCLR;       /**< Offset: 0x08 (R/W  16) MODE0 Interrupt Enable Clear */
  __IO  uint16_t                       RTC_INTENSET;       /**< Offset: 0x0A (R/W  16) MODE0 Interrupt Enable Set */
  __IO  uint16_t                       RTC_INTFLAG;        /**< Offset: 0x0C (R/W  16) MODE0 Interrupt Flag Status and Clear */
  __IO  uint8_t                        RTC_DBGCTRL;        /**< Offset: 0x0E (R/W  8) Debug Control */
  __I   uint8_t                        Reserved2[0x01];
  __I   uint32_t                       RTC_SYNCBUSY;       /**< Offset: 0x10 (R/   32) MODE0 Synchronization Busy Status */
  __IO  uint8_t                        RTC_FREQCORR;       /**< Offset: 0x14 (R/W  8) Frequency Correction */
  __I   uint8_t                        Reserved3[0x03];
  __IO  uint32_t                       RTC_COUNT;          /**< Offset: 0x18 (R/W  32) MODE0 Counter Value */
  __I   uint8_t                        Reserved4[0x04];
  __IO  uint32_t                       RTC_COMP;           /**< Offset: 0x20 (R/W  32) MODE0 Compare n Value */
} rtc_mode0_registers_t;

/** \brief RTC register API structure */
typedef struct
{  /* Real-Time Counter */
  __IO  uint16_t                       RTC_CTRLA;          /**< Offset: 0x00 (R/W  16) MODE1 Control A */
  __I   uint8_t                        Reserved1[0x02];
  __IO  uint32_t                       RTC_EVCTRL;         /**< Offset: 0x04 (R/W  32) MODE1 Event Control */
  __IO  uint16_t                       RTC_INTENCLR;       /**< Offset: 0x08 (R/W  16) MODE1 Interrupt Enable Clear */
  __IO  uint16_t                       RTC_INTENSET;       /**< Offset: 0x0A (R/W  16) MODE1 Interrupt Enable Set */
  __IO  uint16_t                       RTC_INTFLAG;        /**< Offset: 0x0C (R/W  16) MODE1 Interrupt Flag Status and Clear */
  __IO  uint8_t                        RTC_DBGCTRL;        /**< Offset: 0x0E (R/W  8) Debug Control */
  __I   uint8_t                        Reserved2[0x01];
  __I   uint32_t                       RTC_SYNCBUSY;       /**< Offset: 0x10 (R/   32) MODE1 Synchronization Busy Status */
  __IO  uint8_t                        RTC_FREQCORR;       /**< Offset: 0x14 (R/W  8) Frequency Correction */
  __I   uint8_t                        Reserved3[0x03];
  __IO  uint16_t                       RTC_COUNT;          /**< Offset: 0x18 (R/W  16) MODE1 Counter Value */
  __I   uint8_t                        Reserved4[0x02];
  __IO  uint16_t                       RTC_PER;            /**< Offset: 0x1C (R/W  16) MODE1 Counter Period */
  __I   uint8_t                        Reserved5[0x02];
  __IO  uint16_t                       RTC_COMP[2];        /**< Offset: 0x20 (R/W  16) MODE1 Compare n Value */
} rtc_mode1_registers_t;

/** \brief RTC register API structure */
typedef struct
{  /* Real-Time Counter */
  __IO  uint16_t                       RTC_CTRLA;          /**< Offset: 0x00 (R/W  16) MODE2 Control A */
  __I   uint8_t                        Reserved1[0x02];
  __IO  uint32_t                       RTC_EVCTRL;         /**< Offset: 0x04 (R/W  32) MODE2 Event Control */
  __IO  uint16_t                       RTC_INTENCLR;       /**< Offset: 0x08 (R/W  16) MODE2 Interrupt Enable Clear */
  __IO  uint16_t                       RTC_INTENSET;       /**< Offset: 0x0A (R/W  16) MODE2 Interrupt Enable Set */
  __IO  uint16_t                       RTC_INTFLAG;        /**< Offset: 0x0C (R/W  16) MODE2 Interrupt Flag Status and Clear */
  __IO  uint8_t                        RTC_DBGCTRL;        /**< Offset: 0x0E (R/W  8) Debug Control */
  __I   uint8_t                        Reserved2[0x01];
  __I   uint32_t                       RTC_SYNCBUSY;       /**< Offset: 0x10 (R/   32) MODE2 Synchronization Busy Status */
  __IO  uint8_t                        RTC_FREQCORR;       /**< Offset: 0x14 (R/W  8) Frequency Correction */
  __I   uint8_t                        Reserved3[0x03];
  __IO  uint32_t                       RTC_CLOCK;          /**< Offset: 0x18 (R/W  32) MODE2 Clock Value */
  __I   uint8_t                        Reserved4[0x04];
  __IO  uint32_t                       RTC_ALARM;          /**< Offset: 0x20 (R/W  32) MODE2_ALARM Alarm n Value */
  __IO  uint8_t                        RTC_MASK;           /**< Offset: 0x24 (R/W  8) MODE2_ALARM Alarm n Mask */
} rtc_mode2_registers_t;

/** \brief RTC hardware registers */
typedef union
{  /* Real-Time Counter */
       rtc_mode0_registers_t          MODE0;          /**< 32-bit Counter with Single 32-bit Compare */
       rtc_mode1_registers_t          MODE1;          /**< 16-bit Counter with Two 16-bit Compares */
       rtc_mode2_registers_t          MODE2;          /**< Clock/Calendar with Alarm */
} rtc_registers_t;


#endif /* !(defined(__ASSEMBLER__) || defined(__IAR_SYSTEMS_ASM__)) */
#endif /* _PIC32CMMC00_RTC_COMPONENT_H_ */
