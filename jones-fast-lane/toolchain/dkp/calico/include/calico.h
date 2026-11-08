// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once

// Common types and definitions
#if !__ASSEMBLER__
#include "calico/types.h"
#else
#include "calico/asm.inc"
#endif

// Basic arm definitions
#include "calico/arm/psr.h"

// arm9 (armv5) platforms: CP15 definitions
#if __ARM_ARCH >= 5
#include "calico/arm/cp15.h"
#endif

// System bus frequency definition
#include "calico/system/sysclock.h"

// Memory map & IO register definitions
#if defined(__GBA__)
#include "calico/gba/mm.h"
#include "calico/gba/io.h"
#elif defined(__NDS__)
#include "calico/nds/mm.h"
#include "calico/nds/mm_env.h"
#include "calico/nds/io.h"
#include "calico/dev/dldi_defs.h"
#else
#error "Unknown/unsupported platform"
#endif

#if !__ASSEMBLER__

#include "calico/arm/common.h"
#if __ARM_ARCH >= 5
#include "calico/arm/cache.h"
#include "calico/arm/mpu.h"
#endif

#include "calico/system/irq.h"
#include "calico/system/tick.h"
#include "calico/system/thread.h"
#include "calico/system/mutex.h"
#include "calico/system/condvar.h"
#include "calico/system/mailbox.h"
#include "calico/system/dietprint.h"

#include "calico/dev/fugu.h"

#if defined(__GBA__)
#include "calico/gba/bios.h"
#include "calico/gba/keypad.h"
#include "calico/gba/timer.h"
#include "calico/gba/dma.h"
#include "calico/gba/lcd.h"
#endif

#if defined(__NDS__)
#include "calico/nds/system.h"
#include "calico/nds/scfg.h"
#include "calico/nds/bios.h"
#include "calico/nds/timer.h"
#include "calico/nds/dma.h"
#include "calico/nds/ndma.h"
#include "calico/nds/env.h"
#include "calico/nds/tlnc.h"
#include "calico/nds/pxi.h"
#include "calico/nds/smutex.h"
#include "calico/nds/keypad.h"
#include "calico/nds/touch.h"
#include "calico/nds/lcd.h"
#include "calico/nds/pm.h"

#include "calico/dev/blk.h"
#include "calico/dev/disc_io.h"
#include "calico/dev/dldi.h"
#include "calico/dev/netbuf.h"
#include "calico/dev/wlan.h"
#include "calico/nds/wlmgr.h"

#include "calico/nds/gbacart.h"
#include "calico/nds/ntrcard.h"
#include "calico/nds/nitrorom.h"

#ifdef ARM7
#include "calico/nds/arm7/debug.h"

#include "calico/nds/arm7/gpio.h"
#include "calico/nds/arm7/rtc.h"
#include "calico/nds/arm7/spi.h"
#include "calico/nds/arm7/pmic.h"
#include "calico/nds/arm7/nvram.h"
#include "calico/nds/arm7/tsc.h"
#include "calico/nds/arm7/sound.h"
#include "calico/nds/arm7/mic.h"
#include "calico/nds/arm7/ntrwifi.h"

#include "calico/nds/arm7/i2c.h"
#include "calico/nds/arm7/mcu.h"
#include "calico/nds/arm7/codec.h"

#include "calico/nds/arm7/aes.h"
#include "calico/nds/arm7/twlblk.h"
#include "calico/nds/arm7/twlwifi.h"

#include "calico/dev/tmio.h"
#include "calico/dev/sdmmc.h"
#include "calico/dev/sdio.h"
#include "calico/dev/ar6k.h"
#include "calico/dev/mwl.h"
#endif

#ifdef ARM9
#include "calico/nds/arm9/arm7_debug.h"
#include "calico/nds/arm9/sound.h"
#include "calico/nds/arm9/mic.h"

#include "calico/nds/arm9/vram.h"

#include "calico/nds/arm9/ovl.h"
#endif

#endif

#endif
