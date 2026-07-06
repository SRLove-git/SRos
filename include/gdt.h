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

void gdt_init(void);

/* GDT 表（定义在 gdt.c） */
#define GDT_ENTRIES 3
extern struct gdt_entry gdt[GDT_ENTRIES];

#endif /* GDT_H */