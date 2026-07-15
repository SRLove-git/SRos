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
#include "mouse.h"
#include "cursor.h"
#include "sound.h"
#include "multiboot.h"
#include "framebuffer.h"
#include "window.h"
#include "pci.h"
#include "rtl8139.h"

/* 保存 multiboot 信息，供分页启用后使用 */
static multiboot_info_t saved_mboot_info;

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

    serial_printf(SERIAL_COM1, "[kernel] About to iret to ring3, shell_main=%x, stack=%x\n",
                  (u32)&shell_main, user_stack);

    __asm__ volatile (
        "mov $0x23, %%ax\n\t"    /* 用户数据段选择子 (RPL=3) */
        "mov %%ax, %%ds\n\t"     /* DS → 用户数据段 */
        "mov %%ax, %%es\n\t"     /* ES → 用户数据段 */
        "mov %%ax, %%fs\n\t"     /* FS → 用户数据段 */
        "mov %%ax, %%gs\n\t"     /* GS → 用户数据段 */
        "push $0x23\n\t"         /* SS — 用户数据段 (RPL=3) */
        "push %0\n\t"            /* ESP — 用户栈顶 */
        "push $0x200\n\t"        /* EFLAGS — IF=1 */
        "push $0x1B\n\t"         /* CS — 用户代码段 (RPL=3) */
        "push %1\n\t"            /* EIP — 用户函数地址 */
        "iret\n\t"
        :
        : "r"(user_stack), "r"((u32)&shell_main)
        : "ax"
    );
}

void kernel_main(multiboot_info_t *mbd)
{
    serial_init(SERIAL_COM1, 1);

    /* 保存 multiboot 信息（在分页启用前复制一份） */
    if (mbd != NULL) {
        saved_mboot_info = *mbd;
    }

    /* 尝试 VBE 图形初始化 */
    int vbe_ok = 0;

    /* 方法1: 使用 Bochs VBE I/O 端口（QEMU -vga std） */
    if (vbe_init(1024, 768, 32) == 0) {
        vbe_ok = 1;
    }
    /* 方法2: 使用 multiboot 帧缓冲信息（真实 GRUB 引导） */
    else if (mbd != NULL && (mbd->flags & (1 << 12))) {
        serial_printf(SERIAL_COM1, "[kernel] VBE framebuffer from multiboot: addr=0x%x, %dx%d, %dbpp\n",
                      (u32)mbd->framebuffer_addr,
                      mbd->framebuffer_width, mbd->framebuffer_height,
                      mbd->framebuffer_bpp);
        framebuffer_init((u32)mbd->framebuffer_addr,
                         mbd->framebuffer_width,
                         mbd->framebuffer_height,
                         mbd->framebuffer_pitch,
                         mbd->framebuffer_bpp);
        vbe_ok = 1;
    }

    if (!vbe_ok) {
        serial_printf(SERIAL_COM1, "[kernel] VBE not available, using VGA text mode\n");
    }

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

    /* 尝试初始化 SB16 声卡（可能失败，若无 QEMU -soundhw sb16） */
    sb16_init();

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

    /* 初始化 PCI 总线，查找并初始化 RTL8139 网卡 */
    serial_printf(SERIAL_COM1, "[kernel] initializing PCI...\n");
    if (rtl8139_init() == 0) {
        serial_printf(SERIAL_COM1, "[kernel] RTL8139 NIC initialized\n");
        rtl8139_dump_status();
    } else {
        serial_printf(SERIAL_COM1, "[kernel] no RTL8139 NIC found\n");
    }

    /* 如果有帧缓冲，映射并初始化图形模式控制台 */
    if (fb.phys_addr != 0) {
        framebuffer_map();
        fb_console_init();

        fb_printf("SROS Kernel - VBE Graphics Mode\n");
        fb_printf("==============================\n");
        fb_printf("Resolution: %dx%d %dbpp\n", fb.width, fb.height, fb.bpp);
        fb_printf("Framebuffer: 0x%x (phys)\n", fb.phys_addr);
        fb_printf("\n");
        serial_printf(SERIAL_COM1, "[kernel] VBE framebuffer initialized\n");
        cursor_init();   
        wm_init();

        /* 创建演示窗口 */
        window_t *win1 = wm_create_window(80, 60, 300, 200, "System Info");
        if (win1) {
            wm_printf(win1, 10, 10, COLOR_WHITE, WM_CONTENT_BG,
                      "SROS v0.1 - Window Manager Demo");
            wm_printf(win1, 10, 30, COLOR_WHITE, WM_CONTENT_BG,
                      "Resolution: %dx%d", fb.width, fb.height);
            wm_printf(win1, 10, 50, COLOR_WHITE, WM_CONTENT_BG,
                      "Windows: %d active", g_wm.count);
            wm_printf(win1, 10, 80, COLOR_RED, WM_CONTENT_BG,
                      "Click X to close windows");
            wm_printf(win1, 10, 100, COLOR_ORANGE, WM_CONTENT_BG,
                      "Click title bar to focus");
        }

        window_t *win2 = wm_create_window(450, 160, 250, 150, "Hello Window");
        if (win2) {
            wm_printf(win2, 10, 10, COLOR_BLUE, WM_CONTENT_BG,
                      "Hello, World!");
            wm_printf(win2, 10, 35, COLOR_BLACK, WM_CONTENT_BG,
                      "This is a multi-window");
            wm_printf(win2, 10, 55, COLOR_BLACK, WM_CONTENT_BG,
                      "demo with Z-order.");
            wm_printf(win2, 10, 85, COLOR_GREEN, WM_CONTENT_BG,
                      "Windows can overlap!");
        }

        window_t *win3 = wm_create_window(200, 280, 320, 120, "Mouse Help");
        if (win3) {
            wm_printf(win3, 10, 10, COLOR_WHITE, COLOR_GRAY,
                      "  Mouse Controls:");
            wm_printf(win3, 10, 35, COLOR_BLACK, WM_CONTENT_BG,
                      "  Left click title bar -> focus");
            wm_printf(win3, 10, 55, COLOR_BLACK, WM_CONTENT_BG,
                      "  Click X        -> close");
            wm_printf(win3, 10, 75, COLOR_BLACK, WM_CONTENT_BG,
                      "  Timer polls mouse @ 100Hz");
            wm_printf(win3, 10, 95, COLOR_ORANGE, WM_CONTENT_BG,
                      "  Try clicking the X buttons!");
        }

        wm_composite();
    }

    if (fb.addr == NULL) {
        serial_printf(SERIAL_COM1, "[kernel] No framebuffer, using VGA text mode\n");
    }

    /* 初始化进程调度器 */
    scheduler_init();

    /* 初始化 IPC 子系统（信号量 + 消息队列） */
    ipc_init();

    serial_printf(SERIAL_COM1, "[kernel] starting shell\n");
    pit_init(100);
    mouse_init();
    serial_printf(SERIAL_COM1, "[kernel] after mouse init, enabling interrupts\n");
    __asm__ volatile("sti");
    serial_printf(SERIAL_COM1, "[kernel] interrupts enabled, testing shell in ring0\n");
    shell_main();
    __builtin_unreachable();
}
