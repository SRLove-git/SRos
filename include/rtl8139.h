#ifndef RTL8139_H
#define RTL8139_H

#include "types.h"

/* RTL8139 寄存器偏移（I/O 模式） */
#define RTL_REG_IDR0        0x00   /* MAC 地址 (6 字节) */
#define RTL_REG_IDR4        0x04   /* MAC 地址高 2 字节 */
#define RTL_REG_MAR0        0x08   /* 组播地址 */
#define RTL_REG_TX_STATUS0  0x10   /* TX 状态 0 */
#define RTL_REG_TX_STATUS1  0x14   /* TX 状态 1 */
#define RTL_REG_TX_STATUS2  0x18   /* TX 状态 2 */
#define RTL_REG_TX_STATUS3  0x1C   /* TX 状态 3 */
#define RTL_REG_TX_ADDR0    0x20   /* TX 缓冲区地址 0 */
#define RTL_REG_TX_ADDR1    0x24   /* TX 缓冲区地址 1 */
#define RTL_REG_TX_ADDR2    0x28   /* TX 缓冲区地址 2 */
#define RTL_REG_TX_ADDR3    0x2C   /* TX 缓冲区地址 3 */
#define RTL_REG_RBSTART     0x30   /* RX 缓冲区起始地址 */
#define RTL_REG_ERBCR       0x34   /* 早期 RX 字节计数 */
#define RTL_REG_CR          0x37   /* 命令寄存器 (1 字节) */
#define RTL_REG_CAPR        0x38   /* 当前读取包的地址 */
#define RTL_REG_CBR         0x3A   /* 当前写入地址 */
#define RTL_REG_IMR         0x3C   /* 中断屏蔽寄存器 (2 字节) */
#define RTL_REG_ISR         0x3E   /* 中断状态寄存器 (2 字节) */
#define RTL_REG_TCR          0x40   /* 发送配置寄存器 */
#define RTL_REG_RCR          0x44   /* 接收配置寄存器 */
#define RTL_REG_CONFIG1      0x52   /* 配置 1 */
#define RTL_REG_MSR          0x58   /* 介质状态寄存器 */

/* 命令寄存器 (CR) 位 */
#define RTL_CR_RST      0x10   /* 软件复位 */
#define RTL_CR_RE       0x08   /* 接收使能 */
#define RTL_CR_TE       0x04   /* 发送使能 */

/* 中断状态/屏蔽位 */
#define RTL_ISR_ROK     0x0001  /* 接收完成 */
#define RTL_ISR_TOK     0x0004  /* 发送完成 */
#define RTL_ISR_RERR    0x0010  /* 接收错误 */
#define RTL_ISR_TERR    0x0020  /* 发送错误 */
#define RTL_ISR_SERR    0x8000  /* 系统错误 */

/* 接收配置寄存器 (RCR) 位 */
#define RTL_RCR_WRAP    0x80    /* 允许缓冲环绕 */
#define RTL_RCR_AB      0x08    /* 接收广播 */
#define RTL_RCR_AM      0x04    /* 接收组播 */
#define RTL_RCR_APM     0x02    /* 接收物理地址匹配 */
#define RTL_RCR_AAP     0x01    /* 接收所有帧 */

/* 发送配置寄存器 (TCR) 位 */
#define RTL_TCR_CLRABT  0x01    /* 清除发送中止 */

/* RX 包头部 (4 字节前缀) */
typedef struct {
    u16 status;     /* 接收状态 */
    u16 size;       /* 实际数据长度 (包括 4 字节 CRC) */
} __attribute__((packed)) rtl8139_rx_header_t;

#define RTL8139_RX_BUF_SIZE    (8192 + 16)  /* 典型 RX 环缓冲区大小 */
#define RTL8139_TX_BUF_SIZE    2048
#define RTL8139_NUM_TX_BUFS    4

/* 初始化 */
int rtl8139_init(void);

/* 发送以太网帧 */
int rtl8139_send(const u8 *data, u32 len);

/* 接收回调处理 */
void rtl8139_handle_irq(void);

/* 获取 MAC 地址 */
void rtl8139_get_mac(u8 *mac);

/* 打印网卡状态信息 */
void rtl8139_dump_status(void);

/* 全局状态 — 方便其他模块读取 */
extern u8 rtl8139_mac[6];
extern u8 rtl8139_irq;             /* RTL8139 使用的 IRQ 号 */
extern volatile int rtl8139_has_packet;
extern volatile u32 rtl8139_packet_count;

#endif /* RTL8139_H */
