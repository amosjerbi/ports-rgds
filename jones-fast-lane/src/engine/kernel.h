#ifndef SCI_KERNEL_H
#define SCI_KERNEL_H

#include "vm.h"
#include "platform.h"

void  kernel_init(void);
reg_t kernel_call(int num, int argc, uint16_t argv);
void  kernel_set_restarting(int r);
const char *kernel_name(int num);
void  vm_request_restart(void);
int   script_number_of(uint16_t addr);

/* implemented in kgraphics.c */
reg_t kgfx_call(int num, int argc, uint16_t argv, int *handled);
void  kgfx_init(void);
/* implemented in ksave.c */
reg_t ksave_call(int num, int argc, uint16_t argv, int *handled);
void  kernel_flush_disposals(void);
void  kernel_reset_disposals(void);

#endif
