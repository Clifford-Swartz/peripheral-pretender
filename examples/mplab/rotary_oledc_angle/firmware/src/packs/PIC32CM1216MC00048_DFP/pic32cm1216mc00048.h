/*
 * Header file for PIC32CM1216MC00048
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

/* File generated from device description version 2021-03-23T20:32:05Z */
#ifndef _PIC32CM1216MC00048_H_
#define _PIC32CM1216MC00048_H_

/* Header version uses Semantic Versioning 2.0.0 (https://semver.org/) */
#define HEADER_FORMAT_VERSION "2.1.0"

#define HEADER_FORMAT_VERSION_MAJOR (2)
#define HEADER_FORMAT_VERSION_MINOR (1)
#define HEADER_FORMAT_VERSION_PATCH (0)

/* PIC32CM1216MC00048 definitions
  This file defines all structures and symbols for PIC32CM1216MC00048:
    - registers and bitfields
    - peripheral base address
    - peripheral ID
    - PIO definitions
*/

#ifdef __cplusplus
 extern "C" {
#endif

#if !(defined(__ASSEMBLER__) || defined(__IAR_SYSTEMS_ASM__))
#  include <stdint.h>
#endif /* !(defined(__ASSEMBLER__) || defined(__IAR_SYSTEMS_ASM__)) */

#if !defined(SKIP_INTEGER_LITERALS)
#  if defined(_UINT8_) || defined(_UINT16_) || defined(_UINT32_)
#    error "Integer constant value macros already defined elsewhere"
#  endif

#if !(defined(__ASSEMBLER__) || defined(__IAR_SYSTEMS_ASM__))
/* Macros that deal with sizes of integer constants for C/C++ */
#  define _UINT8_(x)   ((uint8_t)(x))    /* C code: 8-bits unsigned integer constant value */
#  define _UINT16_(x)  ((uint16_t)(x))   /* C code: 16-bits unsigned integer constant value */
#  define _UINT32_(x)  ((uint32_t)(x))   /* C code: 32-bits unsigned integer constant value */

#else /* Assembler */

#  define _UINT8_(x) x    /* Assembler: 8-bits unsigned integer constant value */
#  define _UINT16_(x) x   /* Assembler: 16-bits unsigned integer constant value */
#  define _UINT32_(x) x   /* Assembler: 32-bits unsigned integer constant value */
#endif /* !(defined(__ASSEMBLER__) || defined(__IAR_SYSTEMS_ASM__)) */
#endif /* SKIP_INTEGER_LITERALS */

/* ************************************************************************** */
/* CMSIS DEFINITIONS FOR PIC32CM1216MC00048                                 */
/* ************************************************************************** */
#if !(defined(__ASSEMBLER__) || defined(__IAR_SYSTEMS_ASM__))
/* Interrupt Number Definition */
typedef enum IRQn
{
/******  CORTEX-M0PLUS Processor Exceptions Numbers ******************************/
  Reset_IRQn                = -15, /* -15 Reset Vector, invoked on Power up and warm reset */
  NonMaskableInt_IRQn       = -14, /* -14 Non maskable Interrupt, cannot be stopped or preempted */
  HardFault_IRQn            = -13, /* -13 Hard Fault, all classes of Fault    */
  SVCall_IRQn               =  -5, /* -5  System Service Call via SVC instruction */
  PendSV_IRQn               =  -2, /* -2  Pendable request for system service */
  SysTick_IRQn              =  -1, /* -1  System Tick Timer                   */

/******  PIC32CM1216MC00048 specific Interrupt Numbers ***********************************/
  PM_IRQn                   =   0, /* 0   Shared between MCLK OSCCTRL OSC32KCTRL PAC PM SUPC (PM) */
  MCLK_IRQn                 =   0, /* 0   Shared between MCLK OSCCTRL OSC32KCTRL PAC PM SUPC (MCLK) */
  OSC32KCTRL_IRQn           =   0, /* 0   Shared between MCLK OSCCTRL OSC32KCTRL PAC PM SUPC (OSC32KCTRL) */
  OSCCTRL_IRQn              =   0, /* 0   Shared between MCLK OSCCTRL OSC32KCTRL PAC PM SUPC (OSCCTRL) */
  SUPC_IRQn                 =   0, /* 0   Shared between MCLK OSCCTRL OSC32KCTRL PAC PM SUPC (SUPC) */
  PAC_IRQn                  =   0, /* 0   Shared between MCLK OSCCTRL OSC32KCTRL PAC PM SUPC (PAC) */
  WDT_IRQn                  =   1, /* 1   Watchdog Timer (WDT)                */
  RTC_IRQn                  =   2, /* 2   Real-Time Counter (RTC)             */
  EIC_IRQn                  =   3, /* 3   External Interrupt Controller (EIC) */
  FREQM_IRQn                =   4, /* 4   Frequency Meter (FREQM)             */
  TSENS_IRQn                =   5, /* 5   Temperature Sensor (TSENS)          */
  NVMCTRL_IRQn              =   6, /* 6   Non-Volatile Memory Controller (NVMCTRL) */
  DMAC_IRQn                 =   7, /* 7   Direct Memory Access Controller (DMAC) */
  EVSYS_IRQn                =   8, /* 8   Event System Interface (EVSYS)      */
  SERCOM0_IRQn              =   9, /* 9   Serial Communication Interface (SERCOM0) */
  SERCOM1_IRQn              =  10, /* 10  Serial Communication Interface (SERCOM1) */
  SERCOM2_IRQn              =  11, /* 11  Serial Communication Interface (SERCOM2) */
  SERCOM3_IRQn              =  12, /* 12  Serial Communication Interface (SERCOM3) */
  TCC0_IRQn                 =  13, /* 13  Timer Counter for Control Applications (TCC0) */
  TCC1_IRQn                 =  14, /* 14  Timer Counter for Control Applications (TCC1) */
  TCC2_IRQn                 =  15, /* 15  Timer Counter for Control Applications (TCC2) */
  TC0_IRQn                  =  16, /* 16  Basic Timer Counter (TC0)           */
  TC1_IRQn                  =  17, /* 17  Basic Timer Counter (TC1)           */
  TC2_IRQn                  =  18, /* 18  Basic Timer Counter (TC2)           */
  TC3_IRQn                  =  19, /* 19  Basic Timer Counter (TC3)           */
  TC4_IRQn                  =  20, /* 20  Basic Timer Counter (TC4)           */
  ADC0_IRQn                 =  21, /* 21  Analog Digital Converter (ADC0)     */
  ADC1_IRQn                 =  22, /* 22  Analog Digital Converter (ADC1)     */
  AC_IRQn                   =  23, /* 23  Analog Comparators (AC)             */
  DAC_IRQn                  =  24, /* 24  Digital Analog Converter (DAC)      */
  SDADC_IRQn                =  25, /* 25  Sigma-Delta Analog Digital Converter (SDADC) */
  PDEC_IRQn                 =  26, /* 26  Quadrature Decodeur (PDEC)          */

  PERIPH_MAX_IRQn           =  26  /* Max peripheral ID */
} IRQn_Type;
#endif /* !(defined(__ASSEMBLER__) || defined(__IAR_SYSTEMS_ASM__)) */

#if !(defined(__ASSEMBLER__) || defined(__IAR_SYSTEMS_ASM__))
typedef struct _DeviceVectors
{
  /* Stack pointer */
  void* pvStack;
  /* CORTEX-M0PLUS handlers */
  void* pfnReset_Handler;                        /* -15 Reset Vector, invoked on Power up and warm reset */
  void* pfnNonMaskableInt_Handler;               /* -14 Non maskable Interrupt, cannot be stopped or preempted */
  void* pfnHardFault_Handler;                    /* -13 Hard Fault, all classes of Fault */
  void* pvReservedC12;
  void* pvReservedC11;
  void* pvReservedC10;
  void* pvReservedC9;
  void* pvReservedC8;
  void* pvReservedC7;
  void* pvReservedC6;
  void* pfnSVCall_Handler;                       /*  -5 System Service Call via SVC instruction */
  void* pvReservedC4;
  void* pvReservedC3;
  void* pfnPendSV_Handler;                       /*  -2 Pendable request for system service */
  void* pfnSysTick_Handler;                      /*  -1 System Tick Timer */

  /* Peripheral handlers */
  void* pfnSYSTEM_Handler;                       /*   0 System peripherals shared interrupt (MCLK OSCCTRL OSC32KCTRL PAC PM SUPC) */
  void* pfnWDT_Handler;                          /*   1 Watchdog Timer (WDT) */
  void* pfnRTC_Handler;                          /*   2 Real-Time Counter (RTC) */
  void* pfnEIC_Handler;                          /*   3 External Interrupt Controller (EIC) */
  void* pfnFREQM_Handler;                        /*   4 Frequency Meter (FREQM) */
  void* pfnTSENS_Handler;                        /*   5 Temperature Sensor (TSENS) */
  void* pfnNVMCTRL_Handler;                      /*   6 Non-Volatile Memory Controller (NVMCTRL) */
  void* pfnDMAC_Handler;                         /*   7 Direct Memory Access Controller (DMAC) */
  void* pfnEVSYS_Handler;                        /*   8 Event System Interface (EVSYS) */
  void* pfnSERCOM0_Handler;                      /*   9 Serial Communication Interface (SERCOM0) */
  void* pfnSERCOM1_Handler;                      /*  10 Serial Communication Interface (SERCOM1) */
  void* pfnSERCOM2_Handler;                      /*  11 Serial Communication Interface (SERCOM2) */
  void* pfnSERCOM3_Handler;                      /*  12 Serial Communication Interface (SERCOM3) */
  void* pfnTCC0_Handler;                         /*  13 Timer Counter for Control Applications (TCC0) */
  void* pfnTCC1_Handler;                         /*  14 Timer Counter for Control Applications (TCC1) */
  void* pfnTCC2_Handler;                         /*  15 Timer Counter for Control Applications (TCC2) */
  void* pfnTC0_Handler;                          /*  16 Basic Timer Counter (TC0) */
  void* pfnTC1_Handler;                          /*  17 Basic Timer Counter (TC1) */
  void* pfnTC2_Handler;                          /*  18 Basic Timer Counter (TC2) */
  void* pfnTC3_Handler;                          /*  19 Basic Timer Counter (TC3) */
  void* pfnTC4_Handler;                          /*  20 Basic Timer Counter (TC4) */
  void* pfnADC0_Handler;                         /*  21 Analog Digital Converter (ADC0) */
  void* pfnADC1_Handler;                         /*  22 Analog Digital Converter (ADC1) */
  void* pfnAC_Handler;                           /*  23 Analog Comparators (AC) */
  void* pfnDAC_Handler;                          /*  24 Digital Analog Converter (DAC) */
  void* pfnSDADC_Handler;                        /*  25 Sigma-Delta Analog Digital Converter (SDADC) */
  void* pfnPDEC_Handler;                         /*  26 Quadrature Decodeur (PDEC) */
} DeviceVectors;

#endif /* !(defined(__ASSEMBLER__) || defined(__IAR_SYSTEMS_ASM__)) */

#if !(defined(__ASSEMBLER__) || defined(__IAR_SYSTEMS_ASM__))
#if !defined DONT_USE_PREDEFINED_CORE_HANDLERS
/* CORTEX-M0PLUS exception handlers */
void Reset_Handler                 ( void );
void NonMaskableInt_Handler        ( void );
void HardFault_Handler             ( void );
void SVCall_Handler                ( void );
void PendSV_Handler                ( void );
void SysTick_Handler               ( void );
#endif /* DONT_USE_PREDEFINED_CORE_HANDLERS */

#if !defined DONT_USE_PREDEFINED_PERIPHERALS_HANDLERS
/* Peripherals interrupt handlers */
void SYSTEM_Handler                ( void );
void WDT_Handler                   ( void );
void RTC_Handler                   ( void );
void EIC_Handler                   ( void );
void FREQM_Handler                 ( void );
void TSENS_Handler                 ( void );
void NVMCTRL_Handler               ( void );
void DMAC_Handler                  ( void );
void EVSYS_Handler                 ( void );
void SERCOM0_Handler               ( void );
void SERCOM1_Handler               ( void );
void SERCOM2_Handler               ( void );
void SERCOM3_Handler               ( void );
void TCC0_Handler                  ( void );
void TCC1_Handler                  ( void );
void TCC2_Handler                  ( void );
void TC0_Handler                   ( void );
void TC1_Handler                   ( void );
void TC2_Handler                   ( void );
void TC3_Handler                   ( void );
void TC4_Handler                   ( void );
void ADC0_Handler                  ( void );
void ADC1_Handler                  ( void );
void AC_Handler                    ( void );
void DAC_Handler                   ( void );
void SDADC_Handler                 ( void );
void PDEC_Handler                  ( void );
#endif /* DONT_USE_PREDEFINED_PERIPHERALS_HANDLERS */
#endif /* !(defined(__ASSEMBLER__) || defined(__IAR_SYSTEMS_ASM__)) */

/* Configuration of the CORTEX-M0PLUS Processor and Core Peripherals */
#define __MPU_PRESENT                      1 /* MPU present or not                                                        */
#define __NVIC_PRIO_BITS                   2 /* Number of Bits used for Priority Levels                                   */
#define __VTOR_PRESENT                     1 /* Vector Table Offset Register present or not                               */
#define __Vendor_SysTickConfig             0 /* Set to 1 if different SysTick Config is used                              */
#define __ARCH_ARM                         1
#define __ARCH_ARM_CORTEX_M                1

/* CMSIS includes */
#include "core_cm0plus.h"
#if defined USE_CMSIS_INIT
#include "system_pic32cmmc00.h"
#endif /* USE_CMSIS_INIT */

/* ************************************************************************** */
/*   SOFTWARE PERIPHERAL API DEFINITION FOR PIC32CM1216MC00048                */
/* ************************************************************************** */
#include "component/ac.h"
#include "component/adc.h"
#include "component/ccl.h"
#include "component/dac.h"
#include "component/divas.h"
#include "component/dmac.h"
#include "component/dsu.h"
#include "component/eic.h"
#include "component/evsys.h"
#include "component/freqm.h"
#include "component/fuses.h"
#include "component/gclk.h"
#include "component/hmatrixb.h"
#include "component/mclk.h"
#include "component/mtb.h"
#include "component/nvmctrl.h"
#include "component/osc32kctrl.h"
#include "component/oscctrl.h"
#include "component/pac.h"
#include "component/pdec.h"
#include "component/pm.h"
#include "component/port.h"
#include "component/rstc.h"
#include "component/rtc.h"
#include "component/sdadc.h"
#include "component/sercom.h"
#include "component/supc.h"
#include "component/tc.h"
#include "component/tcc.h"
#include "component/tsens.h"
#include "component/wdt.h"

/* ************************************************************************** */
/*   INSTANCE DEFINITIONS FOR PIC32CM1216MC00048 */
/* ************************************************************************** */
#include "instance/ac.h"
#include "instance/adc0.h"
#include "instance/adc1.h"
#include "instance/ccl.h"
#include "instance/dac.h"
#include "instance/divas.h"
#include "instance/dmac.h"
#include "instance/dsu.h"
#include "instance/eic.h"
#include "instance/evsys.h"
#include "instance/freqm.h"
#include "instance/fuses.h"
#include "instance/gclk.h"
#include "instance/hmatrixhs.h"
#include "instance/mclk.h"
#include "instance/mtb.h"
#include "instance/nvmctrl.h"
#include "instance/osc32kctrl.h"
#include "instance/oscctrl.h"
#include "instance/pac.h"
#include "instance/pdec.h"
#include "instance/pm.h"
#include "instance/port.h"
#include "instance/rstc.h"
#include "instance/rtc.h"
#include "instance/sdadc.h"
#include "instance/sercom0.h"
#include "instance/sercom1.h"
#include "instance/sercom2.h"
#include "instance/sercom3.h"
#include "instance/supc.h"
#include "instance/tc0.h"
#include "instance/tc1.h"
#include "instance/tc2.h"
#include "instance/tc3.h"
#include "instance/tc4.h"
#include "instance/tcc0.h"
#include "instance/tcc1.h"
#include "instance/tcc2.h"
#include "instance/tsens.h"
#include "instance/wdt.h"

/* ************************************************************************** */
/*  PERIPHERAL ID DEFINITIONS FOR PIC32CM1216MC00048                          */
/* ************************************************************************** */
#define ID_PAC           (  0) /* Instance index for PAC (PAC) */
#define ID_PM            (  1) /* Instance index for PM (PM) */
#define ID_MCLK          (  2) /* Instance index for MCLK (MCLK) */
#define ID_RSTC          (  3) /* Instance index for RSTC (RSTC) */
#define ID_OSCCTRL       (  4) /* Instance index for OSCCTRL (OSCCTRL) */
#define ID_OSC32KCTRL    (  5) /* Instance index for OSC32KCTRL (OSC32KCTRL) */
#define ID_SUPC          (  6) /* Instance index for SUPC (SUPC) */
#define ID_GCLK          (  7) /* Instance index for GCLK (GCLK) */
#define ID_WDT           (  8) /* Instance index for WDT (WDT) */
#define ID_RTC           (  9) /* Instance index for RTC (RTC) */
#define ID_EIC           ( 10) /* Instance index for EIC (EIC) */
#define ID_FREQM         ( 11) /* Instance index for FREQM (FREQM) */
#define ID_TSENS         ( 12) /* Instance index for TSENS (TSENS) */
#define ID_PORT          ( 32) /* Instance index for PORT (PORT) */
#define ID_DSU           ( 33) /* Instance index for DSU (DSU) */
#define ID_NVMCTRL       ( 34) /* Instance index for NVMCTRL (NVMCTRL) */
#define ID_DMAC          ( 35) /* Instance index for DMAC (DMAC) */
#define ID_MTB           ( 36) /* Instance index for MTB (MTB) */
#define ID_HMATRIXHS     ( 37) /* Instance index for HMATRIXHS (HMATRIXHS) */
#define ID_EVSYS         ( 64) /* Instance index for EVSYS (EVSYS) */
#define ID_SERCOM0       ( 65) /* Instance index for SERCOM0 (SERCOM0) */
#define ID_SERCOM1       ( 66) /* Instance index for SERCOM1 (SERCOM1) */
#define ID_SERCOM2       ( 67) /* Instance index for SERCOM2 (SERCOM2) */
#define ID_SERCOM3       ( 68) /* Instance index for SERCOM3 (SERCOM3) */
#define ID_TCC0          ( 73) /* Instance index for TCC0 (TCC0) */
#define ID_TCC1          ( 74) /* Instance index for TCC1 (TCC1) */
#define ID_TCC2          ( 75) /* Instance index for TCC2 (TCC2) */
#define ID_TC0           ( 76) /* Instance index for TC0 (TC0) */
#define ID_TC1           ( 77) /* Instance index for TC1 (TC1) */
#define ID_TC2           ( 78) /* Instance index for TC2 (TC2) */
#define ID_TC3           ( 79) /* Instance index for TC3 (TC3) */
#define ID_TC4           ( 80) /* Instance index for TC4 (TC4) */
#define ID_ADC0          ( 81) /* Instance index for ADC0 (ADC0) */
#define ID_ADC1          ( 82) /* Instance index for ADC1 (ADC1) */
#define ID_SDADC         ( 83) /* Instance index for SDADC (SDADC) */
#define ID_AC            ( 84) /* Instance index for AC (AC) */
#define ID_DAC           ( 85) /* Instance index for DAC (DAC) */
#define ID_CCL           ( 87) /* Instance index for CCL (CCL) */
#define ID_PDEC          ( 90) /* Instance index for PDEC (PDEC) */

#define ID_PERIPH_MAX    ( 90) /* Number of peripheral IDs */

/* ************************************************************************** */
/*   REGISTER STRUCTURE ADDRESS DEFINITIONS FOR PIC32CM1216MC00048            */
/* ************************************************************************** */
#if !(defined(__ASSEMBLER__) || defined(__IAR_SYSTEMS_ASM__))
#define AC_REGS                          ((ac_registers_t*)0x42005000)                 /* AC Registers Address         */
#define ADC0_REGS                        ((adc_registers_t*)0x42004400)                /* ADC0 Registers Address       */
#define ADC1_REGS                        ((adc_registers_t*)0x42004800)                /* ADC1 Registers Address       */
#define CCL_REGS                         ((ccl_registers_t*)0x42005c00)                /* CCL Registers Address        */
#define DAC_REGS                         ((dac_registers_t*)0x42005400)                /* DAC Registers Address        */
#define DIVAS_REGS                       ((divas_registers_t*)0x48000000)              /* DIVAS Registers Address      */
#define DMAC_REGS                        ((dmac_registers_t*)0x41006000)               /* DMAC Registers Address       */
#define DSU_REGS                         ((dsu_registers_t*)0x41002000)                /* DSU Registers Address        */
#define EIC_REGS                         ((eic_registers_t*)0x40002800)                /* EIC Registers Address        */
#define EVSYS_REGS                       ((evsys_registers_t*)0x42000000)              /* EVSYS Registers Address      */
#define FREQM_REGS                       ((freqm_registers_t*)0x40002c00)              /* FREQM Registers Address      */
#define GCLK_REGS                        ((gclk_registers_t*)0x40001c00)               /* GCLK Registers Address       */
#define HMATRIXHS_REGS                   ((hmatrixb_registers_t*)0x4100a000)           /* HMATRIXHS Registers Address  */
#define MCLK_REGS                        ((mclk_registers_t*)0x40000800)               /* MCLK Registers Address       */
#define MTB_REGS                         ((mtb_registers_t*)0x41008000)                /* MTB Registers Address        */
#define NVMCTRL_REGS                     ((nvmctrl_registers_t*)0x41004000)            /* NVMCTRL Registers Address    */
#define OTP5_FUSES_REGS                  ((fuses_otp5_fuses_registers_t*)0x00806020)   /* FUSES Registers Address      */
#define TEMP_LOG_FUSES_REGS              ((fuses_temp_log_fuses_registers_t*)0x00806030) /* FUSES Registers Address      */
#define USER_FUSES_REGS                  ((fuses_user_fuses_registers_t*)0x00804000)   /* FUSES Registers Address      */
#define OSCCTRL_REGS                     ((oscctrl_registers_t*)0x40001000)            /* OSCCTRL Registers Address    */
#define OSC32KCTRL_REGS                  ((osc32kctrl_registers_t*)0x40001400)         /* OSC32KCTRL Registers Address */
#define PAC_REGS                         ((pac_registers_t*)0x40000000)                /* PAC Registers Address        */
#define PDEC_REGS                        ((pdec_registers_t*)0x42006800)               /* PDEC Registers Address       */
#define PM_REGS                          ((pm_registers_t*)0x40000400)                 /* PM Registers Address         */
#define PORT_REGS                        ((port_registers_t*)0x41000000)               /* PORT Registers Address       */
#define PORT_IOBUS_REGS                  ((port_registers_t*)0x60000000)               /* PORT Registers Address       */
#define RSTC_REGS                        ((rstc_registers_t*)0x40000c00)               /* RSTC Registers Address       */
#define RTC_REGS                         ((rtc_registers_t*)0x40002400)                /* RTC Registers Address        */
#define SDADC_REGS                       ((sdadc_registers_t*)0x42004c00)              /* SDADC Registers Address      */
#define SERCOM0_REGS                     ((sercom_registers_t*)0x42000400)             /* SERCOM0 Registers Address    */
#define SERCOM1_REGS                     ((sercom_registers_t*)0x42000800)             /* SERCOM1 Registers Address    */
#define SERCOM2_REGS                     ((sercom_registers_t*)0x42000c00)             /* SERCOM2 Registers Address    */
#define SERCOM3_REGS                     ((sercom_registers_t*)0x42001000)             /* SERCOM3 Registers Address    */
#define SUPC_REGS                        ((supc_registers_t*)0x40001800)               /* SUPC Registers Address       */
#define TC0_REGS                         ((tc_registers_t*)0x42003000)                 /* TC0 Registers Address        */
#define TC1_REGS                         ((tc_registers_t*)0x42003400)                 /* TC1 Registers Address        */
#define TC2_REGS                         ((tc_registers_t*)0x42003800)                 /* TC2 Registers Address        */
#define TC3_REGS                         ((tc_registers_t*)0x42003c00)                 /* TC3 Registers Address        */
#define TC4_REGS                         ((tc_registers_t*)0x42004000)                 /* TC4 Registers Address        */
#define TCC0_REGS                        ((tcc_registers_t*)0x42002400)                /* TCC0 Registers Address       */
#define TCC1_REGS                        ((tcc_registers_t*)0x42002800)                /* TCC1 Registers Address       */
#define TCC2_REGS                        ((tcc_registers_t*)0x42002c00)                /* TCC2 Registers Address       */
#define TSENS_REGS                       ((tsens_registers_t*)0x40003000)              /* TSENS Registers Address      */
#define WDT_REGS                         ((wdt_registers_t*)0x40002000)                /* WDT Registers Address        */
#endif /* (defined(__ASSEMBLER__) || defined(__IAR_SYSTEMS_ASM__)) */

/* ************************************************************************** */
/*   BASE ADDRESS DEFINITIONS FOR PIC32CM1216MC00048                          */
/* ************************************************************************** */
#define AC_BASE_ADDRESS                  _UINT32_(0x42005000)                          /* AC Base Address */
#define ADC0_BASE_ADDRESS                _UINT32_(0x42004400)                          /* ADC0 Base Address */
#define ADC1_BASE_ADDRESS                _UINT32_(0x42004800)                          /* ADC1 Base Address */
#define CCL_BASE_ADDRESS                 _UINT32_(0x42005c00)                          /* CCL Base Address */
#define DAC_BASE_ADDRESS                 _UINT32_(0x42005400)                          /* DAC Base Address */
#define DIVAS_BASE_ADDRESS               _UINT32_(0x48000000)                          /* DIVAS Base Address */
#define DMAC_BASE_ADDRESS                _UINT32_(0x41006000)                          /* DMAC Base Address */
#define DSU_BASE_ADDRESS                 _UINT32_(0x41002000)                          /* DSU Base Address */
#define EIC_BASE_ADDRESS                 _UINT32_(0x40002800)                          /* EIC Base Address */
#define EVSYS_BASE_ADDRESS               _UINT32_(0x42000000)                          /* EVSYS Base Address */
#define FREQM_BASE_ADDRESS               _UINT32_(0x40002c00)                          /* FREQM Base Address */
#define GCLK_BASE_ADDRESS                _UINT32_(0x40001c00)                          /* GCLK Base Address */
#define HMATRIXHS_BASE_ADDRESS           _UINT32_(0x4100a000)                          /* HMATRIXHS Base Address */
#define MCLK_BASE_ADDRESS                _UINT32_(0x40000800)                          /* MCLK Base Address */
#define MTB_BASE_ADDRESS                 _UINT32_(0x41008000)                          /* MTB Base Address */
#define NVMCTRL_BASE_ADDRESS             _UINT32_(0x41004000)                          /* NVMCTRL Base Address */
#define OTP5_FUSES_BASE_ADDRESS          _UINT32_(0x00806020)                          /* FUSES Base Address */
#define TEMP_LOG_FUSES_BASE_ADDRESS      _UINT32_(0x00806030)                          /* FUSES Base Address */
#define USER_FUSES_BASE_ADDRESS          _UINT32_(0x00804000)                          /* FUSES Base Address */
#define OSCCTRL_BASE_ADDRESS             _UINT32_(0x40001000)                          /* OSCCTRL Base Address */
#define OSC32KCTRL_BASE_ADDRESS          _UINT32_(0x40001400)                          /* OSC32KCTRL Base Address */
#define PAC_BASE_ADDRESS                 _UINT32_(0x40000000)                          /* PAC Base Address */
#define PDEC_BASE_ADDRESS                _UINT32_(0x42006800)                          /* PDEC Base Address */
#define PM_BASE_ADDRESS                  _UINT32_(0x40000400)                          /* PM Base Address */
#define PORT_BASE_ADDRESS                _UINT32_(0x41000000)                          /* PORT Base Address */
#define PORT_IOBUS_BASE_ADDRESS          _UINT32_(0x60000000)                          /* PORT Base Address */
#define RSTC_BASE_ADDRESS                _UINT32_(0x40000c00)                          /* RSTC Base Address */
#define RTC_BASE_ADDRESS                 _UINT32_(0x40002400)                          /* RTC Base Address */
#define SDADC_BASE_ADDRESS               _UINT32_(0x42004c00)                          /* SDADC Base Address */
#define SERCOM0_BASE_ADDRESS             _UINT32_(0x42000400)                          /* SERCOM0 Base Address */
#define SERCOM1_BASE_ADDRESS             _UINT32_(0x42000800)                          /* SERCOM1 Base Address */
#define SERCOM2_BASE_ADDRESS             _UINT32_(0x42000c00)                          /* SERCOM2 Base Address */
#define SERCOM3_BASE_ADDRESS             _UINT32_(0x42001000)                          /* SERCOM3 Base Address */
#define SUPC_BASE_ADDRESS                _UINT32_(0x40001800)                          /* SUPC Base Address */
#define TC0_BASE_ADDRESS                 _UINT32_(0x42003000)                          /* TC0 Base Address */
#define TC1_BASE_ADDRESS                 _UINT32_(0x42003400)                          /* TC1 Base Address */
#define TC2_BASE_ADDRESS                 _UINT32_(0x42003800)                          /* TC2 Base Address */
#define TC3_BASE_ADDRESS                 _UINT32_(0x42003c00)                          /* TC3 Base Address */
#define TC4_BASE_ADDRESS                 _UINT32_(0x42004000)                          /* TC4 Base Address */
#define TCC0_BASE_ADDRESS                _UINT32_(0x42002400)                          /* TCC0 Base Address */
#define TCC1_BASE_ADDRESS                _UINT32_(0x42002800)                          /* TCC1 Base Address */
#define TCC2_BASE_ADDRESS                _UINT32_(0x42002c00)                          /* TCC2 Base Address */
#define TSENS_BASE_ADDRESS               _UINT32_(0x40003000)                          /* TSENS Base Address */
#define WDT_BASE_ADDRESS                 _UINT32_(0x40002000)                          /* WDT Base Address */

/* ************************************************************************** */
/*   PIO DEFINITIONS FOR PIC32CM1216MC00048                                   */
/* ************************************************************************** */
#include "pio/pic32cm1216mc00048.h"

/* ************************************************************************** */
/*   MEMORY MAPPING DEFINITIONS FOR PIC32CM1216MC00048                        */
/* ************************************************************************** */
#define FLASH_SIZE                     _UINT32_(0x00020000)    /*  128kB Memory segment type: flash */
#define FLASH_PAGE_SIZE                _UINT32_(        64)
#define FLASH_NB_OF_PAGES              _UINT32_(      2048)

#define OTP5_SIZE                      _UINT32_(0x00000008)    /*    0kB Memory segment type: fuses */
#define TEMP_LOG_SIZE                  _UINT32_(0x00000008)    /*    0kB Memory segment type: fuses */
#define USER_PAGE_SIZE                 _UINT32_(0x00000100)    /*    0kB Memory segment type: user_page */
#define USER_PAGE_PAGE_SIZE            _UINT32_(        64)
#define USER_PAGE_NB_OF_PAGES          _UINT32_(         4)

#define DATAFLASH_SIZE                 _UINT32_(0x00001000)    /*    4kB Memory segment type: flash */
#define DATAFLASH_PAGE_SIZE            _UINT32_(        64)
#define DATAFLASH_NB_OF_PAGES          _UINT32_(        64)

#define HSRAM_SIZE                     _UINT32_(0x00004000)    /*   16kB Memory segment type: ram */
#define HPB0_SIZE                      _UINT32_(0x00004000)    /*   16kB Memory segment type: io */
#define HPB1_SIZE                      _UINT32_(0x00010000)    /*   64kB Memory segment type: io */
#define HPB2_SIZE                      _UINT32_(0x00008000)    /*   32kB Memory segment type: io */
#define DIVAS_SIZE                     _UINT32_(0x00000020)    /*    0kB Memory segment type: io */
#define PPB_SIZE                       _UINT32_(0x00100000)    /* 1024kB Memory segment type: io */
#define SCS_SIZE                       _UINT32_(0x00001000)    /*    4kB Memory segment type: io */
#define PERIPHERALS_SIZE               _UINT32_(0x20000000)    /* 524288kB Memory segment type: io */

#define FLASH_ADDR                     _UINT32_(0x00000000)    /* FLASH base address (type: flash)*/
#define OTP5_ADDR                      _UINT32_(0x00806020)    /* OTP5 base address (type: fuses)*/
#define TEMP_LOG_ADDR                  _UINT32_(0x00806030)    /* TEMP_LOG base address (type: fuses)*/
#define USER_PAGE_ADDR                 _UINT32_(0x00804000)    /* USER_PAGE base address (type: user_page)*/
#define DATAFLASH_ADDR                 _UINT32_(0x00400000)    /* DATAFLASH base address (type: flash)*/
#define HSRAM_ADDR                     _UINT32_(0x20000000)    /* HSRAM base address (type: ram)*/
#define HPB0_ADDR                      _UINT32_(0x40000000)    /* HPB0 base address (type: io)*/
#define HPB1_ADDR                      _UINT32_(0x41000000)    /* HPB1 base address (type: io)*/
#define HPB2_ADDR                      _UINT32_(0x42000000)    /* HPB2 base address (type: io)*/
#define DIVAS_ADDR                     _UINT32_(0x48000000)    /* DIVAS base address (type: io)*/
#define PPB_ADDR                       _UINT32_(0xe0000000)    /* PPB base address (type: io)*/
#define SCS_ADDR                       _UINT32_(0xe000e000)    /* SCS base address (type: io)*/
#define PERIPHERALS_ADDR               _UINT32_(0x40000000)    /* PERIPHERALS base address (type: io)*/

/* ************************************************************************** */
/*   DEVICE SIGNATURES FOR PIC32CM1216MC00048                                 */
/* ************************************************************************** */
#define CHIP_DSU_DID                   _UINT32_(0X11070106)

/* ************************************************************************** */
/*   ELECTRICAL DEFINITIONS FOR PIC32CM1216MC00048                            */
/* ************************************************************************** */

/* ************************************************************************** */
/* Event Generator IDs for C32CM1216MC00048 */
/* ************************************************************************** */
#define EVENT_ID_GEN_OSCCTRL_XOSC_FAIL                    1 /* ID for OSCCTRL event generator XOSC_FAIL */
#define EVENT_ID_GEN_OSC32KCTRL_XOSC32K_FAIL              2 /* ID for OSC32KCTRL event generator XOSC32K_FAIL */
#define EVENT_ID_GEN_RTC_CMP_0                            3 /* ID for RTC event generator CMP_0 */
#define EVENT_ID_GEN_RTC_CMP_1                            4 /* ID for RTC event generator CMP_1 */
#define EVENT_ID_GEN_RTC_OVF                              5 /* ID for RTC event generator OVF */
#define EVENT_ID_GEN_RTC_PER_0                            6 /* ID for RTC event generator PER_0 */
#define EVENT_ID_GEN_RTC_PER_1                            7 /* ID for RTC event generator PER_1 */
#define EVENT_ID_GEN_RTC_PER_2                            8 /* ID for RTC event generator PER_2 */
#define EVENT_ID_GEN_RTC_PER_3                            9 /* ID for RTC event generator PER_3 */
#define EVENT_ID_GEN_RTC_PER_4                           10 /* ID for RTC event generator PER_4 */
#define EVENT_ID_GEN_RTC_PER_5                           11 /* ID for RTC event generator PER_5 */
#define EVENT_ID_GEN_RTC_PER_6                           12 /* ID for RTC event generator PER_6 */
#define EVENT_ID_GEN_RTC_PER_7                           13 /* ID for RTC event generator PER_7 */
#define EVENT_ID_GEN_EIC_EXTINT_0                        14 /* ID for EIC event generator EXTINT_0 */
#define EVENT_ID_GEN_EIC_EXTINT_1                        15 /* ID for EIC event generator EXTINT_1 */
#define EVENT_ID_GEN_EIC_EXTINT_2                        16 /* ID for EIC event generator EXTINT_2 */
#define EVENT_ID_GEN_EIC_EXTINT_3                        17 /* ID for EIC event generator EXTINT_3 */
#define EVENT_ID_GEN_EIC_EXTINT_4                        18 /* ID for EIC event generator EXTINT_4 */
#define EVENT_ID_GEN_EIC_EXTINT_5                        19 /* ID for EIC event generator EXTINT_5 */
#define EVENT_ID_GEN_EIC_EXTINT_6                        20 /* ID for EIC event generator EXTINT_6 */
#define EVENT_ID_GEN_EIC_EXTINT_7                        21 /* ID for EIC event generator EXTINT_7 */
#define EVENT_ID_GEN_EIC_EXTINT_8                        22 /* ID for EIC event generator EXTINT_8 */
#define EVENT_ID_GEN_EIC_EXTINT_9                        23 /* ID for EIC event generator EXTINT_9 */
#define EVENT_ID_GEN_EIC_EXTINT_10                       24 /* ID for EIC event generator EXTINT_10 */
#define EVENT_ID_GEN_EIC_EXTINT_11                       25 /* ID for EIC event generator EXTINT_11 */
#define EVENT_ID_GEN_EIC_EXTINT_12                       26 /* ID for EIC event generator EXTINT_12 */
#define EVENT_ID_GEN_EIC_EXTINT_13                       27 /* ID for EIC event generator EXTINT_13 */
#define EVENT_ID_GEN_EIC_EXTINT_14                       28 /* ID for EIC event generator EXTINT_14 */
#define EVENT_ID_GEN_EIC_EXTINT_15                       29 /* ID for EIC event generator EXTINT_15 */
#define EVENT_ID_GEN_TSENS_WINMON                        30 /* ID for TSENS event generator WINMON */
#define EVENT_ID_GEN_DMAC_CH_0                           31 /* ID for DMAC event generator CH_0 */
#define EVENT_ID_GEN_DMAC_CH_1                           32 /* ID for DMAC event generator CH_1 */
#define EVENT_ID_GEN_DMAC_CH_2                           33 /* ID for DMAC event generator CH_2 */
#define EVENT_ID_GEN_DMAC_CH_3                           34 /* ID for DMAC event generator CH_3 */
#define EVENT_ID_GEN_TCC0_OVF                            35 /* ID for TCC0 event generator OVF */
#define EVENT_ID_GEN_TCC0_TRG                            36 /* ID for TCC0 event generator TRG */
#define EVENT_ID_GEN_TCC0_CNT                            37 /* ID for TCC0 event generator CNT */
#define EVENT_ID_GEN_TCC0_MC_0                           38 /* ID for TCC0 event generator MC_0 */
#define EVENT_ID_GEN_TCC0_MC_1                           39 /* ID for TCC0 event generator MC_1 */
#define EVENT_ID_GEN_TCC0_MC_2                           40 /* ID for TCC0 event generator MC_2 */
#define EVENT_ID_GEN_TCC0_MC_3                           41 /* ID for TCC0 event generator MC_3 */
#define EVENT_ID_GEN_TCC1_OVF                            42 /* ID for TCC1 event generator OVF */
#define EVENT_ID_GEN_TCC1_TRG                            43 /* ID for TCC1 event generator TRG */
#define EVENT_ID_GEN_TCC1_CNT                            44 /* ID for TCC1 event generator CNT */
#define EVENT_ID_GEN_TCC1_MC_0                           45 /* ID for TCC1 event generator MC_0 */
#define EVENT_ID_GEN_TCC1_MC_1                           46 /* ID for TCC1 event generator MC_1 */
#define EVENT_ID_GEN_TCC2_OVF                            47 /* ID for TCC2 event generator OVF */
#define EVENT_ID_GEN_TCC2_TRG                            48 /* ID for TCC2 event generator TRG */
#define EVENT_ID_GEN_TCC2_CNT                            49 /* ID for TCC2 event generator CNT */
#define EVENT_ID_GEN_TCC2_MC_0                           50 /* ID for TCC2 event generator MC_0 */
#define EVENT_ID_GEN_TCC2_MC_1                           51 /* ID for TCC2 event generator MC_1 */
#define EVENT_ID_GEN_TC0_OVF                             52 /* ID for TC0 event generator OVF */
#define EVENT_ID_GEN_TC0_MC_0                            53 /* ID for TC0 event generator MC_0 */
#define EVENT_ID_GEN_TC0_MC_1                            54 /* ID for TC0 event generator MC_1 */
#define EVENT_ID_GEN_TC1_OVF                             55 /* ID for TC1 event generator OVF */
#define EVENT_ID_GEN_TC1_MC_0                            56 /* ID for TC1 event generator MC_0 */
#define EVENT_ID_GEN_TC1_MC_1                            57 /* ID for TC1 event generator MC_1 */
#define EVENT_ID_GEN_TC2_OVF                             58 /* ID for TC2 event generator OVF */
#define EVENT_ID_GEN_TC2_MC_0                            59 /* ID for TC2 event generator MC_0 */
#define EVENT_ID_GEN_TC2_MC_1                            60 /* ID for TC2 event generator MC_1 */
#define EVENT_ID_GEN_TC3_OVF                             61 /* ID for TC3 event generator OVF */
#define EVENT_ID_GEN_TC3_MC_0                            62 /* ID for TC3 event generator MC_0 */
#define EVENT_ID_GEN_TC3_MC_1                            63 /* ID for TC3 event generator MC_1 */
#define EVENT_ID_GEN_TC4_OVF                             64 /* ID for TC4 event generator OVF */
#define EVENT_ID_GEN_TC4_MC_0                            65 /* ID for TC4 event generator MC_0 */
#define EVENT_ID_GEN_TC4_MC_1                            66 /* ID for TC4 event generator MC_1 */
#define EVENT_ID_GEN_ADC0_RESRDY                         67 /* ID for ADC0 event generator RESRDY */
#define EVENT_ID_GEN_ADC0_WINMON                         68 /* ID for ADC0 event generator WINMON */
#define EVENT_ID_GEN_ADC1_RESRDY                         69 /* ID for ADC1 event generator RESRDY */
#define EVENT_ID_GEN_ADC1_WINMON                         70 /* ID for ADC1 event generator WINMON */
#define EVENT_ID_GEN_SDADC_RESRDY                        71 /* ID for SDADC event generator RESRDY */
#define EVENT_ID_GEN_SDADC_WINMON                        72 /* ID for SDADC event generator WINMON */
#define EVENT_ID_GEN_AC_COMP_0                           73 /* ID for AC event generator COMP_0 */
#define EVENT_ID_GEN_AC_COMP_1                           74 /* ID for AC event generator COMP_1 */
#define EVENT_ID_GEN_AC_WIN_0                            75 /* ID for AC event generator WIN_0 */
#define EVENT_ID_GEN_DAC_EMPTY                           76 /* ID for DAC event generator EMPTY */
#define EVENT_ID_GEN_CCL_LUTOUT_0                        77 /* ID for CCL event generator LUTOUT_0 */
#define EVENT_ID_GEN_CCL_LUTOUT_1                        78 /* ID for CCL event generator LUTOUT_1 */
#define EVENT_ID_GEN_CCL_LUTOUT_2                        79 /* ID for CCL event generator LUTOUT_2 */
#define EVENT_ID_GEN_CCL_LUTOUT_3                        80 /* ID for CCL event generator LUTOUT_3 */
#define EVENT_ID_GEN_PAC_ACCERR                          81 /* ID for PAC event generator ACCERR */
#define EVENT_ID_GEN_PDEC_OVF                            83 /* ID for PDEC event generator OVF */
#define EVENT_ID_GEN_PDEC_ERR                            84 /* ID for PDEC event generator ERR */
#define EVENT_ID_GEN_PDEC_DIR                            85 /* ID for PDEC event generator DIR */
#define EVENT_ID_GEN_PDEC_VLC                            86 /* ID for PDEC event generator VLC */
#define EVENT_ID_GEN_PDEC_MC_0                           87 /* ID for PDEC event generator MC_0 */
#define EVENT_ID_GEN_PDEC_MC_1                           88 /* ID for PDEC event generator MC_1 */

/* ************************************************************************** */
/*  Event User IDs for C32CM1216MC00048 */
/* ************************************************************************** */
#define EVENT_ID_USER_TSENS_START                         0 /* ID for TSENS event user START */
#define EVENT_ID_USER_PORT_EV_0                           1 /* ID for PORT event user EV_0 */
#define EVENT_ID_USER_PORT_EV_1                           2 /* ID for PORT event user EV_1 */
#define EVENT_ID_USER_PORT_EV_2                           3 /* ID for PORT event user EV_2 */
#define EVENT_ID_USER_PORT_EV_3                           4 /* ID for PORT event user EV_3 */
#define EVENT_ID_USER_DMAC_CH_0                           5 /* ID for DMAC event user CH_0 */
#define EVENT_ID_USER_DMAC_CH_1                           6 /* ID for DMAC event user CH_1 */
#define EVENT_ID_USER_DMAC_CH_2                           7 /* ID for DMAC event user CH_2 */
#define EVENT_ID_USER_DMAC_CH_3                           8 /* ID for DMAC event user CH_3 */
#define EVENT_ID_USER_TCC0_EV_0                           9 /* ID for TCC0 event user EV_0 */
#define EVENT_ID_USER_TCC0_EV_1                          10 /* ID for TCC0 event user EV_1 */
#define EVENT_ID_USER_TCC0_MC_0                          11 /* ID for TCC0 event user MC_0 */
#define EVENT_ID_USER_TCC0_MC_1                          12 /* ID for TCC0 event user MC_1 */
#define EVENT_ID_USER_TCC0_MC_2                          13 /* ID for TCC0 event user MC_2 */
#define EVENT_ID_USER_TCC0_MC_3                          14 /* ID for TCC0 event user MC_3 */
#define EVENT_ID_USER_TCC1_EV_0                          15 /* ID for TCC1 event user EV_0 */
#define EVENT_ID_USER_TCC1_EV_1                          16 /* ID for TCC1 event user EV_1 */
#define EVENT_ID_USER_TCC1_MC_0                          17 /* ID for TCC1 event user MC_0 */
#define EVENT_ID_USER_TCC1_MC_1                          18 /* ID for TCC1 event user MC_1 */
#define EVENT_ID_USER_TCC2_EV_0                          19 /* ID for TCC2 event user EV_0 */
#define EVENT_ID_USER_TCC2_EV_1                          20 /* ID for TCC2 event user EV_1 */
#define EVENT_ID_USER_TCC2_MC_0                          21 /* ID for TCC2 event user MC_0 */
#define EVENT_ID_USER_TCC2_MC_1                          22 /* ID for TCC2 event user MC_1 */
#define EVENT_ID_USER_TC0_EVU                            23 /* ID for TC0 event user EVU */
#define EVENT_ID_USER_TC1_EVU                            24 /* ID for TC1 event user EVU */
#define EVENT_ID_USER_TC2_EVU                            25 /* ID for TC2 event user EVU */
#define EVENT_ID_USER_TC3_EVU                            26 /* ID for TC3 event user EVU */
#define EVENT_ID_USER_TC4_EVU                            27 /* ID for TC4 event user EVU */
#define EVENT_ID_USER_ADC0_START                         28 /* ID for ADC0 event user START */
#define EVENT_ID_USER_ADC0_SYNC                          29 /* ID for ADC0 event user SYNC */
#define EVENT_ID_USER_ADC1_START                         30 /* ID for ADC1 event user START */
#define EVENT_ID_USER_ADC1_SYNC                          31 /* ID for ADC1 event user SYNC */
#define EVENT_ID_USER_SDADC_START                        32 /* ID for SDADC event user START */
#define EVENT_ID_USER_SDADC_FLUSH                        33 /* ID for SDADC event user FLUSH */
#define EVENT_ID_USER_AC_SOC_0                           34 /* ID for AC event user SOC_0 */
#define EVENT_ID_USER_AC_SOC_1                           35 /* ID for AC event user SOC_1 */
#define EVENT_ID_USER_DAC_START                          36 /* ID for DAC event user START */
#define EVENT_ID_USER_CCL_LUTIN_0                        37 /* ID for CCL event user LUTIN_0 */
#define EVENT_ID_USER_CCL_LUTIN_1                        38 /* ID for CCL event user LUTIN_1 */
#define EVENT_ID_USER_CCL_LUTIN_2                        39 /* ID for CCL event user LUTIN_2 */
#define EVENT_ID_USER_CCL_LUTIN_3                        40 /* ID for CCL event user LUTIN_3 */
#define EVENT_ID_USER_MTB_START                          42 /* ID for MTB event user START */
#define EVENT_ID_USER_MTB_STOP                           43 /* ID for MTB event user STOP */
#define EVENT_ID_USER_PDEC_EVU_0                         44 /* ID for PDEC event user EVU_0 */
#define EVENT_ID_USER_PDEC_EVU_1                         45 /* ID for PDEC event user EVU_1 */
#define EVENT_ID_USER_PDEC_EVU_2                         46 /* ID for PDEC event user EVU_2 */

#ifdef __cplusplus
}
#endif

#endif /* _PIC32CM1216MC00048_H_ */

