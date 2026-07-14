#ifndef RAMDISK_H
#define RAMDISK_H

#include "types.h"

#define BLOCK_SIZE      4096
#define DISK_BLOCKS     1024

void ramdisk_init(void);
void block_read(u32 block_no, u8 *buf);
void block_write(u32 block_no, const u8 *buf);

#endif
