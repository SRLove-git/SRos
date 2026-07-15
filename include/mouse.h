#ifndef MOUSE_H
#define MOUSE_H

#include "types.h"
#include "idt.h"

/* 鼠标按键 */
#define MOUSE_LEFT_BTN   0x01
#define MOUSE_RIGHT_BTN  0x02
#define MOUSE_MIDDLE_BTN 0x04

/* 鼠标事件结构 */
typedef struct {
    u8  buttons;     // 按键状态（位掩码）
    i8  dx;          // X 轴位移（右为正）
    i8  dy;          // Y 轴位移（下为正）
} mouse_event_t;

/* 缓冲区大小 */
#define MOUSE_BUFFER_SIZE 64

void mouse_init(void);
void mouse_handler(registers_t *regs);

/* 读取鼠标事件（非阻塞，返回 0 表示无事件） */
int mouse_read(mouse_event_t *event);

/* 检查是否有待读取的鼠标事件 */
int mouse_has_data(void);

#endif /* MOUSE_H */
