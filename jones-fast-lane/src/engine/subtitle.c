/* Subtitles for the CD release's speech. The table maps clip numbers to the
 * matching line of the floppy release's text (see tools/mksubs.py). */
#include <stddef.h>
#include "subtitle.h"
#include "platform.h"

static const uint8_t *table;
static const char *strings;
static int count;
static const char *current;
static uint32_t until;

void subtitle_init(const uint8_t *data, uint32_t size)
{
    count = 0;
    if (!data || size < 2) return;
    int n = data[0] | data[1] << 8;
    if (2u + 4u * n > size) return;
    table = data + 2;
    strings = (const char *)data + 2 + 4 * n;
    count = n;
}

int subtitle_available(void) { return count > 0; }

void subtitle_show(int clip, int length_ticks)
{
    int lo = 0, hi = count - 1;
    current = NULL;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int c = table[4 * mid] | table[4 * mid + 1] << 8;
        if (c == clip) {
            current = strings + (table[4 * mid + 2] | table[4 * mid + 3] << 8);
            break;
        }
        if (c < clip) lo = mid + 1; else hi = mid - 1;
    }
    /* stay up a little after the voice ends, and long enough to read */
    int t = length_ticks + 45;
    if (t < 150) t = 150;
    until = plat_ticks() + (uint32_t)t;
}

void subtitle_hide(void) { current = NULL; }

const char *subtitle_current(uint32_t now)
{
    if (current && (int32_t)(now - until) > 0) current = NULL;
    return current;
}
