#ifndef SYSCALL_H
#define SYSCALL_H
#include "types.h"
#include "idt.h"

#define SYS_READ     0   // 读键盘输入（非阻塞）
#define SYS_WRITE    1   // 写一个字符到 VGA
#define SYS_GETCHAR  2   // 获取单个字符（阻塞）
#define SYS_CLEAR    3   // 清屏

#define SYS_FS_OPEN   4   // 打开文件
#define SYS_FS_READ   5   // 读文件
#define SYS_FS_WRITE  6   // 写文件
#define SYS_FS_CLOSE  7   // 关闭文件
#define SYS_FS_CREATE 8   // 创建文件
#define SYS_FS_LS     9   // 列出根目录

#define SYS_KMALLOC      10  // 分配堆内存
#define SYS_KFREE        11  // 释放堆内存
#define SYS_KMALLOC_DUMP 12  // 打印堆状态

#define SYS_EXEC         13  // 加载并执行 ELF 程序
#define SYS_EXIT         14  // 退出用户程序
#define SYS_FORK         15  // 创建子进程
#define SYS_WAIT         16  // 等待子进程退出

void syscall_handler(registers_t *regs);

#endif /* SYSCALL_H */
