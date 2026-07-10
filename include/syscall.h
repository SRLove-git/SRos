#ifndef SYSCALL_H
#define SYSCALL_H
#include "types.h"
#include "idt.h"
#define SYS_READ   0   // 读键盘输入
#define SYS_WRITE  1   // 写输出
#define SYS_GETCHAR 2  // 获取单个字符
#define SYS_CLEAR  3   // 清屏



// 系统调用处理函数类型


void syscall_handler(registers_t *regs);

#endif /* SYSCALL_H */