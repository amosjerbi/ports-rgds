/* Hebrew translation lookup and right-to-left ordering; see hebrew.h. */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "hebrew.h"

#define RING 8
#define BUFSZ 320

static char ring[RING][BUFSZ];
static int ring_at;

static char *next_buf(void) { ring_at = (ring_at + 1) % RING; return ring[ring_at]; }

/* '|' and newlines to spaces, runs of spaces collapsed, trailing dots and spaces dropped */
static void norm(const char *s, char *o, size_t n)
{
    size_t k = 0;
    while (*s == ' ') s++;
    for (; *s && k + 1 < n; s++) {
        char c = *s == '|' || *s == '\n' || *s == '\r' || *s == '~' ? ' ' : *s;
        if (c == ' ' && k && o[k - 1] == ' ') continue;
        o[k++] = c;
    }
    while (k && (o[k - 1] == ' ' || o[k - 1] == '.')) k--;
    o[k] = 0;
}

static const char *find_exact(const char *k)
{
    int lo = 0, hi = he_nexact - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2, c = strcmp(k, he_exact[m].en);
        if (!c) return he_exact[m].he;
        if (c < 0) hi = m - 1; else lo = m + 1;
    }
    return NULL;
}

/* glob-match `s` against a pattern with %d (a number) and %s (any text); captures go to cap[] */
#define MAXCAP 4
typedef struct { const char *p; size_t n; } Cap;

static int match(const char *pat, const char *s, Cap *cap, int nc)
{
    for (; *pat; pat++, s++) {
        if (pat[0] == '%' && (pat[1] == 'd' || pat[1] == 's')) {
            if (nc >= MAXCAP) return 0;
            int num = pat[1] == 'd';
            for (size_t len = 1; s[len - 1]; len++) {
                if (num && !(isdigit((unsigned char)s[len - 1]) || (len == 1 && s[0] == '-') || s[len - 1] == ','))
                    break;
                cap[nc].p = s; cap[nc].n = len;
                if (match(pat + 2, s + len, cap, nc + 1)) return 1;
            }
            return 0;
        }
        if (*pat != *s) return 0;
    }
    return *s == 0;
}

const char *he_tr(const char *s)
{
    char k[BUFSZ];
    norm(s, k, sizeof k);
    if (!k[0]) return s;
    const char *h = find_exact(k);
    if (h) return h;
    Cap cap[MAXCAP];
    for (int i = 0; i < he_npat; i++) {
        if (!match(he_pat[i].en, k, cap, 0)) continue;
        char *o = next_buf();
        size_t n = 0;
        int ci = 0;
        for (const char *p = he_pat[i].he; *p && n + 1 < BUFSZ; p++) {
            if (p[0] == '%' && (p[1] == 'd' || p[1] == 's')) {
                char part[BUFSZ];
                size_t l = cap[ci].n < sizeof part - 1 ? cap[ci].n : sizeof part - 1;
                memcpy(part, cap[ci].p, l); part[l] = 0;
                const char *t = p[1] == 's' ? find_exact(part) : NULL;   /* "your %s": the item too */
                if (!t) t = part;
                for (; *t && n + 1 < BUFSZ; t++) o[n++] = *t;
                ci++; p++;
                continue;
            }
            o[n++] = *p;
        }
        o[n] = 0;
        return o;
    }
    return s;
}

/* ------------------------------------------------------------------ bidi */

enum { N, L, R };

static int strong_l(unsigned char c) { return isalnum(c) || (c >= 0x80 && !he_letter(c)); }

void he_visual(const char *in, char *out, size_t n)
{
    size_t len = strlen(in);
    if (len >= n) len = n - 1;
    uint8_t t[BUFSZ];
    if (len > sizeof t) len = sizeof t;
    const unsigned char *s = (const unsigned char *)in;
    for (size_t i = 0; i < len; i++)
        t[i] = he_letter(s[i]) ? R : strong_l(s[i]) ? L : N;
    /* number punctuation stays with its number: $5, 45%, 4.95, 1.2146/5, 10:30 */
    for (size_t i = 0; i < len; i++) {
        if (t[i] != N) continue;
        int dl = i > 0 && t[i - 1] == L, dr = i + 1 < len && isdigit(s[i + 1]);
        if (s[i] == '$' && dr) t[i] = L;
        else if (s[i] == '%' && i > 0 && isdigit(s[i - 1])) t[i] = L;
        else if (strchr(".,:/", s[i]) && dl && i + 1 < len && strong_l(s[i + 1])) t[i] = L;
    }
    /* other neutrals: left-to-right only between two left-to-right runs */
    for (size_t i = 0; i < len; i++) {
        if (t[i] != N) continue;
        size_t j = i;
        while (j < len && t[j] == N) j++;
        int before = i > 0 ? t[i - 1] : R, after = j < len ? t[j] : R;
        uint8_t d = before == L && after == L ? L : R;
        for (size_t k = i; k < j; k++) t[k] = d;
        i = j - 1;
    }
    /* reverse the line, then put each left-to-right run back in reading order */
    for (size_t i = 0; i < len; i++) {
        char c = in[len - 1 - i];
        if (t[len - 1 - i] == R)
            switch (c) {
            case '(': c = ')'; break;  case ')': c = '('; break;
            case '[': c = ']'; break;  case ']': c = '['; break;
            case '<': c = '>'; break;  case '>': c = '<'; break;
            case '{': c = '}'; break;  case '}': c = '{'; break;
            }
        out[i] = c;
    }
    out[len] = 0;
    for (size_t i = 0; i < len;) {
        if (t[len - 1 - i] != L) { i++; continue; }
        size_t j = i;
        while (j < len && t[len - 1 - j] == L) j++;
        for (size_t a = i, b = j - 1; a < b; a++, b--) { char c = out[a]; out[a] = out[b]; out[b] = c; }
        i = j;
    }
}
