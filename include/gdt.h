#ifndef GDT_H
#define GDT_H

#include "types.h"
/* 8 字节段描述符 */
struct __attribute__((packed)) gdt_entry {
    u16 limit_low;     /* 限长低 16 位 */
    u16 base_low;      /* 基址低 16 位 */
    u8  base_middle;   /* 基址中 8 位 */
    u8  access;        /* 访问权限 */
    u8  granularity;   /* 粒度(高4位) + 限长高4位(低4位) */
    u8  base_high;     /* 基址高 8 位 */
};

/* GDTR 结构体 — 给 lgdt 指令用的 6 字节结构 */
struct __attribute__((packed)) gdt_ptr {
    u16 limit;         /* GDT 大小 - 1 */
    u32 base;          /* GDT 基地址 */
};

/*  TSS 结构体  */
struct __attribute__((packed)) tss {
    u32 prev_tss;    /* 0x00: 前一个 TSS 链接 */
    u32 esp0;        /* 0x04: Ring 0 栈指针 */
    u32 ss0;         /* 0x08: Ring 0 栈段 */
    u32 esp1;        /* 0x0C: Ring 1 栈指针（未使用） */
    u32 ss1;         /* 0x10: Ring 1 栈段 */
    u32 esp2;        /* 0x14: Ring 2 栈指针 */
    u32 ss2;         /* 0x18: Ring 2 栈段 */
    u32 cr3;         /* 0x1C: 页目录基址 */
    u32 eip;         /* 0x20 */
    u32 eflags;      /* 0x24 */
    u32 eax;         /* 0x28 */
    u32 ecx;         /* 0x2C */
    u32 edx;         /* 0x30 */
    u32 ebx;         /* 0x34 */
    u32 esp;         /* 0x38 */
    u32 ebp;         /* 0x3C */
    u32 esi;         /* 0x40 */
    u32 edi;         /* 0x44 */
    u32 es;          /* 0x48 */
    u32 cs;          /* 0x4C */
    u32 ss;          /* 0x50 */
    u32 ds;          /* 0x54 */
    u32 fs;          /* 0x58 */
    u32 gs;          /* 0x5C */
    u32 ldt;         /* 0x60 */
    u16 trap;        /*  0x64 */
    u16 iomap_base;  /* 0x66: I/O 位图基址 */
};

void gdt_init(void);
void tss_init(void);

/* GDT 表（定义在 gdt.c） */
#define GDT_ENTRIES 6
extern struct gdt_entry gdt[GDT_ENTRIES];

/* TSS 结构体（定义在 gdt.c，调度器需要修改 esp0） */
extern struct tss tss;

#endif /* GDT_H */