#ifndef KMALLOC_H
#define KMALLOC_H

#include "types.h"

// 初始化堆分配器
void kmalloc_init(void);

// 分配 size 字节，返回 8 字节对齐地址
void *kmalloc(u32 size);

// 释放
void kfree(void *ptr);

// 调试：打印堆状态
void kmalloc_dump(void);

#endif