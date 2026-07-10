#include "syscall.h"
#include "idt.h"
#include "keyboard.h"
#include "vga.h"

void syscall_handler(registers_t *regs)
{
    u32 syscall_num = regs->eax;

    switch (syscall_num) {

    case SYS_READ: {
        /* 非阻塞批量读取
         * ebx = 用户缓冲区地址
         * ecx = 最大读取长度
         * 返回值 eax = 实际读取的字节数
         */
        char *buf = (char *)regs->ebx;
        u32 len = regs->ecx;
        u32 count = 0;

        while (count < len && kbd_buffer_has_data()) {
            buf[count++] = kbd_buffer_get();
        }
        regs->eax = count;
        break;
    }

    case SYS_GETCHAR: {
        /* 阻塞式读取一个字符
         * 等待直到有按键输入
         * 返回值 eax = 字符的 ASCII 码
         */
        while (!kbd_buffer_has_data()) {
            /* hlt 暂停 CPU 直到下一个中断
             * 键盘 IRQ 触发后，中断处理函数会填充缓冲区
             * 由于是陷阱门 (0xEF)，IF 保持为 1，中断可以正常触发 */
            __asm__ volatile("hlt");
        }
        regs->eax = kbd_buffer_get();
        break;
    }

    case SYS_WRITE: {
        /* 输出一个字符到 VGA 屏幕
         * ebx = 要打印的字符 */
        vga_putc((char)regs->ebx);
        regs->eax = 0;
        break;
    }

    case SYS_CLEAR: {
        vga_init();
        regs->eax = 0;
        break;
    }

    default:
        /* 未知系统调用号，返回 -1 */
        regs->eax = -1;
        break;
    }
}
