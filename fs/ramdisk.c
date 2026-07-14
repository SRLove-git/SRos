#include "ramdisk.h"
#include "ata.h"
#include "serial.h"

void ramdisk_init(void)
{
    if (ata_init() < 0) {
        serial_printf(SERIAL_COM1, "[disk] WARNING: No ATA disk found!\n");
    } else {
        serial_printf(SERIAL_COM1, "[disk] ATA disk ready\n");
    }
}

/* 每个 FS 块 = 8 个 ATA 扇区 */
#define SECTORS_PER_BLOCK  (BLOCK_SIZE / ATA_SECTOR_SIZE)

void block_read(u32 block_no, u8 *buf)
{
    if (block_no >= DISK_BLOCKS)
        return;

    u32 lba = block_no * SECTORS_PER_BLOCK;
    int ret = ata_read_sectors(lba, SECTORS_PER_BLOCK, buf);
    if (ret < 0) {
        serial_printf(SERIAL_COM1, "[disk] ERROR: block_read(%d) failed\n", block_no);
        for (u32 i = 0; i < BLOCK_SIZE; i++)
            buf[i] = 0;
    }
}

void block_write(u32 block_no, const u8 *buf)
{
    if (block_no >= DISK_BLOCKS)
        return;

    u32 lba = block_no * SECTORS_PER_BLOCK;
    int ret = ata_write_sectors(lba, SECTORS_PER_BLOCK, buf);
    if (ret < 0) {
        serial_printf(SERIAL_COM1, "[disk] ERROR: block_write(%d) failed\n", block_no);
    }
}
