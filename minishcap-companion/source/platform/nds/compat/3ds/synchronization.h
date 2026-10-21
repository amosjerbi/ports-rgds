#pragma once
/* NDS shim for libctru's LightLock: the ARM9 build is single-threaded, so locks are no-ops. */
typedef int LightLock;
static inline void LightLock_Init(LightLock* l) { *l = 0; }
static inline void LightLock_Lock(LightLock* l) { (void)l; }
static inline void LightLock_Unlock(LightLock* l) { (void)l; }
