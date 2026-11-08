// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../io.h"
#include "../sound.h"

#define REG_SOUNDCNT       MK_REG(u16, IO_SOUNDCNT)
#define REG_SOUNDCNTVOL    MK_REG(u8,  IO_SOUNDCNT)
#define REG_SOUNDBIAS      MK_REG(u16, IO_SOUNDBIAS)
#define REG_SNDEXCNT       MK_REG(u16, IO_SNDEXCNT)

#define REG_SOUNDxCNT(_x)  MK_REG(u32, IO_SOUNDxCNT(_x))
#define REG_SOUNDxVOL(_x)  MK_REG(u16, IO_SOUNDxCNT(_x))
#define REG_SOUNDxPAN(_x)  MK_REG(u8,  IO_SOUNDxCNT(_x)+2)
#define REG_SOUNDxSAD(_x)  MK_REG(u32, IO_SOUNDxSAD(_x))
#define REG_SOUNDxTMR(_x)  MK_REG(u16, IO_SOUNDxTMR(_x))
#define REG_SOUNDxPNT(_x)  MK_REG(u16, IO_SOUNDxPNT(_x))
#define REG_SOUNDxLEN(_x)  MK_REG(u32, IO_SOUNDxLEN(_x))

#define REG_SNDCAPCNT      MK_REG(u16, IO_SNDCAPxCNT(0))
#define REG_SNDCAPxCNT(_x) MK_REG(u8,  IO_SNDCAPxCNT(_x))
#define REG_SNDCAPxDAD(_x) MK_REG(u32, IO_SNDCAPxDAD(_x))
#define REG_SNDCAPxLEN(_x) MK_REG(u16, IO_SNDCAPxLEN(_x))

#define SOUNDCNT_VOL(_x)       ((_x)&0x7f)
#define SOUNDCNT_MIXER_CFG(_x) (((_x)&0x3f)<<8)
#define SOUNDCNT_OUT_SRC_L(_x) (((_x)&3)<<8)
#define SOUNDCNT_OUT_SRC_R(_x) (((_x)&3)<<10)
#define SOUNDCNT_MUTE_CH1      (1U<<12)
#define SOUNDCNT_MUTE_CH3      (1U<<13)
#define SOUNDCNT_ENABLE        (1U<<15)

#define SNDEXCNT_MIX_RATIO(_x) ((_x)&0xf) // 0..8
#define SNDEXCNT_I2S_32728_HZ  (0U<<13) // SYSTEM_CLOCK/1024
#define SNDEXCNT_I2S_4761x_HZ  (1U<<13) // ???????
#define SNDEXCNT_MUTE          (1U<<14)
#define SNDEXCNT_ENABLE        (1U<<15)

#define SOUNDxCNT_VOL(_x)      ((_x)&0x7f)
#define SOUNDxCNT_VOL_DIV(_x)  (((_x)&3)<<8)
#define SOUNDxCNT_HOLD         (1U<<15)
#define SOUNDxCNT_PAN(_x)      (((_x)&0x7f)<<16)
#define SOUNDxCNT_DUTY(_x)     (((_x)&7)<<24)
#define SOUNDxCNT_MODE(_x)     (((_x)&3)<<27)
#define SOUNDxCNT_FMT(_x)      (((_x)&3)<<29)
#define SOUNDxCNT_ENABLE       (1U<<31)

#define SNDCAPxCNT_CFG(_x)     ((_x)&0xf)
#define SNDCAPxCNT_DST(_x)     ((_x)&1)
#define SNDCAPxCNT_SRC(_x)     (((_x)&1)<<1)
#define SNDCAPxCNT_LOOP        (1U<<2)
#define SNDCAPxCNT_ONESHOT     (1U<<2)
#define SNDCAPxCNT_FMT(_x)     (((_x)&1)<<3)
#define SNDCAPxCNT_ENABLE      (1U<<7)

MK_EXTERN_C_START

MK_INLINE void soundSetMixerVolume(unsigned vol)
{
	REG_SOUNDCNTVOL = vol & 0x7f;
}

MK_INLINE void soundSetMixerConfigDirect(unsigned config)
{
	REG_SOUNDCNT = (REG_SOUNDCNT &~ SOUNDCNT_MIXER_CFG(0x3f)) | SOUNDCNT_MIXER_CFG(config);
}

MK_INLINE void soundSetMixerConfig(SoundOutSrc src_l, SoundOutSrc src_r, bool mute_ch1, bool mute_ch3)
{
	unsigned config = soundMakeMixerConfig(src_l, src_r, mute_ch1, mute_ch3);
	soundSetMixerConfigDirect(config);
}

MK_INLINE void soundChPreparePcm(
	unsigned ch, unsigned vol, SoundVolDiv voldiv, unsigned pan, unsigned timer,
	SoundMode mode, SoundFmt fmt, const void* sad, unsigned pnt, unsigned len)
{
	REG_SOUNDxCNT(ch) =
		SOUNDxCNT_VOL(vol) | SOUNDxCNT_VOL_DIV(voldiv) | SOUNDxCNT_PAN(pan) |
		SOUNDxCNT_MODE(mode) | SOUNDxCNT_FMT(fmt);
	REG_SOUNDxSAD(ch) = (u32)sad;
	REG_SOUNDxTMR(ch) = -timer;
	REG_SOUNDxPNT(ch) = pnt;
	REG_SOUNDxLEN(ch) = len;
}

MK_INLINE void soundChPreparePsg(
	unsigned ch, unsigned vol, SoundVolDiv voldiv, unsigned pan, unsigned timer,
	SoundDuty duty)
{
	REG_SOUNDxCNT(ch) =
		SOUNDxCNT_VOL(vol) | SOUNDxCNT_VOL_DIV(voldiv) | SOUNDxCNT_PAN(pan) |
		SOUNDxCNT_DUTY(duty) | SOUNDxCNT_FMT(SoundFmt_Psg);
	REG_SOUNDxTMR(ch) = -timer;
}

MK_INLINE void soundChStart(unsigned ch)
{
	REG_SOUNDxCNT(ch) |= SOUNDxCNT_ENABLE;
}

MK_INLINE void soundChStop(unsigned ch)
{
	REG_SOUNDxCNT(ch) &= ~SOUNDxCNT_ENABLE;
}

MK_INLINE bool soundChIsActive(unsigned ch)
{
	return REG_SOUNDxCNT(ch) & SOUNDxCNT_ENABLE;
}

MK_INLINE void soundChSetVolume(unsigned ch, unsigned vol, SoundVolDiv voldiv)
{
	REG_SOUNDxVOL(ch) = SOUNDxCNT_VOL(vol) | SOUNDxCNT_VOL_DIV(voldiv);
}

MK_INLINE void soundChSetPan(unsigned ch, unsigned pan)
{
	REG_SOUNDxPAN(ch) = pan & 0x7f;
}

MK_INLINE void soundChSetTimer(unsigned ch, unsigned timer)
{
	REG_SOUNDxTMR(ch) = -timer;
}

MK_INLINE void soundChSetDuty(unsigned ch, SoundDuty duty)
{
	REG_SOUNDxCNT(ch) = (REG_SOUNDxCNT(ch) &~ SOUNDxCNT_DUTY(7)) | SOUNDxCNT_DUTY(duty);
}

MK_INLINE void soundPrepareCapDirect(unsigned cap, unsigned config, void* dad, unsigned len)
{
	REG_SNDCAPxCNT(cap) = SNDCAPxCNT_CFG(config);
	REG_SNDCAPxDAD(cap) = (u32)dad;
	REG_SNDCAPxLEN(cap) = len;
}

MK_INLINE void soundPrepareCap(
	unsigned cap, SoundCapDst dst, SoundCapSrc src, bool loop, SoundCapFmt fmt,
	void* dad, unsigned len)
{
	unsigned config = soundMakeCapConfig(dst, src, loop, fmt);
	soundPrepareCapDirect(cap, config, dad, len);
}

MK_CONSTEXPR u16 soundExpandCapMask(unsigned cap_mask)
{
	u16 ret = 0;
	ret |= (cap_mask&1) * SNDCAPxCNT_ENABLE;
	ret |= ((cap_mask>>1)&1) * (SNDCAPxCNT_ENABLE<<8);
	return ret;
}

MK_CONSTEXPR void soundCapStart(u16 mask)
{
	REG_SNDCAPCNT |= mask;
}

MK_CONSTEXPR void soundCapStop(u16 mask)
{
	REG_SNDCAPCNT &= ~mask;
}

void soundStartServer(u8 thread_prio);

MK_EXTERN_C_END
