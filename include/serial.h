#ifndef SERIAL_H
#define SERIAL_H

#include "types.h"

/* 标准串口 COM1 端口地址 */
#define SERIAL_COM1  0x3F8

/* 初始化串口，baud 为波特率除数（默认 1 = 115200） */
void serial_init(u16 port, u16 baud_divisor);

/* 写入一个字符 */
void serial_putc(u16 port, char c);

/* 写入一个以 null 结尾的字符串 */
void serial_puts(u16 port, const char *s);

/* 写入一个十六进制数（调试用） */
void serial_puthex(u16 port, u32 val);

/* 格式化输出（printf 风格） */
void serial_printf(u16 port, const char *fmt, ...);

#endif /* SERIAL_H */
