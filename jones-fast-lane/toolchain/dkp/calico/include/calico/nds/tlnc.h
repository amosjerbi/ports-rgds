// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../types.h"
#include "system.h"
#include "env.h"

/*! @addtogroup tlnc

	TLNC is a mechanism implemented by the DSi system software which lets
	applications launch other applications. In addition, it is possible to
	pass extra parameter data to the launched applications.

	TLNC works by filling a special area of main RAM that is read on boot.
	Once the desired launch configuration is installed to the TLNC area,
	call @ref pmClearResetJumpTarget and return from main/use exit() to
	reboot the console and let the system software launch the specified
	application.

	@{
*/

//! @private
#define TLNC_MAGIC   0x434e4c54 // 'TLNC'
//! @private
#define TLNC_VERSION 1
//! Maximum size of TLNC parameter data
#define TLNC_MAX_PARAM_SZ 0x2ec

MK_EXTERN_C_START

//! TLNC target application type
typedef enum TlncAppType {
	TlncAppType_Unknown = 0, //!< Unspecified
	TlncAppType_Card    = 1, //!< Boot currently inserted NDS card in Slot 1
	TlncAppType_Temp    = 2, //!< Boot `nand:/tmp/jump.app`
	TlncAppType_Nand    = 3, //!< Boot DSiWare title with the given title ID
	TlncAppType_Ram     = 4, //!< Boot app directly from RAM (not fully implemented)
} TlncAppType;

//! TLNC application launch configuration data
typedef struct TlncData {
	u64 caller_tid;      //!< 0 = anonymous
	u64 target_tid;      //!< 0 = system menu
	u8  valid       : 1; //!< Not checked on DSi, but checked on 3DS
	u8  app_type    : 3; //!< See @ref TlncAppType
	u8  skip_intro  : 1; //!< Intro can only appear when `target_tid==0` (sysmenu)
	u8  unk5        : 1; //!< Unused
	u8  skip_load   : 1; //!< Needed for @ref TlncAppType_Ram
	u8  unk7        : 1; //!< Unused
} TlncData;

//! @private
typedef struct TlncArea {
	u32 magic;
	u8  version;
	u8  data_len;
	u16 data_crc16;
	TlncData data;
} TlncArea;

//! @private
typedef struct TlncParamArea {
	u64 caller_tid;
	u8  _unk_0x008;
	u8  flags;
	u16 caller_makercode;
	u16 head_len;
	u16 tail_len;
	u16 crc16;
	u16 extra;
	u8  data[TLNC_MAX_PARAM_SZ];
} TlncParamArea;

//! TLNC parameter data to pass to the launched application
typedef struct TlncParam {
	u64 tid;
	u16 makercode;
	u16 extra;
	u16 head_len;
	u16 tail_len;
	const void* head;
	const void* tail;
} TlncParam;

bool tlncGetDataTWL(TlncData* data); //!< @private
void tlncSetDataTWL(TlncData const* data); //!< @private

bool tlncGetParamTWL(TlncParam* param); //!< @private
void tlncSetParamTWL(TlncParam const* param); //!< @private

bool tlncSetJumpByArgvTWL(char* const argv[]); //!< @private

/*! @brief Reads the currently installed TLNC launch configuration data, if present
	@param[out] data Buffer to which copy the data describing the application to launch
	@return true on success, false on failure
*/
MK_INLINE bool tlncGetData(TlncData* data)
{
	return systemIsTwlMode() && tlncGetDataTWL(data);
}

//! @brief Installs the specified TLNC launch configuration @p data
MK_INLINE void tlncSetData(TlncData const* data)
{
	if (systemIsTwlMode()) {
		tlncSetDataTWL(data);
	}
}

/*! @brief Reads the currently installed TLNC application parameter data, if present
	@param[out] param Buffer to which copy the TLNC application parameter data
	@return true on success, false on failure
	@note After the function returns, the `head` and `tail` members of @p param will
	point directly to the received data in the shared memory area used for TLNC.
*/
MK_INLINE bool tlncGetParam(TlncParam* param)
{
	return systemIsTwlMode() && tlncGetParamTWL(param);
}

//! @brief Installs the specified TLNC application @p param data
MK_INLINE void tlncSetParam(TlncParam const* param)
{
	if (systemIsTwlMode()) {
		tlncSetParamTWL(param);
	}
}

/*! @brief Configures TLNC to launch a homebrew application
	@param argv NULL terminated argument vector. `argv[0]` should contain the
	full `sd:/` path to the `.nds` file pertaining to the homebrew application.
	@return true on success, false on failure
	@note This is a homebrew extension to the TLNC protocol, which requires an
	appropriate devkitPro-provided homebrew bootloader to be installed.
*/
MK_INLINE bool tlncSetJumpByArgv(char* const argv[])
{
	return systemIsTwlMode() && tlncSetJumpByArgvTWL(argv);
}

MK_EXTERN_C_END

//! @}
