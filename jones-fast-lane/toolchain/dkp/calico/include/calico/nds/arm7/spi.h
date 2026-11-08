// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../../types.h"
#include "../../system/mutex.h"
#include "../io.h"

#define REG_SPICNT  MK_REG(u16, IO_SPICNT)
#define REG_SPIDATA MK_REG(u16, IO_SPIDATA)

#define SPICNT_BUSY       (1U << 7)
#define SPICNT_DEVICE(_x) ((unsigned)((_x) & 3) << 8)
#define SPICNT_HOLD       (1U << 11)
#define SPICNT_IRQ_ENABLE (1U << 14)
#define SPICNT_ENABLE     (1U << 15)

MK_EXTERN_C_START

typedef enum SpiBaudrate {
	SpiBaud_4MHz = 0,
	SpiBaud_2MHz,
	SpiBaud_1MHz,
	SpiBaud_512KHz,
	SpiBaud_8MHz, // DSi-only
} SpiBaudrate;

typedef enum SpiDevice {
	SpiDev_PMIC = 0,
	SpiDev_NVRAM,
	SpiDev_TSC,
} SpiDevice;

extern Mutex g_spiMutex;

MK_INLINE void spiLock(void)
{
	mutexLock(&g_spiMutex);
}

MK_INLINE void spiUnlock(void)
{
	mutexUnlock(&g_spiMutex);
}

MK_INLINE void spiWaitBusy(void)
{
	while (REG_SPICNT & SPICNT_BUSY);
}

MK_INLINE void spiRawStartHold(SpiDevice dev, SpiBaudrate baud)
{
	spiWaitBusy();
	REG_SPICNT = baud | SPICNT_DEVICE(dev) | SPICNT_HOLD | SPICNT_ENABLE;
}

MK_INLINE void spiRawEndHold(SpiDevice dev, SpiBaudrate baud)
{
	REG_SPICNT = baud | SPICNT_DEVICE(dev) | SPICNT_ENABLE;
}

MK_INLINE void spiRawWriteByteAsync(u8 data)
{
	REG_SPIDATA = data;
}

MK_INLINE void spiRawWriteByte(u8 data)
{
	spiRawWriteByteAsync(data);
	spiWaitBusy();
}

MK_INLINE u8 spiRawWriteReadByte(u8 data)
{
	spiRawWriteByte(data);
	return REG_SPIDATA;
}

MK_INLINE u8 spiRawReadByte(void)
{
	return spiRawWriteReadByte(0);
}

MK_EXTERN_C_END
