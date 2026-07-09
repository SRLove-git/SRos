#ifndef PAGING_H
#define PAGING_H

#include "types.h"
#include "idt.h"
#include "vga.h"

void pfa_push(u32 phys_addr);
u32 pfa_alloc(void);
void pfa_free(u32 phys_addr);
void paging_init();
void page_fault_handler(registers_t *regs);
void paging_map_user(u32 virt_addr);
#endif /* PAGING_H */