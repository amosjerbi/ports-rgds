/* SCI0/SCI1-early virtual machine using the original flat 16-bit heap model:
 * scripts, objects, clones, lists, strings and the stack all live in one
 * 64 KB heap and are referenced by 16-bit addresses, exactly as in DOS. */
#ifndef SCI_VM_H
#define SCI_VM_H

#include <stdint.h>

typedef uint16_t reg_t;

#define HEAP_SIZE   0x10000
#define HEAP_START  0x0400     /* addresses below this are text resource numbers */
#define STACK_WORDS 0x0800

extern uint8_t vm_heap[HEAP_SIZE];

static inline reg_t hp16(uint16_t a) { return (reg_t)(vm_heap[a] | vm_heap[(uint16_t)(a + 1)] << 8); }
static inline void  hw16(uint16_t a, reg_t v) { vm_heap[a] = (uint8_t)v; vm_heap[(uint16_t)(a + 1)] = (uint8_t)(v >> 8); }
static inline char *hstr(uint16_t a) { return (char *)&vm_heap[a]; }

/* heap allocator */
uint16_t heap_alloc(uint16_t size);      /* zeroed, returns 0 on failure */
void     heap_free(uint16_t addr);
uint16_t heap_free_bytes(void);
uint16_t heap_largest_free(void);

/* object layout: ptr -> var0 (species); 10-byte header at ptr-10..ptr-2 */
#define OBJ_MAGIC     0x1234
#define INFO_CLASS    0x8000
#define INFO_CLONE    0x0001
#define SEL_SPECIES   0
#define SEL_SUPER     1
#define SEL_INFO      2

int      obj_is_object(reg_t obj);
int      obj_var_count(reg_t obj);
reg_t    obj_var(reg_t obj, int i);
void     obj_set_var(reg_t obj, int i, reg_t v);
int      obj_find_var(reg_t obj, int selector);   /* index or -1 */
uint32_t obj_find_method(reg_t obj, int selector); /* script<<16|offset, 0 if none */
reg_t    obj_clone(reg_t parent);                  /* copy of header + vars, 0 on failure */
void     obj_free(reg_t obj);
const char *obj_name(reg_t obj);

/* selector helpers by id */
int      sel_id(const char *name);               /* -1 if unknown */
const char *sel_name(int id);
reg_t    get_sel(reg_t obj, int sel);            /* property read (0 if missing) */
void     set_sel(reg_t obj, int sel, reg_t v);
int      has_sel(reg_t obj, int sel);
/* invoke a selector (property or method) with arguments; returns acc */
reg_t    invoke(reg_t obj, int sel, int argc, const reg_t *argv);

/* scripts */
uint16_t script_load(int num);                   /* returns base, loads on demand */
void     script_unload(int num);
reg_t    script_export(int num, int index);       /* absolute address */
reg_t    class_addr(int species);

/* VM */
typedef struct {
    reg_t acc, prev;
    uint16_t sp, stack_base, stack_top;
    int  rest;
    int  restarting;
    int  quit;
    int  trace;
} VmState;
extern VmState vm;
extern reg_t vm_self;   /* object whose method made the current kernel call */

int   vm_init(void);
void  vm_run_game(void);
/* save state (heap + script/class tables) */
uint32_t vm_state_size(void);            /* upper bound */
uint32_t vm_state_save(uint8_t *out);    /* returns bytes written */
void  vm_request_restore(uint8_t *buf);  /* takes ownership; applied after unwind */
void  kernel_after_restore(void);        /* kernel.c: reset gfx/sound state */       /* load script 0 export 0 and play: */
reg_t vm_global(int i);
void  vm_set_global(int i, reg_t v);

/* text helpers: an address < HEAP_START is a text resource number */
const char *vm_text(reg_t module, reg_t index);   /* returns pointer into resource */
const char *vm_string(reg_t addr, reg_t next, int *used_next);

reg_t vm_find_object(const char *name);
void  vm_dump_object(reg_t obj);
void  vm_log(const char *fmt, ...);
void  vm_error(const char *fmt, ...);
int   vm_late_sci1(void);   /* later SCI1 interpreter (the CD release) */

#endif
