#ifndef PAGING_H
#define PAGING_H

#include "types.h"
#include "idt.h"
#include "vga.h"
#include "string.h"


void pfa_push(u32 phys_addr);
u32 pfa_alloc(void);
void pfa_free(u32 phys_addr);
void paging_init();
void page_fault_handler(registers_t *regs);
void paging_map_user(u32 virt_addr);
void paging_map_page(u32 virt_addr, u32 phys_addr, u8 flags);
void paging_alloc_page(u32 virt_addr);

/* 克隆当前页目录：深度复制所有用户页面 */
u32 paging_clone_directory(void);
#endif /* PAGING_H */