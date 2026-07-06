#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"
#include "idt.h"     // registers_t


void kbd_buffer_put(char c);
// 从缓冲区读取一个字符（在主循环中调用）
char kbd_buffer_get(void);

// 检查缓冲区是否有数据
int kbd_buffer_has_data(void);
void keyboard_handler(registers_t *regs);

#endif