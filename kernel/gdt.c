#include "gdt.h"
/* Access byte 位宏 */
#define GDT_ACCESS_P     0x80    /* Present */
#define GDT_ACCESS_DPL0  0x00    /* Ring 0 */
#define GDT_ACCESS_DPL3  0x60    /* Ring 3 */
#define GDT_ACCESS_S     0x10    /* 代码/数据段 */
#define GDT_ACCESS_E     0x08    /* 可执行（代码段） */
#define GDT_ACCESS_RW    0x02    /* 可读（代码段）/可写（数据段） */

/* Flags (granularity byte 高 4 位) */
#define GDT_FLAG_G       0x80    /* 4KB 粒度 */
#define GDT_FLAG_DB      0x40    /* 32 位 */

struct gdt_entry gdt[GDT_ENTRIES];
struct gdt_ptr gp;

/* 设置单个 GDT 条目 */
static void gdt_set_entry(int idx, u32 base,u32 limit, u8 access, u8 gran){
    gdt[idx].limit_low = limit & 0xFFFF;
    gdt[idx].base_low = base & 0xFFFF;
    gdt[idx].base_middle = (base >> 16) & 0xFF;
    gdt[idx].access = access;
    gdt[idx].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[idx].base_high = (base >> 24) & 0xFF;
}

static inline void gdt_flush(void){
    __asm__ volatile(
        "lgdt %0\n\t"
        "mov $0x10, %%ax\n\t" /* 0x10 = 第 2 项 (16 = 2*8) */
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "ljmp $0x08, $1f\n\t"  /* 0x08 = 第 1 项 (8 = 1*8) */
        "1:\n\t"
        :
        : "m"(gp)
    );
}

void gdt_init(void){

    /* GDT 指针配置 */
    gp.limit = sizeof(gdt) - 1;
    gp.base = (u32)&gdt;

    /* 第 0 项：空描述符（全为0）*/
    gdt_set_entry(0,0,0,0,0);

    /* 第一项：代码段 */
    //access 1001 1010
    //gran  1100 1111
    gdt_set_entry(1,0,0xFFFFF,0x9A,0xCF);
    
    /* 第二项：数据段*/
    //access 1001 0010
    //gran  1100 1111
    gdt_set_entry(2,0,0xFFFFF,0x92,0xCF);
    /* 加载 GDT */
    gdt_flush();
}
