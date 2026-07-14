#include "ata.h"
#include "io.h"
#include "serial.h"

/* 等待 BSY 清零 */
static int ata_wait_busy(void)
{
    u32 timeout = 30000000;  /* 约 30 秒 @ 100ns/循环 */
    while (--timeout) {
        if (!(inb(ATA_PRIMARY_ALTSTAT) & ATA_STATUS_BSY))
            return 0;
    }
    return -1;  /* 超时 */
}

/* 等待 DRQ 置位（同时检查 ERR） */
static int ata_wait_drq(void)
{
    if (ata_wait_busy() < 0)
        return -1;

    u32 timeout = 30000000;
    while (--timeout) {
        u8 st = inb(ATA_PRIMARY_ALTSTAT);
        if (st & ATA_STATUS_ERR)
            return -1;
        if (st & ATA_STATUS_DRQ)
            return 0;
    }
    return -1;
}

/* 400ns 延时：连续读 4 次 alt status */
static void ata_delay(void)
{
    inb(ATA_PRIMARY_ALTSTAT);
    inb(ATA_PRIMARY_ALTSTAT);
    inb(ATA_PRIMARY_ALTSTAT);
    inb(ATA_PRIMARY_ALTSTAT);
}

/* 检测并初始化 ATA 驱动器 */
int ata_init(void)
{
    serial_printf(SERIAL_COM1, "[ata] Initializing...\n");

    /* 选择主盘 */
    outb(ATA_PRIMARY_DRIVE, ATA_DRIVE_MASTER);
    ata_delay();

    /* 如果检测到不存在，下面的 read 会返回 0xFF */
    if (inb(ATA_PRIMARY_CMD) == 0xFF) {
        serial_printf(SERIAL_COM1, "[ata] No device on primary master\n");
        return -1;
    }

    /* 发送 IDENTIFY */
    outb(ATA_PRIMARY_SECTORS, 0);
    outb(ATA_PRIMARY_LBA_LO,   0);
    outb(ATA_PRIMARY_LBA_MID,  0);
    outb(ATA_PRIMARY_LBA_HI,   0);
    outb(ATA_PRIMARY_CMD, ATA_CMD_IDENTIFY);
    ata_delay();

    /* 等待 */
    if (ata_wait_drq() < 0) {
        serial_printf(SERIAL_COM1, "[ata] IDENTIFY failed\n");
        return -1;
    }

    /* 读取 256 字（512 字节）的识别数据 */
    u16 ident[256];
    for (int i = 0; i < 256; i++)
        ident[i] = inw(ATA_PRIMARY_DATA);

    /* 检查是否为 ATA 设备（不是 ATAPI） */
    if (ident[0] & 0x8000) {
        serial_printf(SERIAL_COM1, "[ata] Not an ATA device (ATAPI)\n");
        return -1;
    }

    /* 读取 LBA28 可寻址扇区数（字 60-61） */
    u32 total_sectors = ident[60] | ((u32)ident[61] << 16);

    /* 读取型号字符串（字 27-46，每个字高低字节反转） */
    char model[41];
    for (int i = 0; i < 20; i++) {
        model[i * 2]     = ident[27 + i] >> 8;    /* 高位字节 */
        model[i * 2 + 1] = ident[27 + i] & 0xFF;  /* 低位字节 */
    }
    model[40] = '\0';

    /* 去掉前后空格 */
    int start = 0, end = 39;
    while (start <= end && model[start] == ' ') start++;
    while (end >= start && model[end]   == ' ') end--;
    model[end + 1] = '\0';

    serial_printf(SERIAL_COM1, "[ata] Detected: %s (%u MB)\n",
                  model + start, total_sectors / 2048);
    return 0;
}

/* 读取 count 个扇区（最多 255）从 LBA 开始到 buf */
int ata_read_sectors(u32 lba, u8 count, u8 *buf)
{
    if (count == 0) return 0;

    if (ata_wait_busy() < 0)
        return -1;

    outb(ATA_PRIMARY_SECTORS, count);
    outb(ATA_PRIMARY_LBA_LO,  (u8)(lba));
    outb(ATA_PRIMARY_LBA_MID, (u8)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI,  (u8)(lba >> 16));
    outb(ATA_PRIMARY_DRIVE,   ATA_DRIVE_MASTER | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_CMD, ATA_CMD_READ);

    for (u8 s = 0; s < count; s++) {
        if (ata_wait_drq() < 0)
            return -1;

        for (int i = 0; i < 256; i++) {
            u16 data = inw(ATA_PRIMARY_DATA);
            buf[s * ATA_SECTOR_SIZE + i * 2]     = (u8)data;
            buf[s * ATA_SECTOR_SIZE + i * 2 + 1] = (u8)(data >> 8);
        }
    }

    return (int)count * ATA_SECTOR_SIZE;
}

/* 写入 count 个扇区到磁盘 */
int ata_write_sectors(u32 lba, u8 count, const u8 *buf)
{
    if (count == 0) return 0;

    if (ata_wait_busy() < 0)
        return -1;

    outb(ATA_PRIMARY_SECTORS, count);
    outb(ATA_PRIMARY_LBA_LO,  (u8)(lba));
    outb(ATA_PRIMARY_LBA_MID, (u8)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI,  (u8)(lba >> 16));
    outb(ATA_PRIMARY_DRIVE,   ATA_DRIVE_MASTER | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_CMD, ATA_CMD_WRITE);

    for (u8 s = 0; s < count; s++) {
        if (ata_wait_drq() < 0)
            return -1;

        for (int i = 0; i < 256; i++) {
            u16 data = (u16)buf[s * ATA_SECTOR_SIZE + i * 2]
                     | ((u16)buf[s * ATA_SECTOR_SIZE + i * 2 + 1] << 8);
            outw(ATA_PRIMARY_DATA, data);
        }
    }

    /* 等待写入完成（刷新缓存） */
    if (ata_wait_busy() < 0)
        return -1;

    return (int)count * ATA_SECTOR_SIZE;
}
