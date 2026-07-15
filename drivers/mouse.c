#include "mouse.h"
#include "io.h"
#include "serial.h"

/* ── 环形缓冲区 ── */
static mouse_event_t mouse_buffer[MOUSE_BUFFER_SIZE];
static u8 mouse_buf_head = 0;   // 写指针
static u8 mouse_buf_tail = 0;   // 读指针

/* ── 鼠标中断状态机 ── */
static u8 mouse_cycle = 0;       // 当前接收到的字节序号（0,1,2）
static u8 mouse_packet[3];       // 3 字节 PS/2 数据包

/* ── 内部辅助函数 ── */

/* 等待 PS/2 控制器输入缓冲区为空（可写入） */
static void ps2_wait_write(void) {
    for (int i = 0; i < 10000; i++) {
        if (!(inb(0x64) & 0x02)) return;
    }
}

/* 等待 PS/2 控制器输出缓冲区有数据（可读取） */
static void ps2_wait_read(void) {
    for (int i = 0; i < 10000; i++) {
        if (inb(0x64) & 0x01) return;
    }
}

/* 清空 PS/2 输出缓冲区中所有剩余字节 */
static void ps2_flush(void) {
    for (int i = 0; i < 100; i++) {
        if (!(inb(0x64) & 0x01)) break;
        (void)inb(0x60);
    }
}

/* 向鼠标发送一个命令，返回 ACK(0xFA) 或 0 */
static u8 mouse_send_cmd(u8 cmd) {
    ps2_wait_write();
    outb(0x64, 0xD4);           // 告诉控制器下一个字节是发给鼠标的
    ps2_wait_write();
    outb(0x60, cmd);            // 发送命令
    ps2_wait_read();
    u8 ack = inb(0x60);         // 读取 ACK
    serial_printf(SERIAL_COM1, "[mouse] cmd 0x%02x -> ack 0x%02x\n", cmd, ack);
    return ack == 0xFA ? 1 : 0;
}

/* 向缓冲区写入一个鼠标事件（在中断中调用） */
static void mouse_buffer_put(mouse_event_t *ev) {
    u8 next = (mouse_buf_head + 1) % MOUSE_BUFFER_SIZE;
    if (next != mouse_buf_tail) {
        mouse_buffer[mouse_buf_head] = *ev;
        mouse_buf_head = next;
    }
}

/* ── 对外接口 ── */

int mouse_read(mouse_event_t *event) {
    if (mouse_buf_head == mouse_buf_tail) return 0;
    *event = mouse_buffer[mouse_buf_tail];
    mouse_buf_tail = (mouse_buf_tail + 1) % MOUSE_BUFFER_SIZE;
    return 1;
}

int mouse_has_data(void) {
    return mouse_buf_head != mouse_buf_tail;
}

/* ── 鼠标初始化 ── */
void mouse_init(void) {
    serial_printf(SERIAL_COM1, "[mouse] initializing...\n");

    /* 先清空可能的残留数据 */
    ps2_flush();

    /* 1. 启用辅助设备（鼠标） */
    ps2_wait_write();
    outb(0x64, 0xA8);

    /* 2. 取消屏蔽 IRQ12（PIC 从片 bit 4）*/
    /*   注意：不修改 PS/2 控制器命令字节，保持 BIOS 默认，
     *   因此鼠标不会触发 IRQ12。鼠标数据通过键盘中断和
     *   定时器中断轮询读取。 */
    outb(0xA1, inb(0xA1) & ~(1 << 4));

    /* 3. 设置默认参数 */
    mouse_send_cmd(0xF6);

    /* 4. 启用数据上报（流模式） */
    mouse_send_cmd(0xF4);

    /* 5. 最后清空 */
    ps2_flush();

    serial_printf(SERIAL_COM1, "[mouse] initialization done\n");
}

/* ── 鼠标中断处理 ── */
void mouse_handler(registers_t *regs) {
    (void)regs;

    u8 data = inb(0x60);

    switch (mouse_cycle) {
        case 0:
            /* 第 0 字节：同步 + 按键状态 */
            /* bit 3 必须为 1，否则视为同步错误，丢弃此字节 */
            if (!(data & 0x08)) {
                /* 可能丢失了同步，尝试重新对齐 */
                return;
            }
            mouse_packet[0] = data;
            mouse_cycle = 1;
            break;

        case 1:
            /* 第 1 字节：X 位移 */
            mouse_packet[1] = data;
            mouse_cycle = 2;
            break;

        case 2:
            /* 第 2 字节：Y 位移 — 完成一个数据包 */
            mouse_packet[2] = data;
            mouse_cycle = 0;

            /* 组装事件 */
            mouse_event_t ev;
            ev.buttons = mouse_packet[0] & 0x07;  // bit 0=左, bit 1=右, bit 2=中

            /* X / Y 位移：直接解释为 8 位有符号整数 */
            ev.dx = (i8)mouse_packet[1];
            ev.dy = (i8)mouse_packet[2];

            /* 检查溢出标志 */
            if (mouse_packet[0] & 0xC0) {
                /* 有溢出，忽略该包 */
                return;
            }

            mouse_buffer_put(&ev);
            break;
    }
}
