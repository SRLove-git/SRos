#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"
#include "idt.h"     // registers_t


/* 方向键特殊编码 */
#define KEY_UP    0xE0
#define KEY_DOWN  0xE1
#define KEY_LEFT  0xE2
#define KEY_RIGHT 0xE3

/* VGA 光标控制字符（不写入显存，仅移动硬件光标） */
#define CURSOR_LEFT  0x01
#define CURSOR_RIGHT 0x02


void kbd_buffer_put(int c);
// 从缓冲区读取一个字符（在主循环中调用）
int kbd_buffer_get(void);

// 检查缓冲区是否有数据
int kbd_buffer_has_data(void);
void keyboard_handler(registers_t *regs);

#endif