#include "serial.h"
#include "gdt.h"
#include "vga.h"
#include "idt.h"
#include "irq.h"
#include "keyboard.h"
/*
 * kernel.c — 内核入口
 *
 * 此时已经进入 32 位保护模式。
 * boot.s 设置了栈并跳转到这里。
 *
 * 当前能做什么：
 *   - 通过串口输出到 QEMU 终端
 *   - 你可以在 QEMU 中按 Ctrl+Alt+2 进入 QEMU monitor
 *
 * 学习任务（自己尝试实现）：
 *   - 初始化 GDT/IDT
 *   - 注册一个 IRQ 处理函数
 *   - 实现 VGA 文本模式输出（0xB8000）
 *   - 启用 A20 地址线
 *   - 设置分页
 */

void kernel_main(void)
{
    serial_init(SERIAL_COM1, 1);

    vga_init();
    vga_puts("Hello, World!\n");
    gdt_init();
    vga_puts("GDT initialized successfully!\n");
    vga_puts("GDT at: ");
    vga_puthex((u32)&gdt);
    vga_puts("\n");
    idt_init();
    pic_remap();
    vga_puts("IDT initialized successfully!\n");
    pit_init(100);
    __asm__ volatile("sti");
    while (1) {
    if (kbd_buffer_has_data()) {
        char c = kbd_buffer_get();
        vga_putc(c);
    }
}
}
