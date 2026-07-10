#include "serial.h"
#include "gdt.h"
#include "vga.h"
#include "idt.h"
#include "irq.h"
#include "shell.h"
#include "paging.h"
static void jump_to_ring3(void)
{
    u32 user_stack = pfa_alloc() + 0x1000;
    paging_map_user((u32)&shell_main);
    paging_map_user(user_stack - 0x1000);

    __asm__ volatile (
        "push $0x23\n\t"         /* SS — 用户数据段 (RPL=3) */
        "push %0\n\t"            /* ESP — 用户栈顶 */
        "push $0x200\n\t"        /* EFLAGS — IF=1 */
        "push $0x1B\n\t"         /* CS — 用户代码段 (RPL=3) */
        "push %1\n\t"            /* EIP — 用户函数地址 */
        "iret\n\t"
        :
        : "r"(user_stack), "r"((u32)&shell_main)
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
    /* unreachable — shell_main runs in Ring3 */
}
