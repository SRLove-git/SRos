#include "serial.h"
#include "gdt.h"
#include "vga.h"
#include "idt.h"
#include "irq.h"
#include "shell.h"
#include "paging.h"
#include "kmalloc.h"
#include "ramdisk.h"
#include "sfs.h"

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

    kmalloc_init();

    // ── 初始化文件系统（供 Shell 使用） ──
    ramdisk_init();
    serial_printf(SERIAL_COM1, "[kernel] FS init step 1 done\n");
    if (sfs_mount() < 0) {
        serial_printf(SERIAL_COM1, "[kernel] No filesystem found, formatting...\n");
        sfs_format();
        serial_printf(SERIAL_COM1, "[kernel] format done, mounting...\n");
        sfs_mount();
    }
    serial_printf(SERIAL_COM1, "[kernel] FS init complete\n");

    gdt_init();
    idt_init();
    pic_remap();

    pfa_push(0x109000);
    pfa_push(0x10A000);
    pfa_push(0x10B000);
    pfa_push(0x10C000);
    paging_init();
    /* paging_init 不返回 */
    __builtin_unreachable();
}

/* 由 paging_init 在启用分页后调用（不返回） */
void __attribute__((noreturn)) kernel_after_paging(void)
{
    serial_printf(SERIAL_COM1, "[kernel] starting shell\n");
    pit_init(100);
    __asm__ volatile("sti");
    jump_to_ring3();
    __builtin_unreachable();
}
