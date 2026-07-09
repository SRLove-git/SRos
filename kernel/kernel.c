#include "serial.h"
#include "gdt.h"
#include "vga.h"
#include "idt.h"
#include "irq.h"
#include "keyboard.h"
#include "paging.h"
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


__attribute__((noreturn)) void user_function(void)
{
    while(1){
        char c;

        /* SYS_GETCHAR: 从键盘读取一个字符 */
        __asm__ volatile(
            "mov $2, %%eax\n\t"
            "int $0x80\n\t"
            : "=a"(c)
        );

        /* SYS_WRITE: 将字符打印到屏幕 */
        __asm__ volatile(
            "mov $1, %%eax\n\t"
            "mov %0, %%ebx\n\t"
            "int $0x80\n\t"
            :
            : "r"((u32)c)
            : "eax", "ebx"
        );
    }
}

static void jump_to_ring3(void)
{
    u32 user_stack = pfa_alloc() + 0x1000;
    paging_map_user((u32)user_function);
    paging_map_user(user_stack - 0x1000);

    __asm__ volatile (
        "push $0x23\n\t"         /* SS — 用户数据段 (RPL=3) */
        "push %0\n\t"            /* ESP — 用户栈顶 */
        "push $0x200\n\t"        /* EFLAGS — IF=1 */
        "push $0x1B\n\t"         /* CS — 用户代码段 (RPL=3) */
        "push %1\n\t"            /* EIP — 用户函数地址 */
        "iret\n\t"
        :
        : "r"(user_stack), "r"((u32)user_function)
    );
}

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
    pfa_push(0x109000);
    pfa_push(0x10A000);
    pfa_push(0x10B000);
    pfa_push(0x10C000);
    vga_puts("[paging] calling paging_init...\n");
    paging_init();
    vga_puts("Paging initialized successfully!\n");
    pit_init(100);
    __asm__ volatile("sti");
    jump_to_ring3();
    while (1) {
    if (kbd_buffer_has_data()) {
        char c = kbd_buffer_get();
        vga_putc(c);
    }
}
}
