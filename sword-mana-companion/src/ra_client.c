#include "ra_client.h"

#include <arpa/inet.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CHUNK 8192 /* reply is "xx " per byte; RetroArch answers one request per frame, so fewer, bigger reads are much faster */

static int sFd = -1;
static struct sockaddr_in sAddr;

bool RaClient_Open(const char* host, int port) {
    sFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sFd < 0)
        return false;
    memset(&sAddr, 0, sizeof sAddr);
    sAddr.sin_family = AF_INET;
    sAddr.sin_port = htons((uint16_t)port);
    return inet_pton(AF_INET, host, &sAddr.sin_addr) == 1;
}

void RaClient_Close(void) {
    if (sFd >= 0)
        close(sFd);
    sFd = -1;
}

static int Transact(const char* cmd, char* reply, size_t cap, int timeoutMs) {
    struct pollfd p = { .fd = sFd, .events = POLLIN };
    /* Drop any stale datagram from a previous timed-out request so replies cannot get out of step. */
    char scratch[16];
    while (recv(sFd, scratch, sizeof scratch, MSG_DONTWAIT) > 0) {}
    if (sendto(sFd, cmd, strlen(cmd), 0, (struct sockaddr*)&sAddr, sizeof sAddr) < 0)
        return -1;
    if (poll(&p, 1, timeoutMs) <= 0)
        return -1;
    ssize_t n = recv(sFd, reply, cap - 1, 0);
    if (n < 0)
        return -1;
    reply[n] = 0;
    return (int)n;
}

bool RaClient_Alive(void) {
    char r[64];
    return Transact("VERSION", r, sizeof r, 200) > 0;
}

bool RaClient_Read(uint32_t addr, void* out, size_t n) {
    uint8_t* dst = out;
    static char reply[CHUNK * 3 + 128];
    while (n) {
        const size_t take = n < CHUNK ? n : CHUNK;
        char cmd[64];
        snprintf(cmd, sizeof cmd, "READ_CORE_MEMORY %x %zu", addr, take);
        if (Transact(cmd, reply, sizeof reply, 100) <= 0)
            return false;
        /* "READ_CORE_MEMORY <addr> xx xx ..." (an error replies with "-1 <message>" in place of the bytes) */
        char* p = strchr(reply, ' ');
        if (!p || !(p = strchr(p + 1, ' ')))
            return false;
        size_t got = 0;
        while (got < take) {
            char* end;
            long v = strtol(p, &end, 16);
            if (end == p || v < 0 || v > 255)
                return false;
            dst[got++] = (uint8_t)v;
            p = end;
        }
        dst += take;
        addr += (uint32_t)take;
        n -= take;
    }
    return true;
}

bool RaClient_Write(uint32_t addr, const void* data, size_t n) {
    const uint8_t* src = data;
    char cmd[CHUNK * 3 + 64], reply[128];
    while (n) {
        const size_t take = n < CHUNK ? n : CHUNK;
        int len = snprintf(cmd, sizeof cmd, "WRITE_CORE_MEMORY %x", addr);
        for (size_t i = 0; i < take; i++)
            len += snprintf(cmd + len, sizeof cmd - (size_t)len, " %02x", src[i]);
        if (Transact(cmd, reply, sizeof reply, 100) <= 0)
            return false;
        /* success echoes "WRITE_CORE_MEMORY <addr> <count>"; failure carries "-1" */
        if (strstr(reply, " -1"))
            return false;
        src += take;
        addr += (uint32_t)take;
        n -= take;
    }
    return true;
}

bool RaClient_Command(const char* cmd) {
    return sFd >= 0 && sendto(sFd, cmd, strlen(cmd), 0, (struct sockaddr*)&sAddr, sizeof sAddr) >= 0;
}
