// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../../types.h"
#include "../io.h"

// Summary of GPIO pins (not official names except for GBA):
// Inherited from GBA (4x in/out RCNT):
//   SC
//   SD
//   SI  <- Connected to RTC interrupt pin
//   SO
// Added by NDS (8x in RCNT_EXT, 8x in/out RCNT_RTC):
//   N0  <- Connected to X Button
//   N1  <- Connected to Y Button
//   N2
//   N3  <- Connected to "Debug" button on dev hardware
//   N4
//   N5
//   N6  <- Connected to TSC /PENIRQ (Pen Interrupt) pin, not used by DSi mode CODEC
//   N7  <- Connected to hinge magnet detector
//   N8  <- Connected to RTC SIO  (Serial Data IO) pin
//   N9  <- Connected to RTC /SCK (Serial Clock) pin
//   N10 <- Connected to RTC CS   (Chip Select) pin
//   N11
//   N12
//   N13
//   N14
//   N15
// Added by DSi (8x in/out GPIO_CNT):
//   T0 aka GPIO18[0]
//   T1 aka GPIO18[1]
//   T2 aka GPIO18[2]
//   T3
//   T4 aka GPIO33[0]
//   T5 aka GPIO33[1] <- Connected to headphone connection state
//   T6 aka GPIO33[2] <- Connected to MCU interrupt pin
//   T7 aka GPIO33[3] <- Connected to sound enable output

#define REG_RCNT     MK_REG(u16, IO_RCNT)
#define REG_RCNT_EXT MK_REG(u16, IO_RCNT_EXT)
#define REG_RCNT_RTC MK_REG(u16, IO_RCNT_RTC)
#define REG_GPIO_CNT MK_REG(u16, IO_GPIO_CNT)
#define REG_GPIO_IRQ MK_REG(u16, IO_GPIO_IRQ)
#define REG_GPIO_WL  MK_REG(u16, IO_GPIO_WL)

// RCNT layout: mm00000iddddOIDC
#define RCNT_SI              (1U<<2)
#define RCNT_SI_DIR_OUT      (1U<<6)
#define RCNT_SI_IRQ_ENABLE   (1U<<8)
#define RCNT_MODE_SIO        (0U<<14) // Theoretical
#define RCNT_MODE_GPIO       (2U<<14)
#define RCNT_MODE_JOY        (3U<<14) // Theoretical
#define RCNT_MODE_MASK       (3U<<14)

// RCNT_EXT layout: 00000000XXXXXXXX
#define RCNT_EXT_X           (1U<<0)
#define RCNT_EXT_Y           (1U<<1)
#define RCNT_EXT_DEBUG       (1U<<3)
#define RCNT_EXT_PENIRQ      (1U<<6)
#define RCNT_EXT_HINGE       (1U<<7)

// RCNT_RTC layout: DDDDRRRRddddrrrr
#define RCNT_RTC_SIO         (1U<<0)
#define RCNT_RTC_SCK         (1U<<1)
#define RCNT_RTC_CS          (1U<<2)
#define RCNT_RTC_SIO_OUT     (1U<<4)
#define RCNT_RTC_SCK_OUT     (1U<<5)
#define RCNT_RTC_CS_OUT      (1U<<6)
#define RCNT_RTC_SCK_0       (RCNT_RTC_SCK_OUT)
#define RCNT_RTC_SCK_1       (RCNT_RTC_SCK_OUT|RCNT_RTC_SCK)
#define RCNT_RTC_CS_0        (RCNT_RTC_CS_OUT)
#define RCNT_RTC_CS_1        (RCNT_RTC_CS_OUT|RCNT_RTC_CS)

// DSi GPIO pins
#define GPIO_PIN_HEADPHONE_CONNECT 5
#define GPIO_PIN_MCUIRQ            6
#define GPIO_PIN_SOUND_ENABLE      7

// GPIO_CNT
#define GPIO_CNT_PIN(_x)         (1U<<(_x))
#define GPIO_CNT_DIR_OUT(_x)     (1U<<(8+(_x)))

// GPIO_IRQ
#define GPIO_IRQ_EDGE_RISING(_x) (1U<<(_x))
#define GPIO_IRQ_ENABLE(_x)      (1U<<(8+(_x)))

// GPIO_WL
#define GPIO_WL_ACTIVE   (1U<<0) // Equivalent to 3DS 0x10147028
#define GPIO_WL_ATHEROS  (0U<<8) // Equivalent to 3DS 0x10147014
#define GPIO_WL_MITSUMI  (1U<<8)
#define GPIO_WL_MASK     (1U<<8)

MK_EXTERN_C_START

typedef enum GpioWlModule {
	GpioWlModule_Atheros = 0,
	GpioWlModule_Mitsumi = 1,
} GpioWlModule;

void gpioSetWlModule(GpioWlModule module);

MK_EXTERN_C_END
