#include "sound.h"
#include "io.h"
#include "serial.h"
#include "irq.h"

/* ── 内部状态 ── */
static volatile int sb16_playing = 0;
static volatile int sb16_irq_fired = 0;

/* ════════════════════════════════════════
   PC Speaker 驱动
   ════════════════════════════════════════ */

/* PIT 计数器 2 的频率基准 */
#define PIT_BASE_FREQ  1193180

void pcspkr_beep(u32 frequency_hz, u32 ms)
{
    if (frequency_hz == 0) return;

    /* 计算除数 */
    u32 divisor = PIT_BASE_FREQ / frequency_hz;

    /* 编程 PIT 计数器 2（端口 0x42）：
     * 模式 3（方波），先写低字节再写高字节 */
    outb(0x43, 0xB6);
    outb(0x42, (u8)(divisor & 0xFF));
    outb(0x42, (u8)((divisor >> 8) & 0xFF));

    /* 启用 PC Speaker：
     * port 0x61 bit 0 = 门控计数器 2
     * port 0x61 bit 1 = 扬声器数据 */
    u8 tmp = inb(0x61);
    outb(0x61, tmp | 0x03);

    /* 忙等待（使用 PIT 滴答计数） */
    u32 start = timer_ticks;
    u32 wait_ticks = (ms * 100) / 1000;  /* PIT 100Hz → 每 tick 10ms */
    if (wait_ticks == 0) wait_ticks = 1;
    while ((timer_ticks - start) < wait_ticks) {
        halt();
    }

    /* 关闭扬声器 */
    pcspkr_off();
}

void pcspkr_off(void)
{
    u8 tmp = inb(0x61);
    outb(0x61, tmp & ~0x03);
}

/* ════════════════════════════════════════
   Sound Blaster 16 驱动
   ════════════════════════════════════════ */

/* ── 内部辅助函数 ── */

/* 等待 DSP 可写入（bit 7 = 0 表示可写） */
static int sb16_wait_write(void)
{
    for (int i = 0; i < 100000; i++) {
        if (!(inb(SB16_DSP_WRITE) & 0x80))
            return 0;
    }
    return -1;  /* 超时 */
}

/* 等待 DSP 有数据可读（bit 7 = 1 表示可读） */
static int sb16_wait_read(void)
{
    for (int i = 0; i < 100000; i++) {
        if (inb(SB16_DSP_STATUS) & 0x80)
            return 0;
    }
    return -1;
}

/* 向 DSP 写一个字节 */
static void sb16_write(u8 data)
{
    if (sb16_wait_write() < 0) {
        serial_printf(SERIAL_COM1, "[sb16] write timeout!\n");
        return;
    }
    outb(SB16_DSP_WRITE, data);
}

/* 从 DSP 读一个字节 */
static u8 sb16_read(void)
{
    if (sb16_wait_read() < 0) {
        serial_printf(SERIAL_COM1, "[sb16] read timeout!\n");
        return 0;
    }
    return inb(SB16_DSP_READ);
}

/* ── 配置 DMA（8 位，通道 1） ── */
static void dma_setup_8bit(u32 phys_addr, u32 len)
{
    /* 8 位 DMA 通道 1：
     *   地址寄存器：0x02
     *   计数寄存器：0x03
     *   页寄存器：  0x83
     *   模式：单次，写（内存→IO），通道 1 → 0x49
     */

    /* 清除字节指针触发器 */
    outb(DMA_FLIP_FLOP, 0x00);

    /* 屏蔽 DMA 通道 1 */
    outb(DMA_MASK_REG, 0x05);   /* bit 2=1: 屏蔽通道, bit 1-0: 通道号 01 */

    /* 清除字节指针触发器（再次） */
    outb(DMA_FLIP_FLOP, 0x00);

    /* 设置模式：0x49 = 单次传输, 写(内存→IO), 通道 1 */
    outb(DMA_MODE_REG, 0x49);

    /* 设置地址（物理地址，DMA 使用 20 位地址）：
     * 页寄存器提供高 4 位（A19-A16），地址寄存器提供低 16 位
     * 注意：DMA 的地址必须是字对齐的（偶数地址） */
    u32 page = phys_addr >> 16;
    u16 addr = (u16)(phys_addr & 0xFFFF);

    outb(DMA_ADDR_CH1, (u8)(addr & 0xFF));
    outb(DMA_ADDR_CH1, (u8)((addr >> 8) & 0xFF));

    outb(DMA_PAGE_CH1, (u8)(page & 0xFF));

    /* 设置计数（长度 - 1） */
    u16 count = (u16)(len - 1);
    outb(DMA_COUNT_CH1, (u8)(count & 0xFF));
    outb(DMA_COUNT_CH1, (u8)((count >> 8) & 0xFF));

    /* 取消屏蔽 DMA 通道 1 */
    outb(DMA_MASK_REG, 0x01);   /* bit 2=0: 取消屏蔽, bit 1-0: 通道号 01 */
}

/* ── 初始化 SB16 ── */
int sb16_init(void)
{
    serial_printf(SERIAL_COM1, "[sb16] Initializing Sound Blaster 16...\n");

    /* 1. 复位 DSP：写 1 到复位端口，等待 3μs，写回 0 */
    outb(SB16_DSP_RESET, 1);
    timer_ticks = timer_ticks;  /* 小延时 */
    outb(SB16_DSP_RESET, 0);

    /* 检查 DSP 是否回应（读取 0x0AA） */
    u8 data = sb16_read();
    if (data != 0xAA) {
        serial_printf(SERIAL_COM1, "[sb16] DSP reset failed (got 0x%02x, expected 0xAA)\n", data);
        serial_printf(SERIAL_COM1, "[sb16] Sound Blaster 16 not detected. Enable with QEMU: -soundhw sb16\n");
        return -1;
    }
    serial_printf(SERIAL_COM1, "[sb16] DSP reset OK (0xAA)\n");

    /* 2. 获取 DSP 版本 */
    sb16_write(SB16_CMD_GET_VERSION);
    u8 ver_major = sb16_read();
    u8 ver_minor = sb16_read();
    serial_printf(SERIAL_COM1, "[sb16] DSP version %d.%02d\n", ver_major, ver_minor);

    if (ver_major < 4) {
        serial_printf(SERIAL_COM1, "[sb16] DSP version too old, need 4.00+\n");
        return -1;
    }

    /* 3. 打开 DSP 扬声器 */
    sb16_write(SB16_CMD_SPEAKER_ON);

    serial_printf(SERIAL_COM1, "[sb16] Initialization complete\n");
    return 0;
}

/* ── SB16 中断处理 ── */
void sb16_handler(registers_t *regs)
{
    (void)regs;

    /* 标记 IRQ 已触发 */
    sb16_irq_fired = 1;

    /* DMA 传输完成 → 停止播放 */
    sb16_playing = 0;

    /* 确认 8 位中断：读 DSP Status 端口 */
    u8 status = inb(SB16_DSP_STATUS);
    (void)status;
}

/* ── 播放 8 位 PCM 数据 ── */
int sb16_play_8bit(const u8 *data, u32 len, u32 sample_rate)
{
    if (sb16_playing) {
        serial_printf(SERIAL_COM1, "[sb16] Already playing\n");
        return -1;
    }
    if (len == 0) return -1;

    serial_printf(SERIAL_COM1, "[sb16] Playing %u bytes at %u Hz...\n", len, sample_rate);

    sb16_playing = 1;
    sb16_irq_fired = 0;

    /* 计算时间常数用于设置采样率
     * time_constant = 256 - (1000000 / sample_rate)
     * 对于 22.050kHz: 256 - 45 = 211 = 0xD3
     * 对于 44.100kHz: 256 - 23 = 233 = 0xE9
     * 对于 11.025kHz: 256 - 90 = 166 = 0xA6 */
    u32 time_const = 256 - (1000000 / sample_rate);
    sb16_write(SB16_CMD_SET_TIME);
    sb16_write((u8)(time_const & 0xFF));

    /* 配置 8 位 DMA 通道 1
     * 数据所在物理地址：由于开启了分页，需要确保缓冲区在 16MB 以内
     * 且物理地址 = 虚拟地址（恒等映射区域） */
    u32 phys_addr = (u32)data;  /* 假设数据在恒等映射区域 */
    dma_setup_8bit(phys_addr, len);

    /* 设置自动初始化块大小（如果不使用自动初始化则跳过） */
    if (len > SB16_AUTO_BLOCK) {
        u16 block_size = SB16_AUTO_BLOCK - 1;
        sb16_write(SB16_CMD_SET_BLOCK);
        sb16_write((u8)(block_size & 0xFF));
        sb16_write((u8)((block_size >> 8) & 0xFF));
    }

    /* 发送单次 8 位 DMA 播放命令
     * 输出模式：0x14 + (输出模式 << 4)
     * 输出模式：0x00 = 单声道, 0x20 = 立体声
     * 有符号/无符号：默认 0x00 = 无符号
     *   0x10 = 有符号
     * 标准 8 位单声道无符号单次：0x14 */
    u8 dma_cmd = SB16_CMD_DMA_8BIT;

    /* 编程 DMA 长度（如果使用单次模式）：
     * 发送长度减 1，低字节先发，高字节后发 */
    u16 dma_len = (u16)(len - 1);
    sb16_write(dma_cmd);
    sb16_write((u8)(dma_len & 0xFF));
    sb16_write((u8)((dma_len >> 8) & 0xFF));

    serial_printf(SERIAL_COM1, "[sb16] DMA transfer started\n");

    /* 等待播放完成（忙等待） */
    while (sb16_playing) {
        halt();
    }

    serial_printf(SERIAL_COM1, "[sb16] Playback finished\n");
    return 0;
}

/* ── 停止播放 ── */
void sb16_stop(void)
{
    if (!sb16_playing) return;

    /* 停止 8 位 DMA */
    sb16_write(SB16_CMD_HALT_8BIT);
    sb16_playing = 0;
}

/* ── 检查播放状态 ── */
int sb16_is_playing(void)
{
    return sb16_playing;
}
