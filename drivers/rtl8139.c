#include "rtl8139.h"
#include "pci.h"
#include "io.h"
#include "serial.h"
#include "paging.h"
#include "string.h"
#include "idt.h"

/* ── 全局状态 ── */
u8 rtl8139_mac[6];
volatile int rtl8139_has_packet = 0;
volatile u32 rtl8139_packet_count = 0;

/* ── 内部状态 ── */
static u16 io_base = 0;            /* I/O 基地址 */
u8 rtl8139_irq = 0;               /* IRQ 号（非 static，供 irq.c 引用） */
static u8 irq_vector = 0;          /* 对应的中断向量 */

/* TX 缓冲：4 个 2KB 缓冲区 */
static u8 *tx_bufs[4];             /* 虚拟地址 */
static u32 tx_phys[4];             /* 物理地址 */
static int tx_cur = 0;

/* RX 环缓冲 */
#define RX_BUF_TOTAL   (8192 + 16)
static u8 *rx_buf;                 /* 虚拟地址 */
static u32 rx_phys;                /* 物理地址 */
static u32 rx_ptr = 0;             /* 当前读取位置 */

/* ── 内部辅助函数 ── */
static inline u8 rtl_readb(u16 reg)    { return inb(io_base + reg); }
static inline u16 rtl_readw(u16 reg)   { return inw(io_base + reg); }
static inline u32 rtl_readl(u16 reg)   { return inl(io_base + reg); }
static inline void rtl_writeb(u16 reg, u8 v)  { outb(io_base + reg, v); }
static inline void rtl_writew(u16 reg, u16 v) { outw(io_base + reg, v); }
static inline void rtl_writel(u16 reg, u32 v) { outl(io_base + reg, v); }

/* serial_printf 不支持 %02X，手动打印 MAC */
static void print_mac(void)
{
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < 6; i++) {
        if (i > 0) serial_putc(SERIAL_COM1, ':');
        serial_putc(SERIAL_COM1, hex[(rtl8139_mac[i] >> 4) & 0xF]);
        serial_putc(SERIAL_COM1, hex[rtl8139_mac[i] & 0xF]);
    }
}

/* 复位芯片 */
static void rtl_reset(void)
{
    rtl_writeb(RTL_REG_CR, RTL_CR_RST);
    /* 等待复位完成（最多 100ms） */
    for (int i = 0; i < 1000; i++) {
        if (!(rtl_readb(RTL_REG_CR) & RTL_CR_RST))
            return;
        /* 简单忙等 — 每次循环 ~100us，用 inb 消耗时间 */
        for (volatile int j = 0; j < 100; j++) inb(0x80);
    }
    serial_printf(SERIAL_COM1, "[rtl8139] reset timeout!\n");
}

/* 初始化 TX 缓冲区 */
static int init_tx_buffers(void)
{
    /* 为每个 TX 缓冲区分配物理页并映射到虚拟地址 */
    for (int i = 0; i < RTL8139_NUM_TX_BUFS; i++) {
        tx_phys[i] = pfa_alloc();
        if (!tx_phys[i]) {
            serial_printf(SERIAL_COM1, "[rtl8139] failed to alloc TX buf %d\n", i);
            return -1;
        }
        u32 virt = 0xF0040000 + i * 0x1000;
        paging_map_page(virt, tx_phys[i], 0x03);  /* Present + Writable */
        tx_bufs[i] = (u8 *)virt;
        memset(tx_bufs[i], 0, RTL8139_TX_BUF_SIZE);

        /* 设置 TX 地址寄存器 */
        rtl_writel(RTL_REG_TX_ADDR0 + i * 4, tx_phys[i]);

        serial_printf(SERIAL_COM1, "[rtl8139] TX buf %d: phys=%x virt=%x\n",
                      i, tx_phys[i], virt);
    }
    return 0;
}

/* 初始化 RX 缓冲区 */
static int init_rx_buffer(void)
{
    /* RX 缓冲需要 8192+16 字节，我们用 3 个连续物理页 */
    /* 注意：需要保证物理连续 - 我们使用当前 PFA 的实现逐个分配但不保证连续 */
    /* 解决方案：RX 缓冲使用 3 页，但我们需要它们在物理上连续 */
    /* 我们分配一整块：分配 3 页并用第一个作为 RX 缓冲即可 */
    /* 实际上 8KB+16 的 RX 缓冲只需最多 3 页（12KB），我们简化处理：*/
    
    /* 为简单，使用 3 个页的虚拟区域，但只用一个页的物理内存
     * 对于 QEMU 上的 RTL8139，8KB 缓冲足够了，但实际芯片需要 8192+16 对齐到 256 字节 */
    
    rx_phys = pfa_alloc();
    if (!rx_phys) {
        serial_printf(SERIAL_COM1, "[rtl8139] failed to alloc RX buf\n");
        return -1;
    }

    u32 virt = 0xF0050000;
    paging_map_page(virt, rx_phys, 0x03);
    /* 映射第二页（用于 RX 环绕） */
    u32 rx_phys2 = pfa_alloc();
    if (!rx_phys2) return -1;
    paging_map_page(virt + 0x1000, rx_phys2, 0x03);
    /* 映射第三页 */
    u32 rx_phys3 = pfa_alloc();
    if (!rx_phys3) return -1;
    paging_map_page(virt + 0x2000, rx_phys3, 0x03);
    
    rx_buf = (u8 *)virt;
    memset(rx_buf, 0, 0x3000);
    
    /* 设置 RX 缓冲区起始地址（物理地址） */
    rtl_writel(RTL_REG_RBSTART, rx_phys);
    rx_ptr = 0;

    serial_printf(SERIAL_COM1, "[rtl8139] RX buf: phys=%x virt=%x\n",
                  rx_phys, virt);
    return 0;
}

/* ── 初始化网卡 ── */
int rtl8139_init(void)
{
    pci_device_t pci_dev;

    /* 只接受精确的 RTL8139 设备匹配，不使用类代码回退 */
    if (pci_find_device(PCI_VENDOR_REALTEK, PCI_DEVICE_RTL8139, &pci_dev) != 0) {
        serial_printf(SERIAL_COM1, "[rtl8139] not found on PCI bus\n");
        return -1;
    }

    serial_printf(SERIAL_COM1, "[rtl8139] found at %d:%d.%d\n",
                  pci_dev.bus, pci_dev.dev, pci_dev.func);
    serial_printf(SERIAL_COM1, "[rtl8139] vendor=%x device=%x\n",
                  pci_dev.vendor_id, pci_dev.device_id);
    serial_printf(SERIAL_COM1, "[rtl8139] IRQ line=%d\n", pci_dev.irq_line);

    /* 2. 提取 I/O 基地址（BAR0，bit 0=1 表示 I/O 空间） */
    io_base = (u16)(pci_dev.bar[0] & 0xFFFFFFFC);
    rtl8139_irq = pci_dev.irq_line;
    irq_vector = 32 + rtl8139_irq;  /* PIC remap: IRQ0 → INT 32 */

    serial_printf(SERIAL_COM1, "[rtl8139] I/O base=%x irq=%d vector=%d\n",
                  io_base, rtl8139_irq, irq_vector);

    if (io_base == 0) {
        serial_printf(SERIAL_COM1, "[rtl8139] invalid I/O base!\n");
        return -1;
    }

    /* 3. 启用总线主控 + I/O 空间 */
    pci_enable_bus_master(&pci_dev);

    /* 4. 复位网卡 */
    rtl_reset();

    /* 5. 读取 MAC 地址 */
    for (int i = 0; i < 6; i++)
        rtl8139_mac[i] = rtl_readb(RTL_REG_IDR0 + i);

    serial_puts(SERIAL_COM1, "[rtl8139] MAC: ");
    print_mac();
    serial_putc(SERIAL_COM1, '\n');

    /* 6. 唤醒芯片 */
    u8 cfg1 = rtl_readb(RTL_REG_CONFIG1);
    cfg1 &= ~(1 << 3);  /* 清除 PMEn（省电模式使能）*/
    rtl_writeb(RTL_REG_CONFIG1, cfg1);

    /* 7. 初始化 TX 和 RX 缓冲区 */
    if (init_tx_buffers() != 0 || init_rx_buffer() != 0) {
        return -1;
    }

    /* 8. 设置接收模式：接收广播、组播、物理地址匹配 */
    rtl_writel(RTL_REG_RCR, RTL_RCR_AB | RTL_RCR_AM | RTL_RCR_APM | RTL_RCR_WRAP);

    /* 9. 设置发送模式 */
    rtl_writel(RTL_REG_TCR, RTL_TCR_CLRABT);

    /* 10. 禁用硬件中断（使用轮询模式，避免干扰键盘/鼠标中断）*/
    rtl_writew(RTL_REG_IMR, 0);

    /* 11. 使能发送和接收 */
    rtl_writeb(RTL_REG_CR, RTL_CR_TE | RTL_CR_RE);

    serial_printf(SERIAL_COM1, "[rtl8139] init OK\n");
    return 0;
}

/* ── 发送以太网帧 ── */
int rtl8139_send(const u8 *data, u32 len)
{
    if (io_base == 0) return -1;
    if (len < 14 || len > 1514) return -1;  /* 最小以太网帧 14 字节 */

    /* 复制数据到当前 TX 缓冲区 */
    memcpy(tx_bufs[tx_cur], data, len);

    /* 填充到至少 64 字节（以太网最小帧长） */
    if (len < 64) {
        memset(tx_bufs[tx_cur] + len, 0, 64 - len);
        len = 64;
    }

    /* 写 TX 状态寄存器：数据长度 + 早期 TX 标志 + TOK 标志 */
    /* 写入物位编码: 位 0-12 = 数据长度, 位 13 = 早期 TX, 位 16 = 所有权 */
    rtl_writel(RTL_REG_TX_STATUS0 + tx_cur * 4, len | 0x3000);

    /* 等待发送完成（轮询 TOK 位）*/
    /* 注意：实际使用中应等待中断，这里轮询简化 */
    for (volatile int wait = 0; wait < 5000; wait++) {
        u32 ts = rtl_readl(RTL_REG_TX_STATUS0 + tx_cur * 4);
        if (ts & 0x2000) {
            /* 发送完成（TOK 位）*/
            break;
        }
        for (volatile int j = 0; j < 10; j++) inb(0x80);
    }

    /* 切换到下一个 TX 缓冲区 */
    tx_cur = (tx_cur + 1) % RTL8139_NUM_TX_BUFS;
    return 0;
}

/* ── 获取 MAC ── */
void rtl8139_get_mac(u8 *mac)
{
    for (int i = 0; i < 6; i++) mac[i] = rtl8139_mac[i];
}

/* ── 中断处理 ── */
void rtl8139_handle_irq(void)
{
    if (io_base == 0) return;

    /* 读取中断状态 */
    u16 isr = rtl_readw(RTL_REG_ISR);
    if (isr == 0) return;

    /* 产生测试包的回声（ping），写回 ISR 来清除中断位 */
    rtl_writew(RTL_REG_ISR, isr);

    /* 接收中断 */
    if (isr & RTL_ISR_ROK) {
        /* 处理所有已接收的包 */
        while (1) {
            /* 读取当前写入地址（硬件写入位置） */
            u16 cbr = rtl_readw(RTL_REG_CBR);
            /* 如果读取指针追上了写入指针，说明没有更多数据 */
            if (rx_ptr == cbr)
                break;

            /* 读取 RX 头部 */
            rtl8139_rx_header_t *hdr = (rtl8139_rx_header_t *)&rx_buf[rx_ptr];
            u16 rx_status = hdr->status;
            u16 rx_size = hdr->size;

            /* 检查是否有有效数据（TOK 位 = bit 0）*/
            if (rx_status & 0x01) {
                /* 计算实际数据长度（减去 4 字节 CRC） */
                u32 data_len = rx_size - 4;

                if (data_len > 0 && data_len < 2000) {
                    rtl8139_packet_count++;
                    serial_printf(SERIAL_COM1, "[rtl8139] RX packet len=%d (status=%x, total=%d)\n",
                                  data_len, rx_status, rtl8139_packet_count);
                }
            }

            /* 移动到下一包（对齐到 4 字节边界） */
            rx_ptr = (rx_ptr + 4 + rx_size + 3) & ~3;

            /* 如果超出缓冲，绕回 */
            if (rx_ptr >= RTL8139_RX_BUF_SIZE) {
                rx_ptr = 0;
            }

            /* 更新 CAPR（当前读取包地址寄存器）到新位置 */
            rtl_writew(RTL_REG_CAPR, rx_ptr - 16);
        }
        /* 标记有包 */
        rtl8139_has_packet = 1;
    }

    /* 发送完成中断 */
    if (isr & RTL_ISR_TOK) {
        /* TX 完成 — 不需要特别处理 */
    }

    /* 错误处理 */
    if (isr & (RTL_ISR_RERR | RTL_ISR_TERR | RTL_ISR_SERR)) {
        serial_printf(SERIAL_COM1, "[rtl8139] error: ISR=%x\n", isr);
    }
}

/* ── 调试状态 ── */
void rtl8139_dump_status(void)
{
    if (io_base == 0) {
        serial_printf(SERIAL_COM1, "[rtl8139] not initialized\n");
        return;
    }
    serial_printf(SERIAL_COM1, "[rtl8139] status dump:\n");
    serial_printf(SERIAL_COM1, "  I/O base : %x\n", io_base);
    serial_printf(SERIAL_COM1, "  IRQ      : %d (vector %d)\n", rtl8139_irq, irq_vector);
    serial_puts(SERIAL_COM1, "  MAC      : ");
    print_mac();
    serial_putc(SERIAL_COM1, '\n');
    serial_printf(SERIAL_COM1, "  CR       : %x\n", rtl_readb(RTL_REG_CR));
    serial_printf(SERIAL_COM1, "  ISR      : %x\n", rtl_readw(RTL_REG_ISR));
    serial_printf(SERIAL_COM1, "  IMR      : %x\n", rtl_readw(RTL_REG_IMR));
    serial_printf(SERIAL_COM1, "  RX ptr   : %d\n", rx_ptr);
    serial_printf(SERIAL_COM1, "  CBR      : %x\n", rtl_readw(RTL_REG_CBR));
    serial_printf(SERIAL_COM1, "  CAPR     : %x\n", rtl_readw(RTL_REG_CAPR));
    serial_printf(SERIAL_COM1, "  Packets  : %d\n", rtl8139_packet_count);
    serial_printf(SERIAL_COM1, "  MSR      : %x\n", rtl_readb(RTL_REG_MSR));
}
