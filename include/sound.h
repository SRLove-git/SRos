#ifndef SOUND_H
#define SOUND_H

#include "types.h"
#include "idt.h"

/* ════════════════════════════════════════
   PC Speaker (PIT 计数器 2)
   ════════════════════════════════════════ */

/* 播放指定频率（Hz）的蜂鸣音，持续 ms 毫秒（忙等待） */
void pcspkr_beep(u32 frequency_hz, u32 ms);

/* 停止 PC Speaker */
void pcspkr_off(void);

/* ════════════════════════════════════════
   Sound Blaster 16
   ════════════════════════════════════════ */

/* SB16 I/O 基址 */
#define SB16_BASE         0x220

/* SB16 寄存器偏移 */
#define SB16_DSP_RESET    (SB16_BASE + 6)    /* 0x226: DSP 复位 */
#define SB16_DSP_READ     (SB16_BASE + 0xA)  /* 0x22A: 读取 DSP 数据 */
#define SB16_DSP_WRITE    (SB16_BASE + 0xC)  /* 0x22C: 写 DSP 命令/数据 */
#define SB16_DSP_STATUS   (SB16_BASE + 0xE)  /* 0x22E: 读状态 / 16位中断确认 */

/* DSP 命令 */
#define SB16_CMD_SPEAKER_ON    0xD1
#define SB16_CMD_SPEAKER_OFF   0xD3
#define SB16_CMD_GET_VERSION   0xE1
#define SB16_CMD_SET_TIME      0x40    /* 设置采样率时间常数 */
#define SB16_CMD_SET_BLOCK     0x48    /* 设置自动初始化块大小 */
#define SB16_CMD_DMA_8BIT      0x14    /* 8位单次 DMA 播放 */
#define SB16_CMD_DMA_8BIT_AUTO 0x1C    /* 8位自动初始化 DMA 播放 */
#define SB16_CMD_DMA_16BIT     0xB0    /* 16位单次 DMA 播放 */
#define SB16_CMD_DMA_16BIT_AUTO 0xBC   /* 16位自动初始化 DMA 播放 */
#define SB16_CMD_PAUSE_8BIT    0xD0    /* 暂停 8位 DMA */
#define SB16_CMD_CONT_8BIT     0xD4    /* 继续 8位 DMA */
#define SB16_CMD_HALT_8BIT     0xD5    /* 停止 8位 DMA */
#define SB16_CMD_PAUSE_16BIT   0xD0    /* 暂停 16位 DMA */
#define SB16_CMD_CONT_16BIT    0xD6    /* 继续 16位 DMA */
#define SB16_CMD_HALT_16BIT    0xD9    /* 停止 16位 DMA */

/* DMA 控制器端口 */
#define DMA_ADDR_CH1     0x02     /* 8位 DMA 通道 1 地址 */
#define DMA_COUNT_CH1    0x03     /* 8位 DMA 通道 1 计数 */
#define DMA_PAGE_CH1     0x83     /* 8位 DMA 通道 1 页寄存器 */
#define DMA_MASK_REG     0x0A     /* DMA 掩码寄存器 */
#define DMA_MODE_REG     0x0B     /* DMA 模式寄存器 */
#define DMA_FLIP_FLOP    0x0C     /* 清除字节指针触发器 */
#define DMA_CLR_MASK     0x0E     /* 清除所有掩码 */

/* SB16 采样率 */
#define SB16_RATE_11025  11025
#define SB16_RATE_22050  22050
#define SB16_RATE_44100  44100

/* 音频缓冲区大小（8位 PCM 数据） */
#define SB16_BUF_SIZE     65536
#define SB16_AUTO_BLOCK   32768    /* 自动初始化块大小 */

/* ── 函数声明 ── */

/* 初始化 SB16，检测 DSP 版本 */
int sb16_init(void);

/* SB16 中断处理 */
void sb16_handler(registers_t *regs);

/* 播放 8 位单声道 PCM 数据（阻塞，播放完成后返回） */
int sb16_play_8bit(const u8 *data, u32 len, u32 sample_rate);

/* 停止播放 */
void sb16_stop(void);

/* 检查是否正在播放 */
int sb16_is_playing(void);

#endif /* SOUND_H */
