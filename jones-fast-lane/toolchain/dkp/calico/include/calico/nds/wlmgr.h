// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "../dev/netbuf.h"
#include "../dev/wlan.h"

/*! @addtogroup wlmgr

	The wireless interface of the NDS is connected to the ARM7, and cannot be accessed
	directly by the ARM9. Moreover, the DSi adds a new, entirely different wireless
	interface supporting newer wireless standards. Calico contains drivers for both the
	old NDS hardware (Mitsumi) and the new DSi hardware (Atheros).

	The wireless manager API abstracts the differences between the two, and provides a
	common low-level interface on the ARM9 that can be used to implement a networking stack.
	Wireless manager commands are issued asynchronously, and it is possible to register
	callbacks to respond to events, as well as receiving network packets.

	Higher level topics such as handling the user's WFC settings or providing a TCP/IP
	network stack are handled by the dswifi library. Please refer to it for more details.

	@{
*/

MK_EXTERN_C_START

//! List of wireless modes
typedef enum WlMgrMode {
	WlMgrMode_Infrastructure = 0, //!< Infrastructure mode (i.e. normal Wi-Fi access)
	WlMgrMode_LocalComms     = 1, //!< Local wireless communications (not yet supported)
} WlMgrMode;

//! List of wireless manager states
typedef enum WlMgrState {
	WlMgrState_Stopped        = 0, //!< Wireless interface is stopped
	WlMgrState_Starting       = 1, //!< Wireless interface is being started
	WlMgrState_Stopping       = WlMgrState_Starting, //!< Wireless interface is being stopped
	WlMgrState_Idle           = 2, //!< Wireless interface is started and idle
	WlMgrState_Scanning       = 3, //!< Wireless interface is scanning for networks
	WlMgrState_Associating    = 4, //!< Wireless interface is associating to a network
	WlMgrState_Associated     = 5, //!< Wireless interface is associated to a network
	WlMgrState_Disassociating = 6, //!< Wireless interface is disassociating from a network
} WlMgrState;

//! List of wireless manager events
typedef enum WlMgrEvent {
	WlMgrEvent_NewState     = 0, //!< The wireless interface entered a new state (see @ref WlMgrState)
	WlMgrEvent_CmdFailed    = 1, //!< The last wireless manager command failed
	WlMgrEvent_ScanComplete = 2, //!< Network scanning completed
	WlMgrEvent_Disconnected = 3, //!< The wireless interface was disconnected from the network
} WlMgrEvent;

#if defined(ARM9)

//! Minimum size of the packet heap memory passed to @ref WlMgrInitConfig
#define WLMGR_MIN_PACKET_MEM_SZ (53*sizeof(NetBuf) + 0x4d00)
//! Default priority of the wireless manager thread
#define WLMGR_DEFAULT_THREAD_PRIO 0x10

//! Wireless manager settings
typedef struct WlMgrInitConfig {
	//! Pointer to packet heap memory, used to set up a shared packet heap between the ARM7 and ARM9 side of the wireless driver
	void*  pktmem;
	//! Size of the packet heap
	size_t pktmem_sz;
	//! Allocation bitmap used to distribute memory between TX (1) and RX (0) packet heaps
	u32    pktmem_allocmap;
} WlMgrInitConfig;

/*! @brief Wireless manager event handler function
	@param[in] user User data passed to @ref wlmgrSetEventHandler
	@param[in] event Event type (see @ref WlMgrEvent)
	@param[in] arg0 Event argument (see below)
	@param[in] arg1 Event argument (see below)

	This handler function is called from within the wireless manager thread when
	an event occurs.

	<table>
	<tr>
		<th>Event type</th>
		<th>@p arg0</th>
		<th>@p arg1</th>
	</tr>
	<tr>
		<td>@ref WlMgrEvent_NewState</td>
		<td>New state (see @ref WlMgrState)</td>
		<td>Previous state (see @ref WlMgrState)</td>
	</tr>
	<tr>
		<td>@ref WlMgrEvent_CmdFailed</td>
		<td>Internal ID of the failed command</td>
		<td>Unused/0</td>
	</tr>
	<tr>
		<td>@ref WlMgrEvent_ScanComplete</td>
		<td>Pointer to @ref WlanBssDesc array containing detected BSSs</td>
		<td>Number of entries in the array</td>
	</tr>
	<tr>
		<td>@ref WlMgrEvent_Disconnected</td>
		<td>IEEE 802.11 disconnect reason</td>
		<td>Unused/0</td>
	</tr>
	</table>
*/
typedef void (*WlMgrEventFn)(void* user, WlMgrEvent event, uptr arg0, uptr arg1);

/*! @brief Raw packet reception handler
	@param[in] user User data passed to @ref wlmgrSetRawRxHandler
	@param[in] pPacket Received network packet
	@note The handler function receives ownership of the network packet, and thus
	it is necessary to release the buffer using @ref netbufFree. Failure to do so
	produces a memory leak which will exhaust the RX packet heap.
*/
typedef void (*WlMgrRawRxFn)(void* user, NetBuf* pPacket);

/*! @brief Initializes the wireless manager
	@param[in] config Settings to use (see @ref WlMgrInitConfig)
	@param[in] thread_prio Desired thread priority of the ARM9 side wireless manager
	@return true on success, false on failure
*/
bool wlmgrInit(const WlMgrInitConfig* config, u8 thread_prio);

//! Returns the default wireless manager settings
MK_INLINE const WlMgrInitConfig* wlmgrGetDefaultConfig(void)
{
	extern const WlMgrInitConfig g_wlmgrDefaultConfig;
	return &g_wlmgrDefaultConfig;
}

//! Initializes the wireless manager using the default settings
MK_INLINE bool wlmgrInitDefault(void)
{
	return wlmgrInit(wlmgrGetDefaultConfig(), WLMGR_DEFAULT_THREAD_PRIO);
}

//! Sets @p cb (with @p user data) as the wireless manager event handler
void wlmgrSetEventHandler(WlMgrEventFn cb, void* user);

//! Returns the current wireless state (see @ref WlMgrState)
WlMgrState wlmgrGetState(void);

//! Returns the current value of the RSSI (Received Signal Strength Indicator)
unsigned wlmgrGetRssi(void);

//! Returns true if the last wireless manager command failed
bool wlmgrLastCmdFailed(void);

//! Returns the current wireless signal strength, in number of bars (0..3)
MK_INLINE unsigned wlmgrGetSignalStrength(void)
{
	return wlanCalcSignalStrength(wlmgrGetRssi());
}

//! Starts the wireless interface in the desired @p mode (see @ref WlMgrMode)
void wlmgrStart(WlMgrMode mode);

//! Stops the wireless interface
void wlmgrStop(void);

/*! @brief Starts a scan for wireless networks
	@param[out] out_table Output array of @ref WlanBssDesc asdasdasdasd
	@param[in] filter (Optional) Wireless scan filter to use (see @ref WlanBssScanFilter)
*/
void wlmgrStartScan(WlanBssDesc* out_table, WlanBssScanFilter const* filter);

/*! @brief Associates to a wireless network
	@param[in] bss Structure describing the wireless network (see @ref WlanBssDesc)
	@param[in] auth Authentication data for WEP/WPA (optional, see @ref WlanAuthData)
*/
void wlmgrAssociate(WlanBssDesc const* bss, WlanAuthData const* auth);

//! Disassociates from the currently associated network
void wlmgrDisassociate(void);

//! Sets @p cb (with @p user data) as the handler function for raw received network packets
void wlmgrSetRawRxHandler(WlMgrRawRxFn cb, void* user);

//! Transmits a raw network @p pPacket
void wlmgrRawTx(NetBuf* pPacket);

#elif defined(ARM7)

/*! @brief Starts the ARM7 side wireless manager server
	@param[in] thread_prio Desired thread priority of the ARM7 side wireless manager
*/
void wlmgrStartServer(u8 thread_prio);

#endif

MK_EXTERN_C_END

//! @}
