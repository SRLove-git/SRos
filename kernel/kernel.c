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
#include "scheduler.h"
#include "ipc.h"

static void jump_to_ring3(void)
{
    /* 标记内核代码/数据范围为用户可访问（Ring3 可执行 shell 代码） */
    extern u32 _bss_end;
    u32 kernel_start = 0x100000;
    u32 kernel_end = ((u32)&_bss_end + 0xFFF) & 0xFFFFF000;
    for (u32 addr = kernel_start; addr < kernel_end; addr += 0x1000)
        paging_map_user(addr);

    u32 user_stack = pfa_alloc() + 0x1000;
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

    /* PFA 初始物理页：必须位于 BSS 之后（_bss_end ≈ 0x110DA0）
     * 选择 0x111000 开始的 4 个页面，不与任何内核段重叠 */
    pfa_push(0x111000);
    pfa_push(0x112000);
    pfa_push(0x113000);
    pfa_push(0x114000);
    paging_init();
    /* paging_init 不返回 */
    __builtin_unreachable();
}

/* 由 paging_init 在启用分页后调用（不返回） */
void __attribute__((noreturn)) kernel_after_paging(void)
{
    /* 填充更多可用物理页到 PFA */
    /* 物理地址 0x300000 ~ 0x400000 区间是空闲的（恒等映射、高于内核和堆） */
    /* 这提供 256 页 = 1MB 的物理内存 */
    for (u32 addr = 0x300000; addr < 0x400000; addr += 0x1000) {
        pfa_push(addr);
    }
    serial_printf(SERIAL_COM1, "[kernel] PFA populated: ~1MB free pages\n");

    /* 初始化进程调度器 */
    scheduler_init();

    /* 初始化 IPC 子系统（信号量 + 消息队列） */
    ipc_init();

    serial_printf(SERIAL_COM1, "[kernel] starting shell\n");
    pit_init(100);
    __asm__ volatile("sti");
    jump_to_ring3();
    __builtin_unreachable();
}
