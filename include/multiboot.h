#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include "types.h"

/* Multiboot 1 信息结构体 — 由 GRUB/QEMU 填充并传给内核 */
typedef struct {
    u32 flags;

    /* 可用内存（仅在 flags[0] 有效） */
    u32 mem_lower;
    u32 mem_upper;

    /* 启动设备（仅在 flags[1] 有效） */
    u32 boot_device;

    /* 命令行（仅在 flags[2] 有效） */
    u32 cmdline;

    /* 模块数量及地址（仅在 flags[3] 有效） */
    u32 mods_count;
    u32 mods_addr;

    /* ELF 符号表（仅在 flags[5] 有效） */
    u32 syms_num;
    u32 syms_size;
    u32 syms_addr;
    u32 syms_shndx;

    /* 内存映射（仅在 flags[6] 有效） */
    u32 mmap_length;
    u32 mmap_addr;

    /* 驱动器信息（仅在 flags[7] 有效） */
    u32 drives_length;
    u32 drives_addr;

    /* ROM 配置表（仅在 flags[8] 有效） */
    u32 config_table;

    /* 引导加载器名称（仅在 flags[9] 有效） */
    u32 boot_loader_name;

    /* APM 表（仅在 flags[10] 有效） */
    u32 apm_table;

    /* VBE 控制信息（仅在 flags[11] 有效） */
    u32 vbe_control_info;
    u32 vbe_mode_info;
    u16 vbe_mode;
    u16 vbe_interface_seg;
    u16 vbe_interface_off;
    u16 vbe_interface_len;

    /* 帧缓冲信息（仅在 flags[12] 有效） */
    u64 framebuffer_addr;
    u32 framebuffer_pitch;
    u32 framebuffer_width;
    u32 framebuffer_height;
    u8  framebuffer_bpp;
    u8  framebuffer_type;
    u8  color_info[6];
} __attribute__((packed)) multiboot_info_t;

/* 帧缓冲类型 */
#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED   0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB       1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT  2

#endif /* MULTIBOOT_H */
