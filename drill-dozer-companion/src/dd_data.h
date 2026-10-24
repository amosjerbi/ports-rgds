#pragma once
/* Drill Dozer (USA, V49E) memory map used by the companion. Found by diffing RAM against the game's own Equipment screen. */
#include <stdint.h>
#define DD_ROM_CODE_ADDR 0x080000ACu /* "V49E" */
#define CHIPS_ADDR 0x02034670u       /* u32, shown on the Equipment screen as "Chips 00038"; the game writes it back to SRAM on Save */
#define CHIPS_MAX 99999
