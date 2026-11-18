/* Hebrew top screen (built with -DHEBREW: make he / make cd-he).
 *
 * Text is translated through a table generated from tools/hebrew.tsv (Hebrew stored as ISO-8859-8, one
 * byte per letter, 0xE0..0xFA), drawn right-to-left with a small built-in pixel font, and the whole panel
 * layout is mirrored. Strings with no translation stay English. */
#ifndef HEBREW_H
#define HEBREW_H
#include <stdint.h>
#include <stddef.h>

typedef struct { uint8_t w, h, rows[10]; } HeGlyph;      /* rows: 1 bit per pixel, MSB = left column */
typedef struct { const char *en, *he; } HePair;

extern const HeGlyph he_font_small[27], he_font_big[27];
extern const HePair he_exact[], he_pat[];
extern const int he_nexact, he_npat;

#define HE_FIRST 0xE0          /* alef */
#define HE_LAST  0xFA          /* tav */
static inline int he_letter(unsigned c) { return c >= HE_FIRST && c <= HE_LAST; }

/* Hebrew for `s` (cleaned up like the location panel does), or `s` itself; the result lives in a small ring
 * of buffers, valid for a few more calls. */
const char *he_tr(const char *s);

/* logical -> visual (left-to-right drawing) order for a right-to-left line; `out` holds strlen(in)+1 */
void he_visual(const char *in, char *out, size_t n);

#endif
