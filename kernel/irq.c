#include "irq.h"
#include "io.h"
#include "idt.h"
#include "vga.h"
#include "keyboard.h"

volatile u32 timer_ticks = 0;

void pit_init(u32 frequency_hz){
    u32 divisor = 1193180 / frequency_hz;

    outb(0x43,0x36);

    outb(0x40,(u8)(divisor & 0xFF));
    outb(0x40,(u8)((divisor >> 8) & 0xFF));

}
void timer_handler(registers_t *regs){
    timer_ticks++;
}
void irq_handler(registers_t *regs){
    if(regs -> int_no == 32){
        timer_handler(regs);
    }
    //键盘
    else if(regs -> int_no == 33){
        keyboard_handler(regs);
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
    /* 屏蔽所有IRQ */
    outb(0x21,0xFC);
    outb(0xA1,0xFF);

}