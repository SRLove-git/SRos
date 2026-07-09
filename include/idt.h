#ifndef IDT_H
#define IDT_H
#include "types.h"

struct idt_entry {
    u16 offset_low;    // 低 16 位偏移
    u16 selector;      // 段选择子
    u8  zero;          // 保留，恒为 0
    u8  type_attr;     // 属性字节
    u16 offset_high;   // 高 16 位偏移
} __attribute__((packed));  // 共 8 字节

struct idt_ptr {
    u16 limit;   // 大小 - 1
    u32 base;    // 基址
} __attribute__((packed));  // 共 6 字节

typedef struct {
    u32 gs, fs, es, ds;                    // 段寄存器（先 push ds/es/fs/gs，栈上 gs 在栈顶）
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;  // pusha 顺序
    u32 int_no;      // 中断号
    u32 err_code;    // 错误码
    u32 eip, cs, eflags;  // CPU 自动压入
    u32 user_esp, user_ss; // 仅特权级切换时存在
} __attribute__((packed)) registers_t;


void idt_init(void);
void memset(void *str, int c, u32 n);
void idt_set_gate(int vector, void (*handler)(void), u16 selector, u8 type_attr);
void isr_handler(registers_t *regs);
// 汇编中断入口声明
extern void isr0(void);
extern void isr1(void);
extern void isr3(void);
extern void isr4(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr32(void);
extern void isr33(void);
extern void isr34(void);
extern void isr35(void);
extern void isr36(void);
extern void isr37(void);
extern void isr38(void);
extern void isr39(void);
extern void isr40(void);
extern void isr41(void);
extern void isr42(void);
extern void isr43(void);
extern void isr44(void);
extern void isr45(void);
extern void isr46(void);
extern void isr47(void);
extern void isr128(void);



#endif /* IDT_H */
