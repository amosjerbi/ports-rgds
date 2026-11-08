/* SCI0/SCI1-early bytecode interpreter with a flat 16-bit heap. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
#include "vm.h"
#include "resource.h"
#include "kernel.h"

uint8_t vm_heap[HEAP_SIZE];
VmState vm;

/* ------------------------------------------------------------------ log */

void vm_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    plat_vlog(fmt, ap);
    va_end(ap);
}

void vm_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    plat_vlog(fmt, ap);
    va_end(ap);
}

/* --------------------------------------------------------------- heap */
/* Blocks: [u16 size|used-bit][payload]. size includes the header, even. */

#define BLK_USED 1
static uint16_t heap_end;

static void heap_reset(void)
{
    memset(vm_heap, 0, sizeof vm_heap);
    heap_end = 0xfff0;
    hw16(HEAP_START, (uint16_t)(heap_end - HEAP_START));
    hw16(heap_end, 0);  /* terminator */
}

static void heap_dump(void);

uint16_t heap_alloc(uint16_t size)
{
    uint32_t need = ((uint32_t)size + 2 + 1) & ~1u;
    if (need < 4) need = 4;
    uint16_t p = HEAP_START;
    while (p < heap_end) {
        uint16_t h = hp16(p), bs = h & ~BLK_USED;
        if (bs == 0) break;
        if (!(h & BLK_USED)) {
            /* coalesce following free blocks */
            uint16_t q = (uint16_t)(p + bs);
            while (q < heap_end && !(hp16(q) & BLK_USED) && hp16(q)) {
                bs = (uint16_t)(bs + hp16(q));
                q = (uint16_t)(p + bs);
            }
            hw16(p, bs);
            if (bs >= need) {
                if (bs - need >= 6) {
                    hw16((uint16_t)(p + need), (uint16_t)(bs - need));
                    bs = (uint16_t)need;
                }
                hw16(p, bs | BLK_USED);
                memset(&vm_heap[p + 2], 0, bs - 2);
                return (uint16_t)(p + 2);
            }
        }
        p = (uint16_t)(p + bs);
    }
    vm_error("heap_alloc(%u) failed: free %u largest %u\n", size, heap_free_bytes(), heap_largest_free());
    heap_dump();
    return 0;
}

void heap_free(uint16_t addr)
{
    if (addr < HEAP_START + 2 || addr >= heap_end)
        return;
    uint16_t p = (uint16_t)(addr - 2);
    hw16(p, hp16(p) & ~BLK_USED);
}

uint16_t heap_free_bytes(void)
{
    uint32_t total = 0;
    for (uint16_t p = HEAP_START; p < heap_end;) {
        uint16_t h = hp16(p), bs = h & ~BLK_USED;
        if (!bs) break;
        if (!(h & BLK_USED)) total += bs - 2;
        p = (uint16_t)(p + bs);
    }
    return total > 0xffff ? 0xffff : (uint16_t)total;
}

uint16_t heap_largest_free(void)
{
    uint16_t best = 0, run = 0;
    for (uint16_t p = HEAP_START; p < heap_end;) {
        uint16_t h = hp16(p), bs = h & ~BLK_USED;
        if (!bs) break;
        if (!(h & BLK_USED)) { run = (uint16_t)(run + bs); if (run - 2 > best) best = (uint16_t)(run - 2); }
        else run = 0;
        p = (uint16_t)(p + bs);
    }
    return best;
}

/* ------------------------------------------------------------- selectors */

#define MAX_SELECTORS 1024
static const char *sel_names[MAX_SELECTORS];
static int sel_count;

static void load_selector_names(void)
{
    uint16_t sz;
    const uint8_t *v = res_load(RES_VOCAB, 997, &sz);
    sel_count = 0;
    if (!v) return;
    int n = v[0] | v[1] << 8;
    for (int i = 0; i < n && i < MAX_SELECTORS; i++) {
        uint16_t o = (uint16_t)(v[2 + 2 * i] | v[3 + 2 * i] << 8);
        uint16_t len = (uint16_t)(v[o] | v[o + 1] << 8);
        char *s = malloc(len + 1);
        memcpy(s, v + o + 2, len);
        s[len] = 0;
        sel_names[i] = s;
    }
    sel_count = n < MAX_SELECTORS ? n : MAX_SELECTORS;
}

int sel_id(const char *name)
{
    for (int i = 0; i < sel_count; i++)
        if (sel_names[i] && !strcmp(sel_names[i], name))
            return i;
    return -1;
}

const char *sel_name(int id)
{
    return (id >= 0 && id < sel_count && sel_names[id]) ? sel_names[id] : "?";
}

/* --------------------------------------------------------------- classes */

#define MAX_CLASSES 256
typedef struct { uint16_t script; reg_t addr; } ClassEntry;
static ClassEntry classes[MAX_CLASSES];
static int class_count;

static void load_class_table(void)
{
    uint16_t sz;
    const uint8_t *v = res_load(RES_VOCAB, 996, &sz);
    class_count = 0;
    if (!v) return;
    for (int i = 0; i + 4 <= sz && class_count < MAX_CLASSES; i += 4)
        classes[class_count++].script = (uint16_t)(v[i + 2] | v[i + 3] << 8);
}

reg_t class_addr(int species)
{
    if (species < 0 || species >= class_count)
        return 0;
    if (!classes[species].addr)
        script_load(classes[species].script);
    if (!classes[species].addr)
        vm_error("class %d not found in script %d\n", species, classes[species].script);
    return classes[species].addr;
}

/* --------------------------------------------------------------- scripts */
/* As in the DOS interpreter, a script's code stays outside the heap (in the
 * resource buffer); only its objects, locals, strings and said-specs are
 * copied into the heap. Heap objects carry a 10-byte header:
 *   obj-10 magic 0x1234   obj-8 script number   obj-6 offset of the method
 *   list in the script    obj-4 unused          obj-2 number of variables */

#define MAX_SCRIPTS 1000
#define MAX_MAP     48
typedef struct { uint16_t src, len, dst; uint8_t obj; } MapEnt;   /* script offset -> heap */
typedef struct {
    uint16_t heap, heapsize;          /* heap block holding the data parts */
    uint16_t size;                    /* resource size */
    uint16_t locals, nlocals;
    uint16_t exports, nexports;       /* script offset of export table */
    uint8_t  loaded, nmap;
    MapEnt   map[MAX_MAP];
    const uint8_t *code;              /* resource data (locked while loaded) */
} ScriptInfo;
static ScriptInfo scripts[MAX_SCRIPTS];

#define OH_MAGIC  10
#define OH_SCRIPT 8
#define OH_FUNCS  6
#define OH_NVARS  2

static inline uint16_t rd16c(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

/* --------------------------------------------------------------- objects */

int obj_is_object(reg_t obj)
{
    return obj >= HEAP_START + OH_MAGIC && hp16((uint16_t)(obj - OH_MAGIC)) == OBJ_MAGIC;
}

int obj_var_count(reg_t obj) { return hp16((uint16_t)(obj - OH_NVARS)); }
reg_t obj_var(reg_t obj, int i) { return hp16((uint16_t)(obj + 2 * i)); }
void obj_set_var(reg_t obj, int i, reg_t v) { hw16((uint16_t)(obj + 2 * i), v); }

static int is_class(reg_t obj) { return (obj_var(obj, SEL_INFO) & INFO_CLASS) != 0; }

static const uint8_t *obj_code(reg_t obj)
{
    uint16_t sn = hp16((uint16_t)(obj - OH_SCRIPT));
    return sn < MAX_SCRIPTS && scripts[sn].loaded ? scripts[sn].code : NULL;
}

int obj_find_var(reg_t obj, int sel)
{
    if (!obj_is_object(obj))
        return -1;
    reg_t cls = is_class(obj) ? obj : obj_var(obj, SEL_SUPER);
    if (!obj_is_object(cls))
        return -1;
    const uint8_t *c = obj_code(cls);
    if (!c) return -1;
    int n = obj_var_count(obj), cn = obj_var_count(cls);
    /* a class's variable selector list sits just before its method list */
    const uint8_t *list = c + hp16((uint16_t)(cls - OH_FUNCS)) - 2 - 2 * cn;
    for (int i = 0; i < n && i < cn; i++)
        if (rd16c(list + 2 * i) == sel)
            return i;
    return -1;
}

/* code address: script number << 16 | offset, 0 if none */
uint32_t obj_find_method(reg_t obj, int sel)
{
    int guard = 0;
    while (obj && obj_is_object(obj) && guard++ < 64) {
        const uint8_t *c = obj_code(obj);
        if (c) {
            const uint8_t *list = c + hp16((uint16_t)(obj - OH_FUNCS));
            int n = rd16c(list - 2);
            for (int i = 0; i < n; i++)
                if (rd16c(list + 2 * i) == sel)
                    return (uint32_t)hp16((uint16_t)(obj - OH_SCRIPT)) << 16 | rd16c(list + 2 * n + 2 + 2 * i);
        }
        obj = obj_var(obj, SEL_SUPER);
    }
    return 0;
}

static int sel_name_id = -1;

const char *obj_name(reg_t obj)
{
    if (!obj_is_object(obj)) return "<not object>";
    if (sel_name_id < 0) sel_name_id = sel_id("name");
    int i = obj_find_var(obj, sel_name_id);
    if (i < 0) return "<noname>";
    reg_t s = obj_var(obj, i);
    return s ? hstr(s) : "<null>";
}

reg_t get_sel(reg_t obj, int sel)
{
    int i = obj_find_var(obj, sel);
    return i < 0 ? 0 : obj_var(obj, i);
}

void set_sel(reg_t obj, int sel, reg_t v)
{
    int i = obj_find_var(obj, sel);
    if (i >= 0) obj_set_var(obj, i, v);
}

int has_sel(reg_t obj, int sel)
{
    return obj_find_var(obj, sel) >= 0 || obj_find_method(obj, sel) != 0;
}

/* Clone: copy header + variables (methods stay with the defining script) */
reg_t obj_clone(reg_t parent)
{
    int n = obj_var_count(parent);
    uint16_t blk = heap_alloc((uint16_t)(OH_MAGIC + 2 * n));
    if (!blk) return 0;
    memcpy(&vm_heap[blk], &vm_heap[parent - OH_MAGIC], OH_MAGIC + 2 * n);
    return (reg_t)(blk + OH_MAGIC);
}

void obj_free(reg_t obj)
{
    hw16((uint16_t)(obj - OH_MAGIC), 0);
    heap_free((uint16_t)(obj - OH_MAGIC));
}

/* --------------------------------------------------------------- scripts */

/* script offset -> heap address (0 if the offset isn't in a copied part) */
static int wide_exports;
int vm_late_sci1(void) { return wide_exports; }

/* The CD release times the machine in room 764 and, if it looks slow, sets
 * global 534 = 3 / 567 = 1, which turns off the character portraits (Talker
 * draws only when global 534 < 2). Paced to real time, the DS always scores
 * "slow", so the two slow outcomes are patched to the fast one
 * (567 = 3, 534 = 0), as ScummVM does for Sierra speed tests. */
static void patch_speed_test(uint8_t *d, uint16_t sz)
{
    /* ldi a; sag 567; ldi b; sag 534 */
    for (uint32_t i = 0; i + 10 <= sz; i++)
        if (d[i] == 0x35 && d[i + 2] == 0xa0 && d[i + 3] == 0x37 && d[i + 4] == 0x02 &&
            d[i + 5] == 0x35 && d[i + 7] == 0xa0 && d[i + 8] == 0x16 && d[i + 9] == 0x02) {
            d[i + 1] = 3;
            d[i + 6] = 0;
        }
}

static uint16_t script_map(const ScriptInfo *s, uint16_t off)
{
    for (int i = 0; i < s->nmap; i++)
        if (off >= s->map[i].src && off < s->map[i].src + s->map[i].len)
            return (uint16_t)(s->map[i].dst + off - s->map[i].src);
    return 0;
}

static void convert_object(reg_t obj, int type)
{
    reg_t species = obj_var(obj, SEL_SPECIES), super = obj_var(obj, SEL_SUPER);
    obj_set_var(obj, SEL_SPECIES, type == 6 ? obj : class_addr(species));
    obj_set_var(obj, SEL_SUPER, super == 0xffff ? 0 : class_addr(super));
}

uint16_t script_load(int num)
{
    if (num < 0 || num >= MAX_SCRIPTS)
        return 0;
    ScriptInfo *s = &scripts[num];
    if (s->loaded)
        return s->heap ? s->heap : 1;
    uint16_t sz;
    const uint8_t *d = res_load(RES_SCRIPT, num, &sz);
    if (!d) {
        vm_error("script %d not found\n", num);
        return 0;
    }
    if (num == 764) patch_speed_test((uint8_t *)d, sz);
    /* pass 1: heap size */
    uint32_t need = 0;
    for (uint32_t p = 0; p + 4 <= sz;) {
        uint16_t type = rd16c(d + p), bsz = rd16c(d + p + 2);
        if (type == 0 || bsz < 4) break;
        if (type == 1 || type == 6) need += OH_MAGIC + 2u * rd16c(d + p + 10);
        else if (type == 10 || type == 5 || type == 4) need += bsz - 4u;
        p += bsz;
    }
    uint16_t heap = 0;
    if (need) {
        heap = heap_alloc((uint16_t)need);
        if (!heap) { res_unlock(RES_SCRIPT, num); return 0; }
    }
    memset(s, 0, sizeof *s);
    s->code = d;
    s->size = sz;
    s->heap = heap;
    s->heapsize = (uint16_t)need;
    s->loaded = 1;

    /* pass 2: copy data parts into the heap, register classes */
    uint16_t h = heap;
    for (uint32_t p = 0; p + 4 <= sz;) {
        uint16_t type = rd16c(d + p), bsz = rd16c(d + p + 2);
        if (type == 0 || bsz < 4) break;
        if (s->nmap >= MAX_MAP && (type == 1 || type == 6 || type == 10 || type == 5 || type == 4)) {
            vm_error("script %d: too many blocks\n", num);
            break;
        }
        if (type == 1 || type == 6) {
            int nv = rd16c(d + p + 10);
            reg_t obj = (reg_t)(h + OH_MAGIC);
            hw16((uint16_t)(obj - OH_MAGIC), OBJ_MAGIC);
            hw16((uint16_t)(obj - OH_SCRIPT), (reg_t)num);
            hw16((uint16_t)(obj - OH_FUNCS), (reg_t)(p + 12 + rd16c(d + p + 8)));
            hw16((uint16_t)(obj - OH_NVARS), (reg_t)nv);
            memcpy(&vm_heap[obj], d + p + 12, 2u * nv);
            s->map[s->nmap++] = (MapEnt){ (uint16_t)(p + 12), (uint16_t)(2 * nv), obj, (uint8_t)type };
            if (type == 6) {
                int species = rd16c(d + p + 12);
                if (species < MAX_CLASSES) classes[species].addr = obj;
            }
            h = (uint16_t)(h + OH_MAGIC + 2 * nv);
        } else if (type == 10 || type == 5 || type == 4) {
            uint16_t len = (uint16_t)(bsz - 4);
            memcpy(&vm_heap[h], d + p + 4, len);
            s->map[s->nmap++] = (MapEnt){ (uint16_t)(p + 4), len, h, 0 };
            if (type == 10) { s->locals = h; s->nlocals = (uint16_t)(len / 2); }
            h = (uint16_t)(h + len);
        } else if (type == 7) {
            s->nexports = rd16c(d + p + 4);
            s->exports = (uint16_t)(p + 6);
            /* later SCI1 (the CD release) stores exports as offset:segment pairs */
            if (s->nexports) wide_exports = bsz - 6 >= 4 * s->nexports;
        }
        p += bsz;
    }
    /* pass 3: relocate pointers inside the copied parts */
    for (uint32_t p = 0; p + 4 <= sz;) {
        uint16_t type = rd16c(d + p), bsz = rd16c(d + p + 2);
        if (type == 0 || bsz < 4) break;
        if (type == 8) {
            int n = rd16c(d + p + 4);
            for (int i = 0; i < n; i++) {
                uint16_t at = script_map(s, rd16c(d + p + 6 + 2 * i));
                if (!at) continue;
                uint16_t to = script_map(s, hp16(at));
                if (to) hw16(at, to);
            }
        }
        p += bsz;
    }
    /* pass 4: species / superclass to addresses */
    for (int i = 0; i < s->nmap; i++)
        if (s->map[i].obj) convert_object(s->map[i].dst, s->map[i].obj);
    if (vm.trace)
        vm_log("loaded script %d: heap %04x+%u (free %u)\n", num, heap, s->heapsize, heap_free_bytes());
    return heap ? heap : 1;
}

void script_unload(int num)
{
    if (num <= 0 || num >= MAX_SCRIPTS || !scripts[num].loaded)
        return;
    ScriptInfo *s = &scripts[num];
    for (int i = 0; i < class_count; i++)
        if (classes[i].addr >= s->heap && classes[i].addr < s->heap + s->heapsize)
            classes[i].addr = 0;
    if (s->heap) heap_free(s->heap);
    res_unlock(RES_SCRIPT, num);
    s->loaded = 0;
    s->code = NULL;
}

static uint16_t export_off(int num, int index)
{
    if (!script_load(num))
        return 0;
    ScriptInfo *s = &scripts[num];
    if (index < 0 || index >= s->nexports) {
        vm_error("script %d has no export %d\n", num, index);
        return 0;
    }
    return rd16c(s->code + s->exports + (wide_exports ? 4 : 2) * index);
}

/* ScriptID: exported object (heap address) */
reg_t script_export(int num, int index)
{
    uint16_t off = export_off(num, index);
    if (!off) return 0;
    uint16_t a = script_map(&scripts[num], off);
    if (!a) vm_error("script %d export %d is not an object\n", num, index);
    return a;
}

int script_number_of(uint16_t addr)
{
    for (int i = 0; i < MAX_SCRIPTS; i++) {
        ScriptInfo *s = &scripts[i];
        if (s->loaded && s->heap && addr >= s->heap && addr < s->heap + s->heapsize)
            return i;
    }
    return -1;
}

reg_t vm_global(int i) { return hp16((uint16_t)(scripts[0].locals + 2 * i)); }
void vm_set_global(int i, reg_t v) { hw16((uint16_t)(scripts[0].locals + 2 * i), v); }

/* ------------------------------------------------------------------ text */

const char *vm_text(reg_t module, reg_t index)
{
    uint16_t sz;
    const char *t = (const char *)res_load(RES_TEXT, module, &sz);
    if (!t) return "";
    res_unlock(RES_TEXT, module);   /* stays cached until purge */
    const char *end = t + sz;
    while (index-- && t < end)
        t += strlen(t) + 1;
    return t < end ? t : "";
}

const char *vm_string(reg_t addr, reg_t next, int *used_next)
{
    if (addr && addr < HEAP_START) {
        if (used_next) *used_next = 1;
        return vm_text(addr, next);
    }
    if (used_next) *used_next = 0;
    return addr ? hstr(addr) : "";
}

/* ---------------------------------------------------------------- execute */

#define PUSH(v) do { hw16(vm.sp, (reg_t)(v)); vm.sp += 2; } while (0)
#define POP()   (vm.sp -= 2, hp16(vm.sp))
#define TOP()   hp16((uint16_t)(vm.sp - 2))

static int depth;
static reg_t execute(int script, uint16_t pc, reg_t self, uint16_t params);

static reg_t send_frame(reg_t obj, reg_t lookup, uint16_t frame, int nwords)
{
    uint16_t end = (uint16_t)(frame + 2 * nwords);
    uint16_t saved_sp = vm.sp;
    reg_t acc = vm.acc;
    if (!obj_is_object(obj)) {
        vm_error("send to non-object %04x (sel %s)\n", obj, sel_name(hp16(frame)));
        return 0;
    }
    for (uint16_t p = frame; p < end;) {
        int sel = hp16(p), argc = hp16((uint16_t)(p + 2));
        int vi = obj_find_var(obj, sel);
        if (vi >= 0 && lookup == obj) {
            if (argc == 0) acc = obj_var(obj, vi);
            else obj_set_var(obj, vi, hp16((uint16_t)(p + 4)));
        } else {
            uint32_t m = obj_find_method(lookup, sel);
            if (!m && vi >= 0) {
                if (argc == 0) acc = obj_var(obj, vi);
                else obj_set_var(obj, vi, hp16((uint16_t)(p + 4)));
            } else if (!m) {
                vm_error("%s does not understand %s\n", obj_name(obj), sel_name(sel));
            } else {
                if (vm.trace > 1)
                    vm_log("%*s(%s %s: argc %d)\n", depth, "", obj_name(obj), sel_name(sel), argc);
                vm.sp = end;
                acc = execute((int)(m >> 16), (uint16_t)m, obj, (uint16_t)(p + 2));
            }
        }
        if (vm.restarting || vm.quit) break;
        p = (uint16_t)(p + 4 + 2 * argc);
    }
    vm.sp = saved_sp;
    return acc;
}

reg_t invoke(reg_t obj, int sel, int argc, const reg_t *argv)
{
    uint16_t frame = vm.sp;
    reg_t saved_acc = vm.acc;
    PUSH(sel);
    PUSH(argc);
    for (int i = 0; i < argc; i++) PUSH(argv[i]);
    reg_t r = send_frame(obj, obj, frame, 2 + argc);
    vm.sp = frame;
    vm.acc = saved_acc;
    return r;
}

static int16_t s16(reg_t v) { return (int16_t)v; }

/* Early SCI1 (floppy) lofs operands are PC-relative; later interpreters
 * (the CD release) use absolute script offsets. Decided on first use. */
static int lofs_abs = -1;

static reg_t lofs(const ScriptInfo *si, uint16_t pc, int16_t rel)
{
    uint16_t off;
    if (lofs_abs < 0) {
        int r = script_map(si, (uint16_t)(pc + rel)) != 0, a = script_map(si, (uint16_t)rel) != 0;
        if (r != a) { lofs_abs = a; vm_log("lofs: %s offsets (script %d)\n", a ? "absolute" : "relative", (int)(si - scripts)); }
        /* both readings valid: the later (wide-export) interpreter is absolute */
        int abs_ = (r && a) ? wide_exports : a;
        off = abs_ ? (uint16_t)rel : (uint16_t)(pc + rel);
    } else
        off = lofs_abs ? (uint16_t)rel : (uint16_t)(pc + rel);
    uint16_t a = script_map(si, off);
    if (!a) vm_error("lofs: offset %04x of script %d is not data\n", off, (int)(si - scripts));
    return a;
}

static reg_t execute(int script, uint16_t pc, reg_t self, uint16_t params)
{
    ScriptInfo *si = &scripts[script];
    if (!si->loaded) { vm_error("execute in unloaded script %d\n", script); return 0; }
    const uint8_t *cb = si->code;
    uint16_t locals = si->locals;
    uint16_t globals = scripts[0].locals;
    uint16_t fp = vm.sp;
    reg_t acc = vm.acc;
    depth++;
    if (depth > 200) {
        vm_error("VM recursion too deep\n");
        depth--;
        return 0;
    }

    for (;;) {
        if (vm.restarting || vm.quit) break;
        if (vm.sp >= vm.stack_top - 64) {
            vm_error("stack overflow\n");
            break;
        }
        uint8_t ob = cb[pc++];
        int op = ob >> 1, bytemode = ob & 1;
#define OPV()  (bytemode ? (reg_t)cb[pc++] : (pc += 2, rd16c(cb + pc - 2)))
#define OPS()  (bytemode ? (reg_t)(int16_t)(int8_t)cb[pc++] : (pc += 2, rd16c(cb + pc - 2)))
#define OPB()  ((reg_t)cb[pc++])
        reg_t a, b;
        switch (op) {
        case 0x00: acc ^= 0xffff; break;
        case 0x01: acc = (reg_t)(POP() + acc); break;
        case 0x02: a = POP(); acc = (reg_t)(a - acc); break;
        case 0x03: acc = (reg_t)(s16(POP()) * s16(acc)); break;
        case 0x04: a = POP(); acc = acc ? (reg_t)(s16(a) / s16(acc)) : 0; break;
        case 0x05: {
            int16_t m = s16(acc), v = s16(POP());
            int16_t r = m ? (int16_t)(v % m) : 0;
            if (r < 0) r = (int16_t)(r + m);
            acc = (reg_t)r;
            break;
        }
        case 0x06: a = POP(); acc = (reg_t)(a >> acc); break;
        case 0x07: a = POP(); acc = (reg_t)(a << acc); break;
        case 0x08: acc ^= POP(); break;
        case 0x09: acc &= POP(); break;
        case 0x0a: acc |= POP(); break;
        case 0x0b: acc = (reg_t)-s16(acc); break;
        case 0x0c: acc = !acc; break;
        case 0x0d: vm.prev = acc; acc = POP() == acc; break;
        case 0x0e: vm.prev = acc; acc = POP() != acc; break;
        case 0x0f: vm.prev = acc; acc = s16(POP()) > s16(acc); break;
        case 0x10: vm.prev = acc; acc = s16(POP()) >= s16(acc); break;
        case 0x11: vm.prev = acc; acc = s16(POP()) < s16(acc); break;
        case 0x12: vm.prev = acc; acc = s16(POP()) <= s16(acc); break;
        case 0x13: vm.prev = acc; acc = POP() > acc; break;
        case 0x14: vm.prev = acc; acc = POP() >= acc; break;
        case 0x15: vm.prev = acc; acc = POP() < acc; break;
        case 0x16: vm.prev = acc; acc = POP() <= acc; break;
        case 0x17: a = OPS(); if (acc) pc = (uint16_t)(pc + a); break;
        case 0x18: a = OPS(); if (!acc) pc = (uint16_t)(pc + a); break;
        case 0x19: a = OPS(); pc = (uint16_t)(pc + a); break;
        case 0x1a: acc = OPS(); break;
        case 0x1b: PUSH(acc); break;
        case 0x1c: a = OPS(); PUSH(a); break;
        case 0x1d: (void)POP(); break;
        case 0x1e: a = TOP(); PUSH(a); break;
        case 0x1f: a = OPV(); memset(&vm_heap[vm.sp], 0, 2u * a); vm.sp = (uint16_t)(vm.sp + 2 * a); break;

        case 0x20: { /* call */
            a = OPS(); b = OPB();
            int words = b / 2 + 1 + vm.rest;
            uint16_t base = (uint16_t)(vm.sp - 2 * words);
            hw16(base, (reg_t)(hp16(base) + vm.rest));
            vm.rest = 0;
            vm.acc = acc;
            acc = execute(script, (uint16_t)(pc + a), self, base);
            vm.sp = base;
            break;
        }
        case 0x21: { /* callk */
            a = OPV(); b = OPB();
            uint16_t base = (uint16_t)(vm.sp - 2 * (b / 2 + 1) - 2 * vm.rest);
            int argc = hp16(base) + vm.rest;
            vm.rest = 0;
            vm.sp = (uint16_t)(base + 2 * (argc + 1));
            vm.acc = acc;
            acc = kernel_call(a, argc, (uint16_t)(base + 2));
            vm.sp = base;
            break;
        }
        case 0x22:   /* callb */
        case 0x23: { /* calle */
            int scr = 0;
            if (op == 0x23) scr = OPV();
            a = OPV(); b = OPB();
            int words = b / 2 + 1 + vm.rest;
            uint16_t base = (uint16_t)(vm.sp - 2 * words);
            hw16(base, (reg_t)(hp16(base) + vm.rest));
            vm.rest = 0;
            uint16_t target = export_off(scr, a);
            vm.acc = acc;
            if (target) acc = execute(scr, target, self, base);
            vm.sp = base;
            break;
        }
        case 0x24: /* ret */
            depth--;
            vm.acc = acc;
            return acc;
        case 0x25: { /* send */
            b = OPB();
            int words = b / 2 + vm.rest;
            uint16_t frame = (uint16_t)(vm.sp - 2 * words);
            hw16((uint16_t)(frame + 2), (reg_t)(hp16((uint16_t)(frame + 2)) + vm.rest));
            vm.rest = 0;
            vm.acc = acc;
            acc = send_frame(acc, acc, frame, words);
            vm.sp = frame;
            break;
        }
        case 0x28: acc = class_addr(OPV()); break;
        case 0x2a: { /* self */
            b = OPB();
            int words = b / 2 + vm.rest;
            uint16_t frame = (uint16_t)(vm.sp - 2 * words);
            hw16((uint16_t)(frame + 2), (reg_t)(hp16((uint16_t)(frame + 2)) + vm.rest));
            vm.rest = 0;
            vm.acc = acc;
            acc = send_frame(self, self, frame, words);
            vm.sp = frame;
            break;
        }
        case 0x2b: { /* super */
            a = OPV(); b = OPB();
            int words = b / 2 + vm.rest;
            uint16_t frame = (uint16_t)(vm.sp - 2 * words);
            hw16((uint16_t)(frame + 2), (reg_t)(hp16((uint16_t)(frame + 2)) + vm.rest));
            vm.rest = 0;
            vm.acc = acc;
            acc = send_frame(self, class_addr(a), frame, words);
            vm.sp = frame;
            break;
        }
        case 0x2c: { /* &rest */
            a = OPV();
            int argc = hp16(params);
            vm.rest = argc - a + 1;
            if (vm.rest < 0) vm.rest = 0;
            for (int i = a; i <= argc; i++) PUSH(hp16((uint16_t)(params + 2 * i)));
            break;
        }
        case 0x2d: { /* lea */
            a = OPV(); b = OPV();
            int type = (a >> 1) & 3;
            int idx = b + ((a & 0x10) ? acc : 0);
            uint16_t vb = type == 0 ? globals : type == 1 ? locals : type == 2 ? fp : params;
            acc = (reg_t)(vb + 2 * idx);
            break;
        }
        case 0x2e: acc = self; break;
        case 0x30: PUSH(vm.prev); break;
        case 0x31: a = OPV(); acc = hp16((uint16_t)(self + a)); break;
        case 0x32: a = OPV(); hw16((uint16_t)(self + a), acc); break;
        case 0x33: a = OPV(); PUSH(hp16((uint16_t)(self + a))); break;
        case 0x34: a = OPV(); hw16((uint16_t)(self + a), POP()); break;
        case 0x35: a = OPV(); acc = (reg_t)(hp16((uint16_t)(self + a)) + 1); hw16((uint16_t)(self + a), acc); break;
        case 0x36: a = OPV(); acc = (reg_t)(hp16((uint16_t)(self + a)) - 1); hw16((uint16_t)(self + a), acc); break;
        case 0x37: a = OPV(); b = (reg_t)(hp16((uint16_t)(self + a)) + 1); hw16((uint16_t)(self + a), b); PUSH(b); break;
        case 0x38: a = OPV(); b = (reg_t)(hp16((uint16_t)(self + a)) - 1); hw16((uint16_t)(self + a), b); PUSH(b); break;
        case 0x39: a = OPS(); acc = lofs(si, pc, (int16_t)a); break;
        case 0x3a: a = OPS(); PUSH(lofs(si, pc, (int16_t)a)); break;
        case 0x3b: PUSH(0); break;
        case 0x3c: PUSH(1); break;
        case 0x3d: PUSH(2); break;
        case 0x3e: PUSH(self); break;

        default:
            if (op >= 0x40) {
                int type = op & 3, to_stack = op & 4, indexed = op & 8, kind = (op >> 4) & 3;
                int idx = OPV();
                uint16_t vb = type == 0 ? globals : type == 1 ? locals : type == 2 ? fp : params;
                if (indexed) idx += acc;
                uint16_t va = (uint16_t)(vb + 2 * idx);
                switch (kind) {
                case 0:
                    if (to_stack) PUSH(hp16(va)); else acc = hp16(va);
                    break;
                case 1:
                    if (to_stack) hw16(va, POP());
                    else { if (indexed) acc = POP(); hw16(va, acc); }
                    break;
                case 2:
                    b = (reg_t)(hp16(va) + 1); hw16(va, b);
                    if (to_stack) PUSH(b); else acc = b;
                    break;
                case 3:
                    b = (reg_t)(hp16(va) - 1); hw16(va, b);
                    if (to_stack) PUSH(b); else acc = b;
                    break;
                }
            } else {
                vm_error("bad opcode %02x at %04x (script %d)\n", ob, pc - 1, script);
                depth--;
                return acc;
            }
        }
    }
    depth--;
    vm.acc = acc;
    return acc;
#undef OPV
#undef OPS
#undef OPB
}

/* ------------------------------------------------------------------ init */

static jmp_buf restart_jmp;

static void unlock_all_scripts(void)
{
    for (int i = 0; i < MAX_SCRIPTS; i++)
        if (scripts[i].loaded && scripts[i].code) res_unlock(RES_SCRIPT, i);
}

int vm_init(void)
{
    memset(&vm, 0, sizeof vm);
    unlock_all_scripts();
    memset(scripts, 0, sizeof scripts);
    memset(classes, 0, sizeof classes);
    heap_reset();
    if (!sel_count) load_selector_names();
    load_class_table();
    vm.stack_base = heap_alloc(STACK_WORDS * 2);
    vm.stack_top = (uint16_t)(vm.stack_base + STACK_WORDS * 2);
    vm.sp = vm.stack_base;
    if (!script_load(0))
        return -1;
    kernel_init();
    return 0;
}

static void heap_dump(void)
{
    for (uint16_t p = HEAP_START; p < heap_end;) {
        uint16_t h = hp16(p), bs = h & ~BLK_USED;
        if (!bs) break;
        const char *what = "";
        char buf[48] = "";
        if (h & BLK_USED) {
            uint16_t a = (uint16_t)(p + 2);
            for (int i = 0; i < MAX_SCRIPTS; i++)
                if (scripts[i].loaded && scripts[i].heap == a) { snprintf(buf, sizeof buf, "script %d", i); break; }
            if (!buf[0] && hp16(a) == OBJ_MAGIC) snprintf(buf, sizeof buf, "clone %s", obj_name((reg_t)(a + OH_MAGIC)));
            if (!buf[0] && a == vm.stack_base) strcpy(buf, "stack");
            what = buf[0] ? buf : "data";
        } else what = "FREE";
        vm_log("  %04x %5u %s\n", p, bs, what);
        p = (uint16_t)(p + bs);
    }
}

void vm_request_restart(void)
{
    vm.restarting = 1;
}

/* ------------------------------------------------------------ save state */
/* Everything the VM needs to resume: the whole heap plus the C-side
 * script/class tables that index into it. */

typedef struct {
    uint16_t heap_end, stack_base, stack_top;
    uint16_t class_count, nscripts, reserved;
} VmSnapHdr;

/* worst case: heap as literal runs + every script slot */
uint32_t vm_state_size(void)
{
    return sizeof(VmSnapHdr) + HEAP_SIZE + HEAP_SIZE / 64 + 4 + sizeof classes
         + MAX_SCRIPTS * (2 + sizeof(ScriptInfo));
}

/* Heap encoding: [u8 n] n<128: n+1 literal bytes follow; n>=128: (n-127)*? zeros.
 * Zero runs: 0x80|len7 with len 1..127 meaning len*4 zero bytes, cheap and
 * good enough since most free space is zeroed. */
static uint32_t heap_pack(uint8_t *out)
{
    uint32_t o = 0, i = 0;
    while (i < HEAP_SIZE) {
        uint32_t z = 0;
        while (i + z + 4 <= HEAP_SIZE && z < 127 * 4 && !(vm_heap[i + z] | vm_heap[i + z + 1] | vm_heap[i + z + 2] | vm_heap[i + z + 3]))
            z += 4;
        if (z) { out[o++] = (uint8_t)(0x80 | (z / 4)); i += z; continue; }
        uint32_t n = 0;
        while (i + n < HEAP_SIZE && n < 128) {
            if (i + n + 8 <= HEAP_SIZE && !memcmp(&vm_heap[i + n], "\0\0\0\0\0\0\0\0", 8)) break;
            n++;
        }
        if (!n) n = 1;
        out[o++] = (uint8_t)(n - 1);
        memcpy(out + o, &vm_heap[i], n);
        o += n; i += n;
    }
    return o;
}

static const uint8_t *heap_unpack(const uint8_t *in)
{
    uint32_t i = 0;
    while (i < HEAP_SIZE) {
        uint8_t c = *in++;
        if (c & 0x80) { uint32_t z = (c & 0x7fu) * 4; memset(&vm_heap[i], 0, z); i += z; }
        else { uint32_t n = c + 1u; memcpy(&vm_heap[i], in, n); in += n; i += n; }
    }
    return in;
}

uint32_t vm_state_save(uint8_t *out)
{
    uint8_t *start = out;
    VmSnapHdr h = { heap_end, vm.stack_base, vm.stack_top, (uint16_t)class_count, 0, 0 };
    for (int i = 0; i < MAX_SCRIPTS; i++) if (scripts[i].loaded) h.nscripts++;
    memcpy(out, &h, sizeof h); out += sizeof h;
    memcpy(out, classes, sizeof classes); out += sizeof classes;
    for (int i = 0; i < MAX_SCRIPTS; i++) {
        if (!scripts[i].loaded) continue;
        uint16_t n = (uint16_t)i;
        ScriptInfo si = scripts[i];
        si.code = NULL;
        memcpy(out, &n, 2); out += 2;
        memcpy(out, &si, sizeof si); out += sizeof si;
    }
    out += heap_pack(out);
    return (uint32_t)(out - start);
}

static uint8_t *pending_restore;

/* Takes ownership of buf (malloc'd snapshot from vm_state_save). The
 * running script is unwound and the state is applied in vm_run_game. */
void vm_request_restore(uint8_t *buf)
{
    free(pending_restore);
    pending_restore = buf;
    vm.restarting = 1;
}

static void vm_state_apply(const uint8_t *in)
{
    VmSnapHdr h;
    unlock_all_scripts();
    memcpy(&h, in, sizeof h); in += sizeof h;
    memcpy(classes, in, sizeof classes); in += sizeof classes;
    memset(scripts, 0, sizeof scripts);
    for (int k = 0; k < h.nscripts; k++) {
        uint16_t n;
        memcpy(&n, in, 2); in += 2;
        if (n < MAX_SCRIPTS) memcpy(&scripts[n], in, sizeof(ScriptInfo));
        in += sizeof(ScriptInfo);
    }
    heap_unpack(in);
    /* code stays outside the heap: re-attach (and re-lock) each script */
    for (int i = 0; i < MAX_SCRIPTS; i++) {
        if (!scripts[i].loaded) continue;
        uint16_t sz;
        scripts[i].code = res_load(RES_SCRIPT, i, &sz);
        if (!scripts[i].code || sz != scripts[i].size) {
            vm_error("restore: script %d missing\n", i);
            scripts[i].loaded = 0;
        }
    }
    heap_end = h.heap_end;
    class_count = h.class_count;
    vm.stack_base = h.stack_base;
    vm.stack_top = h.stack_top;
    vm.sp = vm.stack_base;
    vm.acc = vm.prev = 0;
    vm.rest = 0;
    depth = 0;
}

void vm_run_game(void)
{
    static int restarted;
    int sel_play = sel_id("play"), sel_replay = sel_id("replay");
    int sel = sel_play;
    setjmp(restart_jmp);
    for (;;) {
        if (vm.restarting && pending_restore) {
            vm_state_apply(pending_restore);
            free(pending_restore);
            pending_restore = NULL;
            kernel_after_restore();
            sel = sel_replay;
        } else if (vm.restarting) {
            vm_init();
            restarted = 1;
            sel = sel_play;
        }
        kernel_set_restarting(sel == sel_play ? restarted : 0);
        vm.restarting = 0;
        reg_t game = script_export(0, 0);
        if (!game) return;
        vm_log("game object: %s, %s\n", obj_name(game), sel_name(sel));
        invoke(game, sel, 0, NULL);
        if (!vm.restarting) break;
    }
}

/* ---------------------------------------------------------- debugging */

/* find an object by name (first match; scans the whole heap) */
reg_t vm_find_object(const char *name)
{
    int sn = sel_id("name");
    for (uint32_t a = HEAP_START + OH_MAGIC; a < HEAP_SIZE - 2; a += 2) {
        if (hp16((uint16_t)(a - OH_MAGIC)) != OBJ_MAGIC) continue;
        int i = obj_find_var((reg_t)a, sn);
        if (i < 0) continue;
        reg_t s = obj_var((reg_t)a, i);
        if (s && !strcmp(hstr(s), name)) return (reg_t)a;
    }
    return 0;
}

void vm_dump_object(reg_t obj)
{
    if (!obj_is_object(obj)) return;
    reg_t cls = is_class(obj) ? obj : obj_var(obj, SEL_SUPER);
    const uint8_t *c = obj_code(cls);
    int n = obj_var_count(obj), cn = obj_var_count(cls);
    const uint8_t *list = c + hp16((uint16_t)(cls - OH_FUNCS)) - 2 - 2 * cn;
    vm_log("%s @%04x (%d vars)\n", obj_name(obj), obj, n);
    for (int i = 0; i < n && i < cn; i++)
        vm_log("  [%2d] %-14s = %d\n", i, sel_name(rd16c(list + 2 * i)), (int16_t)obj_var(obj, i));
}
