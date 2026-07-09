#include "idt.h"
#include "vga.h"
#include "types.h"
#include "io.h"
#include "irq.h"
#include "paging.h"
#include "serial.h"

struct idt_entry idt[256];
struct idt_ptr idtr;

void idt_init(void){
    /* 设置IDTR */
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (u32)&idt;
    /* 清空 IDT(所有表项Present = 0)*/
    memset(&idt,0,sizeof(idt));

    idt_set_gate(0,  isr0,  0x08, 0x8E);   // #DE  除零
    idt_set_gate(1,  isr1,  0x08, 0x8E);   // #DB  调试
    idt_set_gate(3,  isr3,  0x08, 0x8E);   // #BP  断点
    idt_set_gate(4,  isr4,  0x08, 0x8E);   // #OF  溢出
    idt_set_gate(6,  isr6,  0x08, 0x8E);   // #UD  无效指令
    idt_set_gate(7,  isr7,  0x08, 0x8E);   // #NM  FPU不可用
    idt_set_gate(8,  isr8,  0x08, 0x8E);   // #DF  双重错误
    idt_set_gate(10, isr10, 0x08, 0x8E);   // #TS  无效TSS
    idt_set_gate(11, isr11, 0x08, 0x8E);   // #NP  段不存在
    idt_set_gate(12, isr12, 0x08, 0x8E);   // #SS  栈段错误
    idt_set_gate(13, isr13, 0x08, 0x8E);   // #GP  通用保护
    idt_set_gate(14, isr14, 0x08, 0x8E);   // #PF  页错误
    idt_set_gate(16, isr16, 0x08, 0x8E);   // #MF  FPU错误
    idt_set_gate(17, isr17, 0x08, 0x8E);   // #AC  对齐检查
    idt_set_gate(18, isr18, 0x08, 0x8E);   // #MC  MachineCheck
    idt_set_gate(19, isr19, 0x08, 0x8E);   // #XM  SIMD异常
    idt_set_gate(32, isr32, 0x08, 0x8E);   //IRQ0:PIT
    idt_set_gate(33, isr33, 0x08, 0x8E);   //IRQ1:Keyboard
    idt_set_gate(34, isr34, 0x08, 0x8E);
    idt_set_gate(35, isr35, 0x08, 0x8E);
    idt_set_gate(36, isr36, 0x08, 0x8E);
    idt_set_gate(37, isr37, 0x08, 0x8E);
    idt_set_gate(38, isr38, 0x08, 0x8E);
    idt_set_gate(39, isr39, 0x08, 0x8E);
    idt_set_gate(40, isr40, 0x08, 0x8E);
    idt_set_gate(41, isr41, 0x08, 0x8E);
    idt_set_gate(42, isr42, 0x08, 0x8E);
    idt_set_gate(43, isr43, 0x08, 0x8E);
    idt_set_gate(44, isr44, 0x08, 0x8E);
    idt_set_gate(45, isr45, 0x08, 0x8E);
    idt_set_gate(46, isr46, 0x08, 0x8E);
    idt_set_gate(47, isr47, 0x08, 0x8E);
    idt_set_gate(128, isr128, 0x08, 0xEF);





    asm volatile("lidt %0" :: "m"(idtr));

}

void idt_set_gate(int vector, void (*handler)(void), u16 selector, u8 type_attr) {
    idt[vector].offset_low = (u32)handler & 0xFFFF;
    idt[vector].selector = selector;
    idt[vector].zero = 0;
    idt[vector].type_attr = type_attr;
    idt[vector].offset_high = ((u32)handler >> 16) & 0xFFFF;
}

void isr_handler(registers_t *regs) {
    vga_printf("Exception: #%d (0x%x), error code: 0x%x\n",
               regs->int_no, regs->int_no, regs->err_code);
    vga_printf("  EIP=0x%x, CS=0x%x, EFLAGS=0x%x\n",
               regs->eip, regs->cs, regs->eflags);
    serial_printf(SERIAL_COM1, "Exception: #%d (0x%x), error code: 0x%x\n",
               regs->int_no, regs->int_no, regs->err_code);
    serial_printf(SERIAL_COM1, "  EIP=0x%x, CS=0x%x, EFLAGS=0x%x\n",
               regs->eip, regs->cs, regs->eflags);

    if (regs->int_no != 14) {
        panic("Unhandled exception!");
    }
    page_fault_handler(regs);
}



void memset(void *str, int c, u32 n){
    unsigned char *s = (unsigned char *)str;
    while(n--){
        s[n] = c;
    }
}
