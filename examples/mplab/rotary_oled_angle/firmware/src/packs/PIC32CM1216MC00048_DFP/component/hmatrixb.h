/*
 * Component description for HMATRIXB
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
#ifndef _PIC32CMMC00_HMATRIXB_COMPONENT_H_
#define _PIC32CMMC00_HMATRIXB_COMPONENT_H_

/* ************************************************************************** */
/*   SOFTWARE API DEFINITION FOR HMATRIXB                                     */
/* ************************************************************************** */

/* -------- HMATRIXB_PRAS : (HMATRIXB Offset: 0x00) (R/W 32) Priority A for Slave -------- */
#define HMATRIXB_PRAS_RESETVALUE              _UINT32_(0x00)                                       /*  (HMATRIXB_PRAS) Priority A for Slave  Reset Value */

#define HMATRIXB_PRAS_M0PR_Pos                _UINT32_(0)                                          /* (HMATRIXB_PRAS) Master 0 Priority Position */
#define HMATRIXB_PRAS_M0PR_Msk                (_UINT32_(0xF) << HMATRIXB_PRAS_M0PR_Pos)            /* (HMATRIXB_PRAS) Master 0 Priority Mask */
#define HMATRIXB_PRAS_M0PR(value)             (HMATRIXB_PRAS_M0PR_Msk & (_UINT32_(value) << HMATRIXB_PRAS_M0PR_Pos)) /* Assigment of value for M0PR in the HMATRIXB_PRAS register */
#define HMATRIXB_PRAS_M1PR_Pos                _UINT32_(4)                                          /* (HMATRIXB_PRAS) Master 1 Priority Position */
#define HMATRIXB_PRAS_M1PR_Msk                (_UINT32_(0xF) << HMATRIXB_PRAS_M1PR_Pos)            /* (HMATRIXB_PRAS) Master 1 Priority Mask */
#define HMATRIXB_PRAS_M1PR(value)             (HMATRIXB_PRAS_M1PR_Msk & (_UINT32_(value) << HMATRIXB_PRAS_M1PR_Pos)) /* Assigment of value for M1PR in the HMATRIXB_PRAS register */
#define HMATRIXB_PRAS_M2PR_Pos                _UINT32_(8)                                          /* (HMATRIXB_PRAS) Master 2 Priority Position */
#define HMATRIXB_PRAS_M2PR_Msk                (_UINT32_(0xF) << HMATRIXB_PRAS_M2PR_Pos)            /* (HMATRIXB_PRAS) Master 2 Priority Mask */
#define HMATRIXB_PRAS_M2PR(value)             (HMATRIXB_PRAS_M2PR_Msk & (_UINT32_(value) << HMATRIXB_PRAS_M2PR_Pos)) /* Assigment of value for M2PR in the HMATRIXB_PRAS register */
#define HMATRIXB_PRAS_M3PR_Pos                _UINT32_(12)                                         /* (HMATRIXB_PRAS) Master 3 Priority Position */
#define HMATRIXB_PRAS_M3PR_Msk                (_UINT32_(0xF) << HMATRIXB_PRAS_M3PR_Pos)            /* (HMATRIXB_PRAS) Master 3 Priority Mask */
#define HMATRIXB_PRAS_M3PR(value)             (HMATRIXB_PRAS_M3PR_Msk & (_UINT32_(value) << HMATRIXB_PRAS_M3PR_Pos)) /* Assigment of value for M3PR in the HMATRIXB_PRAS register */
#define HMATRIXB_PRAS_M4PR_Pos                _UINT32_(16)                                         /* (HMATRIXB_PRAS) Master 4 Priority Position */
#define HMATRIXB_PRAS_M4PR_Msk                (_UINT32_(0xF) << HMATRIXB_PRAS_M4PR_Pos)            /* (HMATRIXB_PRAS) Master 4 Priority Mask */
#define HMATRIXB_PRAS_M4PR(value)             (HMATRIXB_PRAS_M4PR_Msk & (_UINT32_(value) << HMATRIXB_PRAS_M4PR_Pos)) /* Assigment of value for M4PR in the HMATRIXB_PRAS register */
#define HMATRIXB_PRAS_M5PR_Pos                _UINT32_(20)                                         /* (HMATRIXB_PRAS) Master 5 Priority Position */
#define HMATRIXB_PRAS_M5PR_Msk                (_UINT32_(0xF) << HMATRIXB_PRAS_M5PR_Pos)            /* (HMATRIXB_PRAS) Master 5 Priority Mask */
#define HMATRIXB_PRAS_M5PR(value)             (HMATRIXB_PRAS_M5PR_Msk & (_UINT32_(value) << HMATRIXB_PRAS_M5PR_Pos)) /* Assigment of value for M5PR in the HMATRIXB_PRAS register */
#define HMATRIXB_PRAS_M6PR_Pos                _UINT32_(24)                                         /* (HMATRIXB_PRAS) Master 6 Priority Position */
#define HMATRIXB_PRAS_M6PR_Msk                (_UINT32_(0xF) << HMATRIXB_PRAS_M6PR_Pos)            /* (HMATRIXB_PRAS) Master 6 Priority Mask */
#define HMATRIXB_PRAS_M6PR(value)             (HMATRIXB_PRAS_M6PR_Msk & (_UINT32_(value) << HMATRIXB_PRAS_M6PR_Pos)) /* Assigment of value for M6PR in the HMATRIXB_PRAS register */
#define HMATRIXB_PRAS_M7PR_Pos                _UINT32_(28)                                         /* (HMATRIXB_PRAS) Master 7 Priority Position */
#define HMATRIXB_PRAS_M7PR_Msk                (_UINT32_(0xF) << HMATRIXB_PRAS_M7PR_Pos)            /* (HMATRIXB_PRAS) Master 7 Priority Mask */
#define HMATRIXB_PRAS_M7PR(value)             (HMATRIXB_PRAS_M7PR_Msk & (_UINT32_(value) << HMATRIXB_PRAS_M7PR_Pos)) /* Assigment of value for M7PR in the HMATRIXB_PRAS register */
#define HMATRIXB_PRAS_Msk                     _UINT32_(0xFFFFFFFF)                                 /* (HMATRIXB_PRAS) Register Mask  */


/* -------- HMATRIXB_PRBS : (HMATRIXB Offset: 0x04) (R/W 32) Priority B for Slave -------- */
#define HMATRIXB_PRBS_RESETVALUE              _UINT32_(0x00)                                       /*  (HMATRIXB_PRBS) Priority B for Slave  Reset Value */

#define HMATRIXB_PRBS_M8PR_Pos                _UINT32_(0)                                          /* (HMATRIXB_PRBS) Master 8 Priority Position */
#define HMATRIXB_PRBS_M8PR_Msk                (_UINT32_(0xF) << HMATRIXB_PRBS_M8PR_Pos)            /* (HMATRIXB_PRBS) Master 8 Priority Mask */
#define HMATRIXB_PRBS_M8PR(value)             (HMATRIXB_PRBS_M8PR_Msk & (_UINT32_(value) << HMATRIXB_PRBS_M8PR_Pos)) /* Assigment of value for M8PR in the HMATRIXB_PRBS register */
#define HMATRIXB_PRBS_M9PR_Pos                _UINT32_(4)                                          /* (HMATRIXB_PRBS) Master 9 Priority Position */
#define HMATRIXB_PRBS_M9PR_Msk                (_UINT32_(0xF) << HMATRIXB_PRBS_M9PR_Pos)            /* (HMATRIXB_PRBS) Master 9 Priority Mask */
#define HMATRIXB_PRBS_M9PR(value)             (HMATRIXB_PRBS_M9PR_Msk & (_UINT32_(value) << HMATRIXB_PRBS_M9PR_Pos)) /* Assigment of value for M9PR in the HMATRIXB_PRBS register */
#define HMATRIXB_PRBS_M10PR_Pos               _UINT32_(8)                                          /* (HMATRIXB_PRBS) Master 10 Priority Position */
#define HMATRIXB_PRBS_M10PR_Msk               (_UINT32_(0xF) << HMATRIXB_PRBS_M10PR_Pos)           /* (HMATRIXB_PRBS) Master 10 Priority Mask */
#define HMATRIXB_PRBS_M10PR(value)            (HMATRIXB_PRBS_M10PR_Msk & (_UINT32_(value) << HMATRIXB_PRBS_M10PR_Pos)) /* Assigment of value for M10PR in the HMATRIXB_PRBS register */
#define HMATRIXB_PRBS_M11PR_Pos               _UINT32_(12)                                         /* (HMATRIXB_PRBS) Master 11 Priority Position */
#define HMATRIXB_PRBS_M11PR_Msk               (_UINT32_(0xF) << HMATRIXB_PRBS_M11PR_Pos)           /* (HMATRIXB_PRBS) Master 11 Priority Mask */
#define HMATRIXB_PRBS_M11PR(value)            (HMATRIXB_PRBS_M11PR_Msk & (_UINT32_(value) << HMATRIXB_PRBS_M11PR_Pos)) /* Assigment of value for M11PR in the HMATRIXB_PRBS register */
#define HMATRIXB_PRBS_M12PR_Pos               _UINT32_(16)                                         /* (HMATRIXB_PRBS) Master 12 Priority Position */
#define HMATRIXB_PRBS_M12PR_Msk               (_UINT32_(0xF) << HMATRIXB_PRBS_M12PR_Pos)           /* (HMATRIXB_PRBS) Master 12 Priority Mask */
#define HMATRIXB_PRBS_M12PR(value)            (HMATRIXB_PRBS_M12PR_Msk & (_UINT32_(value) << HMATRIXB_PRBS_M12PR_Pos)) /* Assigment of value for M12PR in the HMATRIXB_PRBS register */
#define HMATRIXB_PRBS_M13PR_Pos               _UINT32_(20)                                         /* (HMATRIXB_PRBS) Master 13 Priority Position */
#define HMATRIXB_PRBS_M13PR_Msk               (_UINT32_(0xF) << HMATRIXB_PRBS_M13PR_Pos)           /* (HMATRIXB_PRBS) Master 13 Priority Mask */
#define HMATRIXB_PRBS_M13PR(value)            (HMATRIXB_PRBS_M13PR_Msk & (_UINT32_(value) << HMATRIXB_PRBS_M13PR_Pos)) /* Assigment of value for M13PR in the HMATRIXB_PRBS register */
#define HMATRIXB_PRBS_M14PR_Pos               _UINT32_(24)                                         /* (HMATRIXB_PRBS) Master 14 Priority Position */
#define HMATRIXB_PRBS_M14PR_Msk               (_UINT32_(0xF) << HMATRIXB_PRBS_M14PR_Pos)           /* (HMATRIXB_PRBS) Master 14 Priority Mask */
#define HMATRIXB_PRBS_M14PR(value)            (HMATRIXB_PRBS_M14PR_Msk & (_UINT32_(value) << HMATRIXB_PRBS_M14PR_Pos)) /* Assigment of value for M14PR in the HMATRIXB_PRBS register */
#define HMATRIXB_PRBS_M15PR_Pos               _UINT32_(28)                                         /* (HMATRIXB_PRBS) Master 15 Priority Position */
#define HMATRIXB_PRBS_M15PR_Msk               (_UINT32_(0xF) << HMATRIXB_PRBS_M15PR_Pos)           /* (HMATRIXB_PRBS) Master 15 Priority Mask */
#define HMATRIXB_PRBS_M15PR(value)            (HMATRIXB_PRBS_M15PR_Msk & (_UINT32_(value) << HMATRIXB_PRBS_M15PR_Pos)) /* Assigment of value for M15PR in the HMATRIXB_PRBS register */
#define HMATRIXB_PRBS_Msk                     _UINT32_(0xFFFFFFFF)                                 /* (HMATRIXB_PRBS) Register Mask  */


/* -------- HMATRIXB_MCFG : (HMATRIXB Offset: 0x00) (R/W 32) Master Configuration -------- */
#define HMATRIXB_MCFG_RESETVALUE              _UINT32_(0x02)                                       /*  (HMATRIXB_MCFG) Master Configuration  Reset Value */

#define HMATRIXB_MCFG_ULBT_Pos                _UINT32_(0)                                          /* (HMATRIXB_MCFG) Undefined Length Burst Type Position */
#define HMATRIXB_MCFG_ULBT_Msk                (_UINT32_(0x7) << HMATRIXB_MCFG_ULBT_Pos)            /* (HMATRIXB_MCFG) Undefined Length Burst Type Mask */
#define HMATRIXB_MCFG_ULBT(value)             (HMATRIXB_MCFG_ULBT_Msk & (_UINT32_(value) << HMATRIXB_MCFG_ULBT_Pos)) /* Assigment of value for ULBT in the HMATRIXB_MCFG register */
#define   HMATRIXB_MCFG_ULBT_INFINITE_Val     _UINT32_(0x0)                                        /* (HMATRIXB_MCFG) Infinite Length  */
#define   HMATRIXB_MCFG_ULBT_SINGLE_Val       _UINT32_(0x1)                                        /* (HMATRIXB_MCFG) Single Access  */
#define   HMATRIXB_MCFG_ULBT_FOUR_BEAT_Val    _UINT32_(0x2)                                        /* (HMATRIXB_MCFG) Four Beat Burst  */
#define   HMATRIXB_MCFG_ULBT_EIGHT_BEAT_Val   _UINT32_(0x3)                                        /* (HMATRIXB_MCFG) Eight Beat Burst  */
#define   HMATRIXB_MCFG_ULBT_SIXTEEN_BEAT_Val _UINT32_(0x4)                                        /* (HMATRIXB_MCFG) Sixteen Beat Burst  */
#define HMATRIXB_MCFG_ULBT_INFINITE           (HMATRIXB_MCFG_ULBT_INFINITE_Val << HMATRIXB_MCFG_ULBT_Pos) /* (HMATRIXB_MCFG) Infinite Length Position  */
#define HMATRIXB_MCFG_ULBT_SINGLE             (HMATRIXB_MCFG_ULBT_SINGLE_Val << HMATRIXB_MCFG_ULBT_Pos) /* (HMATRIXB_MCFG) Single Access Position  */
#define HMATRIXB_MCFG_ULBT_FOUR_BEAT          (HMATRIXB_MCFG_ULBT_FOUR_BEAT_Val << HMATRIXB_MCFG_ULBT_Pos) /* (HMATRIXB_MCFG) Four Beat Burst Position  */
#define HMATRIXB_MCFG_ULBT_EIGHT_BEAT         (HMATRIXB_MCFG_ULBT_EIGHT_BEAT_Val << HMATRIXB_MCFG_ULBT_Pos) /* (HMATRIXB_MCFG) Eight Beat Burst Position  */
#define HMATRIXB_MCFG_ULBT_SIXTEEN_BEAT       (HMATRIXB_MCFG_ULBT_SIXTEEN_BEAT_Val << HMATRIXB_MCFG_ULBT_Pos) /* (HMATRIXB_MCFG) Sixteen Beat Burst Position  */
#define HMATRIXB_MCFG_Msk                     _UINT32_(0x00000007)                                 /* (HMATRIXB_MCFG) Register Mask  */


/* -------- HMATRIXB_SCFG : (HMATRIXB Offset: 0x40) (R/W 32) Slave Configuration -------- */
#define HMATRIXB_SCFG_RESETVALUE              _UINT32_(0x10)                                       /*  (HMATRIXB_SCFG) Slave Configuration  Reset Value */

#define HMATRIXB_SCFG_SLOT_CYCLE_Pos          _UINT32_(0)                                          /* (HMATRIXB_SCFG) Maximum Number of Allowed Cycles for a Burst Position */
#define HMATRIXB_SCFG_SLOT_CYCLE_Msk          (_UINT32_(0xFF) << HMATRIXB_SCFG_SLOT_CYCLE_Pos)     /* (HMATRIXB_SCFG) Maximum Number of Allowed Cycles for a Burst Mask */
#define HMATRIXB_SCFG_SLOT_CYCLE(value)       (HMATRIXB_SCFG_SLOT_CYCLE_Msk & (_UINT32_(value) << HMATRIXB_SCFG_SLOT_CYCLE_Pos)) /* Assigment of value for SLOT_CYCLE in the HMATRIXB_SCFG register */
#define HMATRIXB_SCFG_DEFMSTR_TYPE_Pos        _UINT32_(16)                                         /* (HMATRIXB_SCFG) Default Master Type Position */
#define HMATRIXB_SCFG_DEFMSTR_TYPE_Msk        (_UINT32_(0x3) << HMATRIXB_SCFG_DEFMSTR_TYPE_Pos)    /* (HMATRIXB_SCFG) Default Master Type Mask */
#define HMATRIXB_SCFG_DEFMSTR_TYPE(value)     (HMATRIXB_SCFG_DEFMSTR_TYPE_Msk & (_UINT32_(value) << HMATRIXB_SCFG_DEFMSTR_TYPE_Pos)) /* Assigment of value for DEFMSTR_TYPE in the HMATRIXB_SCFG register */
#define   HMATRIXB_SCFG_DEFMSTR_TYPE_NO_DEFAULT_Val _UINT32_(0x0)                                        /* (HMATRIXB_SCFG) No Default Master. At the end of current slave access, if no other master request is pending, the slave is deconnected from all masters. This resusts in having a one cycle latency for the first transfer of a burst.  */
#define   HMATRIXB_SCFG_DEFMSTR_TYPE_LAST_DEFAULT_Val _UINT32_(0x1)                                        /* (HMATRIXB_SCFG) Last Default Master At the end of current slave access, if no other master request is pending, the slave stay connected with the last master havingaccessed it.This resusts in not having the one cycle latency when the last master re-trying access on the slave.  */
#define   HMATRIXB_SCFG_DEFMSTR_TYPE_FIXED_DEFAULT_Val _UINT32_(0x2)                                        /* (HMATRIXB_SCFG) Fixed Default Master At the end of current slave access, if no other master request is pending, the slave connects with fixed master which numberis in FIXED_DEFMSTR register.This resusts in not having the one cycle latency when the fixed master re-trying access on the slave.  */
#define HMATRIXB_SCFG_DEFMSTR_TYPE_NO_DEFAULT (HMATRIXB_SCFG_DEFMSTR_TYPE_NO_DEFAULT_Val << HMATRIXB_SCFG_DEFMSTR_TYPE_Pos) /* (HMATRIXB_SCFG) No Default Master. At the end of current slave access, if no other master request is pending, the slave is deconnected from all masters. This resusts in having a one cycle latency for the first transfer of a burst. Position  */
#define HMATRIXB_SCFG_DEFMSTR_TYPE_LAST_DEFAULT (HMATRIXB_SCFG_DEFMSTR_TYPE_LAST_DEFAULT_Val << HMATRIXB_SCFG_DEFMSTR_TYPE_Pos) /* (HMATRIXB_SCFG) Last Default Master At the end of current slave access, if no other master request is pending, the slave stay connected with the last master havingaccessed it.This resusts in not having the one cycle latency when the last master re-trying access on the slave. Position  */
#define HMATRIXB_SCFG_DEFMSTR_TYPE_FIXED_DEFAULT (HMATRIXB_SCFG_DEFMSTR_TYPE_FIXED_DEFAULT_Val << HMATRIXB_SCFG_DEFMSTR_TYPE_Pos) /* (HMATRIXB_SCFG) Fixed Default Master At the end of current slave access, if no other master request is pending, the slave connects with fixed master which numberis in FIXED_DEFMSTR register.This resusts in not having the one cycle latency when the fixed master re-trying access on the slave. Position  */
#define HMATRIXB_SCFG_FIXED_DEFMSTR_Pos       _UINT32_(18)                                         /* (HMATRIXB_SCFG) Fixed Index of Default Master Position */
#define HMATRIXB_SCFG_FIXED_DEFMSTR_Msk       (_UINT32_(0xF) << HMATRIXB_SCFG_FIXED_DEFMSTR_Pos)   /* (HMATRIXB_SCFG) Fixed Index of Default Master Mask */
#define HMATRIXB_SCFG_FIXED_DEFMSTR(value)    (HMATRIXB_SCFG_FIXED_DEFMSTR_Msk & (_UINT32_(value) << HMATRIXB_SCFG_FIXED_DEFMSTR_Pos)) /* Assigment of value for FIXED_DEFMSTR in the HMATRIXB_SCFG register */
#define HMATRIXB_SCFG_ARBT_Pos                _UINT32_(24)                                         /* (HMATRIXB_SCFG) Arbitration Type Position */
#define HMATRIXB_SCFG_ARBT_Msk                (_UINT32_(0x1) << HMATRIXB_SCFG_ARBT_Pos)            /* (HMATRIXB_SCFG) Arbitration Type Mask */
#define HMATRIXB_SCFG_ARBT(value)             (HMATRIXB_SCFG_ARBT_Msk & (_UINT32_(value) << HMATRIXB_SCFG_ARBT_Pos)) /* Assigment of value for ARBT in the HMATRIXB_SCFG register */
#define   HMATRIXB_SCFG_ARBT_ROUND_ROBIN_Val  _UINT32_(0x0)                                        /* (HMATRIXB_SCFG) Round-Robin Arbitration  */
#define   HMATRIXB_SCFG_ARBT_FIXED_PRIORITY_Val _UINT32_(0x1)                                        /* (HMATRIXB_SCFG) Fixed Priority Arbitration  */
#define HMATRIXB_SCFG_ARBT_ROUND_ROBIN        (HMATRIXB_SCFG_ARBT_ROUND_ROBIN_Val << HMATRIXB_SCFG_ARBT_Pos) /* (HMATRIXB_SCFG) Round-Robin Arbitration Position  */
#define HMATRIXB_SCFG_ARBT_FIXED_PRIORITY     (HMATRIXB_SCFG_ARBT_FIXED_PRIORITY_Val << HMATRIXB_SCFG_ARBT_Pos) /* (HMATRIXB_SCFG) Fixed Priority Arbitration Position  */
#define HMATRIXB_SCFG_Msk                     _UINT32_(0x013F00FF)                                 /* (HMATRIXB_SCFG) Register Mask  */


/* -------- HMATRIXB_MRCR : (HMATRIXB Offset: 0x100) (R/W 32) Master Remap Control -------- */
#define HMATRIXB_MRCR_RESETVALUE              _UINT32_(0x00)                                       /*  (HMATRIXB_MRCR) Master Remap Control  Reset Value */

#define HMATRIXB_MRCR_RCB0_Pos                _UINT32_(0)                                          /* (HMATRIXB_MRCR) Remap Command Bit for Master 0 Position */
#define HMATRIXB_MRCR_RCB0_Msk                (_UINT32_(0x1) << HMATRIXB_MRCR_RCB0_Pos)            /* (HMATRIXB_MRCR) Remap Command Bit for Master 0 Mask */
#define HMATRIXB_MRCR_RCB0(value)             (HMATRIXB_MRCR_RCB0_Msk & (_UINT32_(value) << HMATRIXB_MRCR_RCB0_Pos)) /* Assigment of value for RCB0 in the HMATRIXB_MRCR register */
#define   HMATRIXB_MRCR_RCB0_DIS_Val          _UINT32_(0x0)                                        /* (HMATRIXB_MRCR) Disable remapped address decoding for master  */
#define   HMATRIXB_MRCR_RCB0_ENA_Val          _UINT32_(0x1)                                        /* (HMATRIXB_MRCR) Enable remapped address decoding for master  */
#define HMATRIXB_MRCR_RCB0_DIS                (HMATRIXB_MRCR_RCB0_DIS_Val << HMATRIXB_MRCR_RCB0_Pos) /* (HMATRIXB_MRCR) Disable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB0_ENA                (HMATRIXB_MRCR_RCB0_ENA_Val << HMATRIXB_MRCR_RCB0_Pos) /* (HMATRIXB_MRCR) Enable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB1_Pos                _UINT32_(1)                                          /* (HMATRIXB_MRCR) Remap Command Bit for Master 1 Position */
#define HMATRIXB_MRCR_RCB1_Msk                (_UINT32_(0x1) << HMATRIXB_MRCR_RCB1_Pos)            /* (HMATRIXB_MRCR) Remap Command Bit for Master 1 Mask */
#define HMATRIXB_MRCR_RCB1(value)             (HMATRIXB_MRCR_RCB1_Msk & (_UINT32_(value) << HMATRIXB_MRCR_RCB1_Pos)) /* Assigment of value for RCB1 in the HMATRIXB_MRCR register */
#define   HMATRIXB_MRCR_RCB1_DIS_Val          _UINT32_(0x0)                                        /* (HMATRIXB_MRCR) Disable remapped address decoding for master  */
#define   HMATRIXB_MRCR_RCB1_ENA_Val          _UINT32_(0x1)                                        /* (HMATRIXB_MRCR) Enable remapped address decoding for master  */
#define HMATRIXB_MRCR_RCB1_DIS                (HMATRIXB_MRCR_RCB1_DIS_Val << HMATRIXB_MRCR_RCB1_Pos) /* (HMATRIXB_MRCR) Disable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB1_ENA                (HMATRIXB_MRCR_RCB1_ENA_Val << HMATRIXB_MRCR_RCB1_Pos) /* (HMATRIXB_MRCR) Enable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB2_Pos                _UINT32_(2)                                          /* (HMATRIXB_MRCR) Remap Command Bit for Master 2 Position */
#define HMATRIXB_MRCR_RCB2_Msk                (_UINT32_(0x1) << HMATRIXB_MRCR_RCB2_Pos)            /* (HMATRIXB_MRCR) Remap Command Bit for Master 2 Mask */
#define HMATRIXB_MRCR_RCB2(value)             (HMATRIXB_MRCR_RCB2_Msk & (_UINT32_(value) << HMATRIXB_MRCR_RCB2_Pos)) /* Assigment of value for RCB2 in the HMATRIXB_MRCR register */
#define   HMATRIXB_MRCR_RCB2_DIS_Val          _UINT32_(0x0)                                        /* (HMATRIXB_MRCR) Disable remapped address decoding for master  */
#define   HMATRIXB_MRCR_RCB2_ENA_Val          _UINT32_(0x1)                                        /* (HMATRIXB_MRCR) Enable remapped address decoding for master  */
#define HMATRIXB_MRCR_RCB2_DIS                (HMATRIXB_MRCR_RCB2_DIS_Val << HMATRIXB_MRCR_RCB2_Pos) /* (HMATRIXB_MRCR) Disable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB2_ENA                (HMATRIXB_MRCR_RCB2_ENA_Val << HMATRIXB_MRCR_RCB2_Pos) /* (HMATRIXB_MRCR) Enable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB3_Pos                _UINT32_(3)                                          /* (HMATRIXB_MRCR) Remap Command Bit for Master 3 Position */
#define HMATRIXB_MRCR_RCB3_Msk                (_UINT32_(0x1) << HMATRIXB_MRCR_RCB3_Pos)            /* (HMATRIXB_MRCR) Remap Command Bit for Master 3 Mask */
#define HMATRIXB_MRCR_RCB3(value)             (HMATRIXB_MRCR_RCB3_Msk & (_UINT32_(value) << HMATRIXB_MRCR_RCB3_Pos)) /* Assigment of value for RCB3 in the HMATRIXB_MRCR register */
#define   HMATRIXB_MRCR_RCB3_DIS_Val          _UINT32_(0x0)                                        /* (HMATRIXB_MRCR) Disable remapped address decoding for master  */
#define   HMATRIXB_MRCR_RCB3_ENA_Val          _UINT32_(0x1)                                        /* (HMATRIXB_MRCR) Enable remapped address decoding for master  */
#define HMATRIXB_MRCR_RCB3_DIS                (HMATRIXB_MRCR_RCB3_DIS_Val << HMATRIXB_MRCR_RCB3_Pos) /* (HMATRIXB_MRCR) Disable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB3_ENA                (HMATRIXB_MRCR_RCB3_ENA_Val << HMATRIXB_MRCR_RCB3_Pos) /* (HMATRIXB_MRCR) Enable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB4_Pos                _UINT32_(4)                                          /* (HMATRIXB_MRCR) Remap Command Bit for Master 4 Position */
#define HMATRIXB_MRCR_RCB4_Msk                (_UINT32_(0x1) << HMATRIXB_MRCR_RCB4_Pos)            /* (HMATRIXB_MRCR) Remap Command Bit for Master 4 Mask */
#define HMATRIXB_MRCR_RCB4(value)             (HMATRIXB_MRCR_RCB4_Msk & (_UINT32_(value) << HMATRIXB_MRCR_RCB4_Pos)) /* Assigment of value for RCB4 in the HMATRIXB_MRCR register */
#define   HMATRIXB_MRCR_RCB4_DIS_Val          _UINT32_(0x0)                                        /* (HMATRIXB_MRCR) Disable remapped address decoding for master  */
#define   HMATRIXB_MRCR_RCB4_ENA_Val          _UINT32_(0x1)                                        /* (HMATRIXB_MRCR) Enable remapped address decoding for master  */
#define HMATRIXB_MRCR_RCB4_DIS                (HMATRIXB_MRCR_RCB4_DIS_Val << HMATRIXB_MRCR_RCB4_Pos) /* (HMATRIXB_MRCR) Disable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB4_ENA                (HMATRIXB_MRCR_RCB4_ENA_Val << HMATRIXB_MRCR_RCB4_Pos) /* (HMATRIXB_MRCR) Enable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB5_Pos                _UINT32_(5)                                          /* (HMATRIXB_MRCR) Remap Command Bit for Master 5 Position */
#define HMATRIXB_MRCR_RCB5_Msk                (_UINT32_(0x1) << HMATRIXB_MRCR_RCB5_Pos)            /* (HMATRIXB_MRCR) Remap Command Bit for Master 5 Mask */
#define HMATRIXB_MRCR_RCB5(value)             (HMATRIXB_MRCR_RCB5_Msk & (_UINT32_(value) << HMATRIXB_MRCR_RCB5_Pos)) /* Assigment of value for RCB5 in the HMATRIXB_MRCR register */
#define   HMATRIXB_MRCR_RCB5_DIS_Val          _UINT32_(0x0)                                        /* (HMATRIXB_MRCR) Disable remapped address decoding for master  */
#define   HMATRIXB_MRCR_RCB5_ENA_Val          _UINT32_(0x1)                                        /* (HMATRIXB_MRCR) Enable remapped address decoding for master  */
#define HMATRIXB_MRCR_RCB5_DIS                (HMATRIXB_MRCR_RCB5_DIS_Val << HMATRIXB_MRCR_RCB5_Pos) /* (HMATRIXB_MRCR) Disable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB5_ENA                (HMATRIXB_MRCR_RCB5_ENA_Val << HMATRIXB_MRCR_RCB5_Pos) /* (HMATRIXB_MRCR) Enable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB6_Pos                _UINT32_(6)                                          /* (HMATRIXB_MRCR) Remap Command Bit for Master 6 Position */
#define HMATRIXB_MRCR_RCB6_Msk                (_UINT32_(0x1) << HMATRIXB_MRCR_RCB6_Pos)            /* (HMATRIXB_MRCR) Remap Command Bit for Master 6 Mask */
#define HMATRIXB_MRCR_RCB6(value)             (HMATRIXB_MRCR_RCB6_Msk & (_UINT32_(value) << HMATRIXB_MRCR_RCB6_Pos)) /* Assigment of value for RCB6 in the HMATRIXB_MRCR register */
#define   HMATRIXB_MRCR_RCB6_DIS_Val          _UINT32_(0x0)                                        /* (HMATRIXB_MRCR) Disable remapped address decoding for master  */
#define   HMATRIXB_MRCR_RCB6_ENA_Val          _UINT32_(0x1)                                        /* (HMATRIXB_MRCR) Enable remapped address decoding for master  */
#define HMATRIXB_MRCR_RCB6_DIS                (HMATRIXB_MRCR_RCB6_DIS_Val << HMATRIXB_MRCR_RCB6_Pos) /* (HMATRIXB_MRCR) Disable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB6_ENA                (HMATRIXB_MRCR_RCB6_ENA_Val << HMATRIXB_MRCR_RCB6_Pos) /* (HMATRIXB_MRCR) Enable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB7_Pos                _UINT32_(7)                                          /* (HMATRIXB_MRCR) Remap Command Bit for Master 7 Position */
#define HMATRIXB_MRCR_RCB7_Msk                (_UINT32_(0x1) << HMATRIXB_MRCR_RCB7_Pos)            /* (HMATRIXB_MRCR) Remap Command Bit for Master 7 Mask */
#define HMATRIXB_MRCR_RCB7(value)             (HMATRIXB_MRCR_RCB7_Msk & (_UINT32_(value) << HMATRIXB_MRCR_RCB7_Pos)) /* Assigment of value for RCB7 in the HMATRIXB_MRCR register */
#define   HMATRIXB_MRCR_RCB7_DIS_Val          _UINT32_(0x0)                                        /* (HMATRIXB_MRCR) Disable remapped address decoding for master  */
#define   HMATRIXB_MRCR_RCB7_ENA_Val          _UINT32_(0x1)                                        /* (HMATRIXB_MRCR) Enable remapped address decoding for master  */
#define HMATRIXB_MRCR_RCB7_DIS                (HMATRIXB_MRCR_RCB7_DIS_Val << HMATRIXB_MRCR_RCB7_Pos) /* (HMATRIXB_MRCR) Disable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB7_ENA                (HMATRIXB_MRCR_RCB7_ENA_Val << HMATRIXB_MRCR_RCB7_Pos) /* (HMATRIXB_MRCR) Enable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB8_Pos                _UINT32_(8)                                          /* (HMATRIXB_MRCR) Remap Command Bit for Master 8 Position */
#define HMATRIXB_MRCR_RCB8_Msk                (_UINT32_(0x1) << HMATRIXB_MRCR_RCB8_Pos)            /* (HMATRIXB_MRCR) Remap Command Bit for Master 8 Mask */
#define HMATRIXB_MRCR_RCB8(value)             (HMATRIXB_MRCR_RCB8_Msk & (_UINT32_(value) << HMATRIXB_MRCR_RCB8_Pos)) /* Assigment of value for RCB8 in the HMATRIXB_MRCR register */
#define   HMATRIXB_MRCR_RCB8_DIS_Val          _UINT32_(0x0)                                        /* (HMATRIXB_MRCR) Disable remapped address decoding for master  */
#define   HMATRIXB_MRCR_RCB8_ENA_Val          _UINT32_(0x1)                                        /* (HMATRIXB_MRCR) Enable remapped address decoding for master  */
#define HMATRIXB_MRCR_RCB8_DIS                (HMATRIXB_MRCR_RCB8_DIS_Val << HMATRIXB_MRCR_RCB8_Pos) /* (HMATRIXB_MRCR) Disable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB8_ENA                (HMATRIXB_MRCR_RCB8_ENA_Val << HMATRIXB_MRCR_RCB8_Pos) /* (HMATRIXB_MRCR) Enable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB9_Pos                _UINT32_(9)                                          /* (HMATRIXB_MRCR) Remap Command Bit for Master 9 Position */
#define HMATRIXB_MRCR_RCB9_Msk                (_UINT32_(0x1) << HMATRIXB_MRCR_RCB9_Pos)            /* (HMATRIXB_MRCR) Remap Command Bit for Master 9 Mask */
#define HMATRIXB_MRCR_RCB9(value)             (HMATRIXB_MRCR_RCB9_Msk & (_UINT32_(value) << HMATRIXB_MRCR_RCB9_Pos)) /* Assigment of value for RCB9 in the HMATRIXB_MRCR register */
#define   HMATRIXB_MRCR_RCB9_DIS_Val          _UINT32_(0x0)                                        /* (HMATRIXB_MRCR) Disable remapped address decoding for master  */
#define   HMATRIXB_MRCR_RCB9_ENA_Val          _UINT32_(0x1)                                        /* (HMATRIXB_MRCR) Enable remapped address decoding for master  */
#define HMATRIXB_MRCR_RCB9_DIS                (HMATRIXB_MRCR_RCB9_DIS_Val << HMATRIXB_MRCR_RCB9_Pos) /* (HMATRIXB_MRCR) Disable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB9_ENA                (HMATRIXB_MRCR_RCB9_ENA_Val << HMATRIXB_MRCR_RCB9_Pos) /* (HMATRIXB_MRCR) Enable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB10_Pos               _UINT32_(10)                                         /* (HMATRIXB_MRCR) Remap Command Bit for Master 10 Position */
#define HMATRIXB_MRCR_RCB10_Msk               (_UINT32_(0x1) << HMATRIXB_MRCR_RCB10_Pos)           /* (HMATRIXB_MRCR) Remap Command Bit for Master 10 Mask */
#define HMATRIXB_MRCR_RCB10(value)            (HMATRIXB_MRCR_RCB10_Msk & (_UINT32_(value) << HMATRIXB_MRCR_RCB10_Pos)) /* Assigment of value for RCB10 in the HMATRIXB_MRCR register */
#define   HMATRIXB_MRCR_RCB10_DIS_Val         _UINT32_(0x0)                                        /* (HMATRIXB_MRCR) Disable remapped address decoding for master  */
#define   HMATRIXB_MRCR_RCB10_ENA_Val         _UINT32_(0x1)                                        /* (HMATRIXB_MRCR) Enable remapped address decoding for master  */
#define HMATRIXB_MRCR_RCB10_DIS               (HMATRIXB_MRCR_RCB10_DIS_Val << HMATRIXB_MRCR_RCB10_Pos) /* (HMATRIXB_MRCR) Disable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB10_ENA               (HMATRIXB_MRCR_RCB10_ENA_Val << HMATRIXB_MRCR_RCB10_Pos) /* (HMATRIXB_MRCR) Enable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB11_Pos               _UINT32_(11)                                         /* (HMATRIXB_MRCR) Remap Command Bit for Master 11 Position */
#define HMATRIXB_MRCR_RCB11_Msk               (_UINT32_(0x1) << HMATRIXB_MRCR_RCB11_Pos)           /* (HMATRIXB_MRCR) Remap Command Bit for Master 11 Mask */
#define HMATRIXB_MRCR_RCB11(value)            (HMATRIXB_MRCR_RCB11_Msk & (_UINT32_(value) << HMATRIXB_MRCR_RCB11_Pos)) /* Assigment of value for RCB11 in the HMATRIXB_MRCR register */
#define   HMATRIXB_MRCR_RCB11_DIS_Val         _UINT32_(0x0)                                        /* (HMATRIXB_MRCR) Disable remapped address decoding for master  */
#define   HMATRIXB_MRCR_RCB11_ENA_Val         _UINT32_(0x1)                                        /* (HMATRIXB_MRCR) Enable remapped address decoding for master  */
#define HMATRIXB_MRCR_RCB11_DIS               (HMATRIXB_MRCR_RCB11_DIS_Val << HMATRIXB_MRCR_RCB11_Pos) /* (HMATRIXB_MRCR) Disable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB11_ENA               (HMATRIXB_MRCR_RCB11_ENA_Val << HMATRIXB_MRCR_RCB11_Pos) /* (HMATRIXB_MRCR) Enable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB12_Pos               _UINT32_(12)                                         /* (HMATRIXB_MRCR) Remap Command Bit for Master 12 Position */
#define HMATRIXB_MRCR_RCB12_Msk               (_UINT32_(0x1) << HMATRIXB_MRCR_RCB12_Pos)           /* (HMATRIXB_MRCR) Remap Command Bit for Master 12 Mask */
#define HMATRIXB_MRCR_RCB12(value)            (HMATRIXB_MRCR_RCB12_Msk & (_UINT32_(value) << HMATRIXB_MRCR_RCB12_Pos)) /* Assigment of value for RCB12 in the HMATRIXB_MRCR register */
#define   HMATRIXB_MRCR_RCB12_DIS_Val         _UINT32_(0x0)                                        /* (HMATRIXB_MRCR) Disable remapped address decoding for master  */
#define   HMATRIXB_MRCR_RCB12_ENA_Val         _UINT32_(0x1)                                        /* (HMATRIXB_MRCR) Enable remapped address decoding for master  */
#define HMATRIXB_MRCR_RCB12_DIS               (HMATRIXB_MRCR_RCB12_DIS_Val << HMATRIXB_MRCR_RCB12_Pos) /* (HMATRIXB_MRCR) Disable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB12_ENA               (HMATRIXB_MRCR_RCB12_ENA_Val << HMATRIXB_MRCR_RCB12_Pos) /* (HMATRIXB_MRCR) Enable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB13_Pos               _UINT32_(13)                                         /* (HMATRIXB_MRCR) Remap Command Bit for Master 13 Position */
#define HMATRIXB_MRCR_RCB13_Msk               (_UINT32_(0x1) << HMATRIXB_MRCR_RCB13_Pos)           /* (HMATRIXB_MRCR) Remap Command Bit for Master 13 Mask */
#define HMATRIXB_MRCR_RCB13(value)            (HMATRIXB_MRCR_RCB13_Msk & (_UINT32_(value) << HMATRIXB_MRCR_RCB13_Pos)) /* Assigment of value for RCB13 in the HMATRIXB_MRCR register */
#define   HMATRIXB_MRCR_RCB13_DIS_Val         _UINT32_(0x0)                                        /* (HMATRIXB_MRCR) Disable remapped address decoding for master  */
#define   HMATRIXB_MRCR_RCB13_ENA_Val         _UINT32_(0x1)                                        /* (HMATRIXB_MRCR) Enable remapped address decoding for master  */
#define HMATRIXB_MRCR_RCB13_DIS               (HMATRIXB_MRCR_RCB13_DIS_Val << HMATRIXB_MRCR_RCB13_Pos) /* (HMATRIXB_MRCR) Disable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB13_ENA               (HMATRIXB_MRCR_RCB13_ENA_Val << HMATRIXB_MRCR_RCB13_Pos) /* (HMATRIXB_MRCR) Enable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB14_Pos               _UINT32_(14)                                         /* (HMATRIXB_MRCR) Remap Command Bit for Master 14 Position */
#define HMATRIXB_MRCR_RCB14_Msk               (_UINT32_(0x1) << HMATRIXB_MRCR_RCB14_Pos)           /* (HMATRIXB_MRCR) Remap Command Bit for Master 14 Mask */
#define HMATRIXB_MRCR_RCB14(value)            (HMATRIXB_MRCR_RCB14_Msk & (_UINT32_(value) << HMATRIXB_MRCR_RCB14_Pos)) /* Assigment of value for RCB14 in the HMATRIXB_MRCR register */
#define   HMATRIXB_MRCR_RCB14_DIS_Val         _UINT32_(0x0)                                        /* (HMATRIXB_MRCR) Disable remapped address decoding for master  */
#define   HMATRIXB_MRCR_RCB14_ENA_Val         _UINT32_(0x1)                                        /* (HMATRIXB_MRCR) Enable remapped address decoding for master  */
#define HMATRIXB_MRCR_RCB14_DIS               (HMATRIXB_MRCR_RCB14_DIS_Val << HMATRIXB_MRCR_RCB14_Pos) /* (HMATRIXB_MRCR) Disable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB14_ENA               (HMATRIXB_MRCR_RCB14_ENA_Val << HMATRIXB_MRCR_RCB14_Pos) /* (HMATRIXB_MRCR) Enable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB15_Pos               _UINT32_(15)                                         /* (HMATRIXB_MRCR) Remap Command Bit for Master 15 Position */
#define HMATRIXB_MRCR_RCB15_Msk               (_UINT32_(0x1) << HMATRIXB_MRCR_RCB15_Pos)           /* (HMATRIXB_MRCR) Remap Command Bit for Master 15 Mask */
#define HMATRIXB_MRCR_RCB15(value)            (HMATRIXB_MRCR_RCB15_Msk & (_UINT32_(value) << HMATRIXB_MRCR_RCB15_Pos)) /* Assigment of value for RCB15 in the HMATRIXB_MRCR register */
#define   HMATRIXB_MRCR_RCB15_DIS_Val         _UINT32_(0x0)                                        /* (HMATRIXB_MRCR) Disable remapped address decoding for master  */
#define   HMATRIXB_MRCR_RCB15_ENA_Val         _UINT32_(0x1)                                        /* (HMATRIXB_MRCR) Enable remapped address decoding for master  */
#define HMATRIXB_MRCR_RCB15_DIS               (HMATRIXB_MRCR_RCB15_DIS_Val << HMATRIXB_MRCR_RCB15_Pos) /* (HMATRIXB_MRCR) Disable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_RCB15_ENA               (HMATRIXB_MRCR_RCB15_ENA_Val << HMATRIXB_MRCR_RCB15_Pos) /* (HMATRIXB_MRCR) Enable remapped address decoding for master Position  */
#define HMATRIXB_MRCR_Msk                     _UINT32_(0x0000FFFF)                                 /* (HMATRIXB_MRCR) Register Mask  */

#define HMATRIXB_MRCR_RCB_Pos                 _UINT32_(0)                                          /* (HMATRIXB_MRCR Position) Remap Command Bit for Master x5 */
#define HMATRIXB_MRCR_RCB_Msk                 (_UINT32_(0xFFFF) << HMATRIXB_MRCR_RCB_Pos)          /* (HMATRIXB_MRCR Mask) RCB */
#define HMATRIXB_MRCR_RCB(value)              (HMATRIXB_MRCR_RCB_Msk & (_UINT32_(value) << HMATRIXB_MRCR_RCB_Pos)) 

/* -------- HMATRIXB_SFR : (HMATRIXB Offset: 0x110) (R/W 32) Special Function -------- */
#define HMATRIXB_SFR_RESETVALUE               _UINT32_(0x00)                                       /*  (HMATRIXB_SFR) Special Function  Reset Value */

#define HMATRIXB_SFR_SFR_Pos                  _UINT32_(0)                                          /* (HMATRIXB_SFR) Special Function Register Position */
#define HMATRIXB_SFR_SFR_Msk                  (_UINT32_(0xFFFFFFFF) << HMATRIXB_SFR_SFR_Pos)       /* (HMATRIXB_SFR) Special Function Register Mask */
#define HMATRIXB_SFR_SFR(value)               (HMATRIXB_SFR_SFR_Msk & (_UINT32_(value) << HMATRIXB_SFR_SFR_Pos)) /* Assigment of value for SFR in the HMATRIXB_SFR register */
#define HMATRIXB_SFR_Msk                      _UINT32_(0xFFFFFFFF)                                 /* (HMATRIXB_SFR) Register Mask  */


/** \brief HMATRIXB register offsets definitions */
#define HMATRIXB_PRAS_REG_OFST         _UINT32_(0x00)      /* (HMATRIXB_PRAS) Priority A for Slave Offset */
#define HMATRIXB_PRBS_REG_OFST         _UINT32_(0x04)      /* (HMATRIXB_PRBS) Priority B for Slave Offset */
#define HMATRIXB_MCFG_REG_OFST         _UINT32_(0x00)      /* (HMATRIXB_MCFG) Master Configuration Offset */
#define HMATRIXB_MCFG0_REG_OFST        _UINT32_(0x00)      /* (HMATRIXB_MCFG0) Master Configuration Offset */
#define HMATRIXB_MCFG1_REG_OFST        _UINT32_(0x04)      /* (HMATRIXB_MCFG1) Master Configuration Offset */
#define HMATRIXB_MCFG2_REG_OFST        _UINT32_(0x08)      /* (HMATRIXB_MCFG2) Master Configuration Offset */
#define HMATRIXB_MCFG3_REG_OFST        _UINT32_(0x0C)      /* (HMATRIXB_MCFG3) Master Configuration Offset */
#define HMATRIXB_MCFG4_REG_OFST        _UINT32_(0x10)      /* (HMATRIXB_MCFG4) Master Configuration Offset */
#define HMATRIXB_MCFG5_REG_OFST        _UINT32_(0x14)      /* (HMATRIXB_MCFG5) Master Configuration Offset */
#define HMATRIXB_MCFG6_REG_OFST        _UINT32_(0x18)      /* (HMATRIXB_MCFG6) Master Configuration Offset */
#define HMATRIXB_MCFG7_REG_OFST        _UINT32_(0x1C)      /* (HMATRIXB_MCFG7) Master Configuration Offset */
#define HMATRIXB_MCFG8_REG_OFST        _UINT32_(0x20)      /* (HMATRIXB_MCFG8) Master Configuration Offset */
#define HMATRIXB_MCFG9_REG_OFST        _UINT32_(0x24)      /* (HMATRIXB_MCFG9) Master Configuration Offset */
#define HMATRIXB_MCFG10_REG_OFST       _UINT32_(0x28)      /* (HMATRIXB_MCFG10) Master Configuration Offset */
#define HMATRIXB_MCFG11_REG_OFST       _UINT32_(0x2C)      /* (HMATRIXB_MCFG11) Master Configuration Offset */
#define HMATRIXB_MCFG12_REG_OFST       _UINT32_(0x30)      /* (HMATRIXB_MCFG12) Master Configuration Offset */
#define HMATRIXB_MCFG13_REG_OFST       _UINT32_(0x34)      /* (HMATRIXB_MCFG13) Master Configuration Offset */
#define HMATRIXB_MCFG14_REG_OFST       _UINT32_(0x38)      /* (HMATRIXB_MCFG14) Master Configuration Offset */
#define HMATRIXB_MCFG15_REG_OFST       _UINT32_(0x3C)      /* (HMATRIXB_MCFG15) Master Configuration Offset */
#define HMATRIXB_SCFG_REG_OFST         _UINT32_(0x40)      /* (HMATRIXB_SCFG) Slave Configuration Offset */
#define HMATRIXB_SCFG0_REG_OFST        _UINT32_(0x40)      /* (HMATRIXB_SCFG0) Slave Configuration Offset */
#define HMATRIXB_SCFG1_REG_OFST        _UINT32_(0x44)      /* (HMATRIXB_SCFG1) Slave Configuration Offset */
#define HMATRIXB_SCFG2_REG_OFST        _UINT32_(0x48)      /* (HMATRIXB_SCFG2) Slave Configuration Offset */
#define HMATRIXB_SCFG3_REG_OFST        _UINT32_(0x4C)      /* (HMATRIXB_SCFG3) Slave Configuration Offset */
#define HMATRIXB_SCFG4_REG_OFST        _UINT32_(0x50)      /* (HMATRIXB_SCFG4) Slave Configuration Offset */
#define HMATRIXB_SCFG5_REG_OFST        _UINT32_(0x54)      /* (HMATRIXB_SCFG5) Slave Configuration Offset */
#define HMATRIXB_SCFG6_REG_OFST        _UINT32_(0x58)      /* (HMATRIXB_SCFG6) Slave Configuration Offset */
#define HMATRIXB_SCFG7_REG_OFST        _UINT32_(0x5C)      /* (HMATRIXB_SCFG7) Slave Configuration Offset */
#define HMATRIXB_SCFG8_REG_OFST        _UINT32_(0x60)      /* (HMATRIXB_SCFG8) Slave Configuration Offset */
#define HMATRIXB_SCFG9_REG_OFST        _UINT32_(0x64)      /* (HMATRIXB_SCFG9) Slave Configuration Offset */
#define HMATRIXB_SCFG10_REG_OFST       _UINT32_(0x68)      /* (HMATRIXB_SCFG10) Slave Configuration Offset */
#define HMATRIXB_SCFG11_REG_OFST       _UINT32_(0x6C)      /* (HMATRIXB_SCFG11) Slave Configuration Offset */
#define HMATRIXB_SCFG12_REG_OFST       _UINT32_(0x70)      /* (HMATRIXB_SCFG12) Slave Configuration Offset */
#define HMATRIXB_SCFG13_REG_OFST       _UINT32_(0x74)      /* (HMATRIXB_SCFG13) Slave Configuration Offset */
#define HMATRIXB_SCFG14_REG_OFST       _UINT32_(0x78)      /* (HMATRIXB_SCFG14) Slave Configuration Offset */
#define HMATRIXB_SCFG15_REG_OFST       _UINT32_(0x7C)      /* (HMATRIXB_SCFG15) Slave Configuration Offset */
#define HMATRIXB_MRCR_REG_OFST         _UINT32_(0x100)     /* (HMATRIXB_MRCR) Master Remap Control Offset */
#define HMATRIXB_SFR_REG_OFST          _UINT32_(0x110)     /* (HMATRIXB_SFR) Special Function Offset */
#define HMATRIXB_SFR0_REG_OFST         _UINT32_(0x110)     /* (HMATRIXB_SFR0) Special Function Offset */
#define HMATRIXB_SFR1_REG_OFST         _UINT32_(0x114)     /* (HMATRIXB_SFR1) Special Function Offset */
#define HMATRIXB_SFR2_REG_OFST         _UINT32_(0x118)     /* (HMATRIXB_SFR2) Special Function Offset */
#define HMATRIXB_SFR3_REG_OFST         _UINT32_(0x11C)     /* (HMATRIXB_SFR3) Special Function Offset */
#define HMATRIXB_SFR4_REG_OFST         _UINT32_(0x120)     /* (HMATRIXB_SFR4) Special Function Offset */
#define HMATRIXB_SFR5_REG_OFST         _UINT32_(0x124)     /* (HMATRIXB_SFR5) Special Function Offset */
#define HMATRIXB_SFR6_REG_OFST         _UINT32_(0x128)     /* (HMATRIXB_SFR6) Special Function Offset */
#define HMATRIXB_SFR7_REG_OFST         _UINT32_(0x12C)     /* (HMATRIXB_SFR7) Special Function Offset */
#define HMATRIXB_SFR8_REG_OFST         _UINT32_(0x130)     /* (HMATRIXB_SFR8) Special Function Offset */
#define HMATRIXB_SFR9_REG_OFST         _UINT32_(0x134)     /* (HMATRIXB_SFR9) Special Function Offset */
#define HMATRIXB_SFR10_REG_OFST        _UINT32_(0x138)     /* (HMATRIXB_SFR10) Special Function Offset */
#define HMATRIXB_SFR11_REG_OFST        _UINT32_(0x13C)     /* (HMATRIXB_SFR11) Special Function Offset */
#define HMATRIXB_SFR12_REG_OFST        _UINT32_(0x140)     /* (HMATRIXB_SFR12) Special Function Offset */
#define HMATRIXB_SFR13_REG_OFST        _UINT32_(0x144)     /* (HMATRIXB_SFR13) Special Function Offset */
#define HMATRIXB_SFR14_REG_OFST        _UINT32_(0x148)     /* (HMATRIXB_SFR14) Special Function Offset */
#define HMATRIXB_SFR15_REG_OFST        _UINT32_(0x14C)     /* (HMATRIXB_SFR15) Special Function Offset */

#if !(defined(__ASSEMBLER__) || defined(__IAR_SYSTEMS_ASM__))
/** \brief PRS register API structure */
typedef struct
{
  __IO  uint32_t                       HMATRIXB_PRAS;      /**< Offset: 0x00 (R/W  32) Priority A for Slave */
  __IO  uint32_t                       HMATRIXB_PRBS;      /**< Offset: 0x04 (R/W  32) Priority B for Slave */
} hmatrixb_prs_registers_t;

#define HMATRIXB_PRS_NUMBER 4

/** \brief HMATRIXB register API structure */
typedef struct
{  /* HSB Matrix */
  __IO  uint32_t                       HMATRIXB_MCFG[16];  /**< Offset: 0x00 (R/W  32) Master Configuration */
  __IO  uint32_t                       HMATRIXB_SCFG[16];  /**< Offset: 0x40 (R/W  32) Slave Configuration */
        hmatrixb_prs_registers_t       PRS[HMATRIXB_PRS_NUMBER]; /**< Offset: 0x80  */
  __I   uint8_t                        Reserved1[0x60];
  __IO  uint32_t                       HMATRIXB_MRCR;      /**< Offset: 0x100 (R/W  32) Master Remap Control */
  __I   uint8_t                        Reserved2[0x0C];
  __IO  uint32_t                       HMATRIXB_SFR[16];   /**< Offset: 0x110 (R/W  32) Special Function */
} hmatrixb_registers_t;


#endif /* !(defined(__ASSEMBLER__) || defined(__IAR_SYSTEMS_ASM__)) */
#endif /* _PIC32CMMC00_HMATRIXB_COMPONENT_H_ */
