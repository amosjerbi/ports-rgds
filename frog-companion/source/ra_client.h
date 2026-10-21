#pragma once
/* Client for RetroArch's network command interface (UDP, network_cmd_enable = true, default port 55355).
 * READ_CORE_MEMORY reads the emulated GBA address space (EWRAM 0x02..., IWRAM 0x03..., ROM 0x08...). */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool RaClient_Open(const char* host, int port);
void RaClient_Close(void);
/* Reads n bytes at a GBA address. Returns false on timeout or a core error. */
bool RaClient_Read(uint32_t addr, void* out, size_t n);
/* Writes n bytes at a GBA address (WRITE_CORE_MEMORY). Returns false if the core refused. */
bool RaClient_Write(uint32_t addr, const void* data, size_t n);
/* Sends a RetroArch command that has no reply (SAVE_STATE_SLOT n, LOAD_STATE_SLOT n, ...). */
bool RaClient_Command(const char* cmd);
/* RetroArch answers VERSION whenever the command interface is up. */
bool RaClient_Alive(void);
