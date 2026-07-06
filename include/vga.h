#ifndef VGA_H
#define VGA_H
#include "types.h"
// 属性 = 背景色 << 4 | 前景色

extern int cursor_row; // 光标行
extern int cursor_col; // 光标列

void vga_init(void);
void vga_putc(char c);
void vga_puts(const char *s);
void vga_putchar_at(char c, u8 color, int row, int col);
void vga_scroll(); // 滚动屏幕
void vga_puthex(u32 n);
void vga_set_cursor(int row, int col);// 设置硬件光标位置
void vga_printf(const char *fmt, ...);
void panic(const char *msg);
#endif /* VGA_H */