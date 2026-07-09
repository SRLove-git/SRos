#include "serial.h"
#include "io.h"
#include "stdarg.h"

/* 串口寄存器偏移（相对于基址） */
#define REG_DATA      0
#define REG_INTR_EN   1
#define REG_FIFO      2
#define REG_LINE_CTL  3
#define REG_MODEM_CTL 4
#define REG_LINE_STAT 5

/* Line status 寄存器位 */
#define LSR_TX_READY  (1 << 5)

/*
 * 串口驱动 — 通过 QEMU 的串口输出调试信息。
 *
 * QEMU 启动时加参数：
 *   -serial stdio
 * 即可将串口输出重定向到终端。
 */

void serial_init(u16 port, u16 baud_divisor)
{
    /* 禁用中断 */
    outb(port + REG_INTR_EN, 0x00);

    /* 设置 DLAB=1 以配置波特率 */
    outb(port + REG_LINE_CTL, 0x80);
    outb(port + REG_DATA,     (u8)(baud_divisor & 0xFF));
    outb(port + REG_INTR_EN,  (u8)((baud_divisor >> 8) & 0xFF));

    /* 配置数据位=8, 停止位=1, 无校验 */
    outb(port + REG_LINE_CTL, 0x03);

    /* 启用 FIFO, 清空队列 */
    outb(port + REG_FIFO, 0xC7);

    /* RTS/DSR 就绪 */
    outb(port + REG_MODEM_CTL, 0x0B);
}

void serial_putc(u16 port, char c)
{
    /* 等待发送寄存器空 */
    while (!(inb(port + REG_LINE_STAT) & LSR_TX_READY))
        ;

    outb(port, (u8)c);
}

void serial_puts(u16 port, const char *s)
{
    while (*s) {
        if (*s == '\n')
            serial_putc(port, '\r');
        serial_putc(port, *s++);
    }
}

void serial_puthex(u16 port, u32 val)
{
    static const char hex[] = "0123456789ABCDEF";
    int i;

    serial_puts(port, "0x");
    for (i = 28; i >= 0; i -= 4)
        serial_putc(port, hex[(val >> i) & 0xF]);
}

void serial_printf(u16 port, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 's':
                    serial_puts(port, va_arg(args, const char *));
                    break;
                case 'd':
                case 'x':
                    serial_puthex(port, va_arg(args, u32));
                    break;
                case 'c':
                    serial_putc(port, (char)va_arg(args, int));
                    break;
                case '%':
                    serial_putc(port, '%');
                    break;
                default:
                    serial_putc(port, '%');
                    serial_putc(port, *fmt);
                    break;
            }
        } else {
            serial_putc(port, *fmt);
        }
        fmt++;
    }
    va_end(args);
}
