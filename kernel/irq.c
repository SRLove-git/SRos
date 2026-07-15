#include "irq.h"
#include "io.h"
#include "idt.h"
#include "vga.h"
#include "keyboard.h"
#include "mouse.h"
#include "cursor.h"
#include "window.h"
#include "sound.h"
#include "rtl8139.h"

volatile u32 timer_ticks = 0;

void pit_init(u32 frequency_hz){
    u32 divisor = 1193180 / frequency_hz;

    outb(0x43,0x36);

    outb(0x40,(u8)(divisor & 0xFF));
    outb(0x40,(u8)((divisor >> 8) & 0xFF));

}
void timer_handler(registers_t *regs){
    (void)regs;
    timer_ticks++;
    /* 轮询鼠标数据（PS/2 命令字节未设 bit 1，鼠标不会触发 IRQ12） */
    u8 ps2_st = inb(0x64);
    if ((ps2_st & 0x21) == 0x21) {  /* 输出缓冲满 且 数据来自鼠标 */
        mouse_handler(regs);
    }
    /* 窗口管理器更新（轮询鼠标 + 光标 + 点击处理 + 合成） */
    wm_update();
}
void irq_handler(registers_t *regs){
    if(regs -> int_no == 32){
        timer_handler(regs);
    }
    //键盘
    else if(regs -> int_no == 33){
        keyboard_handler(regs);
    }
    //鼠标 (IRQ12 → INT 44)
    else if(regs -> int_no == 44){
        mouse_handler(regs);
    }
    //声卡 SB16 (IRQ5 → INT 37)
    else if(regs -> int_no == 37){
        sb16_handler(regs);
    }
    // RTL8139 网卡（动态 IRQ，通过全局变量 rtl8139_irq_vector 比较）
    else if(rtl8139_irq != 0 && regs -> int_no == (u32)(32 + rtl8139_irq)){
        rtl8139_handle_irq();
    }
    /* 发送EOI */
    if(regs -> int_no >= 40){
        outb(0xA0,0x20);
    }
    outb(0x20,0x20);

}

void pic_remap(){
    /* ICW1 */ 
    outb(0x20,0x11);
    outb(0xA0,0x11);
    /* ICW2 */
    outb(0x21,0x20);
    outb(0xA1,0x28);
    /* ICW3 */
    outb(0x21,0x04);
    outb(0xA1,0x02);
    /* ICW4 */
    outb(0x21,0x01);
    outb(0xA1,0x01);
    /* 屏蔽所有IRQ（保留 IRQ0 定时器、IRQ1 键盘、IRQ5 SB16） */
    outb(0x21,0xDC);   /* 0xDC = 1101 1100，IRQ5 取消屏蔽 */
    outb(0xA1,0xFF);

}